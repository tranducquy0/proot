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
 * A deliberately small HTTP-proxy-ish relay.
 *
 * The tracer redirects the connect(2) of the monitored sockets to one
 * of the loopback ports served by this thread, and publishes the real
 * destination in the matching NetmonShared::pending entry.  The thread
 * then connects to that destination and splices the two sockets
 * together without ever looking at, let alone rewriting, the payload:
 * whatever protocol the guest speaks keeps working.  The "http proxy"
 * part is limited to *observing* the first bytes of the
 * client-to-server stream, where an HTTP request line and a TLS server
 * name are recognised so that they can be logged; everything else is
 * relayed verbatim.
 *
 * Because the relay is a pure byte pump, a bug here degrades a
 * connection rather than corrupting the guest's data, and the tracer
 * refuses to redirect anything at all while this thread is not ready.
 *
 * This file must not use talloc or note(): neither is thread-safe, and
 * it runs concurrently with the tracer's event loop.
 */

#include <stdio.h>       /* snprintf(3), */
#include <stdlib.h>      /* calloc(3), free(3), getenv(3), */
#include <stdarg.h>      /* va_list, */
#include <string.h>      /* str*(3), mem*(3), */
#include <strings.h>     /* strncasecmp(3), */
#include <errno.h>       /* errno(3), E*, */
#include <fcntl.h>       /* fcntl(2), O_NONBLOCK, */
#include <unistd.h>      /* close(2), open(2), write(2), */
#include <time.h>        /* time(2), */
#include <poll.h>        /* poll(2), */
#include <pthread.h>     /* pthread_*, */
#include <inttypes.h>    /* PRIu64, */
#include <netinet/in.h>  /* struct sockaddr_in, INADDR_LOOPBACK, */
#include <arpa/inet.h>   /* inet_ntop(3), */
#include <sys/socket.h>  /* socket(2), connect(2), recv(2), send(2), */
#include <sys/mman.h>    /* mmap(2), */

#include "attribute.h"
#include "cli/note.h"
#include "extension/netmon/netmon.h"

/* Size of the buffers used to splice the two directions of a
 * connection.  */
#define NETMON_BUFSIZE  16384

/* How much of the client-to-server stream is inspected to recognise a
 * request line or a TLS server name.  */
#define NETMON_SNIFF_SIZE  1024

/* Backlog of the per-connection listening sockets.  */
#define NETMON_BACKLOG  4

typedef enum {
	NETMON_CONN_FREE = 0,
	NETMON_CONN_CONNECTING, /* upstream connect(2) in progress, */
	NETMON_CONN_RELAY,      /* established, splicing both ways,  */
	NETMON_CONN_CLOSING     /* one direction is over, drain it,  */
} NetmonConnState;

typedef struct {
	NetmonConnState state;
	int      client;
	int      server;
	time_t   started;
	NetmonPending dest;
	int      iface;

	/* client -> server, */
	char     c2s[NETMON_BUFSIZE];
	size_t   c2s_head;
	size_t   c2s_tail;

	/* server -> client, */
	char     s2c[NETMON_BUFSIZE];
	size_t   s2c_head;
	size_t   s2c_tail;

	uint64_t up;
	uint64_t down;
	bool     sniffed;
	bool     c2s_eof;
	bool     s2c_eof;
} NetmonConn;

static void proxy_logf(NetmonShared *shared, const char *message, ...) FORMAT(printf, 2, 3);

/**
 * Write the whole @length bytes of @message to the proxy log.  The
 * caller formats one whole line at a time so that concurrent
 * connections do not interleave their output.
 */
static void proxy_log(NetmonShared *shared, const char *message, size_t length)
{
	size_t offset = 0;

	while (offset < length) {
		ssize_t status = write(shared->log_fd, message + offset, length - offset);

		if (status > 0) {
			offset += (size_t) status;
			continue;
		}

		/* Retry on EINTR, give up on anything else: losing a log
		 * line must never take the relay down.  */
		if (status < 0 && errno == EINTR)
			continue;
		return;
	}
}

/**
 * Format "HH:MM:SS <message>\n" and log it.
 */
