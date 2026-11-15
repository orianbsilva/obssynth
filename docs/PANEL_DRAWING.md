# Drawing a panel on a small machine

What `obssynthone` learned on a Raspberry Pi 2 (Cortex-A7, VideoCore IV, carbonian-shell as the compositor),
for every panel made with `common/panel_kit.hpp` (`obssynth4d`, `obsdrums`, `obssynthct1`, `obssynthone`).

## The measurements (Pi 2, window 1186x723)

| What | Before | After |
|---|---|---|
| The first frame (the background SVG rasterized) | 2.4–2.9 s | the same, once |
| Changing the engine (a new background) | 2.0–2.4 s, the panel frozen | 4–124 ms |
| A frame while a knob is dragged: drawing (CPU) | 8 ms | 9–10 ms |
| A frame while a knob is dragged: `SDL_RenderPresent` | 25 ms | 30 ms (the compositor's pace) |
| The panel idle, the pointer passing over it | a redraw per event | nothing drawn |
| `obssynth4d` idle (redraws all the time) | 83% CPU | — |

`OBS_PANEL_TIMES=1` prints the frames, their drawing and their present every two seconds;
`OBS_PANEL_DRAG="seconds:x,y[,dy]"` drags from a point (panel units) by itself, `OBS_PANEL_CLICKS` clicks.

## The rules

1. **Draw only on a change.** Wait for events (`SDL_WaitEventTimeout`); an event is not a change. Compare the values
   with the ones last drawn: a pointer passing over the panel draws nothing and presents nothing.
2. **Rasterize an SVG once per (layout, scale).** A background is seconds of CPU on a Cortex-A7. Keep one texture per
   engine and page; rasterize the others in a thread right after the first frame (to an `SDL_Surface` only: the
   textures are made by the drawing thread). A new window size drops them all.
3. **Keep the panel in a texture and redraw the damage.** The panel lives in a target texture between frames. A change
   redraws the union of the areas it touches, under a clip rect: the background's piece, then the controls and
   displays that intersect it (skip the others before building their text). The screen gets that one texture.
4. **Know what a value draws.** A knob's value is drawn by its knob; some values are also drawn elsewhere (a panel's
   title, the top bar, a display, "DRY: NOT RUNNING"). Those either add their area to the damage or ask for the whole
   panel. A preset, a page, an engine, a list from the plugin: the whole panel.
5. **The audio never waits for the screen.** The audio callback is the engine's only writer; the UI and the MIDI
   threads queue commands. The audio thread asks for `SCHED_FIFO` (`alsa_backend.cpp`), which needs the user's
   `RLIMIT_RTPRIO` (the `audio` group's `limits.d`); without it, the compositor and the panel can starve it.
6. **The profile is the engine's, not the panel's.** A small machine plays the lite profile
   (`lib/synth/engine_profile.hpp`); the panel's cost is separate and has to fit in what is left.

## What is left

- The present costs ~30 ms whatever the panel draws: the compositor composes the whole screen. A partial present
  (`eglSwapBuffersWithDamage`, the surface's damage) would tell it less changed; SDL does not expose it.
- The rules live in the panels' SDK (`src/app/common/panel_app.cpp`): a product of it gets them. The four panels
  (`obssynthone`, `obssynthct1`, `obsdrums`, `obssynth4d`) are on it; `obssynth4d` idle on MAIN draws nothing (it drew
  every frame), and what moves by itself (a halo, the scope, a meter of the matrix) draws only its own area.
