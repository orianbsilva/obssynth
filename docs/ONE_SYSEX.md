# ObsSynth One: words for the plugin (SysEx)

A CLAP parameter is a number. Some of what a program wants to say to ObsSynthOne is a name: a recording (`vcsl-cowbell`),
an engine (`wt`), a preset (`Pluck`). A host that knows no list of names can say them in the stream of the notes, at
the time they are due, as a **SysEx** event (`CLAP_EVENT_MIDI_SYSEX`, on the plugin's note port, whose dialects include MIDI).

## The message

```
F0 7D  <NAME> ' ' <VALUE>  F7
```

* `F0` and `F7` are SysEx's start and end; a host may leave them out.
* `7D` is MIDI's manufacturer id for non-commercial use: a message that does not start with it is not ours and is ignored.
* `NAME` is a parameter's name (`OSC_SMP_SAMPLE`, `OSC_FILT_TYPE`, `OSC_CUTOFF`...: the host's
  parameter list, or `app/obssynthone/one_params.hpp`), or `PROGRAM` for a preset.
* `VALUE` is, by parameter:

| Parameter | VALUE |
|---|---|
| `PROGRAM` | a preset's name (case aside) or its number from 1 |
| `OSC_PART_TYPE` | the engine: `std` `wt` `fm` `grain` `smp` (also `standard`, `wavetable`, `gr`, `granular`, `sample`) |
| `OSC_WAVE` | `sine` `sawtooth` `square` `triangle` `pulse` `noise` (also `saw`, `tri`, `white`) |
| `OSC_FILT_TYPE` | `lp` `hp` `bp` `notch` (also `lowpass`, `highpass`, `bandpass`) |
| `OSC_SMP_SAMPLE` | a recording's file name, with or without `.wav`, case aside |
| any other | a number **in the parameter's own units** (Hz for a cutoff, seconds for an attack...), as the panel shows it |

The text is ASCII, 120 bytes at most. A name the plugin does not know is ignored (the value stays what it was).

## Rules

* It is read on the audio thread, at the event's time: no filesystem, no allocation. The recordings the plugin knows are the
  ones it listed when it opened (the project's `pcm/` and the samples folder, `~/.local/share/obssynth4d/samples`, up to
  64): a file added later is there after the plugin is opened again.
* What a message sets is shown by the panel and told to the host as a parameter change, like a knob's.
* A number is better sent as a parameter event (0..1) when the host has them: the host records and automates it. SysEx is for
  what a number cannot say.

## From carbonian-strudel

`carbonian-strudel` says these words itself (`src/maps.rs` is the table): `s("vcsl-cowbell")` (a word that is not a wave names a
recording), `preset("Pluck")` and `preset(3)`; `engine("smp")` and `ftype("hp")` are numbers it converts. See
`docs/strudel.md` in carbonian.
