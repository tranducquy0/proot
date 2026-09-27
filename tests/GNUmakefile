# GNUmakefile for the restructured PRoot test suite.
# Tests live in category subdirectories and are discovered
# recursively; each test-*.sh / test-*.c is run by `make check`.

DIR     = $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
DIR_NOS = $(patsubst %/,%,$(DIR))
ROOTFS  = $(DIR_NOS)/rootfs
PROOT   = $(DIR)/../src/proot
CC      = gcc
PROOT_RAW = $(PROOT)

# Tests write scratch files to $TMPDIR; default to a writable dir inside
# the tests tree so they pass even when /tmp is not writable (e.g. termux).
TMPDIR  = $(DIR_NOS)/tmp

# Recursively find every test case (skip the built rootfs), as paths
# relative to DIR so the generated check-<relpath> targets line up.
# DIR ends with a slash, so use $(DIR)% (no extra slash) for the prefix.
TESTS_SH = $(patsubst $(DIR)%,%,$(shell find $(DIR) -name "test-*.sh" -not -path "$(ROOTFS)/*"))
TESTS_C  = $(patsubst $(DIR)%,%,$(shell find $(DIR) -name "test-*.c"  -not -path "$(ROOTFS)/*" -not -name "*.unreliable.c"))
TESTS    = $(TESTS_SH) $(TESTS_C)

.PHONY: check clean_failure check_failure setup

check: | clean_failure check_failure

memcheck: PROOT_RAW := $(PROOT)
memcheck: PROOT := $(shell which valgrind) -q --error-exitcode=1 $(PROOT)
memcheck: check

clean_failure:
	@rm -f failure

check_failure: $(addprefix check-,$(TESTS))
	@bash -c '! test -e failure'

check-bindings/test-bindings-dir-ls.sh: bindings/test-bindings-dir-ls.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-dir-ls.sh $(silently); $(call check,$(notdir bindings/test-bindings-dir-ls.sh))

check-bindings/test-bindings-dont-pollute.sh: bindings/test-bindings-dont-pollute.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-dont-pollute.sh $(silently); $(call check,$(notdir bindings/test-bindings-dont-pollute.sh))

check-bindings/test-bindings-multi.sh: bindings/test-bindings-multi.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-multi.sh $(silently); $(call check,$(notdir bindings/test-bindings-multi.sh))

check-bindings/test-bindings-ordering-nested.sh: bindings/test-bindings-ordering-nested.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-ordering-nested.sh $(silently); $(call check,$(notdir bindings/test-bindings-ordering-nested.sh))

check-bindings/test-bindings-ordering-symlink.sh: bindings/test-bindings-ordering-symlink.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-ordering-symlink.sh $(silently); $(call check,$(notdir bindings/test-bindings-ordering-symlink.sh))

check-bindings/test-bindings-proc-fd.sh: bindings/test-bindings-proc-fd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-proc-fd.sh $(silently); $(call check,$(notdir bindings/test-bindings-proc-fd.sh))

check-bindings/test-bindings-rmdir-dot.sh: bindings/test-bindings-rmdir-dot.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-rmdir-dot.sh $(silently); $(call check,$(notdir bindings/test-bindings-rmdir-dot.sh))

check-bindings/test-bindings-smoke.sh: bindings/test-bindings-smoke.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-bindings-smoke.sh $(silently); $(call check,$(notdir bindings/test-bindings-smoke.sh))

check-bindings/test-chroot-rel-binary.sh: bindings/test-chroot-rel-binary.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex bindings/test-chroot-rel-binary.sh $(silently); $(call check,$(notdir bindings/test-chroot-rel-binary.sh))

check-capabilities/test-caps-setcap.sh: capabilities/test-caps-setcap.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex capabilities/test-caps-setcap.sh $(silently); $(call check,$(notdir capabilities/test-caps-setcap.sh))

check-chroot-rootfs/test-chroot-mount-hostfs-wd.sh: chroot-rootfs/test-chroot-mount-hostfs-wd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-chroot-mount-hostfs-wd.sh $(silently); $(call check,$(notdir chroot-rootfs/test-chroot-mount-hostfs-wd.sh))

check-chroot-rootfs/test-chroot-mount-wd.sh: chroot-rootfs/test-chroot-mount-wd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-chroot-mount-wd.sh $(silently); $(call check,$(notdir chroot-rootfs/test-chroot-mount-wd.sh))

check-chroot-rootfs/test-chroot-path-bin.sh: chroot-rootfs/test-chroot-path-bin.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-chroot-path-bin.sh $(silently); $(call check,$(notdir chroot-rootfs/test-chroot-path-bin.sh))

check-chroot-rootfs/test-chroot-path-env.sh: chroot-rootfs/test-chroot-path-env.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-chroot-path-env.sh $(silently); $(call check,$(notdir chroot-rootfs/test-chroot-path-env.sh))

check-chroot-rootfs/test-chroot-path-search.sh: chroot-rootfs/test-chroot-path-search.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-chroot-path-search.sh $(silently); $(call check,$(notdir chroot-rootfs/test-chroot-path-search.sh))

