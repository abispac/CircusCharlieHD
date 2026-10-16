#!/usr/bin/env bash
# Cross-compile the Windows 64-bit release from Linux or macOS with MinGW-w64
# and pack it, with the assets and the launcher .bat files, into
# dist/CircusCharlieHD-windows-x64.zip.
#
#   Debian/Ubuntu: sudo apt install mingw-w64 zip
#   macOS:         brew install mingw-w64 zip
#
# The SDL2 and SDL2_image MinGW development packages are downloaded from the
# official GitHub releases into build-win/sdk the first time.
set -euo pipefail

cd "$(dirname "$0")/.."

SDL_VERSION=2.30.11
IMG_VERSION=2.8.8
CXX=x86_64-w64-mingw32-g++
SDK=build-win/sdk
OUT=dist/CircusCharlieHD-windows-x64

command -v "$CXX" >/dev/null || { echo "$CXX not found: install mingw-w64" >&2; exit 1; }

mkdir -p "$SDK"
if [ ! -d "$SDK/SDL2-$SDL_VERSION" ]; then
  curl -sSL -o "$SDK/sdl2.tar.gz" \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-devel-$SDL_VERSION-mingw.tar.gz"
  tar xzf "$SDK/sdl2.tar.gz" -C "$SDK"
fi
if [ ! -d "$SDK/SDL2_image-$IMG_VERSION" ]; then
  curl -sSL -o "$SDK/sdl2img.tar.gz" \
    "https://github.com/libsdl-org/SDL_image/releases/download/release-$IMG_VERSION/SDL2_image-devel-$IMG_VERSION-mingw.tar.gz"
  tar xzf "$SDK/sdl2img.tar.gz" -C "$SDK"
fi

S="$SDK/SDL2-$SDL_VERSION/x86_64-w64-mingw32"
I="$SDK/SDL2_image-$IMG_VERSION/x86_64-w64-mingw32"
FLAGS=(-std=c++20 -O2 -DNDEBUG -Isrc "-I$S/include/SDL2" "-I$I/include/SDL2")
LIBS=("-L$S/lib" "-L$I/lib" -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -static-libgcc -static-libstdc++)

rm -rf "$OUT"
mkdir -p "$OUT"
echo "Compiling CircusCharlieHD.exe"
"$CXX" "${FLAGS[@]}" src/main.cpp -o "$OUT/CircusCharlieHD.exe" "${LIBS[@]}" -mwindows
echo "Compiling CircusCharlieHD-console.exe"
"$CXX" "${FLAGS[@]}" src/main.cpp -o "$OUT/CircusCharlieHD-console.exe" "${LIBS[@]}" -mconsole
cp "$S/bin/SDL2.dll" "$I/bin/SDL2_image.dll" "$OUT/"
cp -R assets "$OUT/assets"
cp -R dist-extras/windows/. "$OUT/"

rm -f "$OUT.zip"
(cd dist && zip -qr "CircusCharlieHD-windows-x64.zip" "CircusCharlieHD-windows-x64")
echo "Wrote $OUT.zip"
