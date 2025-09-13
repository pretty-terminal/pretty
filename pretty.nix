{
  lib,
  stdenv,
  pkg-config,
  sdl3,
}:
stdenv.mkDerivation {
  name = "pretty";
  src = ./.;

  hardeningDisable = [ "format" ];

  nativeBuildInputs = [ pkg-config ];

  buildInputs = [ sdl3 ];

  env.PREFIX = "${placeholder "out"}";

  meta = {
    description = "Portable, reliable, ergonimic - TTY Resources";
    maintainers = with lib.maintainers; [ sigmanificient gurjaka ];
    license = lib.licenses.mit;
    mainProgram = "pretty";
  };
}
