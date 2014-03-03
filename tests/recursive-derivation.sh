source common.sh

outPath1=$(nix-build -A usesUsesRecursive recursive-derivation.nix --no-out-link)
outPath2=$(nix-build -A usesUsesDirect recursive-derivation.nix --no-out-link)

[ "$outPath1" == "$outPath2" ]
