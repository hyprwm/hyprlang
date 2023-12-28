{
  description = "The official implementation library for the hypr config language";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = {
    self,
    nixpkgs,
    ...
  }: let
    inherit (nixpkgs) lib;
    genSystems = lib.genAttrs [
      # Add more systems if they are supported
      "x86_64-linux"
      "aarch64-linux"
    ];
    pkgsFor = nixpkgs.legacyPackages;
    mkDate = longDate: (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  in {
    overlays.default = _: prev: {
      hyprlang = prev.callPackage ./nix/default.nix {
        stdenv = prev.gcc13Stdenv;
        version = "0.pre" + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
      };
    };

    packages = genSystems (system:
      (self.overlays.default null pkgsFor.${system})
      // {default = self.packages.${system}.hyprlang;});

    formatter = genSystems (system: pkgsFor.${system}.alejandra);
  };
}
