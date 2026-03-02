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

  outputs =
    {
      self,
      nixpkgs,
      systems,
      ...
    }@inputs:
    let
      inherit (nixpkgs) lib;
      eachSystem = lib.genAttrs (import systems);
      pkgsFor = eachSystem (
        system:
        import nixpkgs {
          localSystem.system = system;
          overlays = with self.overlays; [ hyprlang-with-deps ];
        }
      );
    in
    {
      overlays = import ./nix/overlays.nix { inherit lib self inputs; };

      packages = eachSystem (system: {
        default = self.packages.${system}.hyprlang;
        inherit (pkgsFor.${system}) hyprlang hyprlang-with-tests;
      });

      formatter = eachSystem (system: pkgsFor.${system}.nixfmt-tree);
    };
}
