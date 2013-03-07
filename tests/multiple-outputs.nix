with import ./config.nix;

rec {

  a = mkDerivation {
    name = "multiple-outputs-a";
    outputs = [ "first" "second" ];
    builder = builtins.toFile "builder.sh"
      ''
        mkdir $first $second
        test -z $all
        echo "first" > $first/file
        echo "second" > $second/file
        ln -s $first $second/link
      '';
    helloString = "Hello, world!";
  };

  b = mkDerivation {
    defaultOutput = assert a.second.helloString == "Hello, world!"; a;
    firstOutput = assert a.outputName == "first"; a.first.first;
    secondOutput = assert a.second.outputName == "second"; a.second.first.first.second.second.first.second;
    allOutputs = a.all;
    name = "multiple-outputs-b";
    builder = builtins.toFile "builder.sh"
      ''
        mkdir $out
        test "$firstOutput $secondOutput" = "$allOutputs"
        test "$defaultOutput" = "$firstOutput"
        test "$(cat $firstOutput/file)" = "first"
        test "$(cat $secondOutput/file)" = "second"
        echo "success" > $out/file
      '';
  };

  c = mkDerivation {
    name = "multiple-outputs-c";
    builder = builtins.toFile "builder.sh"
      ''
        mkdir $out
      '';
  };

  d = mkDerivation {
    name = "multiple-outputs-d";
    builder = builtins.toFile "builder.sh"
      ''
        mkdir $out
      '';
  };

  cyclic = (mkDerivation {
    name = "cyclic-outputs";
    outputs = [ "a" "b" "c" ];
    builder = builtins.toFile "builder.sh"
      ''
        mkdir $a $b $c
        echo $a > $b/foo
        echo $b > $c/bar
        echo $c > $a/baz
      '';
  }).a;

}
