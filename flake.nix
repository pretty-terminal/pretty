{
  description = "Pretty's development flake - portable, reliable, ergonimic - TTY";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

    git-hooks = {
      url = "github:cachix/git-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, git-hooks }: let
    supportedSystems = ["x86_64-linux"];
    forAllSystems = f: nixpkgs.lib.genAttrs
      supportedSystems (system: f nixpkgs.legacyPackages.${system});
  in {
    formatter = forAllSystems (pkgs: pkgs.alejandra);

    checks = forAllSystems (
      pkgs: {
        pre-commit-check = git-hooks.lib.${pkgs.system}.run {
          src = ./.;
          hooks = {
            trim-trailing-whitespace.enable = true;
            clang-format = {
              enable = true;
              name = "format the code";
              entry = pkgs.lib.getExe self.packages.${pkgs.system}.cpp-fmt;
            };
          };
        };
      }
    );

    packages = forAllSystems (pkgs: {
      default = self.packages.${pkgs.system}.pretty;

      pretty = pkgs.callPackage ./pretty.nix { };

      cpp-fmt = pkgs.writeShellScriptBin "cpp-fmt" ''
        find . -type f -name "*.c" -or -name "*.h" \
          | xargs ${pkgs.lib.getExe' pkgs.clang-tools "clang-format"} -i --verbose
      '';
    });

    devShells = forAllSystems (pkgs: {
      default = pkgs.mkShell {
        inherit (self.checks.${pkgs.system}.pre-commit-check) shellHook;

        env.CC = pkgs.stdenv.cc;
        inputsFrom = [ self.packages.${pkgs.system}.pretty ];

        hardeningDisable = [ "format" ];
        packages = with pkgs; [
          compiledb
        ] ++ [
          self.packages.${pkgs.system}.cpp-fmt
        ];
      };
    });
  };
}
