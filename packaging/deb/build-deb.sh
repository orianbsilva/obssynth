#!/usr/bin/env bash
# The obssynth .deb: the four panels (obssynth4d, obssynthone, obssynthct1, obsdrums) and their CLAP plugins
# (ObsSynth4D, ObsSynthOne, ObsSynthCT1, ObsDrums), for the machine it is built on (dpkg --print-architecture).
# SDL3, SDL3_image and SDL3_ttf are the system's (Debian 13: libsdl3-0, libsdl3-image0, libsdl3-ttf0); the
# package depends on them, as on everything else the binaries link (dpkg-shlibdeps).
#
# The layout keeps what the programs already look for:
#   /usr/lib/clap/ObsSynth/   the .clap files, each one's panel beside it (its editor) and pcm/ (the banks, the
#                             recordings); CLAP hosts search /usr/lib/clap and its folders
#   /usr/bin/<panel>          links to the panels there (a panel finds pcm/ beside its real path)
#   /usr/share/applications/  a launcher per panel
#
# Usage: packaging/deb/build-deb.sh [OUTPUT_DIR]   (default: the repository's dist/)
# Needs: gcc/g++, cmake, dpkg-dev (dpkg-deb, dpkg-shlibdeps), fakeroot, libsdl3-dev, libsdl3-image-dev,
# libsdl3-ttf-dev, libasound2-dev, libpulse-dev. Built with gcc: libstdc++, as on every Debian.
set -euo pipefail

src=$(cd "$(dirname "$0")/../.." && pwd)
out=${1:-$src/dist}
build=$src/build-deb
arch=$(dpkg --print-architecture)
panels=(obssynth4d obssynthone obssynthct1 obsdrums)
plugins=(ObsSynth4D ObsSynthOne ObsSynthCT1 ObsDrums)

# the version: the last tag and the commits after it (v0.1.0-beta.1-26-gabc1234 → 0.1.0~beta.1+26.gabc1234)
desc=$(git -C "$src" describe --tags --always 2>/dev/null || echo 0.0.0)
version=$(echo "${desc#v}" | sed -E 's/^([0-9.]+)-(alpha|beta|rc)/\1~\2/; s/-([0-9]+)-g([0-9a-f]+)$/+\1.g\2/')
[[ $version =~ ^[0-9] ]] || version="0.0.0+g$version"

echo "obssynth $version ($arch)"
CC=gcc CXX=g++ cmake -S "$src" -B "$build" -DCMAKE_BUILD_TYPE=Release -DOBS_FETCH_SDL=OFF -DOBS_VST3=OFF >/dev/null
targets=("${panels[@]}")
for p in "${panels[@]}"; do targets+=("${p}_clap"); done
cmake --build "$build" -j"$(nproc)" --target "${targets[@]}"

root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
pkg=$root/obssynth
lib=$pkg/usr/lib/clap/ObsSynth
mkdir -p "$lib" "$pkg/usr/bin" "$pkg/usr/share/applications" "$pkg/usr/share/doc/obssynth" "$pkg/DEBIAN"
for p in "${plugins[@]}"; do install -m 644 "$build/clap/$p.clap" "$lib/"; done
for p in "${panels[@]}"; do
    install -m 755 "$build/src/app/$p" "$lib/"
    ln -s "../lib/clap/ObsSynth/$p" "$pkg/usr/bin/$p"
done
install -d "$lib/pcm"
install -m 644 "$src"/pcm/*.wav "$lib/pcm/"
[ -f "$src/pcm/SAMPLES.md" ] && install -m 644 "$src/pcm/SAMPLES.md" "$pkg/usr/share/doc/obssynth/"
strip --strip-unneeded "$lib"/*.clap "${panels[@]/#/$lib/}"

launcher() { # id, name, what it is
    cat > "$pkg/usr/share/applications/$1.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$2
Comment=$3
Exec=$1
Icon=audio-x-generic
Terminal=false
Categories=AudioVideo;Audio;Midi;
EOF
}
launcher obssynth4d "ObsSynth4D" "Four oscillators (STD, WT, FM, granular), a mod matrix, vector synthesis"
launcher obssynthone "ObsSynth One" "One oscillator, many engines"
launcher obssynthct1 "ObsSynthCT1" "Five operators, cross FM"
launcher obsdrums "ObsDrums" "Sixteen drum pads"

cat > "$pkg/usr/share/doc/obssynth/copyright" <<EOF
ObsSynth: https://github.com/orianbsilva/obssynth
The recordings in pcm/ (VCSL) are CC0; see SAMPLES.md.
EOF

# what the binaries link, as the system's packages (SDL3 and the rest)
mkdir -p "$root/debian" && echo "Source: obssynth" > "$root/debian/control"
depends=$(cd "$root" && dpkg-shlibdeps -O "${panels[@]/#/$lib/}" "$lib"/*.clap 2>/dev/null | sed -n 's/^shlibs:Depends=//p')
size=$(du -sk "$pkg/usr" | cut -f1)
cat > "$pkg/DEBIAN/control" <<EOF
Package: obssynth
Version: $version
Architecture: $arch
Maintainer: Orian Beltrame <942965+orianbsilva@users.noreply.github.com>
Installed-Size: $size
Depends: $depends
Section: sound
Priority: optional
Homepage: https://github.com/orianbsilva/obssynth
Description: ObsSynth synthesizers: standalone panels and CLAP plugins
 ObsSynth4D, ObsSynth One, ObsSynthCT1 and ObsDrums, each as a standalone panel
 (its own audio and MIDI in) and as a CLAP plugin for a DAW, whose editor is
 the same panel. Plugins in /usr/lib/clap/ObsSynth.
EOF

mkdir -p "$out"
deb=$out/obssynth_${version}_${arch}.deb
fakeroot dpkg-deb --build -Zxz "$pkg" "$deb" >/dev/null
echo "$deb"
dpkg-deb -I "$deb" | sed -n '/Package:/,/Description:/p'
