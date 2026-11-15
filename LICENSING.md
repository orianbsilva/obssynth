# Licensing

The Obs instruments (ObsSynth4D, ObsSynth One, ObsSynthCT1, ObsDrums: the engine, the panels, the plugins and the
tools) are free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). Copyright (C) 2025-2026 Orian Beltrame.

If you distribute these programs or plugins, or work derived from them, you must give your recipients the complete
corresponding source under the same licence.

## Third-party material

In this tree:

| What | Licence | Where |
| --- | --- | --- |
| Steinberg ASIO: the driver interface (`IASIO` and its types) declared in the Windows ASIO backend, after the ASIO SDK 2.3.4 | GPL-3.0 (the SDK's GPL option; Steinberg ASIO SDK, (c) Steinberg Media Technologies GmbH) | `src/lib/audio/asio_backend.cpp` |
| nanovg, Mikko Mononen | zlib | `sdk/nanovg/` |
| glad, the OpenGL loader it generated | public domain / zlib | `sdk/glad/` |
| JSON for Modern C++, Niels Lohmann | MIT | `src/vendor/json.hpp` |
| Hershey Simplex Roman, Allen V. Hershey | public domain | `src/app/synth_view/hershey_font.hpp` |
| Two recordings from VCSL (Versilian Community Sample Library) | CC0 1.0 | `pcm/vcsl-*.wav`; see `pcm/SAMPLES.md` |

Fetched by CMake at build time (not in this tree): CLAP and clap-wrapper (MIT), the VST3 SDK through clap-wrapper
(MIT), SDL3, SDL3_image and SDL3_ttf (zlib), Dear ImGui and imgui-knobs (MIT), doctest (MIT).

The wavetables in `pcm/` (`bank_*`, `cycle_*`, `*_wave_*`) are the project's own, under the GPL like the code.

## Trademarks

VST is a registered trademark of Steinberg Media Technologies GmbH. ASIO is a trademark of Steinberg Media
Technologies GmbH. CLAP is the CLever Audio Plug-in API of the free-audio project. The MIDI controllers the
programs support (Akai APC Key 25, Novation Launchkey MK4 and Launchpad X) are named only to say what works with
them; their names are their owners' trademarks, and the Obs instruments are not affiliated with or endorsed by them.
