source common.sh

clearStore

# Find the output path.
outPath=$(nix-evaluate -A outPath simple.nix | tr -d \")
echo "output path is $outPath"

echo $outPath > $TEST_ROOT/sub-paths

# First try a substituter that fails, then one that succeeds
export NIX_SUBSTITUTERS=$(pwd)/substituter2.sh:$(pwd)/substituter.sh

nix-build -j0 -vv simple.nix --no-out-link

text=$(cat "$outPath"/hello)
if test "$text" != "Hallo Wereld"; then echo "wrong substitute output: $text"; exit 1; fi
