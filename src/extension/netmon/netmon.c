/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/**
 * Network monitoring.
 *
 * PRoot runs a small HTTP-proxy-ish relay on the loopback address and
 * redirects the connect(2) of the tracees towards it, so that their
 * outbound traffic can be observed and accounted for without the
 * guest's cooperation.  What the relay sees is then reported back to
 * the guest as a fake /proc/net/dev.
 *
 * The redirection is deliberately narrow: only AF_INET and AF_INET6
 * destinations outside the loopback and link-local ranges, and only
 * those whose port is on the monitored list, are diverted.  Everything
 * else -- above all datagrams -- keeps going straight to its
 * destination, so the cost of a wrong guess is a slightly incomplete
 * report rather than a broken connection.
 */

#include <stdio.h>      /* snprintf(3), */
#include <string.h>     /* str*(3), mem*(3), */
#include <errno.h>      /* EINPROGRESS, */
#include <stdlib.h>     /* getenv(3), */
#include <sys/socket.h> /* AF_INET, */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_LOOPBACK, */
#include <arpa/inet.h>  /* ntohs(3), ntohl(3), */
#include <linux/limits.h> /* PATH_MAX, */

#include "cli/note.h"
#include "extension/extension.h"
#include "extension/netmon/netmon.h"
#include "path/temp.h"
#include "tracee/mem.h"
#include "tracee/reg.h"
#include "syscall/sysnum.h"

static FilteredSysnum filtered_sysnums[] = {
	/* The exit stage is needed to check whether the kernel
	 * accepted the redirected connection.  */
	{ PR_connect,		FILTER_SYSEXIT },
	FILTERED_SYSNUM_END,
};

/**
 * Return the extension state, or NULL if the proxy is not running and
 * the guest's networking has to be left alone.
 */
static NetmonState *get_running_state(Extension *extension)
{
	NetmonState *state = talloc_get_type_abort(extension->config, NetmonState);

	if (state->shared == NULL)
		return NULL;
	if (__atomic_load_n(&state->shared->ready, __ATOMIC_ACQUIRE) == 0)
		return NULL;

	return state;
}

/**
 * Return true if @address is one this extension must never divert:
 * the loopback and the unspecified addresses are where the guest
 * reaches services that have nothing to do with the internet, and
 * diverting them would break local clients for no monitoring gain.
 */
static bool is_local_destination(int family, const void *raw_address)
{
	if (family == AF_INET) {
		const struct sockaddr_in *in = raw_address;
		uint32_t host = ntohl(in->sin_addr.s_addr);

		if (host == 0)
			return true;
		if ((host >> 24) == 127)
			return true;
		if ((host >> 16) == 0xA9FE)   /* 169.254.0.0/16, */
			return true;
	}
	else {
		const struct sockaddr_in6 *in6 = raw_address;

		if (IN6_IS_ADDR_UNSPECIFIED(&in6->sin6_addr))
			return true;
		if (IN6_IS_ADDR_LOOPBACK(&in6->sin6_addr))
			return true;
		if (IN6_IS_ADDR_LINKLOCAL(&in6->sin6_addr))
			return true;
	}

	return false;
}

/**
 * Reserve a free hand-off entry for a connection that is about to be
 * redirected.  This function returns the index of the entry, or -1 if
 * every slot is busy.
 */
static int claim_slot(NetmonShared *shared)
{
	int i;

	for (i = 0; i < NETMON_MAX_CONNS; i++) {
		if (__atomic_load_n(&shared->pending[i].used, __ATOMIC_ACQUIRE) == 0)
			return i;
	}

	return -1;
}

/** Give a hand-off entry back to the pool, see claim_slot().  */
static void release_slot(NetmonShared *shared, int slot)
{
	if (slot < 0 || slot >= NETMON_MAX_CONNS)
		return;

	__atomic_store_n(&shared->pending[slot].used, 0, __ATOMIC_RELEASE);
}

/**
 * Remember that the socket @fd of @tracee is bound to @destination
 * rather than to the proxy, so that getpeername(2) can be answered
 * with the address the guest asked for.
 */
static void remember_socket(NetmonState *state, Tracee *tracee, int fd,
			const NetmonPending *destination)
{
	NetmonSocket *socket = NULL;
	int i;

	for (i = 0; i < NETMON_SOCKET_MAX; i++) {
		if (state->sockets[i].pid == tracee->pid && state->sockets[i].fd == fd) {
			socket = &state->sockets[i];
			break;
		}
	}

	if (socket == NULL) {
		for (i = 0; i < NETMON_SOCKET_MAX; i++) {
			if (state->sockets[i].pid == 0) {
				socket = &state->sockets[i];
				break;
			}
		}
		/* Every entry is taken by a live process: reuse the
		 * first one.  A stale record only costs one wrong
		 * getpeername(2) answer.  */
		if (socket == NULL)
			socket = &state->sockets[0];
	}

	socket->pid = tracee->pid;
	socket->fd = fd;
	socket->family = destination->family;
	socket->port = destination->dst_port;
	memcpy(socket->addr, destination->addr, sizeof(socket->addr));
}

