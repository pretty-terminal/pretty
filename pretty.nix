{
  lib,
  stdenv,
}:
stdenv.mkDerivation {
  name = "pretty";
  src = ./.;

  hardeningDisable = [ "format" ];

  env.PREFIX = "${placeholder "out"}";

  lib = {
    description = "Portable, reliable, ergonimic - TTY Resources";
    maintainers = with lib.maintainers; [ sigmanificient gurjaka ];
    license = lib.licenses.mit;
    mainProgram = "pretty";
  };
}
