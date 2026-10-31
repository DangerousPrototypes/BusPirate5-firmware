{
  description = "Bus Pirate 5 firmware";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    pico-sdk = {
      url = "git+https://github.com/raspberrypi/pico-sdk?ref=refs/tags/2.2.0&submodules=1";
      flake = false;
    };

    ansi-colours = {
      url = "github:mina86/ansi_colours/v1.2.2";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, pico-sdk, ansi-colours }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.gcc14Stdenv.mkDerivation {
            pname = "bus-pirate5-rev10-firmware";
            version = self.shortRev or self.dirtyShortRev or "unknown";

            src = pkgs.lib.cleanSource ./.;

            nativeBuildInputs = with pkgs; [
              cmake
              gcc-arm-embedded-14
              python3
              picotool
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build \
                -DPICO_SDK_PATH=${pico-sdk} \
                -DANSI_COLOURS_PATH=${ansi-colours} \
                -DCMAKE_BUILD_TYPE=Release \
                -DGIT_COMMIT_HASH=${self.shortRev or self.dirtyShortRev or "unknown"} \
                -Dpicotool_DIR=${pkgs.picotool}/lib/cmake/picotool
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build --parallel $NIX_BUILD_CORES --target bus_pirate5_rev10
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out
              cp build/src/bus_pirate5_rev10.uf2 $out/
              runHook postInstall
            '';

            dontStrip = true;
            dontFixup = true;
          };
        });

      # cmake -S . -B build_rp2040
      # cmake --build build_rp2040 --parallel --target bus_pirate5_rev10
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = (pkgs.mkShell.override { stdenv = pkgs.gcc14Stdenv; }) {
            packages = with pkgs; [
              gcc-arm-embedded-14
              cmake
              python3
              git
            ];

            shellHook = ''
              export PICO_SDK_PATH="${pico-sdk}"
            '';
          };
        });
    };
}
