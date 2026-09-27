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
 * Check the netmon extension from inside the guest: run a server on a
 * non-loopback address of the host, fetch a document from it through
 * the proxy PRoot is supposed to insert, and check that the traffic
 * shows up in the synthesized /proc/net/dev.
 *
 * The server listens on the address the kernel would use to reach the
 * internet, which is what makes the extension divert the connection:
 * it leaves 127.0.0.0/8 and the link-local range alone.
 *
 * Must be run with PROOT_NETMON_PORTS=NETMON_TEST_PORT.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NETMON_TEST_PORT     18080
#define NETMON_TEST_SLICES   400		/* 400 * 50ms = 20s, */
#define NETMON_TEST_RESPONSE "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nbody"

static int passed = 0;

static void skip(const char *reason)
{
	printf("SKIP: %s\n", reason);
	exit(125);
}

static void fail(const char *reason)
{
	printf("not ok %d - %s\n", passed + 1, reason);
	exit(1);
}

static void ok(const char *description)
{
	passed++;
	printf("ok %d - %s\n", passed, description);
}

static void set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Return the local address the kernel would use to reach the
 * internet, that is the source address of the default route.  No
 * packet is sent: connect(2) on a datagram socket only picks a route.
 */
static struct in_addr find_external_address(void)
{
	struct sockaddr_in remote;
	struct sockaddr_in local;
	struct in_addr loopback;
	socklen_t size = sizeof(local);
	int fd;

	inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
	remote.sin_family = AF_INET;
	remote.sin_port = htons(9);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		skip("no datagram socket available");

	if (connect(fd, (struct sockaddr *) &remote, sizeof(remote)) < 0) {
		close(fd);
		skip("no default route");
	}

	if (getsockname(fd, (struct sockaddr *) &local, &size) < 0) {
		close(fd);
		skip("getsockname() failed");
	}
	close(fd);

	inet_pton(AF_INET, "127.0.0.1", &loopback);
	if (local.sin_addr.s_addr == 0 || local.sin_addr.s_addr == loopback.s_addr)
		skip("the default route is a loopback one");

	return local.sin_addr;
}

/**
 * Read the byte counters of the interface @wanted out of
 * /proc/net/dev.  Returns 1 when the file has the shape the kernel
 * gives it and holds a row for @wanted, 0 otherwise -- in which case
 * @path is dumped so that the failure says what was read instead.
 */
