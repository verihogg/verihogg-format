{ pkgs ? import <nixpkgs> {} }:
let
  shared = import ./nix/shared.nix {
    inherit pkgs;
  };
in
pkgs.stdenv.mkDerivation {
  pname = "verihogg-format";
  version = "0.1.0";

  src = ./.;

  nativeBuildInputs = shared.nativeBuildInputs;

  buildInputs = shared.buildInputs;

  meta = with pkgs.lib; {
    description = "SystemVerilog formatter powered by Slang";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
