{ pkgs ? import <nixpkgs> {} }:
{
  buildInputs = with pkgs; [
    sv-lang
  ] ++ sv-lang.buildInputs;

  nativeBuildInputs = with pkgs; [
    cmake
    ninja
    pkg-config
  ];

  shellOnlyPackages = with pkgs; [
    llvmPackages_21.clang-tools
  ];
}
