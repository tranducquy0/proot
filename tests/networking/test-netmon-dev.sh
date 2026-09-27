if ! command -v uname >/dev/null 2>&1; then
    exit 125;
fi

# The helper that drives the traffic through the proxy.
if [ ! -x "${ROOTFS}/bin/test-netmon-dev" ]; then
    exit 125;
fi

# The request log is how we check that the proxy really saw the
# traffic, so keep it out of the test output.
LOG="${TMPDIR:-/tmp}/proot-netmon-$$.log"
export PROOT_NETMON_LOG="$LOG"
export PROOT_NETMON_PORTS=18080
trap 'rm -f "$LOG"' EXIT

# Without --netmon the extension is not even loaded: the connection
# must still work, and nothing must be logged.
: > "$LOG"
${PROOT} -b /proc -r ${ROOTFS} ${ROOTFS}/bin/test-netmon-dev
[ -s "$LOG" ] && {
    echo "traffic was logged although --netmon was not given" >&2
    cat "$LOG" >&2
    exit 1
}

# With it, the connection is relayed and reported.
NETMON_EXPECT_PROXY=1 ${PROOT} --netmon -b /proc -r ${ROOTFS} \
    ${ROOTFS}/bin/test-netmon-dev

# The proxy must have logged the request line and its Host header.
grep -q 'GET /netmon-test HTTP/1.1' "$LOG" || {
    echo "the request line was not logged" >&2
    cat "$LOG" >&2
    exit 1
}
grep -q 'host: netmon.invalid' "$LOG" || {
    echo "the Host header was not logged" >&2
    cat "$LOG" >&2
    exit 1
}

# The counters a synthesized /proc/net/dev reports only depend on what
# the proxy relayed, so two identical runs have to agree exactly.
first=$(NETMON_EXPECT_PROXY=1 ${PROOT} --netmon -b /proc -r ${ROOTFS} \
	${ROOTFS}/bin/test-netmon-dev | grep '^LO_TX=')
second=$(NETMON_EXPECT_PROXY=1 ${PROOT} --netmon -b /proc -r ${ROOTFS} \
	${ROOTFS}/bin/test-netmon-dev | grep '^LO_TX=')
[ -n "$first" ] || {
    echo "no loopback counter was reported" >&2
    exit 1
}
[ "$first" = "$second" ] || {
    echo "the reported counters are not deterministic: '${first}' then '${second}'" >&2
    exit 1
}
[ "$first" != "LO_TX=0" ] || {
    echo "no byte was reported as sent" >&2
    exit 1
}

# Quiet mode keeps the counters but drops the per-request lines.
: > "$LOG"
NETMON_EXPECT_PROXY=1 PROOT_NETMON_QUIET=1 ${PROOT} --netmon -b /proc \
    -r ${ROOTFS} ${ROOTFS}/bin/test-netmon-dev >/dev/null
if grep -q 'GET /netmon-test' "$LOG"; then
    echo "PROOT_NETMON_QUIET was ignored" >&2
    exit 1
fi
