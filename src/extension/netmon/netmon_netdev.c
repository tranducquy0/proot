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
 * Host interface discovery and rendering of the fake /proc/net/dev.
 *
 * The counters reported here describe what the local proxy observed:
 * a connection the tracer redirected is charged twice, once on "lo"
 * for the guest-to-proxy hop and once on the interface that carries
 * the proxy-to-destination hop, exactly like the kernel would account
 * for a relayed connection.  Traffic the guest does not route through
 * the proxy (datagrams, ports outside the monitored list) is not
 * counted at all, so the totals are a lower bound on what the guest
 * actually did.
 */

#include <stdio.h>       /* FILE, fopen(3), fprintf(3), */
#include <stdlib.h>      /* strtoul(3), */
#include <string.h>      /* str*(3), */
#include <errno.h>       /* errno(3), */
#include <inttypes.h>    /* PRIu64, */
#include <ifaddrs.h>     /* getifaddrs(3), freeifaddrs(3), */
#include <net/if.h>      /* IFNAMSIZ, */
#include <netinet/in.h>  /* struct sockaddr_in, INADDR_LOOPBACK, */
#include <arpa/inet.h>   /* ntohl(3), */
#include <sys/socket.h>  /* AF_INET, */
#include <linux/limits.h> /* PATH_MAX, */

#include "cli/note.h"
#include "extension/netmon/netmon.h"

/**
 * Field widths of the two header lines and of the data lines, taken
 * from the kernel's net/core/net-procfs.c so that ifconfig(8),
 * netstat(1), vnstat(1) and friends can parse what we produce.
 */
#define NETMON_NETDEV_HEADER						\
	"Inter-|   Receive                                                |  Transmit\n" \
	" face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"

#define NETMON_NETDEV_ROW						\
	"%6s: %7" PRIu64 " %7" PRIu64 " %4" PRIu64 " %4" PRIu64 " %4" PRIu64	\
	" %5" PRIu64 " %10" PRIu64 " %9" PRIu64 " "				\
	"%8" PRIu64 " %7" PRIu64 " %4" PRIu64 " %4" PRIu64 " %4" PRIu64	\
	" %5" PRIu64 " %7" PRIu64 " %10" PRIu64 "\n"

/* Relaxed atomics: the only requirement is that a reader never sees a
 * torn 64-bit value, not that the counters are mutually consistent.  */
#define LOAD(field)  __atomic_load_n(&(field), __ATOMIC_RELAXED)
#define BUMP(field)  __atomic_add_fetch(&(field), 1, __ATOMIC_RELAXED)

/**
 * Return a non-zero value if @port is one of the destination ports
 * this extension is allowed to intercept.  The list can be
 * overridden with the PROOT_NETMON_PORTS environment variable.
 */
bool netmon_port_is_monitored(unsigned int port)
{
	/* Parsed at most once, from the tracer's thread only.  */
	static unsigned int ports[32];
	static size_t nb_ports = 0;
	static bool parsed = false;
	const char *list;
	const char *cursor;

	if (!parsed) {
		parsed = true;

		list = getenv("PROOT_NETMON_PORTS");
		if (list == NULL)
			list = NETMON_DEFAULT_PORTS;

		cursor = list;
		while (*cursor != '\0' && nb_ports < sizeof(ports) / sizeof(ports[0])) {
			char *end = NULL;
			unsigned long value;

			errno = 0;
			value = strtoul(cursor, &end, 10);
			if (errno != 0 || end == cursor)
				break;
			if (value > 0 && value <= 65535)
				ports[nb_ports++] = (unsigned int) value;

			cursor = end;
			while (*cursor == ',' || *cursor == ' ')
				cursor++;
		}

		if (nb_ports == 0)
			note(NULL, WARNING, USER, "netmon: no usable port in "
				"\"%s\", no traffic will be intercepted", list);
	}

	for (size_t i = 0; i < nb_ports; i++) {
		if (ports[i] == port)
			return true;
	}

	return false;
}

/**
 * Return how many bits are set in @value, that is the length of the
 * prefix a netmask stands for.
 */
static int popcount32(uint32_t value)
{
	int count = 0;

	while (value != 0) {
		count += (int) (value & 1);
		value >>= 1;
	}

	return count;
}

/**
 * Store @name and @addr/@mask into the first free route of @shared,
 * ignoring duplicates.  An entry created beforehand with a null mask
 * is a placeholder -- the default route, whose interface is known
 * before getifaddrs(3) has described it -- so it gets its real prefix
 * filled in when the loop finally reaches it.  Returns the index of
 * the entry, or -1 if @shared is full.
 */
