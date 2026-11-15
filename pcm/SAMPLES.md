# The recordings in pcm/

The wavetables (`bank_*`, `cycle_*`, `*_wave_*`) are the project's own. The recordings the presets and kits start
with:

| File | From | License |
|---|---|---|
| `vcsl-pipe-organ-quiet-C4.wav` | VCSL (Versilian Community Sample Library), `Aerophones/Edge-blown Aerophones/Pipe Organ/Quiet/NT5_Man3Quiet_C4_rr1.wav`; organ sampled by Simon Dalzell of Ivy Audio | CC0 1.0 |
| `vcsl-cowbell.wav` | VCSL, `Idiophones/Struck Idiophones/Cowbells/Cowbell1_Hit_v3_rr1_Mid.wav` | CC0 1.0 |

VCSL: https://github.com/sgossner/VCSL (CC0 1.0; credit to the authors and Versilian Studios is encouraged). Both
cut here to mono, 16 bit, 44.1 kHz, from the first sound (the organ 3 s, the cowbell 0.8 s, a fade at the end),
peak at -1 dBFS. ObsSynth4D's default grains prefer the ALSA test voice where the system has it (not shipped: it is
alsa-utils', GPL-2); else this organ.
