{
  description = "The official implementation library for the hypr config language";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";
  };

  outputs = {
    self,
    nixpkgs,
    systems,
  }: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
      });
    mkDate = longDate: (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  in {
    overlays = {
      default = self.overlays.hyprlang;
      hyprlang = final: prev: {
        hyprlang = prev.callPackage ./nix/default.nix {
          stdenv = prev.gcc13Stdenv;
          version = "0.pre" + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
        };
        hyprlang-with-tests = final.hyprlang.override {doCheck = true;};
      };
    };

    packages = eachSystem (system:
      (self.overlays.default null pkgsFor.${system})
      // {default = self.packages.${system}.hyprlang;});

    formatter = eachSystem (system: pkgsFor.${system}.alejandra);
  };
}
