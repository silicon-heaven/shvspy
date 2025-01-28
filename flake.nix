{
  description = "Silicon Heaven Spy";

  outputs = {
    self,
    flake-utils,
    nixpkgs,
  }: let
    inherit (flake-utils.lib) eachDefaultSystem;
    inherit (nixpkgs.lib) assertMsg;
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
      assert assertMsg self.submodules
      "Unable to build without submodules. Append '?submodules=1#' to the URL.";
        stdenv.mkDerivation {
          name = "shvspy-${rev}";
          src = ./.;
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
          cmakeFlags = ["-DUSE_QT6=ON"];
          meta.mainProgram = "shvspy";
        };
  in
    {
      overlays.default = final: _: {
        shvspy = final.callPackage shvspy {};
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
