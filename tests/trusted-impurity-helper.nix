with import ./config.nix;

derivation {
  name = "trusted-impurity-helper";
  inherit system;
  builder = ./trusted-impurity-helper-builder;
  __useImpurityHelper = true;
}
