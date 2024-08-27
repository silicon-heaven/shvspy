{
  description = "Silicon Heaven Spy";

  inputs.libshv.url = "github:silicon-heaven/libshv";

  outputs = {
    self,
    flake-utils,
    nixpkgs,
    libshv,
  }: let
    inherit (flake-utils.lib) eachDefaultSystem;
    inherit (nixpkgs.lib) hasSuffix composeManyExtensions;
    rev = self.shortRev or self.dirtyShortRev or "unknown";

    shvspy = {
      stdenv,
      cmake,
      qt6,
      libshvForClients,
      necrolog,
      doctest,
      openldap,
      copyDesktopItems,
      makeDesktopItem,
    }:
      stdenv.mkDerivation {
        name = "shvspy-${rev}";
        src = builtins.path {
          path = ./.;
          filter = path: type: ! hasSuffix ".nix" path;
        };
        buildInputs = [
          qt6.wrapQtAppsHook
          qt6.qtbase
          qt6.qtserialport
          qt6.qtwebsockets
          qt6.qtsvg
          qt6.qtnetworkauth
          necrolog
          libshvForClients
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
          "-DSHVSPY_USE_LOCAL_NECROLOG=ON"
          "-DSHVSPY_USE_LOCAL_LIBSHV=ON"
          "-DUSE_QT6=ON"
        ];
        meta.mainProgram = "shvspy";
      };
  in
    {
      overlays = {
        pkgs = final: prev: {
          shvspy = final.callPackage shvspy {};
        };
        default = composeManyExtensions [
          libshv.overlays.default
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
