source common.sh

clearStore

# Find the output path.
outPath=$(nix-evaluate -A outPath simple.nix | tr -d \")
echo "output path is $outPath"

echo $outPath > $TEST_ROOT/sub-paths

export NIX_SUBSTITUTERS=$(pwd)/substituter.sh

nix-build simple.nix --dry-run 2>&1 | grep -q "1.00 MiB.*2.00 MiB"

nix-build -vv simple.nix --no-out-link

text=$(cat "$outPath"/hello)
if test "$text" != "Hallo Wereld"; then echo "wrong substitute output: $text"; exit 1; fi