check-chroot-rootfs/test-chroot-rootfs-symlink-exec.sh: chroot-rootfs/test-chroot-rootfs-symlink-exec.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-chroot-rootfs-symlink-exec.sh $(silently); $(call check,$(notdir chroot-rootfs/test-chroot-rootfs-symlink-exec.sh))

check-chroot-rootfs/test-chroot-script-exec.sh: chroot-rootfs/test-chroot-script-exec.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-chroot-script-exec.sh $(silently); $(call check,$(notdir chroot-rootfs/test-chroot-script-exec.sh))

check-chroot-rootfs/test-cli-option-precedence.sh: chroot-rootfs/test-cli-option-precedence.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-cli-option-precedence.sh $(silently); $(call check,$(notdir chroot-rootfs/test-cli-option-precedence.sh))

check-chroot-rootfs/test-kompat-force-kompat.sh: chroot-rootfs/test-kompat-force-kompat.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-kompat-force-kompat.sh $(silently); $(call check,$(notdir chroot-rootfs/test-kompat-force-kompat.sh))

check-chroot-rootfs/test-kompat-kernel-release.sh: chroot-rootfs/test-kompat-kernel-release.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-kompat-kernel-release.sh $(silently); $(call check,$(notdir chroot-rootfs/test-kompat-kernel-release.sh))

check-chroot-rootfs/test-kompat-release-length.sh: chroot-rootfs/test-kompat-release-length.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-kompat-release-length.sh $(silently); $(call check,$(notdir chroot-rootfs/test-kompat-release-length.sh))

check-chroot-rootfs/test-kompat-uts-tuple.sh: chroot-rootfs/test-kompat-uts-tuple.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex chroot-rootfs/test-kompat-uts-tuple.sh $(silently); $(call check,$(notdir chroot-rootfs/test-kompat-uts-tuple.sh))

check-cwd/test-cwd-bind-wd.sh: cwd/test-cwd-bind-wd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-bind-wd.sh $(silently); $(call check,$(notdir cwd/test-cwd-bind-wd.sh))

check-cwd/test-cwd-bindings.sh: cwd/test-cwd-bindings.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-bindings.sh $(silently); $(call check,$(notdir cwd/test-cwd-bindings.sh))

check-cwd/test-cwd-dot-wd.sh: cwd/test-cwd-dot-wd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-dot-wd.sh $(silently); $(call check,$(notdir cwd/test-cwd-dot-wd.sh))

check-cwd/test-cwd-getcwd-symlink.c: cwd/test-cwd-getcwd-symlink.c setup $(ROOTFS)/bin/test-cwd-getcwd-symlink
$(ROOTFS)/bin/test-cwd-getcwd-symlink: cwd/test-cwd-getcwd-symlink.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-cwd-getcwd-symlink $(silently); $(call check,test-cwd-getcwd-symlink)

check-cwd/test-cwd-mkdir-rmdir.sh: cwd/test-cwd-mkdir-rmdir.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-mkdir-rmdir.sh $(silently); $(call check,$(notdir cwd/test-cwd-mkdir-rmdir.sh))

check-cwd/test-cwd-pwd-physical.sh: cwd/test-cwd-pwd-physical.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-pwd-physical.sh $(silently); $(call check,$(notdir cwd/test-cwd-pwd-physical.sh))

check-cwd/test-cwd-pwd.sh: cwd/test-cwd-pwd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-pwd.sh $(silently); $(call check,$(notdir cwd/test-cwd-pwd.sh))

check-cwd/test-cwd-recreate.sh: cwd/test-cwd-recreate.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-recreate.sh $(silently); $(call check,$(notdir cwd/test-cwd-recreate.sh))

check-cwd/test-cwd-relative-bind.sh: cwd/test-cwd-relative-bind.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-relative-bind.sh $(silently); $(call check,$(notdir cwd/test-cwd-relative-bind.sh))

check-cwd/test-cwd-rename-rmdir.c: cwd/test-cwd-rename-rmdir.c setup $(ROOTFS)/bin/test-cwd-rename-rmdir
$(ROOTFS)/bin/test-cwd-rename-rmdir: cwd/test-cwd-rename-rmdir.c
	$(Q)$(CC) -static $< -o $@ $(silently) 2>/dev/null || $(CC) $< -o $@ $(silently) || true
	$(PROOT) -r $(ROOTFS) -b . /bin/test-cwd-rename-rmdir $(silently); $(call check,test-cwd-rename-rmdir)
	$(PROOT) /bin/test-cwd-rename-rmdir $(silently); $(call check,test-cwd-rename-rmdir)

check-cwd/test-cwd-symlink-errors.sh: cwd/test-cwd-symlink-errors.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-symlink-errors.sh $(silently); $(call check,$(notdir cwd/test-cwd-symlink-errors.sh))

