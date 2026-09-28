---
name: proot-netmon
description: Work on the --netmon extension - connect(2) redirection, traffic accounting, the synthesized /proc/net/dev, and extension-rewritten paths bypassing the path cache
---

## Key files

- `src/extension/netmon/netmon.c` — `redirect_connect()`, `redirect_connect_exit()`, the slot claim/release
- `src/extension/netmon/netmon.h` — `NetmonState`, `NetmonShared`, `NetmonPending`
- `src/extension/netmon/netmon_proxy.c` — the loopback relay thread (no talloc, no note)
- `src/extension/netmon/netmon_netdev.c` — interface discovery, `/proc/net/dev` rendering
- `tests/networking/test-netmon-dev.{c,sh}` — the test
- `src/path/path.c` — `translate_path_ex()`, where `TRANSLATED_PATH` is fired
- `src/syscall/enter.c` — `translate_path2()`, the path cache
- `src/path/cache.{c,h}` — the cache itself

## Flow

1. `INITIALIZATION` mmaps a `NetmonShared` (counters + per-connection listening sockets), then starts the relay thread.
2. Each in-flight connection owns one loopback listening port, so the relay recognises the destination from the accepted socket alone — no extra syscall in the tracee, and no race with `connect(2)`.
3. `SYSCALL_ENTER_END` on `PR_connect` claims a slot, publishes the real destination, and rewrites the guest's sockaddr to the slot's port. Only `AF_INET`/`AF_INET6`, never loopback or link-local, and only the ports in `PROOT_NETMON_PORTS`.
4. `SYSCALL_EXIT_END` gives the slot back if the kernel refused the connection, remembers the real destination for `getpeername`, and restores the guest's sockaddr.
5. `GUEST_PATH` then `TRANSLATED_PATH` (always the same `translate_path()` call) redirect `open("/proc/net/dev")` to a temporary file rendered from the shared counters.

## Two invariants

**The guest's sockaddr is restored on the way out.** `redirect_connect()` writes the
proxy address straight into the guest's buffer, and the kernel only ever reads it.
`redirect_connect_exit()` therefore puts the original back — for *every* outcome, not
just success, and before anything else. Programs reuse one `struct sockaddr` across
many `connect(2)` calls, and the buffer belongs to the guest the moment `connect(2)`
returns. Only write it back if it still holds the redirected address
(`read_data() == 0` plus `memcmp()`), since the guest may have reused it meanwhile.
Skipping this makes every second `connect(2)` look like a loopback connection.

**A path an extension rewrote is never cached.** `translate_path2()` memoizes
`REGULAR` + `AT_FDCWD` translations. On a cache hit it never calls `translate_path()`,
so `GUEST_PATH`/`TRANSLATED_PATH` do not run and the extension's answer is frozen
forever. That is invisible for `mountinfo` and `link2symlink`, but fatal for netmon:
the synthesized `/proc/net/dev` would be rendered once and the guest would read that
snapshot for the rest of its life. `translate_path_ex()` reports whether an extension
changed the result; `translate_path2()` skips `path_cache_store()` when it did. Use
`translate_path_ex()` with `extended` anywhere the answer can depend on more than the
translation's own arguments.

## Test

```bash
make -C tests check-networking/test-netmon-dev.sh
```

The helper must set `SO_REUSEADDR`: the script reuses one port across runs and the
side that closes first lingers in `TIME_WAIT`, so without it the second `bind(2)`
fails and every run after the first silently skips.

`read_counters()` is tri-state on purpose: `1` well-formed, `0` malformed, `-1`
unopenable. Only the malformed case is a failure; the unopenable one is only a
failure under `NETMON_EXPECT_PROXY`, because that file is synthesized and must exist.
Without the proxy the guest reads the host's own `/proc/net/dev`, which an
unprivileged process is not always allowed to open.

## Environment notes (Termux)

- The host `/proc/net/dev` is unreadable (`EACCES`); Android denies all of `/proc/net`.
  The no-proxy run prints `SKIP: /proc/net/dev is not readable` and passes.
- Static linking of a C test helper fails (`ld.lld: error: unable to find library -lc`);
  the Makefile falls back to a dynamic build.
- `make -C tests setup` prints harmless `Permission denied` from `find` over
  `tests/rootfs` and `tests/tmp`.
- Test runs leave artifacts in `tests/`: `d1/`, `dl1`, `r1`, `rl1`, `rootfs/`, `tmp/`,
  and stray `tests/<hex>` files. Never stage them.
- When comparing a baseline run against a patched one, both must have every
  `tests/rootfs/bin` helper rebuilt. `$(ROOTFS)/bin/%` compiles *and* runs in a single
  recipe, so an up-to-date binary makes make skip the recipe and the test silently
  vanishes from the output — a change in the total CHECK line count means the runs
  were not comparable, not that tests were added.