static void proxy_logf(NetmonShared *shared, const char *message, ...)
{
	struct tm broken_down;
	char line[512];
	char stamp[16];
	va_list args;
	time_t now;
	int offset;

	now = time(NULL);
	if (strftime(stamp, sizeof(stamp), "%H:%M:%S",
			localtime_r(&now, &broken_down)) == 0)
		strcpy(stamp, "--:--:--");

	offset = snprintf(line, sizeof(line), "netmon: %s ", stamp);
	if (offset < 0 || (size_t) offset >= sizeof(line))
		return;

	va_start(args, message);
	vsnprintf(line + offset, sizeof(line) - offset, message, args);
	va_end(args);

	offset = (int) strlen(line);
	if ((size_t) offset + 1 >= sizeof(line))
		return;
	line[offset++] = '\n';

	proxy_log(shared, line, (size_t) offset);
}

/**
 * Render @bytes into @buffer in a human readable way, the way vnstat
 * and friends do.
 */
static const char *human_size(char *buffer, size_t size, uint64_t bytes)
{
	static const char units[] = "BKMGTPE";
	double value = (double) bytes;
	size_t unit = 0;

	while (value >= 1024.0 && unit + 1 < sizeof(units) - 1) {
		value /= 1024.0;
		unit++;
	}

	if (unit == 0)
		snprintf(buffer, size, "%" PRIu64 " B", bytes);
	else
		snprintf(buffer, size, "%.1f %ciB", value, units[unit]);

	return buffer;
}

/** Rebuild the sockaddr described by @pending.  */
static socklen_t pending_to_sockaddr(const NetmonPending *pending,
			struct sockaddr_storage *storage)
{
	memset(storage, 0, sizeof(*storage));

	if (pending->family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *) (void *) storage;

		in->sin_family = AF_INET;
		in->sin_port = pending->dst_port;
		memcpy(&in->sin_addr, pending->addr, sizeof(in->sin_addr));
		return sizeof(*in);
	}
	else {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) (void *) storage;

		in6->sin6_family = AF_INET6;
		in6->sin6_port = pending->dst_port;
		memcpy(&in6->sin6_addr, pending->addr, sizeof(in6->sin6_addr));
		return sizeof(*in6);
	}
}

/**
 * Render @address into @buffer as "1.2.3.4:80" or "[::1]:80".
 */
static const char *endpoint(char *buffer, size_t size,
			const struct sockaddr *address)
{
	char host[INET6_ADDRSTRLEN] = "?";

	if (address == NULL) {
		snprintf(buffer, size, "?");
		return buffer;
	}

	if (address->sa_family == AF_INET) {
		const struct sockaddr_in *in = (const struct sockaddr_in *) (const void *) address;

		inet_ntop(AF_INET, &in->sin_addr, host, sizeof(host));
		snprintf(buffer, size, "%s:%u", host, (unsigned int) ntohs(in->sin_port));
	}
	else {
		const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *) (const void *) address;

		inet_ntop(AF_INET6, &in6->sin6_addr, host, sizeof(host));
		snprintf(buffer, size, "[%s]:%u", host, (unsigned int) ntohs(in6->sin6_port));
	}

	return buffer;
}

/**
 * Look for an HTTP "Host:" header in the first @length bytes of
 * @header.  This function returns a pointer inside @header, or NULL.
 */
static char *find_host_header(char *header, size_t length)
{
	size_t i;

	for (i = 0; i + 5 <= length; i++) {
		size_t left;

		if (strncasecmp(header + i, "Host:", 5) != 0)
			continue;

		/* The header name has to start a line.  */
		if (i != 0 && header[i - 1] != '\n' && header[i - 1] != '\r')
			continue;

		header += i + 5;
		length -= i + 5;

		while (length > 0 && (*header == ' ' || *header == '\t')) {
			header++;
			length--;
		}

		for (left = 0; left < length; left++) {
			if (header[left] == '\r' || header[left] == '\n' || header[left] == ' ') {
				header[left] = '\0';
				break;
			}
		}

		return header[0] != '\0' ? header : NULL;
	}

	return NULL;
}