check-cwd/test-cwd-thread-fork.c: cwd/test-cwd-thread-fork.c setup $(ROOTFS)/bin/test-cwd-thread-fork
$(ROOTFS)/bin/test-cwd-thread-fork: cwd/test-cwd-thread-fork.c
	$(Q)$(CC) -pthread -static $< -o $@ $(silently) 2>/dev/null || $(CC) -pthread $< -o $@ $(silently) || true
	$(PROOT) /bin/test-cwd-thread-fork $(silently); $(call check,test-cwd-thread-fork)

check-cwd/test-cwd-wd-pwd.sh: cwd/test-cwd-wd-pwd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex cwd/test-cwd-wd-pwd.sh $(silently); $(call check,$(notdir cwd/test-cwd-wd-pwd.sh))

check-exec/test-exec-32bit.sh: exec/test-exec-32bit.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex exec/test-exec-32bit.sh $(silently); $(call check,$(notdir exec/test-exec-32bit.sh))

check-exec/test-exec-argv0-shebang.sh: exec/test-exec-argv0-shebang.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex exec/test-exec-argv0-shebang.sh $(silently); $(call check,$(notdir exec/test-exec-argv0-shebang.sh))

check-exec/test-exec-brk-heap.c: exec/test-exec-brk-heap.c setup $(ROOTFS)/bin/test-exec-brk-heap
$(ROOTFS)/bin/test-exec-brk-heap: exec/test-exec-brk-heap.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) /bin/test-exec-brk-heap $(silently); $(call check,test-exec-brk-heap)

check-exec/test-exec-ctor-once.c: exec/test-exec-ctor-once.c setup $(ROOTFS)/bin/test-exec-ctor-once
$(ROOTFS)/bin/test-exec-ctor-once: exec/test-exec-ctor-once.c
	$(Q)$(CC) -fPIE -pie $< -o $@ $(silently) || true
	$(PROOT) /bin/test-exec-ctor-once $(silently); $(call check,test-exec-ctor-once)

check-exec/test-exec-heap-rlimit.c: exec/test-exec-heap-rlimit.c setup $(ROOTFS)/bin/test-exec-heap-rlimit
$(ROOTFS)/bin/test-exec-heap-rlimit: exec/test-exec-heap-rlimit.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -w . /bin/test-exec-heap-rlimit $(silently); $(call check,test-exec-heap-rlimit)

check-exec/test-exec-loader-errors.c: exec/test-exec-loader-errors.c setup $(ROOTFS)/bin/test-exec-loader-errors
$(ROOTFS)/bin/test-exec-loader-errors: exec/test-exec-loader-errors.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-exec-loader-errors $(silently); $(call check,test-exec-loader-errors)

check-exec/test-exec-nested-shebang.sh: exec/test-exec-nested-shebang.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex exec/test-exec-nested-shebang.sh $(silently); $(call check,$(notdir exec/test-exec-nested-shebang.sh))

check-exec/test-exec-pthread-placeholder.c: exec/test-exec-pthread-placeholder.c setup $(ROOTFS)/bin/test-exec-pthread-placeholder
$(ROOTFS)/bin/test-exec-pthread-placeholder: exec/test-exec-pthread-placeholder.c
	$(Q)$(CC) -pthread $< -o $@ $(silently) || true
	$(PROOT) /bin/test-exec-pthread-placeholder $(silently); $(call check,test-exec-pthread-placeholder)

check-exec/test-exec-q-argv-rewrite.sh: exec/test-exec-q-argv-rewrite.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex exec/test-exec-q-argv-rewrite.sh $(silently); $(call check,$(notdir exec/test-exec-q-argv-rewrite.sh))

check-exec/test-exec-sh-pwd.sh: exec/test-exec-sh-pwd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex exec/test-exec-sh-pwd.sh $(silently); $(call check,$(notdir exec/test-exec-sh-pwd.sh))

check-exec/test-exec-shebang-edge.sh: exec/test-exec-shebang-edge.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex exec/test-exec-shebang-edge.sh $(silently); $(call check,$(notdir exec/test-exec-shebang-edge.sh))

check-exec/test-exec-vfork.c: exec/test-exec-vfork.c setup $(ROOTFS)/bin/test-exec-vfork
$(ROOTFS)/bin/test-exec-vfork: exec/test-exec-vfork.c
	$(Q)$(CC) -static $< -o $@ $(silently) 2>/dev/null || $(CC) $< -o $@ $(silently) || true
	$(PROOT) /bin/test-exec-vfork $(silently); $(call check,test-exec-vfork)
	$(PROOT) /bin/test-exec-vfork 1 $(silently); $(call check,test-exec-vfork)

check-exec/test-fork-bomb.c: exec/test-fork-bomb.c setup $(ROOTFS)/bin/test-fork-bomb
$(ROOTFS)/bin/test-fork-bomb: exec/test-fork-bomb.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-fork-bomb $(silently); $(call check,test-fork-bomb)

check-exec/test-pipe-fork-kill.c: exec/test-pipe-fork-kill.c setup $(ROOTFS)/bin/test-pipe-fork-kill
$(ROOTFS)/bin/test-pipe-fork-kill: exec/test-pipe-fork-kill.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-pipe-fork-kill $(silently); $(call check,test-pipe-fork-kill)

