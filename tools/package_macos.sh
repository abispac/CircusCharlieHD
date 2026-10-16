#!/usr/bin/env bash
# Build the macOS release on a Mac and pack it into
# dist/CircusCharlieHD-macOS-<arch>.zip: a self-contained "Circus Charlie HD.app"
# that carries the SDL2 libraries (so Homebrew is not needed to play) plus a
# README. Run it from the repository root on the Mac:
#
#   brew install cmake ninja sdl2 sdl2_image dylibbundler
#   tools/package_macos.sh
#
# Without SIGN_IDENTITY the app is ad-hoc signed: it runs, but the first
# launch on another Mac needs a right-click > Open. With a Developer ID
# certificate installed, sign and notarize it so it opens with no warning:
#
#   security find-identity -v -p codesigning            (shows the identity name)
#   xcrun notarytool store-credentials cchd-notary --apple-id YOU@EXAMPLE.COM \
#         --team-id TEAMID --password APP-SPECIFIC-PASSWORD   (once)
#   SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
#   NOTARY_PROFILE=cchd-notary tools/package_macos.sh
set -euo pipefail

cd "$(dirname "$0")/.."

for tool in cmake ninja dylibbundler; do
  command -v "$tool" >/dev/null || { echo "$tool not found: brew install $tool" >&2; exit 1; }
done

ARCH="$(uname -m)"
PKG="dist/CircusCharlieHD-macOS-$ARCH"
APP="$PKG/Circus Charlie HD.app"
ZIP="dist/CircusCharlieHD-macOS-$ARCH.zip"
IDENTITY="${SIGN_IDENTITY:--}"
PROFILE="${NOTARY_PROFILE:-}"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

rm -rf "$PKG"
mkdir -p "$PKG"
cp -R "build/Circus Charlie HD.app" "$APP"
dylibbundler -od -b \
  -x "$APP/Contents/MacOS/circus_charlie_hd" \
  -d "$APP/Contents/Frameworks" \
  -p @executable_path/../Frameworks >/dev/null

if [ "$IDENTITY" = "-" ]; then
  codesign --force --deep --sign - "$APP"
  echo "Ad-hoc signed (set SIGN_IDENTITY to use a Developer ID certificate)"
else
  SIGN=(codesign --force --timestamp --options runtime --sign "$IDENTITY")
  for lib in "$APP"/Contents/Frameworks/*.dylib; do
    "${SIGN[@]}" "$lib"
  done
  "${SIGN[@]}" "$APP/Contents/MacOS/circus_charlie_hd"
  "${SIGN[@]}" "$APP"
  codesign --verify --deep --strict "$APP"
  echo "Signed with $IDENTITY"
fi

cp dist-extras/macos/README.txt "$PKG/"

if [ -n "$PROFILE" ] && [ "$IDENTITY" != "-" ]; then
  rm -f dist/notarize.zip
  ditto -c -k --keepParent "$APP" dist/notarize.zip
  xcrun notarytool submit dist/notarize.zip --keychain-profile "$PROFILE" --wait
  xcrun stapler staple "$APP"
  rm -f dist/notarize.zip
  spctl --assess --type execute -v "$APP"
  echo "Notarized and stapled"
fi

rm -f "$ZIP"
(cd dist && ditto -c -k --keepParent "CircusCharlieHD-macOS-$ARCH" "CircusCharlieHD-macOS-$ARCH.zip")
echo "Wrote $ZIP"
