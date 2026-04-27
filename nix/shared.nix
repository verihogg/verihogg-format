{ pkgs ? import <nixpkgs> {} }:
{
  buildInputs = with pkgs; [
    sv-lang
    microsoft-gsl
  ] ++ sv-lang.buildInputs;

  nativeBuildInputs = with pkgs; [
    cmake
    ninja
    pkg-config
  ];

  shellOnlyPackages = with pkgs; [
    llvmPackages_22.clang-tools
  ];
}