check-fake-id0/test-fake-id0-getres.c: fake-id0/test-fake-id0-getres.c setup $(ROOTFS)/bin/test-fake-id0-getres
$(ROOTFS)/bin/test-fake-id0-getres: fake-id0/test-fake-id0-getres.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-fake-id0-getres $(silently); $(call check,test-fake-id0-getres)

check-fake-id0/test-fake-id0-mknod.sh: fake-id0/test-fake-id0-mknod.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex fake-id0/test-fake-id0-mknod.sh $(silently); $(call check,$(notdir fake-id0/test-fake-id0-mknod.sh))

check-fake-id0/test-fake-id0-permission-bypass.sh: fake-id0/test-fake-id0-permission-bypass.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex fake-id0/test-fake-id0-permission-bypass.sh $(silently); $(call check,$(notdir fake-id0/test-fake-id0-permission-bypass.sh))

check-fake-id0/test-fake-id0-setuid-exec-32.sh: fake-id0/test-fake-id0-setuid-exec-32.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex fake-id0/test-fake-id0-setuid-exec-32.sh $(silently); $(call check,$(notdir fake-id0/test-fake-id0-setuid-exec-32.sh))

check-fake-id0/test-fake-id0-setuid-exec.sh: fake-id0/test-fake-id0-setuid-exec.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex fake-id0/test-fake-id0-setuid-exec.sh $(silently); $(call check,$(notdir fake-id0/test-fake-id0-setuid-exec.sh))

check-fake-id0/test-fake-id0-setuid-thread.c: fake-id0/test-fake-id0-setuid-thread.c setup $(ROOTFS)/bin/test-fake-id0-setuid-thread
$(ROOTFS)/bin/test-fake-id0-setuid-thread: fake-id0/test-fake-id0-setuid-thread.c
	$(Q)$(CC) -pthread -static $< -o $@ $(silently) 2>/dev/null || $(CC) -pthread $< -o $@ $(silently) || true
	$(PROOT) -0 /bin/test-fake-id0-setuid-thread $(silently); $(call check,test-fake-id0-setuid-thread)
	! $(PROOT) -i 123:456 /bin/test-fake-id0-setuid-thread $(silently); $(call check,test-fake-id0-setuid-thread)

check-fake-id0/test-fake-id0-stat.sh: fake-id0/test-fake-id0-stat.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex fake-id0/test-fake-id0-stat.sh $(silently); $(call check,$(notdir fake-id0/test-fake-id0-stat.sh))

check-fake-id0/test-fake-id0-uid-gid.sh: fake-id0/test-fake-id0-uid-gid.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex fake-id0/test-fake-id0-uid-gid.sh $(silently); $(call check,$(notdir fake-id0/test-fake-id0-uid-gid.sh))

check-misc/test-misc-07e9b1a2.c: misc/test-misc-07e9b1a2.c setup $(ROOTFS)/bin/test-misc-07e9b1a2
$(ROOTFS)/bin/test-misc-07e9b1a2: misc/test-misc-07e9b1a2.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-misc-07e9b1a2 $(silently); $(call check,test-misc-07e9b1a2)

check-misc/test-misc-11111111.sh: misc/test-misc-11111111.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-11111111.sh $(silently); $(call check,$(notdir misc/test-misc-11111111.sh))

check-misc/test-misc-22222222.sh: misc/test-misc-22222222.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-22222222.sh $(silently); $(call check,$(notdir misc/test-misc-22222222.sh))

check-misc/test-misc-305ae31d.sh: misc/test-misc-305ae31d.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-305ae31d.sh $(silently); $(call check,$(notdir misc/test-misc-305ae31d.sh))

check-misc/test-misc-517e1d6a.sh: misc/test-misc-517e1d6a.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-517e1d6a.sh $(silently); $(call check,$(notdir misc/test-misc-517e1d6a.sh))

check-misc/test-misc-53355a5b.sh: misc/test-misc-53355a5b.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-53355a5b.sh $(silently); $(call check,$(notdir misc/test-misc-53355a5b.sh))

check-misc/test-misc-8a83376a.sh: misc/test-misc-8a83376a.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-8a83376a.sh $(silently); $(call check,$(notdir misc/test-misc-8a83376a.sh))

check-misc/test-misc-b161bc0a.sh: misc/test-misc-b161bc0a.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-b161bc0a.sh $(silently); $(call check,$(notdir misc/test-misc-b161bc0a.sh))

check-misc/test-misc-bbbbbbbb.sh: misc/test-misc-bbbbbbbb.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-bbbbbbbb.sh $(silently); $(call check,$(notdir misc/test-misc-bbbbbbbb.sh))

check-misc/test-misc-c15999f9.sh: misc/test-misc-c15999f9.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-c15999f9.sh $(silently); $(call check,$(notdir misc/test-misc-c15999f9.sh))

check-misc/test-misc-readdir.sh: misc/test-misc-readdir.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-readdir.sh $(silently); $(call check,$(notdir misc/test-misc-readdir.sh))