/** Return the record of the socket @fd, or NULL.  */
static const NetmonSocket *find_socket(const NetmonState *state,
			const Tracee *tracee, int fd)
{
	int i;

	for (i = 0; i < NETMON_SOCKET_MAX; i++) {
		if (state->sockets[i].pid == tracee->pid && state->sockets[i].fd == fd)
			return &state->sockets[i];
	}

	return NULL;
}

/** Stop attributing traffic to a socket the guest is done with.  */
static void forget_socket(NetmonState *state, const Tracee *tracee, int fd)
{
	int i;

	for (i = 0; i < NETMON_SOCKET_MAX; i++) {
		if (state->sockets[i].pid == tracee->pid && state->sockets[i].fd == fd) {
			state->sockets[i].pid = 0;
			state->sockets[i].fd = -1;
			return;
		}
	}
}

/**
 * Point the sockaddr the guest is connecting with at the loopback
 * proxy, and publish the real destination so that the proxy thread can
 * pick it up.  This function returns true if the connection has been
 * redirected.
 */
static bool redirect_connect(NetmonState *state, Tracee *tracee, word_t address)
{
	NetmonShared *shared = state->shared;
	struct sockaddr_storage original;
	struct sockaddr_storage redirected;
	NetmonPending *destination;
	unsigned int port;
	int listener;
	int size;
	int slot;
	int family;
	int fd;

	fd = (int) peek_reg(tracee, CURRENT, SYSARG_1);
	size = (int) peek_reg(tracee, CURRENT, SYSARG_3);

	/* Never touch the guest's buffer before knowing which
	 * structure it holds, and never more of it than the biggest
	 * address we know about.  */
	if (address == 0 || size < (int) sizeof(sa_family_t))
		return false;
	if (size > (int) sizeof(struct sockaddr_in6))
		size = (int) sizeof(struct sockaddr_in6);

	if (read_data(tracee, &original, address, (word_t) size) < 0)
		return false;

	family = original.ss_family;
	if (family == AF_INET) {
		if (size < (int) sizeof(struct sockaddr_in))
			return false;
		port = ntohs(((const struct sockaddr_in *) (const void *) &original)->sin_port);
	}
	else if (family == AF_INET6) {
		if (size < (int) sizeof(struct sockaddr_in6))
			return false;
		port = ntohs(((const struct sockaddr_in6 *) (const void *) &original)->sin6_port);
	}
	else {
		/* AF_UNIX, AF_NETLINK, ... are none of our business.  */
		return false;
	}

	if (is_local_destination(family, &original))
		return false;
	if (!netmon_port_is_monitored(port))
		return false;

	/* Listener 2 * slot serves IPv4, 2 * slot + 1 serves IPv6.  */
	listener = (family == AF_INET) ? 0 : 1;
	slot = claim_slot(shared);
	if (slot < 0) {
		/* All the slots are taken: leave this connection
		 * alone rather than delaying it.  */
		VERBOSE(tracee, 1, "netmon: no free slot left, connect(%d) on "
			"port %u is not monitored", fd, port);
		return false;
	}

	if (shared->listen_fd[2 * slot + listener] < 0) {
		release_slot(shared, slot);
		return false;
	}

	/* Publish the destination before the kernel is allowed to
	 * complete the connection: the proxy thread can be woken up by
	 * the handshake as soon as it is.  */
	destination = &shared->pending[slot];
	destination->family = family;
	if (family == AF_INET) {
		const struct sockaddr_in *in = (const struct sockaddr_in *) (const void *) &original;

		destination->dst_port = in->sin_port;
		memcpy(destination->addr, &in->sin_addr, sizeof(in->sin_addr));
	}
	else {
		const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *) (const void *) &original;

		destination->dst_port = in6->sin6_port;
		memcpy(destination->addr, &in6->sin6_addr, sizeof(in6->sin6_addr));
	}

	redirected = original;
	if (family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *) (void *) &redirected;

		in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		in->sin_port = htons((uint16_t) shared->slot_port[slot]);
		size = (int) sizeof(struct sockaddr_in);
	}
	else {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) (void *) &redirected;

		in6->sin6_addr = in6addr_loopback;
		in6->sin6_port = htons((uint16_t) shared->slot_port[slot]);
		size = (int) sizeof(struct sockaddr_in6);
	}

	/* Leave the guest's address alone if its buffer turns out not
	 * to be writable.  */
	if (write_data(tracee, address, &redirected, (word_t) size) < 0) {
		release_slot(shared, slot);
		return false;
	}

	__atomic_store_n(&destination->used, 1, __ATOMIC_RELEASE);

	state->connecting.fd = fd;
	state->connecting.slot = slot;
	state->connecting.dest = *destination;

	VERBOSE(tracee, 2, "netmon: connect(%d, %s port %u) redirected to slot %d",
		fd, (family == AF_INET) ? "inet" : "inet6", port, slot);

	return true;
}

