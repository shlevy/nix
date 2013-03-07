source common.sh

clearStore

outPath=$(nix-build dependencies.nix --no-out-link)

echo "pushing $outPath"

mkdir -p $TEST_ROOT/cache

nix-push --dest $TEST_ROOT/cache --manifest $outPath --bzip2