check-misc/test-misc-readdir2.sh: misc/test-misc-readdir2.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex misc/test-misc-readdir2.sh $(silently); $(call check,$(notdir misc/test-misc-readdir2.sh))

check-mount-emulation/test-mount-emu-dev-null.sh: mount-emulation/test-mount-emu-dev-null.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex mount-emulation/test-mount-emu-dev-null.sh $(silently); $(call check,$(notdir mount-emulation/test-mount-emu-dev-null.sh))

check-mount-emulation/test-mount-emu-make.sh: mount-emulation/test-mount-emu-make.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex mount-emulation/test-mount-emu-make.sh $(silently); $(call check,$(notdir mount-emulation/test-mount-emu-make.sh))

check-mount-emulation/test-mount-emu-nested.sh: mount-emulation/test-mount-emu-nested.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex mount-emulation/test-mount-emu-nested.sh $(silently); $(call check,$(notdir mount-emulation/test-mount-emu-nested.sh))

check-mount-emulation/test-mount-emu-raw-guard.sh: mount-emulation/test-mount-emu-raw-guard.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex mount-emulation/test-mount-emu-raw-guard.sh $(silently); $(call check,$(notdir mount-emulation/test-mount-emu-raw-guard.sh))

check-mount-emulation/test-mount-emu-raw-pass.sh: mount-emulation/test-mount-emu-raw-pass.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex mount-emulation/test-mount-emu-raw-pass.sh $(silently); $(call check,$(notdir mount-emulation/test-mount-emu-raw-pass.sh))

check-mount-emulation/test-mount-emu-tmpdir.sh: mount-emulation/test-mount-emu-tmpdir.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex mount-emulation/test-mount-emu-tmpdir.sh $(silently); $(call check,$(notdir mount-emulation/test-mount-emu-tmpdir.sh))

check-networking/test-net-unix-long-path.c: networking/test-net-unix-long-path.c setup $(ROOTFS)/bin/test-net-unix-long-path
$(ROOTFS)/bin/test-net-unix-long-path: networking/test-net-unix-long-path.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-net-unix-long-path $(silently); $(call check,test-net-unix-long-path)

check-networking/test-netmon-dev.sh: networking/test-netmon-dev.sh setup $(ROOTFS)/bin/test-netmon-dev
$(ROOTFS)/bin/test-netmon-dev: networking/test-netmon-dev.c
	$(Q)$(CC) $< -o $@ $(silently) || true
check-networking/test-netmon-dev.sh: $(ROOTFS)/bin/test-netmon-dev
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex networking/test-netmon-dev.sh $(silently); $(call check,$(notdir networking/test-netmon-dev.sh))

check-networking/test-net-unix-socket.c: networking/test-net-unix-socket.c setup $(ROOTFS)/bin/test-net-unix-socket
$(ROOTFS)/bin/test-net-unix-socket: networking/test-net-unix-socket.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-net-unix-socket $(silently); $(call check,test-net-unix-socket)

check-path-translation/test-path-translation-cwd-link.sh: path-translation/test-path-translation-cwd-link.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-cwd-link.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-cwd-link.sh))

check-path-translation/test-path-translation-dev-std-fd.sh: path-translation/test-path-translation-dev-std-fd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-dev-std-fd.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-dev-std-fd.sh))

check-path-translation/test-path-translation-faccessat.c: path-translation/test-path-translation-faccessat.c setup $(ROOTFS)/bin/test-path-translation-faccessat
$(ROOTFS)/bin/test-path-translation-faccessat: path-translation/test-path-translation-faccessat.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	echo test | env PROOT_DONT_POLLUTE_ROOTFS=1 $(PROOT) -b /bin:/this_shall_not_exist_outside_proot /bin/test-path-translation-faccessat $(silently); $(call check,test-path-translation-faccessat)

check-path-translation/test-path-translation-fchownat.c: path-translation/test-path-translation-fchownat.c setup $(ROOTFS)/bin/test-path-translation-fchownat
$(ROOTFS)/bin/test-path-translation-fchownat: path-translation/test-path-translation-fchownat.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-path-translation-fchownat $(silently); $(call check,test-path-translation-fchownat)

check-path-translation/test-path-translation-fd-pipe.sh: path-translation/test-path-translation-fd-pipe.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-fd-pipe.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-fd-pipe.sh))

check-path-translation/test-path-translation-fd-symlink.sh: path-translation/test-path-translation-fd-symlink.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-fd-symlink.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-fd-symlink.sh))

check-path-translation/test-path-translation-lutimes.c: path-translation/test-path-translation-lutimes.c setup $(ROOTFS)/bin/test-path-translation-lutimes
$(ROOTFS)/bin/test-path-translation-lutimes: path-translation/test-path-translation-lutimes.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-path-translation-lutimes $(silently); $(call check,test-path-translation-lutimes)

check-path-translation/test-path-translation-open-enotdir.c: path-translation/test-path-translation-open-enotdir.c setup $(ROOTFS)/bin/test-path-translation-open-enotdir
$(ROOTFS)/bin/test-path-translation-open-enotdir: path-translation/test-path-translation-open-enotdir.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-path-translation-open-enotdir $(silently); $(call check,test-path-translation-open-enotdir)

