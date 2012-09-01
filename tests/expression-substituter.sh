#! /bin/sh -e
echo substituter args: $* >&2

barOutPath=`nix-instantiate --eval-only -v 0 -A bar.outPath ./expression-substitutes.nix | tr -d \"`
barDrvPath=`nix-instantiate --eval-only -v 0 -A bar.drvPath ./expression-substitutes.nix | tr -d \"`
bazOutPath=`nix-instantiate --eval-only -v 0 -A bar.baz.outPath ./expression-substitutes.nix | tr -d \"`
bazDrvPath=`nix-instantiate --eval-only -v 0 -A bar.baz.drvPath ./expression-substitutes.nix | tr -d \"`
substitutedExprPath=`nix-instantiate --eval-only -v 0 -A substitutedExpr ./expression-substitutes.nix | tr -d \"`
substitutedExprString=`echo -e \`nix-instantiate --eval-only -v 0 -A substitutedExprString ./expression-substitutes.nix | tr -d \"\``
filePath=`nix-instantiate --eval-only -v 0 -A file ./expression-substitutes.nix | tr -d \"`
fileString=`nix-instantiate --eval-only -v 0 -A fileString ./expression-substitutes.nix | tr -d \"`
aDotNixPath=`nix-instantiate --eval-only -v 0 -A aDotNix ./expression-substitutes.nix | tr -d \"`
aDotNixString=`nix-instantiate --eval-only -v 0 -A aDotNixString ./expression-substitutes.nix | tr -d \"`

if test $1 = "--query"; then
    while read cmd args; do
        echo "CMD = $cmd, ARGS = $args" >&2
        if test "$cmd" = "have"; then
            for path in $args; do 
                if test "$path" = "$barOutPath" || test "$path" = "$bazOutPath"; then
                    echo $path
                fi
            done
            echo
        elif test "$cmd" = "info"; then
            for path in $args; do
                if test "$path" = "$barOutPath"
                    echo "$path"
                    echo "$barDrvPath"
                    echo 1
                    echo "$bazOutPath"
                    echo $((1 * 1024 * 1024))
                    echo $((2 * 1024 * 1024))
                elif test "$path" = "$bazOutPath"
                    echo "$path"
                    echo "$bazDrvPath"
                    echo 0
                    echo $((1 * 1024 * 1024)) # download size
                    echo $((2 * 1024 * 1024)) # nar size
                fi
            done
            echo
        elif test "$cmd" = "haveFile"; then
            for path in $args; do
                base=`basename "$path"`
                dir=`dirname "$path"`
                if test "$path" = "$barOutPath/default.nix"; then
                    echo "$path"
                elif test "$path" = "$barOutPath" || test "$path" = "$bazOutPath"; then
                    echo "$path"
                elif test "$dir" = "$barOutPath" || test "$dir" = "$bazOutPath"; then
                    if test "$base" = "file" || test "$base" = "a.nix"; then
                        echo "$path"
                    fi
                fi
            done
            echo
        elif test "$cmd" = "fileInfo"; then
            for path in $args; do
                if test "$path" = "$barOutPath/default.nix"; then
                    echo "$path"
                    echo "regular" # File type
                    echo `expr length "$substitutedExprString"` # File length
                    echo `basename "$substitutedExprPath" | sed 's|-.*||'` # File hash, base-32
                elif test "$path" = "$barOutPath"; then
                    echo "$path"
                    echo "directory" # File type
                    echo "3" # File count
                    echo "file" # Entry
                    echo "a.nix" # Entry
                    echo "default.nix" # Entry
                elif test "$path" = "$bazOutPath"; then
                    echo "$path"
                    echo "directory"
                    echo "2"
                    echo "file"
                    echo "a.nix"
                elif test "$path" = "$barOutPath/file"; then
                    echo "$path"
                    echo "symlink" # File type
                    echo "$bazOutPath/file" # Target
                elif test "$path" = "$barOutPath/a.nix"; then
                    echo "$path"
                    echo "symlink" # File type
                    echo "$bazOutPath/a.nix" # Target
                elif test "$path" = "$bazOutPath/file"; then
                    echo "$path"
                    echo "regular" # File type
                    echo "" # Not executable ("executable" otherwise)
                    echo `expr length "$fileString"`
                    echo `basename "$filePath" | sed 's|-.*||'`
                elif test "$path" = "$bazOutPath/a.nix"; then
                    echo "$path"
                    echo "regular"
                    echo ""
                    echo `expr length "$aDotNixString"`
                    echo `basename "$aDotNixPath" | sed 's|-.*||'`
                fi
            done
            echo
        else
            echo "bad command $cmd"
            exit 1
        fi
    done
elif test $1 = "--read"; then
    if test "$2" = "$barOutPath/default.nix"; then
        echo "$substitutedExprString"
    elif test "$2" = "$bazOutPath/file"; then
        echo "$fileString"
    elif test "$2" = "$bazOutPath/a.nix"; then
        echo "$aDotNixString"
    else
        exit 1
elif test $1 = "--substitute"; then
    exit 1
else
    echo "unknown substituter operation"
    exit 1
fi
