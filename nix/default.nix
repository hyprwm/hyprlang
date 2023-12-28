{
  lib,
  stdenv,
  cmake,
  version ? "git",
}:
stdenv.mkDerivation {
  pname = "hyprlang";
  inherit version;
  src = ../.;

  nativeBuildInputs = [cmake];

  doCheck = true;

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprlang";
    description = "The official implementation library for the hypr config language";
    license = licenses.gpl3Plus;
    platforms = platforms.linux;
  };
}