static int read_counters(const char *path, const char *wanted,
			long *rx, long *tx)
{
	/* bytes packets errs drop fifo frame compressed multicast
	 * bytes packets errs drop fifo colls carrier compressed */
	enum { NB_FIELDS = 16, TX_BYTES = 8 };
	char line[1024];
	int headers = 0, rows = 0, matched = 0;
	FILE *fp;

	*rx = 0;
	*tx = 0;

	fp = fopen(path, "r");
	if (fp == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {
		unsigned long long fields[NB_FIELDS];
		char *colon, *name, *end, *cursor;
		int count = 0;

		if (strncmp(line, "Inter-|", 7) == 0
		    || strncmp(line, " face |", 7) == 0) {
			headers++;
			continue;
		}

		colon = strchr(line, ':');
		if (colon == NULL)
			continue;

		rows++;

		/* The kernel right-aligns the interface name in six
		 * columns, so trim the blanks on both sides.  */
		*colon = '\0';
		name = line;
		while (*name == ' ')
			name++;
		cursor = colon;
		while (cursor > name && cursor[-1] == ' ')
			cursor--;
		*cursor = '\0';

		if (strcmp(name, wanted) != 0)
			continue;

		for (cursor = colon + 1; count < NB_FIELDS; ) {
			unsigned long long value;

			while (*cursor == ' ')
				cursor++;
			if (*cursor == '\0' || *cursor == '\n' || *cursor == '\r')
				break;

			value = strtoull(cursor, &end, 10);
			if (end == cursor)
				break;

			fields[count++] = value;
			cursor = end;
		}

		if (count != NB_FIELDS) {
			rewind(fp);
			printf("FAIL: %s: row \"%s\" has %d fields, %d expected\n",
				path, line, count, (int) NB_FIELDS);
			fclose(fp);
			return 0;
		}

		*rx = (long) fields[0];
		*tx = (long) fields[TX_BYTES];
		matched = 1;
	}

	fclose(fp);

	/* A single-interface namespace is normal: an Android app runs in
	 * its own network namespace, where /proc/net only shows lo.  */
	if (headers != 2 || rows < 1 || !matched) {
		FILE *dump = fopen(path, "r");

		printf("FAIL: %s: %d header lines, %d interfaces, lo found: %d\n",
			path, headers, rows, matched);
		if (dump != NULL) {
			int shown = 0;

			while (fgets(line, sizeof(line), dump) != NULL && shown < 12) {
				printf("  | %s", line);
				shown++;
			}
			fclose(dump);
		}
		return 0;
	}

	return 1;
}

int main(void)
{
	struct sockaddr_in address;
	struct sockaddr_in peer;
	char request[] =
		"GET /netmon-test HTTP/1.1\r\n"
		"Host: netmon.invalid\r\n"
		"\r\n";
	char answer[4096];
	char served[4096];
	socklen_t size;
	size_t total = 0, served_total = 0;
	struct in_addr loopback;
	struct in_addr external;
	int server_fd, client_fd, server_conn = -1;
	int answered = 0, connected = 0, slice;

	external = find_external_address();
	inet_pton(AF_INET, "127.0.0.1", &loopback);

	/* The server the guest is about to fetch from.  */
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
		skip("no TCP socket available");

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr = external;
	address.sin_port = htons(NETMON_TEST_PORT);

	if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
		close(server_fd);
		skip("the test port is already in use");
	}
	if (listen(server_fd, 4) < 0) {
		close(server_fd);
		skip("can't listen on the test port");
	}

	/* The client, whose connection the extension has to divert.  */
	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd < 0)
		fail("no client socket");

	set_nonblocking(client_fd);
	if (connect(client_fd, (struct sockaddr *) &address, sizeof(address)) < 0
	    && errno != EINPROGRESS)
		fail("the connection could not even be started");

	if (send(client_fd, request, sizeof(request) - 1, MSG_NOSIGNAL)
	    != (ssize_t) (sizeof(request) - 1))
		fail("the request could not be sent");

	/* Drive the server and the client from this single thread.  */
	for (slice = 0; !answered && slice < NETMON_TEST_SLICES; slice++) {
		fd_set readable, writable;
		struct timeval window;
		int fd_max;

		FD_ZERO(&readable);
		FD_ZERO(&writable);
		FD_SET(server_fd, &readable);
		FD_SET(client_fd, &readable);
		if (!connected)
			FD_SET(client_fd, &writable);
		if (server_conn >= 0)
			FD_SET(server_conn, &readable);

		window.tv_sec = 0;
		window.tv_usec = 50000;
		fd_max = server_fd > client_fd ? server_fd : client_fd;
		if (server_conn > fd_max)
			fd_max = server_conn;

		if (select(fd_max + 1, &readable, &writable, NULL, &window) < 0) {
			if (errno == EINTR)
				continue;
			fail("select() failed");
		}

		if (!connected && FD_ISSET(client_fd, &writable)) {
			int error = 0;

			size = sizeof(error);
			if (getsockopt(client_fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0)
				fail("getsockopt(SO_ERROR) failed");
			if (error != 0)
				fail("the connection through the proxy was refused");
			connected = 1;
		}

		if (FD_ISSET(server_fd, &readable)) {
			server_conn = accept(server_fd, NULL, NULL);
			if (server_conn < 0)
				fail("the server could not accept the connection");
		}

		if (server_conn >= 0 && FD_ISSET(server_conn, &readable)) {
			ssize_t n = recv(server_conn, served + served_total,
					sizeof(served) - served_total - 1, 0);

			if (n <= 0)
				break;

			served_total += (size_t) n;
			served[served_total] = '\0';

			if (strstr(served, "GET /netmon-test") != NULL
			    && strstr(served, "Host: netmon.invalid") != NULL) {
				if (send(server_conn, NETMON_TEST_RESPONSE,
						sizeof(NETMON_TEST_RESPONSE) - 1, 0)
				    != (ssize_t) (sizeof(NETMON_TEST_RESPONSE) - 1))
					fail("the server could not answer");
			}
		}

		if (FD_ISSET(client_fd, &readable)) {
			ssize_t n = recv(client_fd, answer + total,
					sizeof(answer) - total - 1, 0);

			if (n > 0) {
				total += (size_t) n;
				answer[total] = '\0';
				if (strstr(answer, "200 OK") != NULL
				    && strstr(answer, "body") != NULL)
					answered = 1;
			}
			else if (n == 0) {
				break;
			}
			else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
				break;
			}
		}
	}

	if (served_total == 0 || strstr(served, "GET /netmon-test") == NULL)
		fail("the request never reached the server");
	ok("the request reached the server");

	if (!answered)
		fail("no usable answer came back through the proxy");
	ok("the answer came back");

	/* getpeername(2) must still report the address the guest asked
	 * for rather than the loopback address of the proxy.  */
	close(client_fd);
	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd >= 0) {
		memset(&peer, 0, sizeof(peer));
		size = sizeof(peer);

		if (connect(client_fd, (struct sockaddr *) &address, sizeof(address)) == 0
		    || errno == EINPROGRESS) {
			if (getpeername(client_fd, (struct sockaddr *) &peer, &size) == 0) {
				if (peer.sin_addr.s_addr != external.s_addr
				    || ntohs(peer.sin_port) != NETMON_TEST_PORT) {
					char text[64];

					inet_ntop(AF_INET, &peer.sin_addr, text, sizeof(text));
					fail("getpeername() reported the proxy "
						"instead of the destination");
					(void) text;
				}
			}
		}
		close(client_fd);
		ok("getpeername() still reports the requested destination");
	}

	close(server_fd);
	if (server_conn >= 0)
		close(server_conn);

	/* Finally, the traffic has to be visible in /proc/net/dev.  When
	 * the proxy is in the way the counters are only updated once the
	 * relay has seen the connection end, so give it a moment.  */
	{
		long rx = 0, tx = 0;
		int expect_proxy;
		int attempt;

		expect_proxy = (getenv("NETMON_EXPECT_PROXY") != NULL);

		for (attempt = 0; attempt < 40; attempt++) {
			if (!read_counters("/proc/net/dev", "lo", &rx, &tx))
				fail("/proc/net/dev does not have the shape the "
					"kernel gives it");
			if (rx > 0 || attempt == 39)
				break;
			usleep(50000);
		}

		if (expect_proxy) {
			if (rx <= 0)
				fail("the loopback hop was not accounted for");
			ok("the loopback hop was accounted for");

			if (tx <= 0)
				fail("nothing was transmitted towards the proxy");
			ok("the bytes sent to the proxy were accounted for");
		}

		/* Reported so that the caller can tell a synthesized
		 * /proc/net/dev, which only knows about the traffic it
		 * relayed, from the host's own one.  */
		printf("LO_RX=%ld\nLO_TX=%ld\n", rx, tx);
	}

	return 0;
}
