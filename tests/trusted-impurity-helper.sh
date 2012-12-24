source common.sh

nix-build trusted-impurity-helper.nix --option impure-commands-dir $PWD --no-out-link
