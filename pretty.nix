{
  lib,
  stdenv,
  pkg-config,
  sdl3,
  sdl3-ttf,
}:
stdenv.mkDerivation {
  name = "pretty";
  src = ./.;

  hardeningDisable = ["format"];

  nativeBuildInputs = [pkg-config];

  buildInputs = [
    sdl3
    sdl3-ttf
  ];

  env.PREFIX = "${placeholder "out"}";

  meta = {
    description = "Portable, reliable, ergonimic - TTY Resources";
    maintainers = with lib.maintainers; [sigmanificient gurjaka];
    license = lib.licenses.mit;
    mainProgram = "pretty";
  };
}
