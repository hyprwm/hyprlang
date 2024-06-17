{
  lib,
  stdenv,
  cmake,
  hyprutils,
  pkg-config,
  version ? "git",
  doCheck ? false,
}:
stdenv.mkDerivation {
  pname = "hyprlang";
  inherit version doCheck;
  src = ../.;

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [hyprutils];

  outputs = ["out" "dev"];

  cmakeBuildType = "RelWithDebInfo";

  dontStrip = true;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprlang";
    description = "The official implementation library for the hypr config language";
    license = licenses.lgpl3Only;
    platforms = platforms.linux;
  };
}
