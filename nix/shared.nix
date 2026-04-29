{ pkgs ? import <nixpkgs> {} }:
{
  buildInputs = with pkgs; [
    sv-lang
  ] ++ sv-lang.buildInputs;

  nativeBuildInputs = with pkgs; [
    cmake
    ninja
    pkg-config
    microsoft-gsl
  ];

  shellOnlyPackages = with pkgs; [
    llvmPackages_22.clang-tools
  ];
}
