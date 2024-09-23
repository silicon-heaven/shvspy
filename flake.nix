{
  description = "Silicon Heaven Spy";

  outputs = {
    self,
    flake-utils,
    nixpkgs,
  }: let
    inherit (flake-utils.lib) eachDefaultSystem;
    inherit (nixpkgs.lib) hasSuffix composeManyExtensions;
    rev = self.shortRev or self.dirtyShortRev or "unknown";

    shvspy = {
      stdenv,
      cmake,
      qt6,
      doctest,
      openldap,
      copyDesktopItems,
      makeDesktopItem,
    }:
      stdenv.mkDerivation {
        name = "shvspy-${rev}";
        src = builtins.path {
          path = ./.;
          filter = path: _: ! hasSuffix ".nix" path;
        };
        buildInputs = [
          qt6.wrapQtAppsHook
          qt6.qtbase
          qt6.qtserialport
          qt6.qtwebsockets
          qt6.qtsvg
          qt6.qtnetworkauth
          doctest
          openldap
        ];
        nativeBuildInputs = [
          cmake
          qt6.qttools
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
        cmakeFlags = [
          "-DUSE_QT6=ON"
        ];
        meta.mainProgram = "shvspy";
      };
  in
    {
      overlays = {
        pkgs = final: _: {
          shvspy = final.callPackage shvspy {};
        };
        default = composeManyExtensions [
          self.overlays.pkgs
        ];
      };
    }
    // eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system}.extend self.overlays.default;
    in {
      packages.default = pkgs.shvspy;
      legacyPackages = pkgs;

      formatter = pkgs.alejandra;
    });
}
