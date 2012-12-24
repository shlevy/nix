#!/bin/sh

read -N 1 c
echo -n $c >&2
touch $2/temp-file
touch $3$(nix-store -q --outputs $1)
echo -n $(basename $0)$4$5
