{
  inputs.rtos-nix.url = "github:ZainKergayeProjects/rtos.nix";
  inputs.can2040.url = "github:uofu-embed/can2040";
  inputs.can2040.flake = false;

  outputs =
    {
      self,
      rtos-nix,
      can2040,
    }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];
      forAllSystems = rtos-nix.nixpkgs.lib.genAttrs supportedSystems;
      pkgs = forAllSystems (system: rtos-nix.nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (system: {
        default = pkgs.${system}.stdenv.mkDerivation {
          name = "lab00";
          src = ./.;
          buildInputs = with pkgs.${system}; [
            cmake
            git
            gcc-arm-embedded
            python3
            rtos-nix.packages.${system}.pico-sdk-overriden
            picotool
            unity-test
            pioasm
          ];
          phases = [ "installPhase" ];
          installPhase = ''
            export PICO_SDK_PATH=${rtos-nix.packages.${system}.pico-sdk-overriden}/lib/pico-sdk
            export FREERTOS_PATH=${rtos-nix.freertos}
            export OPENOCD_PATH=${pkgs.${system}.openocd-rp2040}
            export UNITY_PATH=${rtos-nix.unity}
            export CAN_PATH=${can2040}
            mkdir -p $out
            cmake -B $out -S $src/ -DCMAKE_BUILD_TYPE=Debug
            cd $out
            cmake --build . --target all -j6
          '';
        };
      });

      devShells = forAllSystems (system: {
        default = rtos-nix.devShells.${system}.default;
      });
    };
}
