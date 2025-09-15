{
  lib,
  stdenv,
  pkg-config,
  sdl3,
  sdl3-ttf,
  fontconfig,
}:
stdenv.mkDerivation {
  name = "pretty";
  src = ./.;

  hardeningDisable = ["format"];

  nativeBuildInputs = [pkg-config];

  buildInputs = [
    sdl3
    sdl3-ttf
    fontconfig
  ];

  env = {
    PREFIX = "${placeholder "out"}";
    RELEASE = true;
  };

  meta = {
    description = "Portable, reliable, ergonimic - TTY Resources";
    maintainers = with lib.maintainers; [sigmanificient gurjaka];
    license = lib.licenses.mit;
    mainProgram = "pretty";
  };
}
