{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    nixpkgs-patcher.url = "github:gepbird/nixpkgs-patcher";
    flake-parts.url = "github:hercules-ci/flake-parts";
    systems.url = "github:nix-systems/default";
    flake-compat = {
      url = "github:NixOS/flake-compat";
      flake = false;
    };
    scr1 = {
      url = "github:syntacore/scr1";
      flake = false;
    };
    nixpkgs-patch-clang-tools-bash = {
      url = "https://github.com/NixOS/nixpkgs/pull/563394.diff";
      flake = false;
    };
  };

  outputs =
    inputs@{
      self,
      nixpkgs,
      nixpkgs-patcher,
      flake-parts,
      systems,
      scr1,
      ...
    }:
    flake-parts.lib.mkFlake { inputs = inputs // { }; } {
      systems = import systems;
      perSystem =
        {
          self',
          pkgs,
          system,
          ...
        }:
        let
          inherit (pkgs) lib;
        in
        {
          packages.default = self'.packages.verihogg-format;
          packages.verihogg-format = pkgs.stdenv.mkDerivation {
            pname = "verihogg-format";
            version = "0.1.0";
            src = ./.;

            meta = {
              description = "SystemVerilog formatter powered by Slang";
              license = lib.licenses.mit;
              platforms = lib.platforms.linux;
            };

            nativeBuildInputs = with pkgs; [
              gtest
              cmake
              ninja
              pkg-config
              microsoft-gsl
            ];

            buildInputs =
              with pkgs;
              [
                sv-lang
                microsoft-gsl
              ]
              ++ sv-lang.buildInputs;

            preConfigure = ''
              export SCR1_ROOT=${scr1}
            '';
          };

          devShells.default =
            let
              nixpkgs-patched = nixpkgs-patcher.lib.patchNixpkgs { inherit inputs system; };
              pkgs = import nixpkgs-patched { inherit system; };
            in
            pkgs.mkShell {
              buildInputs =
                self'.packages.default.buildInputs
                ++ self'.packages.default.nativeBuildInputs
                ++ [ pkgs.llvmPackages_22.clang-tools ];

              shellHook = ''
                export SCR1_ROOT=${scr1}
              '';
            };
        };
    };
}
