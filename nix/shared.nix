{ pkgs ? import <nixpkgs> {} }:
{
  scr1 = builtins.fetchGit {
    url = "https://github.com/syntacore/scr1";
    ref = "master";
    rev = "ebb5e3551a9d93c0ee95f0b767dd878b8927e702";
  };

  buildInputs = with pkgs; [
    sv-lang
    microsoft-gsl
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
