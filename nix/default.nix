{
  lib,
  stdenv,
  cmake,
  version ? "git",
  doCheck ? false,
}:
stdenv.mkDerivation {
  pname = "hyprlang";
  inherit version doCheck;
  src = ../.;

  nativeBuildInputs = [cmake];

  outputs = ["out" "dev"];

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprlang";
    description = "The official implementation library for the hypr config language";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
