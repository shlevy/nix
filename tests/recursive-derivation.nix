with import ./config.nix;

let
  drvFun = input: mkDerivation {
    name = "uses-input";
    args = [ "-c" "ln -sv ${input} $out" ];
  };
in rec {
  recursive = mkDerivation {
    name = "simple";
    builder = ./recursive-derivation.builder.sh;
    PATH = "${path}:${nixBinDir}";
    simple = builtins.toString ./simple.nix;
    __recursive = true;
  };

  usesRecursive = drvFun recursive;

  usesDirect = drvFun (import ./simple.nix);

  usesUsesRecursive = drvFun usesRecursive;

  usesUsesDirect = drvFun usesDirect;
}