/**
 * The connect(2) reached its exit stage: either the proxy thread will
 * pick the connection up, or nobody ever will and the hand-off entry
 * has to be given back.
 */
static void redirect_connect_exit(NetmonState *state, Tracee *tracee)
{
	NetmonShared *shared = state->shared;
	int result;
	int slot;

	slot = state->connecting.slot;
	if (slot < 0)
		return;

	result = (int) peek_reg(tracee, CURRENT, SYSARG_RESULT);

	/* A non-blocking connect(2) legitimately reports that it is
	 * still in progress.  */
	if (result >= 0 || result == -EINPROGRESS) {
		remember_socket(state, tracee, state->connecting.fd,
				&state->connecting.dest);
		VERBOSE(tracee, 2, "netmon: slot %d handed over (result %d)",
			slot, result);
	}
	else {
		VERBOSE(tracee, 2, "netmon: slot %d released (result %d)",
			slot, result);
		release_slot(shared, slot);
	}

	state->connecting.slot = -1;
	state->connecting.fd = -1;
}

/**
 * Answer getpeername(2) with the address the guest asked for rather
 * than with the one of the proxy its socket has been redirected to.
 */
static void fix_up_getpeername(NetmonState *state, Tracee *tracee)
{
	const NetmonSocket *socket;
	struct sockaddr_storage address;
	word_t len_addr;
	word_t len;
	int size;
	int fd;

	if ((int) peek_reg(tracee, CURRENT, SYSARG_RESULT) < 0)
		return;

	fd = (int) peek_reg(tracee, ORIGINAL, SYSARG_1);
	socket = find_socket(state, tracee, fd);
	if (socket == NULL)
		return;

	/* PRoot remembered the size the guest offered in the 6th
	 * argument during the enter stage.  */
	len = peek_reg(tracee, MODIFIED, SYSARG_6);
	if (len > sizeof(address))
		len = sizeof(address);
	if (len < (word_t) sizeof(sa_family_t))
		return;

	memset(&address, 0, sizeof(address));
	if (socket->family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *) (void *) &address;

		if (len < (word_t) sizeof(struct sockaddr_in))
			return;

		in->sin_family = AF_INET;
		in->sin_port = socket->port;
		memcpy(&in->sin_addr, socket->addr, sizeof(in->sin_addr));
		size = (int) sizeof(struct sockaddr_in);
	}
	else {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) (void *) &address;

		if (len < (word_t) sizeof(struct sockaddr_in6))
			return;

		in6->sin6_family = AF_INET6;
		in6->sin6_port = socket->port;
		memcpy(&in6->sin6_addr, socket->addr, sizeof(in6->sin6_addr));
		size = (int) sizeof(struct sockaddr_in6);
	}

	if (write_data(tracee, peek_reg(tracee, ORIGINAL, SYSARG_2),
				&address, (word_t) size) < 0)
		return;

	len_addr = peek_reg(tracee, ORIGINAL, SYSARG_3);
	if (len_addr != 0)
		(void) write_data(tracee, len_addr, &size, sizeof(size));
}

/**
 * Make the guest's open(2) of /proc/net/dev land on a file that
 * reports what the proxy has seen so far.  @context is the talloc
 * context the backing file is attached to.
 */
static void fake_netdev(NetmonState *state, TALLOC_CTX *context, char *path)
{
	if (state->netdev_path == NULL) {
		const char *name = create_temp_file(context, "netdev");

		if (name == NULL)
			return;
		state->netdev_path = talloc_strdup(context, name);
		if (state->netdev_path == NULL)
			return;
	}

	/* The same file is reused for every open, so that a program
	 * polling /proc/net/dev cannot fill the temporary directory.  */
	if (netmon_netdev_write(state, state->netdev_path) < 0)
		return;

	strncpy(path, state->netdev_path, PATH_MAX - 1);
	path[PATH_MAX - 1] = '\0';
}

