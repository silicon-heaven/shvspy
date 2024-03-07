{
  description = "Silicon Heaven Spy Flake";

  outputs = {
    self,
    flake-utils,
    nixpkgs,
  }:
    with builtins;
    with flake-utils.lib;
    with nixpkgs.lib; let
      packages = pkgs:
        with pkgs;
        with qt6Packages; rec {
          shvspy = stdenv.mkDerivation {
            name = "shvspy";
            src = builtins.path {
              path = ./.;
              filter = path: type: ! hasSuffix ".nix" path;
            };
            buildInputs = [
              wrapQtAppsHook
              qtbase
              qtserialport
              qtwebsockets
              qtsvg
              qtnetworkauth
              doctest
              openldap
            ];
            nativeBuildInputs = [
              qttools
              cmake
              copyDesktopItems
            ];
            desktopItems = [
              (makeDesktopItem {
                name = "shvspy";
                exec = "shvspy";
                desktopName = "SHVSpy";
                categories = ["Network" "RemoteAccess"];
              })
            ];
          };
        };
    in
      {
        overlays = {
          shvspy = final: prev: packages (id prev);
          default = self.overlays.shvspy;
        };
      }
      // eachDefaultSystem (system: let
        pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
      in {
        packages = filterPackages system rec {
          inherit (pkgs) shvspy;
          default = shvspy;
        };
        legacyPackages = pkgs;

        checks.default = self.packages.${system}.default;

        formatter = pkgs.alejandra;
      });
}
