source common.sh

pullCache () {
    echo "pulling cache..."
    nix-pull file://$TEST_ROOT/cache/MANIFEST
}

clearStore
clearManifests
pullCache

outPath=$(nix-evaluate -A outPath dependencies.nix | tr -d \")

echo "building $outPath using substitutes..."
nix-store -r $outPath

cat $outPath/input-2/bar

clearStore
clearManifests
pullCache

echo "building $outPath's derivation using substitutes..."
nix-build dependencies.nix --no-out-link

cat $outPath/input-2/bar

# Check that the derivers are set properly.
nix-store -q --deriver $(readLink $outPath/input-2) | grep -q -- "-input-2.drv"

clearManifests