int netmon_callback(Extension *extension, ExtensionEvent event,
		intptr_t data1, intptr_t data2)
{
	Tracee *tracee = TRACEE(extension);
	NetmonState *state;

	switch (event) {
	case INITIALIZATION: {
		const char *log_path = getenv("PROOT_NETMON_LOG");

		extension->config = talloc_zero(extension, NetmonState);
		if (extension->config == NULL)
			return -1;

		state = talloc_get_type_abort(extension->config, NetmonState);
		state->connecting.slot = -1;
		state->connecting.fd = -1;
		state->owns_proxy = true;

		if (netmon_proxy_start(state, log_path) < 0) {
			note(tracee, WARNING, USER, "netmon: the proxy could not "
				"be started, no traffic will be monitored");
			return -1;
		}

		extension->filtered_sysnums = filtered_sysnums;

		note(tracee, INFO, USER, "netmon: the traffic of the tracees is "
			"relayed through a local proxy and reported in "
			NETMON_NETDEV_PATH);

		return 0;
	}

	case INHERIT_PARENT:
		/* The proxy and its counters are process-wide, but the
		 * bookkeeping below is not, so give the child its own
		 * state pointing at the same shared memory.  */
		return 1;

	case INHERIT_CHILD: {
		Extension *parent = (Extension *) data1;
		NetmonState *parent_state;

		extension->config = talloc_zero(extension, NetmonState);
		if (extension->config == NULL)
			return -1;

		parent_state = talloc_get_type_abort(parent->config, NetmonState);

		state = talloc_get_type_abort(extension->config, NetmonState);
		state->shared = parent_state->shared;
		state->connecting.slot = -1;
		state->connecting.fd = -1;
		state->owns_proxy = false;

		extension->filtered_sysnums = filtered_sysnums;

		return 0;
	}

	case REMOVED: {
		/* Only the extension that started the proxy may stop it:
		 * every other one just shares its memory.  */
		state = talloc_get_type_abort(extension->config, NetmonState);
		if (state->owns_proxy)
			netmon_proxy_stop(state);
		return 0;
	}

	case GUEST_PATH: {
		const char *user_path = (const char *) data2;
		const char *base = (const char *) data1;

		state = talloc_get_type_abort(extension->config, NetmonState);
		state->faking_netdev = false;

		/* Only the paths that are opened are of interest here,
		 * and restricting the check to the opening syscalls
		 * keeps this out of the way of the hundreds of path
		 * translations PRoot does for everything else.  */
		switch (get_sysnum(tracee, ORIGINAL)) {
		case PR_open:
		case PR_openat:
		case PR_openat2:
			break;
		default:
			return 0;
		}

		if (user_path == NULL)
			return 0;

		if (user_path[0] == '/') {
			state->faking_netdev =
				(strcmp(user_path, NETMON_NETDEV_PATH) == 0);
		}
		else if (base != NULL) {
			/* Relative to the current working directory,
			 * which is a guest path too.  */
			char resolved[PATH_MAX];

			if (snprintf(resolved, sizeof(resolved), "%s/%s",
					base, user_path) < (int) sizeof(resolved))
				state->faking_netdev =
					(strcmp(resolved, NETMON_NETDEV_PATH) == 0);
		}

		return 0;
	}

	case TRANSLATED_PATH:
		state = talloc_get_type_abort(extension->config, NetmonState);
		if (!state->faking_netdev)
			return 0;

		state->faking_netdev = false;
		fake_netdev(state, extension->config, (char *) data1);

		return 0;

	case SYSCALL_ENTER_END: {
		NetmonState *running = get_running_state(extension);

		switch (get_sysnum(tracee, ORIGINAL)) {
		case PR_connect:
			if (running != NULL)
				(void) redirect_connect(running, tracee,
					peek_reg(tracee, CURRENT, SYSARG_2));
			break;

		case PR_close:
			if (running != NULL)
				forget_socket(running, tracee,
					(int) peek_reg(tracee, CURRENT, SYSARG_1));
			break;

		default:
			break;
		}

		return 0;
	}

	case SYSCALL_EXIT_END: {
		state = talloc_get_type_abort(extension->config, NetmonState);

		switch (get_sysnum(tracee, ORIGINAL)) {
		case PR_connect:
			redirect_connect_exit(state, tracee);
			break;

		case PR_getpeername: {
			NetmonState *running = get_running_state(extension);

			if (running != NULL)
				fix_up_getpeername(running, tracee);
			break;
		}

		default:
			break;
		}

		return 0;
	}

	default:
		return 0;
	}
}
