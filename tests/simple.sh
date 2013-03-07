source common.sh

test "$(nix-evaluate -A system simple.nix | tr -d \")" = "$system"

outPath=$(nix-build -vv simple.nix --no-out-link)

echo "output path is $outPath"

text=$(cat "$outPath"/hello)
if test "$text" != "Hello World!"; then exit 1; fi

# Directed delete: $outPath is not reachable from a root, so it should
# be deleteable.
nix-store --delete $outPath
if test -e $outPath/hello; then false; fi
