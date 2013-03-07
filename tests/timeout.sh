# Test the `--timeout' option.

source common.sh

test "$(nix-evaluate -A system timeout.nix | tr -d \")" = "$system"

failed=0
messages="`nix-build --timeout 2 timeout.nix --no-out-link 2>&1 || failed=1`"
if test $failed -ne 0; then
    echo "error: \`nix-build' succeeded; should have timed out" >&2
    exit 1
fi

if ! echo "$messages" | grep "timed out"; then
    echo "error: \`nix-build' may have failed for reasons other than timeout" >&2
    echo >&2
    echo "output of \`nix-build' follows:" >&2
    echo "$messages" >&2
    exit 1
fi
