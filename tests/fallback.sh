source common.sh

clearStore

drvName=$(nix-evaluate -A name simple.nix | tr -d \")
echo "derivation is $drvName"

outPath=$(nix-evaluate -A outPath simple.nix | tr -d \")
echo "output path is $outPath"

# Build with a substitute that fails.  This should fail.
export NIX_SUBSTITUTERS=$(pwd)/substituter2.sh
if nix-build --no-out-link simple.nix; then echo unexpected fallback; exit 1; fi

# Build with a substitute that fails.  This should fall back to a source build.
export NIX_SUBSTITUTERS=$(pwd)/substituter2.sh
nix-build --fallback --no-out-link simple.nix

text=$(cat "$outPath"/hello)
if test "$text" != "Hello World!"; then exit 1; fi
