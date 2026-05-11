{ pkgs ? import <nixpkgs> {} }:
{
  buildInputs = with pkgs; [
    sv-lang
    microsoft-gsl
  ] ++ sv-lang.buildInputs;

  nativeBuildInputs = with pkgs; [
    gtest
    cmake
    ninja
    git
    pkg-config
    microsoft-gsl
  ];

  shellOnlyPackages = with pkgs; [
    llvmPackages_22.clang-tools
  ];
}