check-path-translation/test-path-translation-openat-dirfd.c: path-translation/test-path-translation-openat-dirfd.c setup $(ROOTFS)/bin/test-path-translation-openat-dirfd
$(ROOTFS)/bin/test-path-translation-openat-dirfd: path-translation/test-path-translation-openat-dirfd.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	echo test | $(PROOT) /bin/test-path-translation-openat-dirfd $(silently); $(call check,test-path-translation-openat-dirfd)

check-path-translation/test-path-translation-proc-self-fd.sh: path-translation/test-path-translation-proc-self-fd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-proc-self-fd.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-proc-self-fd.sh))

check-path-translation/test-path-translation-readlink-cwd.sh: path-translation/test-path-translation-readlink-cwd.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-readlink-cwd.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-readlink-cwd.sh))

check-path-translation/test-path-translation-readlink-getcwd.c: path-translation/test-path-translation-readlink-getcwd.c setup $(ROOTFS)/bin/test-path-translation-readlink-getcwd
$(ROOTFS)/bin/test-path-translation-readlink-getcwd: path-translation/test-path-translation-readlink-getcwd.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-path-translation-readlink-getcwd $(silently); $(call check,test-path-translation-readlink-getcwd)

check-path-translation/test-path-translation-rootfs-symlinks.sh: path-translation/test-path-translation-rootfs-symlinks.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-rootfs-symlinks.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-rootfs-symlinks.sh))

check-path-translation/test-path-translation-self-exe.c: path-translation/test-path-translation-self-exe.c setup $(ROOTFS)/bin/test-path-translation-self-exe
$(ROOTFS)/bin/test-path-translation-self-exe: path-translation/test-path-translation-self-exe.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-path-translation-self-exe $(silently); $(call check,test-path-translation-self-exe)

check-path-translation/test-path-translation-symlink-bindings.sh: path-translation/test-path-translation-symlink-bindings.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-symlink-bindings.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-symlink-bindings.sh))

check-path-translation/test-path-translation-symlink-ls.sh: path-translation/test-path-translation-symlink-ls.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex path-translation/test-path-translation-symlink-ls.sh $(silently); $(call check,$(notdir path-translation/test-path-translation-symlink-ls.sh))

check-proc/test-proc-auxv.c: proc/test-proc-auxv.c setup $(ROOTFS)/bin/test-proc-auxv
$(ROOTFS)/bin/test-proc-auxv: proc/test-proc-auxv.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	@which gdb >/dev/null 2>&1 || rm -f $(ROOTFS)/bin/test-proc-auxv $(silently); $(call check,test-proc-auxv)
	$(PROOT) -b /proc -r $(ROOTFS) gdb -return-child-result -ex run -ex quit /bin/test-proc-auxv $(silently); $(call check,test-proc-auxv)

check-proc/test-proc-cmdline-sh.sh: proc/test-proc-cmdline-sh.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex proc/test-proc-cmdline-sh.sh $(silently); $(call check,$(notdir proc/test-proc-cmdline-sh.sh))

check-proc/test-proc-cmdline.c: proc/test-proc-cmdline.c setup $(ROOTFS)/bin/test-proc-cmdline
$(ROOTFS)/bin/test-proc-cmdline: proc/test-proc-cmdline.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-proc-cmdline $(silently); $(call check,test-proc-cmdline)

check-proc/test-proc-comm.sh: proc/test-proc-comm.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex proc/test-proc-comm.sh $(silently); $(call check,$(notdir proc/test-proc-comm.sh))

check-proc/test-proc-exe-empty-argv.c: proc/test-proc-exe-empty-argv.c setup $(ROOTFS)/bin/test-proc-exe-empty-argv
$(ROOTFS)/bin/test-proc-exe-empty-argv: proc/test-proc-exe-empty-argv.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) /bin/test-proc-exe-empty-argv $(silently); $(call check,test-proc-exe-empty-argv)

check-proc/test-proc-exe-execlp.c: proc/test-proc-exe-execlp.c setup $(ROOTFS)/bin/test-proc-exe-execlp
$(ROOTFS)/bin/test-proc-exe-execlp: proc/test-proc-exe-execlp.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-proc-exe-execlp $(silently); $(call check,test-proc-exe-execlp)

check-proc/test-proc-exe-null-argv.c: proc/test-proc-exe-null-argv.c setup $(ROOTFS)/bin/test-proc-exe-null-argv
$(ROOTFS)/bin/test-proc-exe-null-argv: proc/test-proc-exe-null-argv.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) /bin/test-proc-exe-null-argv $(silently); $(call check,test-proc-exe-null-argv)

check-proc/test-proc-fd-dotdot.c: proc/test-proc-fd-dotdot.c setup $(ROOTFS)/bin/test-proc-fd-dotdot
$(ROOTFS)/bin/test-proc-fd-dotdot: proc/test-proc-fd-dotdot.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-proc-fd-dotdot $(silently); $(call check,test-proc-fd-dotdot)