/** Read the big-endian 16-bit integer at @header + @offset.  */
static size_t be16(const char *header, size_t offset)
{
	return (size_t) (((unsigned char) header[offset] << 8)
			| (unsigned char) header[offset + 1]);
}

/**
 * Extract the server name of a TLS ClientHello out of the first
 * @length bytes of @header.  This function returns a pointer inside
 * @header, or NULL if there is no server name extension to be found.
 *
 * See RFC 8446 appendix A and RFC 6066 section 3.
 */
static char *find_tls_sni(char *header, size_t length)
{
	size_t offset = 5;   /* TLS record header, */
	size_t extensions;
	size_t end;

	/* A handshake record holding a ClientHello, and long enough to
	 * hold the mandatory parts of one.  */
	if (length < 44 || header[0] != 0x16 || header[1] != 0x03 || header[5] != 0x01)
		return NULL;

	/* Skip the handshake header, the client version, the random and
	 * the session id, then the cipher suites and the compression
	 * methods, to reach the extensions.  */
	offset += 4;                             /* handshake header, */
	offset += 2 + 32;                        /* version and random, */
	if (offset + 1 > length)
		return NULL;
	offset += 1 + (size_t) header[offset];   /* session id, */
	if (offset + 2 > length)
		return NULL;
	offset += 2 + be16(header, offset);       /* cipher suites, */
	if (offset + 1 > length)
		return NULL;
	offset += 1 + (size_t) header[offset];   /* compression methods, */
	if (offset + 2 > length)
		return NULL;
	extensions = be16(header, offset);
	offset += 2;

	end = offset + extensions;
	if (end > length)
		end = length;

	while (offset + 4 <= end) {
		size_t type = be16(header, offset);
		size_t size = be16(header, offset + 2);
		size_t body = offset + 4;

		if (body + size > end)
			break;

		/* server_name extension, */
		if (type == 0 && size >= 5) {
			size_t cursor = body + 2;
			size_t stop = cursor + be16(header, body);

			if (stop > body + size)
				stop = body + size;

			while (cursor + 3 <= stop) {
				size_t name_type = (size_t) header[cursor];
				size_t name_size = be16(header, cursor + 1);
				size_t name_at = cursor + 3;

				if (name_at + name_size > stop)
					break;

				/* host_name, */
				if (name_type == 0 && name_size > 0) {
					char *value = header + name_at;

					value[name_size] = '\0';
					return value;
				}

				cursor = name_at + name_size;
			}
			break;
		}

		offset = body + size;
	}

	return NULL;
}

/**
 * Inspect, without consuming, the first bytes the guest sends, and
 * report what it recognises.  Everything goes through MSG_PEEK so
 * that the relay stays a pure byte pump.
 */
static void conn_sniff(NetmonShared *shared, NetmonConn *conn)
{
	struct sockaddr_storage storage;
	char destination[64];
	char header[NETMON_SNIFF_SIZE + 1];
	char text[NETMON_SNIFF_SIZE + 64];
	char *host;
	ssize_t length;
	size_t end;

	if (conn->sniffed || !shared->log_http)
		return;

	/* Nothing is read from the client socket until the relay is
	 * established, so this always still sees the very first bytes
	 * of the stream, whatever the guest sent in the meantime.  */
	length = recv(conn->client, header, NETMON_SNIFF_SIZE, MSG_PEEK);
	if (length <= 0)
		return;

	conn->sniffed = true;
	header[length] = '\0';

	pending_to_sockaddr(&conn->dest, &storage);

	/* A TLS ClientHello comes first on an https connection, and it
	 * is the only place where the destination name is visible
	 * without decrypting anything.  */
	host = find_tls_sni(header, (size_t) length);
	if (host != NULL) {
		proxy_logf(shared, "TLS SNI %s to %s", host,
			endpoint(destination, sizeof(destination),
				(struct sockaddr *) (void *) &storage));
		return;
	}

	/* Otherwise look for an HTTP request line.  */
	for (end = 0; end < (size_t) length; end++) {
		if (header[end] == '\r' || header[end] == '\n') {
			header[end] = '\0';
			break;
		}
	}

	if (end == 0)
		return;

	/* "METHOD target HTTP/1.x": at least one character for the
	 * method, one for the target, a space, then the eight
	 * characters of the version.  */
	if (end < 9)
		return;
	if (header[end - 9] != ' ' || strncmp(header + end - 8, "HTTP/1.", 7) != 0)
		return;

	host = find_host_header(header + end + 1, (size_t) length - end - 1);
	if (host != NULL)
		snprintf(text, sizeof(text), "%s  (host: %s)", header, host);
	else
		snprintf(text, sizeof(text), "%s", header);

	proxy_logf(shared, "%s to %s", text,
		endpoint(destination, sizeof(destination),
			(struct sockaddr *) (void *) &storage));
}

