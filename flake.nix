{
  description = "The official implementation library for the hypr config language";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";

    hyprutils = {
      url = "github:hyprwm/hyprutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };
  };

  outputs = {
    self,
    nixpkgs,
    systems,
    ...
  } @ inputs: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = with self.overlays; [hyprlang];
      });
    mkDate = longDate: (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  in {
    overlays = {
      default = self.overlays.hyprlang;
      hyprlang = lib.composeManyExtensions [
        inputs.hyprutils.overlays.default
        (final: prev: {
          hyprlang = final.callPackage ./nix/default.nix {
            stdenv = final.gcc13Stdenv;
            version = "0.pre" + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
          };
          hyprlang-with-tests = final.hyprlang.override {doCheck = true;};
        })
      ];
    };

    packages = eachSystem (system: {
      default = self.packages.${system}.hyprlang;
      inherit (pkgsFor.${system}) hyprlang hyprlang-with-tests;
    });

    formatter = eachSystem (system: pkgsFor.${system}.alejandra);
  };
}