check-proc/test-proc-fd-open.c: proc/test-proc-fd-open.c setup $(ROOTFS)/bin/test-proc-fd-open
$(ROOTFS)/bin/test-proc-fd-open: proc/test-proc-fd-open.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-proc-fd-open $(silently); $(call check,test-proc-fd-open)

check-proc/test-proc-self-exe-foreign.sh: proc/test-proc-self-exe-foreign.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex proc/test-proc-self-exe-foreign.sh $(silently); $(call check,$(notdir proc/test-proc-self-exe-foreign.sh))

check-proc/test-proc-self-exe-script.sh: proc/test-proc-self-exe-script.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex proc/test-proc-self-exe-script.sh $(silently); $(call check,$(notdir proc/test-proc-self-exe-script.sh))

check-proc/test-proc-self-exe.sh: proc/test-proc-self-exe.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex proc/test-proc-self-exe.sh $(silently); $(call check,$(notdir proc/test-proc-self-exe.sh))

check-proc/test-proc-self-root.sh: proc/test-proc-self-root.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex proc/test-proc-self-root.sh $(silently); $(call check,$(notdir proc/test-proc-self-root.sh))

check-proc/test-proc-tracer-pid.sh: proc/test-proc-tracer-pid.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex proc/test-proc-tracer-pid.sh $(silently); $(call check,$(notdir proc/test-proc-tracer-pid.sh))

check-ptrace/test-ptrace-gdb.sh: ptrace/test-ptrace-gdb.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex ptrace/test-ptrace-gdb.sh $(silently); $(call check,$(notdir ptrace/test-ptrace-gdb.sh))

check-ptrace/test-ptrace-guest-parity.sh: ptrace/test-ptrace-guest-parity.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex ptrace/test-ptrace-guest-parity.sh $(silently); $(call check,$(notdir ptrace/test-ptrace-guest-parity.sh))

check-ptrace/test-ptrace-run.sh: ptrace/test-ptrace-run.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex ptrace/test-ptrace-run.sh $(silently); $(call check,$(notdir ptrace/test-ptrace-run.sh))

check-ptrace/test-ptrace-strace-execve.sh: ptrace/test-ptrace-strace-execve.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex ptrace/test-ptrace-strace-execve.sh $(silently); $(call check,$(notdir ptrace/test-ptrace-strace-execve.sh))

check-ptrace/test-ptrace-strace-fork.sh: ptrace/test-ptrace-strace-fork.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex ptrace/test-ptrace-strace-fork.sh $(silently); $(call check,$(notdir ptrace/test-ptrace-strace-fork.sh))

check-ptrace/test-ptrace-syscall-loop.c: ptrace/test-ptrace-syscall-loop.c setup $(ROOTFS)/bin/test-ptrace-syscall-loop
$(ROOTFS)/bin/test-ptrace-syscall-loop: ptrace/test-ptrace-syscall-loop.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-ptrace-syscall-loop $(silently); $(call check,test-ptrace-syscall-loop)

check-ptrace/test-ptrace-traceme.c: ptrace/test-ptrace-traceme.c setup $(ROOTFS)/bin/test-ptrace-traceme
$(ROOTFS)/bin/test-ptrace-traceme: ptrace/test-ptrace-traceme.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-ptrace-traceme $(silently); $(call check,test-ptrace-traceme)

check-ro-bind/test-ro-bind-enforce.sh: ro-bind/test-ro-bind-enforce.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex ro-bind/test-ro-bind-enforce.sh $(silently); $(call check,$(notdir ro-bind/test-ro-bind-enforce.sh))

check-ro-bind/test-ro-bind-symlink-follow.sh: ro-bind/test-ro-bind-symlink-follow.sh setup
	$(Q)env ROOT_RAW="$(PROOT_RAW)" PROOT="$(PROOT)" ROOTFS=$(ROOTFS) TMPDIR="$(TMPDIR)" sh -ex ro-bind/test-ro-bind-symlink-follow.sh $(silently); $(call check,$(notdir ro-bind/test-ro-bind-symlink-follow.sh))

check-threading-signals/test-fork-no-wait.c: threading-signals/test-fork-no-wait.c setup $(ROOTFS)/bin/test-fork-no-wait
$(ROOTFS)/bin/test-fork-no-wait: threading-signals/test-fork-no-wait.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-fork-no-wait $(silently); $(call check,test-fork-no-wait)

check-threading-signals/test-fork-wait-status.c: threading-signals/test-fork-wait-status.c setup $(ROOTFS)/bin/test-fork-wait-status
$(ROOTFS)/bin/test-fork-wait-status: threading-signals/test-fork-wait-status.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-fork-wait-status $(silently); $(call check,test-fork-wait-status)

check-threading-signals/test-pipe-nonblock.c: threading-signals/test-pipe-nonblock.c setup $(ROOTFS)/bin/test-pipe-nonblock
$(ROOTFS)/bin/test-pipe-nonblock: threading-signals/test-pipe-nonblock.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-pipe-nonblock $(silently); $(call check,test-pipe-nonblock)

