source common.sh

clearStore

if nix-evaluate --readonly-mode -A outPath ./import-derivation.nix; then
    echo "read-only evaluation of an imported derivation unexpectedly failed"
    exit 1
fi

outPath=$(nix-build ./import-derivation.nix --no-out-link)

[ "$(cat $outPath)" = FOO579 ]