static int add_route(NetmonShared *shared, const char *name,
			uint32_t addr, uint32_t mask)
{
	int index;

	for (index = 0; index < shared->route_count; index++) {
		if (strcmp(shared->routes[index].name, name) != 0)
			continue;

		if (shared->routes[index].mask == 0 && mask != 0) {
			shared->routes[index].addr = addr;
			shared->routes[index].mask = mask;
		}
		return index;
	}

	if (shared->route_count >= NETMON_MAX_IFACES)
		return -1;

	index = shared->route_count++;
	strncpy(shared->routes[index].name, name, NETMON_IFNAMSIZ - 1);
	shared->routes[index].name[NETMON_IFNAMSIZ - 1] = '\0';
	shared->routes[index].addr = addr;
	shared->routes[index].mask = mask;

	return index;
}

/**
 * Read /proc/net/route and return the name of the interface the kernel
 * would use to reach a non-local destination, that is the one holding
 * the default route with the lowest metric.  This function returns
 * NULL if the route table is unreadable or has no default route.
 */
static char *find_default_route(char *buffer, size_t size)
{
	char best[NETMON_IFNAMSIZ] = "";
	unsigned long best_metric = ~0UL;
	char *line;
	FILE *fp;

	fp = fopen("/proc/net/route", "r");
	if (fp == NULL)
		return NULL;

	/* Skip the header line.  */
	if (fgets(buffer, (int) size, fp) == NULL) {
		fclose(fp);
		return NULL;
	}

	while ((line = fgets(buffer, (int) size, fp)) != NULL) {
		char ifname[NETMON_IFNAMSIZ];
		unsigned long destination;
		unsigned long metric;
		int nb_fields;

		nb_fields = sscanf(line, "%15s %lx %*x %*x %*d %*d %lx",
					ifname, &destination, &metric);
		if (nb_fields != 3)
			continue;
		if (destination != 0)
			continue;
		if (metric >= best_metric)
			continue;

		best_metric = metric;
		strncpy(best, ifname, sizeof(best) - 1);
		best[sizeof(best) - 1] = '\0';
	}

	fclose(fp);

	if (best[0] == '\0')
		return NULL;

	strncpy(buffer, best, size - 1);
	buffer[size - 1] = '\0';
	return buffer;
}

/**
 * Discover the host interfaces the fake /proc/net/dev can report.
 *
 * The loopback device always comes first, so that the proxy can charge
 * the guest-to-proxy hop to it, and the default route comes second, so
 * that a device with plenty of interfaces -- a phone typically has
 * wlan0, rmnet0, ccmni0, ap0, usb0, dummy0 and more -- cannot push it
 * out of the table.  Every other interface with an IPv4 address is then
 * appended until the table is full.
 */
void netmon_netdev_init(NetmonShared *shared)
{
	char buffer[PATH_MAX];
	char *default_route;
	struct ifaddrs *list = NULL;
	struct ifaddrs *cursor;

	shared->default_route = -1;

	/* Entry 0 is the loopback device: it matches 127.0.0.0/8, which
	 * is where the tracer sends the redirected connections.  Note
	 * that every address here is in host byte order.  */
	add_route(shared, "lo", (uint32_t) INADDR_LOOPBACK, 0xFF000000);

	/* Claim the default route before anything else, then refine its
	 * entry once getifaddrs(3) has given us its prefix.  A null mask
	 * means "matches nothing in particular", so the entry is safe to
	 * create early and is only a last resort for the lookup.  */
	default_route = find_default_route(buffer, sizeof(buffer));
	if (default_route != NULL)
		shared->default_route = add_route(shared, default_route, 0, 0);

	if (getifaddrs(&list) != 0) {
		note(NULL, WARNING, SYSTEM, "netmon: getifaddrs()");
		return;
	}

	for (cursor = list; cursor != NULL; cursor = cursor->ifa_next) {
		struct sockaddr_in *address;
		int index;

		if (cursor->ifa_addr == NULL)
			continue;
		if (cursor->ifa_addr->sa_family != AF_INET)
			continue;
		if ((cursor->ifa_flags & IFF_UP) == 0)
			continue;
		if ((cursor->ifa_flags & IFF_LOOPBACK) != 0)
			continue;

		address = (struct sockaddr_in *) (void *) cursor->ifa_addr;

		/* add_route() only fills in the prefix of an entry it
		 * already knows about when that entry is the null-mask
		 * placeholder, which is how the default route gets its
		 * real prefix.  */
		index = add_route(shared, cursor->ifa_name, ntohl(address->sin_addr.s_addr),
			cursor->ifa_netmask != NULL
				? ntohl(((struct sockaddr_in *) (void *) cursor->ifa_netmask)->sin_addr.s_addr)
				: 0xFFFFFFFF);
		if (index < 0)
			continue;
	}

	freeifaddrs(list);

	/* Report the loopback device plus the first non-loopback one
	 * even if it holds no IPv4 address, so that the guest does not
	 * see an empty /proc/net/dev.  */
	if (shared->route_count < 2) {
		if (getifaddrs(&list) != 0)
			return;
		for (cursor = list; cursor != NULL; cursor = cursor->ifa_next) {
			if ((cursor->ifa_flags & IFF_UP) == 0)
				continue;
			if ((cursor->ifa_flags & IFF_LOOPBACK) != 0)
				continue;
			add_route(shared, cursor->ifa_name, 0, 0);
			break;
		}
		freeifaddrs(list);
	}

	/* No default route in the routing table, which happens on the
	 * devices that route per-uid: fall back to the first interface
	 * that owns a prefix, so that non-local traffic is at least
	 * billed to something better than the loopback device.  */
	if (shared->default_route < 0) {
		int i;

		for (i = 0; i < shared->route_count; i++) {
			if (shared->routes[i].mask != 0)
				shared->default_route = i;
		}
		if (shared->default_route < 0 && shared->route_count > 1)
			shared->default_route = 1;
	}

	shared->iface_count = shared->route_count;
}