check-threading-signals/test-signal-sigtrap.c: threading-signals/test-signal-sigtrap.c setup $(ROOTFS)/bin/test-signal-sigtrap
$(ROOTFS)/bin/test-signal-sigtrap: threading-signals/test-signal-sigtrap.c
	$(Q)$(CC)  $< -o $@ $(silently) || true
	$(PROOT) -b /proc -r $(ROOTFS) /bin/test-signal-sigtrap $(silently); $(call check,test-signal-sigtrap)

check-threading-signals/test-thread-pthread-join.c: threading-signals/test-thread-pthread-join.c setup $(ROOTFS)/bin/test-thread-pthread-join
$(ROOTFS)/bin/test-thread-pthread-join: threading-signals/test-thread-pthread-join.c
	$(Q)$(CC) -pthread $< -o $@ $(silently) || true
	@which gdb >/dev/null 2>&1 || rm -f $(ROOTFS)/bin/test-thread-pthread-join $(silently); $(call check,test-thread-pthread-join)
	$(PROOT) -b /proc -r $(ROOTFS) gdb -return-child-result -ex run -ex quit /bin/test-thread-pthread-join $(silently); $(call check,test-thread-pthread-join)

check_c = $(Q)if [ -e $< ]; then			\
		$(2) $(silently); $(call check,$(1))	\
	else						\
		echo "  CHECK	$(1) skipped";		\
	fi

check = case "$$?" in					\
		0)   echo "  CHECK	$(1) ok";;	\
		125) echo "  CHECK	$(1) skipped";;	\
		*)   echo "  CHECK	$(1) FAILED";		\
		touch failure ;;			\
	esac

######################################################################
# Build a clean rootfs

ROOTFS_BIN = \
	   $(ROOTFS)/bin/argv \
	   $(ROOTFS)/bin/argv0 \
	   $(ROOTFS)/bin/cat \
	   $(ROOTFS)/bin/chdir_getcwd \
	   $(ROOTFS)/bin/echo \
	   $(ROOTFS)/bin/exec \
	   $(ROOTFS)/bin/false \
	   $(ROOTFS)/bin/fchdir_getcwd \
	   $(ROOTFS)/bin/fork-wait \
	   $(ROOTFS)/bin/getresgid \
	   $(ROOTFS)/bin/getresuid \
	   $(ROOTFS)/bin/ptrace-2 \
	   $(ROOTFS)/bin/ptrace \
	   $(ROOTFS)/bin/puts_proc_self_exe \
	   $(ROOTFS)/bin/pwd \
	   $(ROOTFS)/bin/readdir \
	   $(ROOTFS)/bin/readlink \
	   $(ROOTFS)/bin/symlink \
	   $(ROOTFS)/bin/true \
	   $(ROOTFS)/bin/abs-true \
	   $(ROOTFS)/bin/rel-true \
	   $(ROOTFS)/bin/exec-m32 \
	   $(ROOTFS)/bin/exec-suid \
	   $(ROOTFS)/bin/exec-sgid \
	   $(ROOTFS)/bin/exec-m32-suid \
	   $(ROOTFS)/bin/exec-m32-sgid

ROOTFS_DIR = $(ROOTFS)/bin $(ROOTFS)/tmp $(TMPDIR)

$(ROOTFS_BIN): | $(ROOTFS_DIR)

$(ROOTFS_DIR):
	@mkdir -p $@

setup: $(ROOTFS_BIN)

$(ROOTFS)/bin/%: helpers/%.c
	$(Q)$(CC) -static $< -o $@ $(silently) 2>/dev/null || $(CC) $< -o $@ $(silently) || true

$(ROOTFS)/bin/abs-true:
	$(Q)ln -fs /bin/true $@

$(ROOTFS)/bin/rel-true:
	$(Q)ln -fs ./true $@

$(ROOTFS)/bin/exec-m32: helpers/exec.c
	$(Q)$(CC) -m32 -static $^ -o $@ $(silently) || true

$(ROOTFS)/bin/exec-suid: $(ROOTFS)/bin/exec
	$(Q)cp $^ $@ $(silently) || true
	$(Q)chmod u+s $@ $(silently) || true

$(ROOTFS)/bin/exec-sgid: $(ROOTFS)/bin/exec
	$(Q)cp $^ $@ $(silently) || true
	$(Q)chmod g+s $@ $(silently) || true

$(ROOTFS)/bin/exec-m32-suid: $(ROOTFS)/bin/exec-m32
	$(Q)cp $^     $@ $(silently) || true
	$(Q)chmod u+s $@ $(silently) || true

$(ROOTFS)/bin/exec-m32-sgid: $(ROOTFS)/bin/exec-m32
	$(Q)cp $^     $@ $(silently) || true
	$(Q)chmod g+s $@ $(silently) || true

.SECONDARY: $(ROOTFS_BIN)

######################################################################
# Beautified output

V = 0
ifeq ($(V), 0)
    quiet = quiet_
    Q     = @
    silently = >/dev/null 2>&1
else
    quiet = 
    Q     = 
    silently = 
endif
