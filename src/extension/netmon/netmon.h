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

#ifndef NETMON_H
#define NETMON_H

#include <stdbool.h>    /* bool, */
#include <stdint.h>     /* uint*_t, */
#include <pthread.h>    /* pthread_*, */
#include <netinet/in.h> /* struct sockaddr_storage, */
#include <time.h>       /* time_t, */

#include "extension/extension.h"
#include "tracee/tracee.h"
#include "arch.h"

/* Guest path whose content is synthesized by this extension.  */
#define NETMON_NETDEV_PATH  "/proc/net/dev"

/* Size of the buffer carved off the tracee's stack to carry a
 * sockaddr between the two stages of an interposed connect(2).  */
#define NETMON_SCRATCH_SIZE 128

/* How many host interfaces the fake /proc/net/dev can report.  The
 * first entry is always the loopback device, as the kernel does.  */
#define NETMON_MAX_IFACES   8
#define NETMON_IFNAMSIZ     16

/* How many connections may be relayed at the same time.  Each of them
 * gets its own loopback listening port, see the comment on
 * @NetmonPending.  */
#define NETMON_MAX_CONNS    32

/* Number of listening sockets: one per connection, for each address
 * family, so that a redirected connect(2) keeps its family.  */
#define NETMON_NB_LISTENERS (2 * NETMON_MAX_CONNS)

/* How many proxied sockets are remembered, so that getpeername(2) can
 * hand back the address the guest actually asked for.  */
#define NETMON_SOCKET_MAX   64

/* Destination ports that are worth intercepting, overridable through
 * the PROOT_NETMON_PORTS environment variable.  The default list is
 * deliberately restricted to proxy-shaped services: PRoot has no way
 * to tell a TCP socket from a UDP one without spending a chained
 * getsockopt(2) on every single connect(2), and a datagram aimed at,
 * say, 53/123/1900 must never be silently diverted.  */
#define NETMON_DEFAULT_PORTS "80,443,3128,8000,8080,8443,8888"

/**
 * The real destination of one redirected connection.
 *
 * Handing it over to the proxy thread is the delicate part of this
 * extension.  The obvious way -- keying the entry by the local port the
 * kernel assigns during connect(2) -- needs a second syscall in the
 * tracee, because only the tracee can run getsockname(2) on its own
 * socket, and PRoot has no clean way to interpose one.
 *
 * Instead each in-flight connection gets its own loopback listening
 * port, so the proxy thread recognises the destination of a connection
 * it just accepted from the accepted socket's own address, as soon as
 * accept(2) returns it.  The tracer fills a free entry before it lets
 * the kernel run connect(2); the release store and the acquire load
 * below are all that orders the two.
 */
typedef struct {
	int      used;
	int      family;    /* AF_INET or AF_INET6, */
	uint16_t dst_port;  /* network byte order, */
	unsigned char addr[16];
} NetmonPending;

/**
 * A host interface the fake /proc/net/dev can report, together with
 * the prefix it is responsible for.  Addresses are in host byte
 * order.
 */
typedef struct {
	char     name[NETMON_IFNAMSIZ];
	uint32_t addr;
	uint32_t mask;
} NetmonRoute;

/**
 * State shared between the tracer (which redirects connect(2) and
 * renders /proc/net/dev) and the proxy thread (which does the actual
 * relaying and the accounting).  It lives in a MAP_SHARED anonymous
 * mapping so that both sides see the same memory.  Everything the two
 * threads race on is a __atomic_* access.
 */
