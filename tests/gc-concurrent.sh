source common.sh

clearStore

outPath1=$(nix-evaluate gc-concurrent.nix -A test1.outPath | tr -d \")

outPath2=$(nix-evaluate gc-concurrent.nix -A test2.outPath | tr -d \")

outPath3=$(nix-build simple.nix --no-out-link)

! test -e $outPath3.lock
touch $outPath3.lock

rm -f "$NIX_STATE_DIR"/gcroots/foo*
ln -s $outPath3 "$NIX_STATE_DIR"/gcroots/foo2

# Start build #1 in the background.  It starts immediately.
nix-build -vv gc-concurrent.nix -A test1 --no-out-link &
pid1=$!

# Start build #2 in the background after 10 seconds.
(sleep 10 && nix-build -vv gc-concurrent.nix -A test2 --no-out-link) &
pid2=$!

# Run the garbage collector while the build is running.
sleep 6
nix-collect-garbage

# Wait for build #1/#2 to finish.
echo waiting for pid $pid1 to finish...
wait $pid1
echo waiting for pid $pid2 to finish...
wait $pid2

# Check that the root of build #1 and its dependencies haven't been
# deleted.  The should not be deleted by the GC because they were
# being built during the GC.
cat $outPath1/foobar
cat $outPath1/input-2/bar

# Check that build #2 has succeeded.  It should succeed because the
# derivation is a GC root.
cat $outPath2/foobar

rm -f "$NIX_STATE_DIR"/gcroots/foo*

# The collector should have deleted lock files for paths that have
# been built previously.
! test -e $outPath3.lock

# If we run the collector now, it should delete outPath1/2.
nix-collect-garbage
! test -e $outPath1
! test -e $outPath2