/** Make @fd non-blocking.  */
static void set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags >= 0)
		(void) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Release @conn, charging its traffic to the interfaces it used.
 */
static void conn_release(NetmonShared *shared, NetmonConn *conn, const char *reason)
{
	struct sockaddr_storage storage;
	char destination[64];
	char up[32], down[32];
	long elapsed;

	if (conn->state == NETMON_CONN_FREE)
		return;

	if (conn->client >= 0)
		close(conn->client);
	if (conn->server >= 0)
		close(conn->server);

	if (conn->state == NETMON_CONN_RELAY || conn->state == NETMON_CONN_CLOSING) {
		/* The guest-to-proxy hop went through the loopback
		 * device, the proxy-to-destination hop through the
		 * routed interface.  */
		netmon_charge(shared, 0, conn->up, conn->down);
		if (conn->iface > 0)
			netmon_charge(shared, conn->iface, conn->up, conn->down);

		pending_to_sockaddr(&conn->dest, &storage);
		elapsed = (long) (time(NULL) - conn->started);
		if (elapsed < 0)
			elapsed = 0;

		proxy_logf(shared, "closed %s (%s) up %s down %s in %lds",
			endpoint(destination, sizeof(destination),
				(struct sockaddr *) (void *) &storage),
			reason,
			human_size(up, sizeof(up), conn->up),
			human_size(down, sizeof(down), conn->down),
			elapsed);
	}

	memset(conn, 0, sizeof(*conn));
	conn->client = -1;
	conn->server = -1;
	conn->iface = -1;
}

/**
 * Start the upstream connect(2) towards the real destination.  This
 * function returns -1 if the connection cannot even be attempted.
 */
