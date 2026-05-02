{ pkgs ? import <nixpkgs> {} }:
{
  buildInputs = with pkgs; [
    sv-lang
  ] ++ sv-lang.buildInputs;

  nativeBuildInputs = with pkgs; [
    gtest
    cmake
    ninja
    pkg-config
    microsoft-gsl
  ];

  shellOnlyPackages = with pkgs; [
    llvmPackages_22.clang-tools
  ];
}