typedef struct {
	/* Hand-off table and the matching listening sockets.  Both are
	 * filled before the proxy thread is started, except for
	 * @pending which the tracer claims and the proxy releases.  */
	NetmonPending pending[NETMON_MAX_CONNS];
	int  listen_fd[NETMON_NB_LISTENERS];
	int  slot_port[NETMON_MAX_CONNS];
	int  nb_listeners;

	/* Counters, only ever relaxed-incremented by the proxy thread
	 * and read by whoever renders /proc/net/dev.  */
	uint64_t rx_bytes[NETMON_MAX_IFACES];
	uint64_t tx_bytes[NETMON_MAX_IFACES];
	uint64_t rx_packets[NETMON_MAX_IFACES];
	uint64_t tx_packets[NETMON_MAX_IFACES];
	uint64_t rx_errors;
	uint64_t tx_errors;
	uint64_t connections;  /* connections the proxy took over, */
	uint64_t refused;      /* connections the proxy gave up on, */
	uint64_t unresolved;   /* destinations the proxy could not reach, */

	/* Immutable once the proxy thread has been started.  */
	int  log_fd;
	int  log_http;
	int  iface_count;
	int  route_count;
	int  default_route;  /* index of the default route, -1 if none, */
	NetmonRoute routes[NETMON_MAX_IFACES];

	/* Set by the tracer, read by the proxy thread.  */
	int shutdown;
	int ready;
} NetmonShared;

/** Record of one socket whose connect(2) was redirected, so that
 * getpeername(2) can be answered with the address the guest actually
 * asked for instead of the proxy's.  The table is shared with the
 * children of the initial tracee, so it is keyed by pid as well as by
 * file descriptor.  */
typedef struct {
	pid_t        pid;
	int          fd;
	int          family;
	unsigned char addr[16];
	uint16_t     port;   /* network byte order, */
} NetmonSocket;

/** Extension private state.  It is shared, through talloc_reference(),
 * with the children of the initial tracee, which is what makes the
 * proxy counters process-wide.  Only the tracer's thread ever touches
 * it.  */
typedef struct {
	NetmonShared *shared;

	/* Destination of the connect(2) currently in flight, kept
	 * between its enter and its exit stage, together with the
	 * hand-off entry that has to be released if the kernel
	 * refuses the connection.  */
	struct {
		int   fd;
		int   slot;
		NetmonPending dest;
	} connecting;

	/* Set by the GUEST_PATH event and consumed by the
	 * TRANSLATED_PATH one, which always bracket the same
	 * translate_path(3) call.  */
	int faking_netdev;

	NetmonSocket sockets[NETMON_SOCKET_MAX];

	/* Path of the temporary file backing the fake
	 * /proc/net/dev, created on first use.  */
	char *netdev_path;

	/* True for the extension that actually started the proxy: the
	 * children share its memory but must not shut it down.  */
	bool owns_proxy;

	pthread_t thread;
	bool      thread_started;
} NetmonState;

/** Event handler, registered like any other built-in extension.  */
extern int netmon_callback(Extension *extension, ExtensionEvent event,
			intptr_t data1, intptr_t data2);

/**
 * Start the proxy thread.  @log_path is the file the request log is
 * appended to, or NULL to log to the standard error stream.  This
 * function returns 0 on success, -1 on error, in which case the tracer
 * must leave the guest's networking alone.
 */
extern int netmon_proxy_start(NetmonState *state, const char *log_path);

/** Ask the proxy thread to terminate and wait for it.  */
extern void netmon_proxy_stop(NetmonState *state);

/**
 * Discover the host interfaces that can be reported by the fake
 * /proc/net/dev and pick the one the kernel would use to reach a
 * non-local destination.  @shared must not be visible to the proxy
 * thread yet, so this has to be called before netmon_proxy_start().
 */
extern void netmon_netdev_init(NetmonShared *shared);

/**
 * Return the index of the entry of @shared::routes that would carry
 * traffic towards @destination.
 */
extern int netmon_route_lookup(const NetmonShared *shared,
			const struct sockaddr *destination);

/**
 * Add @up bytes sent by the tracee and @down bytes sent back to it to
 * the counters of interface @iface, bumping the packet counts.  This
 * is the only place allowed to write the shared counters, so that all
 * the updates go through the same relaxed atomics.
 */
extern void netmon_charge(NetmonShared *shared, int iface,
			uint64_t up, uint64_t down);

/** Render the fake /proc/net/dev into the file at @path.  */
extern int netmon_netdev_write(const NetmonState *state, const char *path);

/**
 * Return a non-zero value if @port is one of the destination ports
 * this extension is allowed to intercept.  The list can be
 * overridden with the PROOT_NETMON_PORTS environment variable.
 */
extern bool netmon_port_is_monitored(unsigned int port);

#endif /* NETMON_H */
