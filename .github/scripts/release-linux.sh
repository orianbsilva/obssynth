#!/usr/bin/env bash
# The Linux release packages, built inside ubuntu:24.04 (glibc 2.39: Debian 13, Ubuntu 24.04 and on) by release.yml:
#   release-linux.sh VERSION SHA
# Writes ObsPlugins-linux-x64.tar.gz (every plugin as VST3 and CLAP, each with its panel and pcm/) and
# synth-linux-x64.tar.gz (the standalone panels, the CLI, the examples) in the current folder (the checkout).
set -euo pipefail
version="$1" sha="$2" platform=linux-x64 build=build-release-linux

cmake -S . -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DOBS_VST3=ON
cmake --build "$build" --parallel "$(nproc)"
(cd "$build" && ctest --output-on-failure)

# ── the plugins: clap-wrapper's Linux layout is $build/plugins/<Name>.vst3/ and, beside them, <Name>.clap with pcm/
# and the panels
out="ObsPlugins-$platform"
rm -rf "$out" && mkdir -p "$out/VST3" "$out/CLAP"
cp -r "$build"/plugins/*.vst3 "$out/VST3/"
cp "$build"/plugins/*.clap "$out/CLAP/"
cp -r "$build/plugins/pcm" "$out/CLAP/"
for p in "$build"/plugins/obs*; do [ -f "$p" ] && [ -x "$p" ] && cp "$p" "$out/CLAP/"; done
cp docs/release/INSTALL-plugins.txt "$out/INSTALL.txt"
echo "$version  $sha" > "$out/VERSION.txt"
tar -czf "$out.tar.gz" "$out"
ls -R "$out" | grep -v '\.wav$' | head -60

# ── the standalone programs
out="synth-$platform"
rm -rf "$out" && mkdir -p "$out"
cp -r "$build/bin/." "$out/"
for p in "$build"/src/app/obs*; do [ -f "$p" ] && [ -x "$p" ] && cp "$p" "$out/"; done
cp -r pcm "$out/"
cp README.md "$out/"
cp -r docs "$out/"
echo "$version  $sha" > "$out/VERSION.txt"
tar -czf "$out.tar.gz" "$out"

# what they need of glibc (2.39 at most: Ubuntu 24.04)
for f in ObsPlugins-$platform/CLAP/*.clap; do echo "$f: $(objdump -T "$f" | grep -o 'GLIBC_[0-9.]*' | sort -V | tail -1)"; done