/**
 * Return the index of the entry of @shared::routes that would carry
 * traffic towards @destination.  Destinations that are not AF_INET
 * (typically IPv6) are handed to the default route.
 */
int netmon_route_lookup(const NetmonShared *shared, const struct sockaddr *destination)
{
	const struct sockaddr_in *address;
	uint32_t target;
	int best;
	int i;

	/* Destinations we cannot classify (typically IPv6) are left to
	 * the default route.  */
	if (destination == NULL || destination->sa_family != AF_INET)
		return shared->default_route >= 0 ? shared->default_route
						  : (shared->route_count > 0 ? 0 : -1);

	address = (const struct sockaddr_in *) (const void *) destination;
	target = ntohl(address->sin_addr.s_addr);

	/* The longest prefix wins, so that a destination on the same LAN
	 * as an interface is not billed to the default route.  */
	best = -1;
	for (i = 0; i < shared->route_count; i++) {
		uint32_t mask = shared->routes[i].mask;

		if (mask == 0)
			continue;
		if ((target & mask) != (shared->routes[i].addr & mask))
			continue;
		if (best < 0 || popcount32(mask) > popcount32(shared->routes[best].mask))
			best = i;
	}

	if (best >= 0)
		return best;

	if (shared->default_route >= 0)
		return shared->default_route;

	return shared->route_count > 0 ? 0 : -1;
}

/**
 * Add @up bytes sent by the tracee and @down bytes sent back to it to
 * the counters of interface @iface.  @down is accounted as received
 * traffic and @up as transmitted traffic, which is the direction the
 * guest sees them in.
 */
void netmon_charge(NetmonShared *shared, int iface, uint64_t up, uint64_t down)
{
	if (iface < 0 || iface >= NETMON_MAX_IFACES)
		return;

	if (up != 0) {
		__atomic_add_fetch(&shared->tx_bytes[iface], up, __ATOMIC_RELAXED);
		BUMP(shared->tx_packets[iface]);
	}

	if (down != 0) {
		__atomic_add_fetch(&shared->rx_bytes[iface], down, __ATOMIC_RELAXED);
		BUMP(shared->rx_packets[iface]);
	}
}

/**
 * Render the fake /proc/net/dev into the file at @path.  This
 * function returns -1 if the file cannot be written, otherwise 0.
 */
int netmon_netdev_write(const NetmonState *state, const char *path)
{
	const NetmonShared *shared = state->shared;
	FILE *fp;
	int i;

	fp = fopen(path, "w");
	if (fp == NULL) {
		note(NULL, WARNING, SYSTEM, "netmon: can't open '%s'", path);
		return -1;
	}

	fputs(NETMON_NETDEV_HEADER, fp);

	for (i = 0; i < shared->iface_count; i++) {
		uint64_t rx_bytes    = LOAD(shared->rx_bytes[i]);
		uint64_t tx_bytes    = LOAD(shared->tx_bytes[i]);
		uint64_t rx_packets  = LOAD(shared->rx_packets[i]);
		uint64_t tx_packets  = LOAD(shared->tx_packets[i]);

		/* Multicast, compressed and collided frames are not
		 * measured by the proxy: report them as zero rather
		 * than inventing a value.  */
		fprintf(fp, NETMON_NETDEV_ROW,
			shared->routes[i].name,
			rx_bytes, rx_packets,
			/* errs */ (uint64_t) 0,
			/* drop */ (uint64_t) 0,
			/* fifo  */ (uint64_t) 0,
			/* frame */ (uint64_t) 0,
			/* compressed */ (uint64_t) 0,
			/* multicast  */ (uint64_t) 0,
			tx_bytes, tx_packets,
			/* errs */ (uint64_t) 0,
			/* drop */ (uint64_t) 0,
			/* fifo */ (uint64_t) 0,
			/* colls */ (uint64_t) 0,
			/* carrier */ (uint64_t) 0,
			/* compressed */ (uint64_t) 0);
	}

	if (fflush(fp) != 0) {
		note(NULL, WARNING, SYSTEM, "netmon: can't flush '%s'", path);
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}
