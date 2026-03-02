{
  inputs,
  self,
  lib,
  ...
}:
let
  mkDate =
    longDate:
    (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);

  version = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in
{
  default = self.overlays.hyprlang;

  hyprlang-with-deps = lib.composeManyExtensions [
    inputs.hyprutils.overlays.default
    self.overlays.hyprlang
  ];

  hyprlang = final: prev: {
    hyprlang = final.callPackage ./default.nix {
      stdenv = final.gcc15Stdenv;
      version =
        version
        + "+date="
        + (mkDate (self.lastModifiedDate or "19700101"))
        + "_"
        + (self.shortRev or "dirty");
    };
    hyprlang-with-tests = final.hyprlang.override { doCheck = true; };
  };
}
