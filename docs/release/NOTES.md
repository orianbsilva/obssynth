# Obs instruments @VERSION@

Four instruments from the Carbonian synth, as **VST3 and CLAP plugins** for Windows and Linux, and as standalone
programs.

| | |
|---|---|
| **ObsSynthCT1** | *Cross Table One*: five operators, each a standard wave or a wavetable, cross-modulating through a 5 × 5 FM matrix; filter, drive, delay, reverb; 14 factory presets |
| **ObsSynth4D** | four oscillators (standard, wavetable, FM, granular), a mod matrix, tube, delay, vector and XY; 34 factory presets |
| **ObsDrums** | sixteen pads, synthesized and sampled, factory kits, a master bus |
| **ObsSynthOne** | one oscillator with four engines (standard, wavetable, FM, granular) and a sample player, a filter, envelopes, a small mod matrix, delay and tube |

## Downloads

| File | What |
|---|---|
| `ObsInstruments-@VERSION@-windows-x64-setup.exe` | **Windows installer** (recommended): the VST3s into `Common Files\VST3`, the CLAPs into `Common Files\CLAP`, the standalone programs in the Start menu; each part optional, with an uninstaller |
| `ObsPlugins-windows-x64.zip` | the four as **VST3** and **CLAP**, Windows 10+ x64. The editor opens inside the host's window (tested in Ableton Live 12) |
| `ObsPlugins-linux-x64.tar.gz` | the four as **CLAP** (editor in its own Wayland window) and **VST3** (sound and parameters; no editor yet) |
| `synth-windows-x64.zip`, `synth-linux-x64.tar.gz` | the standalone panels (`obssynthct1`, `obssynth4d`, `obsdrums`, `obssynthone`), the CLI and the examples |
| `obssynth_*_amd64.deb` | Debian 13 package: the panels and their CLAPs, installed system-wide |
| `SHA256SUMS.txt` | the files' checksums |

Installing: on Windows run the installer, then rescan the plugins in the DAW (in Ableton Live: *Preferences →
Plug-ins → Use VST3 Plug-in System Folders*, then *Rescan*). On Debian 13 (and Carbonian): `sudo apt install ./obssynth_*.deb`.
By hand: `INSTALL.txt` in each plugin package.

## Notes

- Each plugin carries its editor (the product's panel, a program of its own: a crash there does not take the host
  down) and its wavetables. No Visual C++ redistributable is needed on Windows.
- **PROGRAM** chooses a factory preset or kit. The plugin's state (the project) keeps every value.
- The editor's **SETUP → LOOK** chooses its colours: the system's (on Carbonian) or one of eight palettes.
- If an editor does not show: `%TEMP%\ObsSynthCT1.log` (or `ObsSynth4D.log`, `ObsDrums.log`) says which step failed.
- **Windows** needs a 64-bit CPU with **AVX2** (Intel since 2013, AMD since 2015): the build is made for it.
  ObsSynthCT1's, ObsSynth4D's and ObsDrums' editors open inside the host's window; ObsSynthOne's opens in a window of
  its own for now.
- **Windows**: nothing is signed yet. SmartScreen may warn about the installer: *More info → Run anyway*. For the
  zip: *Properties → Unblock* before extracting.
- **Recordings**: `pcm/` ships two (VCSL, CC0; `pcm/SAMPLES.md`): ObsSynth4D's granular oscillators start with
  the organ (Linux: the ALSA test voice when present), ObsDrums' USER 3 / USER 4 pads with the cowbell and the organ.
- Tested in Ableton Live 12: ObsSynthCT1 and ObsSynth4D with their editors. ObsDrums: in a VST3 test host only so
  far. Each VST3 passes Steinberg's validator in the build.
- A pre-release: please report what a host does with them.

Built from `@COMMIT@`. The VST3s are made from the CLAP code by free-audio's clap-wrapper (MIT) with the VST3 SDK
(Steinberg, MIT). VST is a trademark of Steinberg Media Technologies GmbH.