static int conn_start_upstream(NetmonShared *shared, NetmonConn *conn)
{
	struct sockaddr_storage storage;
	socklen_t length;
	int status;
	int fd;

	length = pending_to_sockaddr(&conn->dest, &storage);

	fd = socket(conn->dest.family, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;

	set_nonblocking(fd);

	status = connect(fd, (struct sockaddr *) (void *) &storage, length);
	if (status < 0 && errno != EINPROGRESS && errno != EINTR) {
		close(fd);
		return -1;
	}

	conn->server = fd;
	conn->iface = netmon_route_lookup(shared, (struct sockaddr *) (void *) &storage);
	conn->state = NETMON_CONN_CONNECTING;

	__atomic_add_fetch(&shared->connections, 1, __ATOMIC_RELAXED);
	return 0;
}

/**
 * Check whether the upstream connect(2) succeeded.  This function
 * returns 0 while it is still in progress, 1 once it succeeded, and
 * -1 if it failed.
 */
static int conn_connect_result(NetmonShared *shared, NetmonConn *conn)
{
	struct sockaddr_storage storage;
	char destination[64];
	int error = 0;
	socklen_t size = sizeof(error);

	if (getsockopt(conn->server, SOL_SOCKET, SO_ERROR, &error, &size) < 0)
		error = errno;

	if (error == 0)
		return 1;

	pending_to_sockaddr(&conn->dest, &storage);
	proxy_logf(shared, "cannot reach %s: %s",
		endpoint(destination, sizeof(destination),
			(struct sockaddr *) (void *) &storage),
		strerror(error));

	__atomic_add_fetch(&shared->unresolved, 1, __ATOMIC_RELAXED);
	return -1;
}

/**
 * Move whatever is buffered towards @fd.  This function returns the
 * number of bytes transferred, 0 if there was nothing to do, or -1 if
 * the destination is gone.
 */
static ssize_t conn_flush(char *buffer, size_t *head, size_t *tail, int fd)
{
	size_t pending = *tail - *head;
	ssize_t status;

	if (pending == 0)
		return 0;

	status = send(fd, buffer + *head, pending, MSG_NOSIGNAL);
	if (status > 0) {
		*head += (size_t) status;
		if (*head == *tail)
			*head = *tail = 0;
		return status;
	}

	if (status < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
		return 0;

	return -1;
}

/**
 * Read from @fd into @buffer if there is room left.  This function
 * returns the number of bytes read, 0 if @fd reached its end, -1 on a
 * fatal error and -2 if it simply had nothing to offer.
 */
static ssize_t conn_fill(char *buffer, size_t *head, size_t *tail, int fd)
{
	size_t pending = *tail - *head;
	size_t room = NETMON_BUFSIZE - pending;
	ssize_t status;

	if (room == 0)
		return -2;

	status = recv(fd, buffer + *tail, room, 0);
	if (status > 0) {
		*tail += (size_t) status;
		return status;
	}

	if (status == 0)
		return 0;

	if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
		return -2;

	return -1;
}

/**
 * Handle one poll(2) round for @conn.  @client_events and
 * @server_events are the poll(2) results for its two sockets.
 */
static void conn_service(NetmonShared *shared, NetmonConn *conn,
			short client_events, short server_events)
{
	ssize_t status;

	if (conn->state == NETMON_CONN_CONNECTING) {
		if ((server_events & (POLLERR | POLLHUP | POLLOUT)) == 0)
			return;

		if (conn_connect_result(shared, conn) < 0) {
			conn_release(shared, conn, "unreachable");
			return;
		}

		conn->state = NETMON_CONN_RELAY;

		/* The guest may have sent something while the upstream
		 * connection was being established.  */
		if (client_events & POLLIN)
			conn_sniff(shared, conn);
	}

	if (conn->state != NETMON_CONN_RELAY && conn->state != NETMON_CONN_CLOSING)
		return;

	/* Sniff before consuming anything, so that the head of the
	 * stream is still available.  */
	if (client_events & POLLIN)
		conn_sniff(shared, conn);

	/* client -> server, */
	if (client_events & (POLLIN | POLLHUP | POLLERR)) {
		status = conn_fill(conn->c2s, &conn->c2s_head, &conn->c2s_tail,
					conn->client);
		if (status == 0)
			conn->c2s_eof = true;
		else if (status == -1) {
			__atomic_add_fetch(&shared->rx_errors, 1, __ATOMIC_RELAXED);
			conn_release(shared, conn, "read error");
			return;
		}
	}

	/* server -> client, */
	if (server_events & (POLLIN | POLLHUP | POLLERR)) {
		status = conn_fill(conn->s2c, &conn->s2c_head, &conn->s2c_tail,
					conn->server);
		if (status == 0)
			conn->s2c_eof = true;
		else if (status == -1) {
			__atomic_add_fetch(&shared->rx_errors, 1, __ATOMIC_RELAXED);
			conn_release(shared, conn, "read error");
			return;
		}
	}

	/* Push both directions out.  */
	if (server_events & POLLOUT) {
		status = conn_flush(conn->c2s, &conn->c2s_head, &conn->c2s_tail,
					conn->server);
		if (status > 0)
			conn->up += (uint64_t) status;
		else if (status < 0) {
			__atomic_add_fetch(&shared->tx_errors, 1, __ATOMIC_RELAXED);
			conn_release(shared, conn, "write error");
			return;
		}

		if (conn->c2s_eof && conn->c2s_head == conn->c2s_tail)
			shutdown(conn->server, SHUT_WR);
	}

	if (client_events & POLLOUT) {
		status = conn_flush(conn->s2c, &conn->s2c_head, &conn->s2c_tail,
					conn->client);
		if (status > 0)
			conn->down += (uint64_t) status;
		else if (status < 0) {
			__atomic_add_fetch(&shared->tx_errors, 1, __ATOMIC_RELAXED);
			conn_release(shared, conn, "write error");
			return;
		}

		if (conn->s2c_eof && conn->s2c_head == conn->s2c_tail)
			shutdown(conn->client, SHUT_WR);
	}

	/* Both directions are over, the connection can go.  */
	if (conn->c2s_eof && conn->c2s_head == conn->c2s_tail
	    && conn->s2c_eof && conn->s2c_head == conn->s2c_tail) {
		conn_release(shared, conn, "eof");
		return;
	}

	/* A half-closed connection only has to drain what is left.  */
	if (conn->c2s_eof && conn->s2c_eof)
		conn->state = NETMON_CONN_CLOSING;
}

/**
 * Accept a connection on the listening socket of @slot and look up the
 * destination the tracer published for it.  This function returns the
 * connection slot, or NULL if the connection had to be dropped.
 */
static NetmonConn *conn_accept(NetmonShared *shared, NetmonConn *conns, int slot)
{
	struct sockaddr_storage peer;
	NetmonConn *connection = NULL;
	socklen_t size = sizeof(peer);
	size_t i;
	int fd;

	fd = accept(shared->listen_fd[2 * slot], (struct sockaddr *) (void *) &peer, &size);
	if (fd < 0)
		return NULL;

	/* The release store the tracer did before letting connect(2)
	 * run has to be visible now.  */
	if (__atomic_load_n(&shared->pending[slot].used, __ATOMIC_ACQUIRE) == 0) {
		proxy_logf(shared, "slot %d was not prepared, dropping the "
			"connection", slot);
		close(fd);
		__atomic_add_fetch(&shared->refused, 1, __ATOMIC_RELAXED);
		return NULL;
	}

	for (i = 0; i < NETMON_MAX_CONNS; i++) {
		if (conns[i].state == NETMON_CONN_FREE) {
			connection = &conns[i];
			break;
		}
	}

	if (connection == NULL) {
		proxy_logf(shared, "no room left for the connection on slot %d, "
			"dropping it", slot);
		close(fd);
		__atomic_add_fetch(&shared->refused, 1, __ATOMIC_RELAXED);
		return NULL;
	}

	memset(connection, 0, sizeof(*connection));
	connection->client = fd;
	connection->server = -1;
	connection->iface = -1;
	connection->started = time(NULL);
	connection->dest = shared->pending[slot];

	/* The entry belongs to this connection now, and the tracer may
	 * reuse the slot for the next one.  */
	__atomic_store_n(&shared->pending[slot].used, 0, __ATOMIC_RELEASE);

	if (conn_start_upstream(shared, connection) < 0) {
		__atomic_add_fetch(&shared->unresolved, 1, __ATOMIC_RELAXED);
		conn_release(shared, connection, "no upstream socket");
		return NULL;
	}

	return connection;
}

/**
 * The proxy thread: accept the redirected connections and splice them
 * to their real destination.
 */
static void *proxy_thread(void *argument)
{
	NetmonState *state = argument;
	NetmonShared *shared = state->shared;
	NetmonConn *conns_map[NETMON_MAX_CONNS];
	struct pollfd fds[NETMON_NB_LISTENERS + 2 * NETMON_MAX_CONNS];
	NetmonConn *conns;
	time_t started;
	size_t i;

	/* Not talloc: this thread must not touch the tracer's
	 * allocator.  */
	conns = calloc(NETMON_MAX_CONNS, sizeof(*conns));
	if (conns == NULL) {
		proxy_log(shared, "netmon: out of memory, the proxy is disabled\n",
			sizeof("netmon: out of memory, the proxy is disabled\n") - 1);
		__atomic_add_fetch(&shared->refused, 1, __ATOMIC_RELAXED);
		return NULL;
	}

	for (i = 0; i < NETMON_MAX_CONNS; i++) {
		conns[i].client = -1;
		conns[i].server = -1;
		conns[i].iface = -1;
	}

	__atomic_store_n(&shared->ready, 1, __ATOMIC_RELAXED);
	proxy_logf(shared, "relaying through 127.0.0.1:%d..%d",
		shared->slot_port[0], shared->slot_port[NETMON_MAX_CONNS - 1]);
	started = time(NULL);

	while (__atomic_load_n(&shared->shutdown, __ATOMIC_RELAXED) == 0) {
		size_t nb_fds = 0;
		size_t j;
		int status;

		memset(conns_map, 0, sizeof(conns_map));

		/* The per-connection listening sockets, */
		for (i = 0; i < (size_t) shared->nb_listeners; i++) {
			fds[nb_fds].fd = shared->listen_fd[i];
			fds[nb_fds].events = POLLIN;
			fds[nb_fds].revents = 0;
			nb_fds++;
		}

		/* ... then both ends of every live connection.  */
		for (i = 0; i < NETMON_MAX_CONNS; i++) {
			NetmonConn *conn = &conns[i];

			if (conn->state == NETMON_CONN_FREE)
				continue;

			conns_map[i] = conn;

			fds[nb_fds].fd = conn->client;
			fds[nb_fds].events = POLLIN;
			if (conn->s2c_head != conn->s2c_tail)
				fds[nb_fds].events |= POLLOUT;
			fds[nb_fds].revents = 0;
			nb_fds++;

			fds[nb_fds].fd = conn->server;
			fds[nb_fds].events = POLLIN;
			if (conn->state == NETMON_CONN_CONNECTING
			    || conn->c2s_head != conn->c2s_tail)
				fds[nb_fds].events |= POLLOUT;
			fds[nb_fds].revents = 0;
			nb_fds++;
		}

		status = poll(fds, nb_fds, 200);
		if (status < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		/* Accepting first leaves the freshly created connection
		 * out of this round, which is fine since poll(2) is
		 * level-triggered.  */
		for (i = 0; i < (size_t) shared->nb_listeners; i++) {
			if ((fds[i].revents & POLLIN) == 0)
				continue;
			conn_accept(shared, conns, i / 2);
			break;
		}

		for (i = 0, j = (size_t) shared->nb_listeners; i < NETMON_MAX_CONNS; i++) {
			NetmonConn *conn = conns_map[i];
			short client_events, server_events;

			if (conn == NULL)
				continue;

			client_events = fds[j].revents;
			server_events = fds[j + 1].revents;
			j += 2;

			conn_service(shared, conn, client_events, server_events);
		}
	}

	for (i = 0; i < NETMON_MAX_CONNS; i++)
		conn_release(shared, &conns[i], "shutdown");

	proxy_logf(shared, "stopped after %lds", (long) (time(NULL) - started));
	free(conns);

	__atomic_store_n(&shared->ready, 0, __ATOMIC_RELAXED);
	return NULL;
}

/**
 * Create a listening socket for @family on the loopback address, bound
 * to @port (0 to let the kernel pick one) and non-blocking.  This
 * function returns the file descriptor, or -1 on error.
 */
static int create_listener(int family, unsigned int port, unsigned int *chosen_port)
{
	struct sockaddr_storage address;
	unsigned int chosen;
	socklen_t size;
	int status;
	int fd;
	int on = 1;

	fd = socket(family, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	if (family == AF_INET6) {
		/* Keep the two families apart so that a single slot can
		 * have one port number for each of them.  */
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0) {
			close(fd);
			return -1;
		}
	}

	memset(&address, 0, sizeof(address));
	if (family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *) (void *) &address;

		in->sin_family = AF_INET;
		in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		in->sin_port = htons((uint16_t) port);
	}
	else {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) (void *) &address;

		in6->sin6_family = AF_INET6;
		in6->sin6_addr = in6addr_loopback;
		in6->sin6_port = htons((uint16_t) port);
	}

	status = bind(fd, (struct sockaddr *) (void *) &address,
			(family == AF_INET) ? sizeof(struct sockaddr_in)
					    : sizeof(struct sockaddr_in6));
	if (status < 0) {
		close(fd);
		return -1;
	}

	if (listen(fd, NETMON_BACKLOG) < 0) {
		close(fd);
		return -1;
	}

	memset(&address, 0, sizeof(address));
	size = sizeof(address);
	if (getsockname(fd, (struct sockaddr *) (void *) &address, &size) < 0) {
		close(fd);
		return -1;
	}

	chosen = (family == AF_INET)
		? ntohs(((struct sockaddr_in *) (void *) &address)->sin_port)
		: ntohs(((struct sockaddr_in6 *) (void *) &address)->sin6_port);

	set_nonblocking(fd);

	*chosen_port = chosen;
	return fd;
}

/**
 * Create the shared memory, the per-connection listening sockets and
 * the proxy thread.  This function returns 0 on success, -1 on error,
 * in which case the tracer must leave the guest's networking alone.
 */
int netmon_proxy_start(NetmonState *state, const char *log_path)
{
	NetmonShared *shared;
	const char *quiet;
	int slot;
	int i;

	shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (shared == MAP_FAILED) {
		note(NULL, ERROR, SYSTEM, "netmon: mmap()");
		return -1;
	}

	memset(shared, 0, sizeof(*shared));
	shared->log_fd = -1;
	for (i = 0; i < NETMON_NB_LISTENERS; i++)
		shared->listen_fd[i] = -1;

	/* The proxy thread is started before the tracer redirects a
	 * single connect(2), so it can rely on these fields.  */
	netmon_netdev_init(shared);

	quiet = getenv("PROOT_NETMON_QUIET");
	shared->log_http = (quiet == NULL || strcmp(quiet, "1") != 0);

	if (log_path != NULL && log_path[0] != '\0') {
		shared->log_fd = open(log_path,
				O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
		if (shared->log_fd < 0) {
			note(NULL, WARNING, SYSTEM,
				"netmon: can't open the log file '%s'", log_path);
			shared->log_fd = STDERR_FILENO;
		}
	}
	else {
		shared->log_fd = STDERR_FILENO;
	}

	/* One pair of listening sockets per concurrent connection, the
	 * v4 one first.  */
	for (slot = 0; slot < NETMON_MAX_CONNS; slot++) {
		unsigned int port = 0;
		int fd4, fd6;

		fd4 = create_listener(AF_INET, 0, &port);
		if (fd4 < 0) {
			note(NULL, WARNING, SYSTEM, "netmon: can't listen on the "
				"loopback address, the proxy is disabled");
			goto error;
		}

		/* The v6 socket wants the very same port so that a
		 * guest using either family is redirected to a port
		 * this extension picked; it is not fatal if that port
		 * happens to be taken for ::1, this slot then only
		 * serves IPv4.  */
		fd6 = create_listener(AF_INET6, port, &port);

		shared->listen_fd[2 * slot] = fd4;
		shared->listen_fd[2 * slot + 1] = fd6;
		shared->slot_port[slot] = (int) port;
		shared->nb_listeners += 2;
	}

	state->shared = shared;

	if (pthread_create(&state->thread, NULL, proxy_thread, state) != 0) {
		note(NULL, ERROR, INTERNAL, "netmon: pthread_create()");
		state->shared = NULL;
		goto error;
	}
	state->thread_started = true;

	return 0;

error:
	for (i = 0; i < NETMON_NB_LISTENERS; i++) {
		if (shared->listen_fd[i] >= 0)
			close(shared->listen_fd[i]);
	}
	if (shared->log_fd >= 0 && shared->log_fd != STDERR_FILENO)
		close(shared->log_fd);
	munmap(shared, sizeof(*shared));
	state->shared = NULL;
	return -1;
}

/**
 * Ask the proxy thread to terminate and wait for it.  The shared
 * memory is released afterwards, so this must not be called while a
 * tracee may still try to connect().
 */
void netmon_proxy_stop(NetmonState *state)
{
	NetmonShared *shared = state->shared;
	int i;

	if (shared == NULL)
		return;

	__atomic_store_n(&shared->shutdown, 1, __ATOMIC_RELAXED);

	if (state->thread_started) {
		pthread_join(state->thread, NULL);
		state->thread_started = false;
	}

	for (i = 0; i < NETMON_NB_LISTENERS; i++) {
		if (shared->listen_fd[i] >= 0) {
			close(shared->listen_fd[i]);
			shared->listen_fd[i] = -1;
		}
	}

	if (shared->log_fd >= 0 && shared->log_fd != STDERR_FILENO) {
		close(shared->log_fd);
		shared->log_fd = -1;
	}

	munmap(shared, sizeof(*shared));
	state->shared = NULL;
}
