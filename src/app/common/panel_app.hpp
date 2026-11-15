#pragma once
// The panels' SDK: one program around every product's front panel (obssynthone, obssynthct1, obsdrums, obssynth4d).
// What is the same in all of them is here, once: the window (its own, or inside a host's on Windows), the panel's
// units (1600 x 900, letterboxed and centred), the settings and the palette (followed when it is the system's), the
// top bar (common/top_bar.hpp: ‹ the preset ›, SAVE, CC, SETUP), SETUP (common/setup_page.hpp), the CC overlay
// (common/cc_overlay.hpp: a click learns, a right click forgets), the presets (the factory's and the person's,
// SAVE with a name), the computer keyboard playing notes, the plugin's link (--remote: the panel only shows and
// turns), and the drawing: the backgrounds rasterized once per layout and scale (the others by a thread), the panel
// kept in a texture between frames and only what changed drawn again (docs/PANEL_DRAWING.md), nothing drawn when
// nothing changed. OBS_PANEL_SHOT=file.bmp:seconds, OBS_PANEL_CLICKS="s:x,y;...", OBS_PANEL_DRAG="s:x,y[,dy]",
// OBS_PANEL_TIMES=1 (the frames' drawing and present every two seconds).
// A product says only what is its own (Product): its parameters (0..1 on the panel), its layout and background,
// what it draws besides its controls (displays, titles), its presets and controllers, and its sound (Engine).
#include "cc_overlay.hpp"
#include "panel_kit.hpp"
#include "platform_io.hpp"

#include <SDL3/SDL.h>

#include <functional>
#include <string>
#include <vector>

namespace synth::panel
{
    constexpr float W = 1600, H = 900, BAR = 58; // the panel's units; the top bar is the SDK's

    enum class Kind { Knob, Fader, Button, Cell, Hot, Pad, Switch, Toggle, XY };

    // a control on the panel (panel units): knob x,y = centre, a = radius; fader x,y = top, a = height; toggle (an
    // LED) x,y = centre; switch x,y = its first choice's corner, a, b = each choice's size, side by side (options);
    // the others x,y,a,b = a rectangle. A knob, a fader or a cell drags its value: a parameter (param, 0..1), a
    // value of the product's own (v, from lo to hi), or one it reads and sets itself (get, set: 0..1); a switch
    // takes the choice clicked (and steps by the wheel), a toggle flips, an XY pad drags two values (v across, vy
    // up); a button or a hot spot calls click(the mouse button); a pad calls click(the velocity, 1..127: higher on
    // the pad, harder).
    struct Control
    {
        Kind kind = Kind::Knob;
        float x = 0, y = 0, a = 0, b = 0;
        int param = -1;
        float *v = nullptr;   // instead of param: the product's own value, from lo to hi
        float lo = 0, hi = 1;
        float *vy = nullptr;  // an XY pad's second value, up (lo to hi as v)
        std::function<float()> get;     // instead of param or v: a value through the product, 0..1 (a log range, a
        std::function<void(float)> set; // value that is not always there: a matrix cell's route)
        int steps = 0;        // > 1: a stepped value of the product's own (the wheel steps, a drag snaps)
        std::vector<std::string> options; // a switch's choices
        std::string label;
        bool on = false;
        std::function<void(int)> click;
        std::function<bool(int, bool)> press; // a press the product answers first (the mouse button, a second click
                                              // in a row): true, nothing more; else a left press drags the value
        std::function<std::string(float)> say; // the value as words (given 0..1), for the product's own values
        float travel = 200;   // a drag's length for the whole range (Shift: ten times)
        float text = 1;       // the label's and the value's size (a body laid out for another size)
        float def = -1;       // a double click's value (0..1); below 0: the parameter's default, else the middle
        bool jump = false;    // a fader: the press puts the value where it is
        std::string id;       // the CC overlay's name for it ("pad.3.TUNE"); "" a parameter's "p.NAME", or none
        int group = -1;       // a column, a pad...: a press tells the product (pressed)
        float badgeX = 0, badgeY = 0; // the overlay's badge moved from where its kind has it
        int tag = -1;         // the product's own number for it (what it was made from)
        bool held = false;    // set by the SDK: being dragged now
    };

    // what the SDK lends a product while it runs
    struct Host
    {
        ccov::Overlay *overlay = nullptr;
        std::function<void()> whole;                                // draw the whole panel next
        std::function<void(int index, const std::string &name)> shownPreset; // a preset the plugin loaded: shown as loaded
        std::function<void(const std::string &)> notice;          // a moment in the status line
        SDL_FPoint pointer{-1, -1};                                 // where the pointer is (panel units), each frame
        std::function<int()> presetAt;                              // the preset shown (-1: none, the start)
        std::function<void(int)> loadPreset;                        // load that one next (a list of the product's)
    };

    // a product's sound: its engine standing alone, or its plugin through the link (--remote). Values are 0..1.
    class Engine
    {
    public:
        virtual ~Engine() = default;
        virtual bool start(const std::vector<float> &values, const io::Settings &settings) = 0;
        virtual bool connect(const char *link) = 0;
        virtual bool remote() const = 0;
        virtual bool remoteAlive() const = 0;
        // what the host changed: id a parameter, or -1 and value the program's index
        virtual bool pollRemote(int &id, float &value) = 0;
        virtual void setParam(int id, float value) = 0;
        virtual void noteOn(int note, float velocity) = 0;
        virtual void noteOff(int note) = 0;
        virtual bool pollCc(int &channel, int &cc, float &value) = 0; // the CCs that came in (the overlay)
        virtual io::MidiIn &midiIn() = 0;
        virtual const io::AudioOut &audioOut() const = 0;
        virtual bool reopenAudio(const io::Settings &settings) = 0;
        virtual int sounding() const { return 0; } // the voices playing (the status line)
    };

    struct PresetEntry
    {
        std::string name, path; // path "": the factory's
        int factory = -1;
    };

    // the part being drawn (panel units): a product skips what does not touch it
    struct Clip
    {
        float x = 0, y = 0, w = W, h = H;
        bool touches(float rx, float ry, float rw, float rh) const { return rx < x + w && rx + rw > x && ry < y + h && ry + rh > y; }
        bool touches(const SDL_FRect &r) const { return touches(r.x, r.y, r.w, r.h); }
    };

    class Product
    {
    public:
        virtual ~Product() = default;
        Host *host = nullptr; // set by run()

        // who it is: "obssynthone" (settings, data, the MIDI port's name), "ObsSynth One" (the window), the bar's
        // name and number ("OBSSYNTH", "ONE") and where the number starts
        virtual const char *id() const = 0;
        virtual const char *title() const = 0;
        virtual const char *barName() const { return "OBSSYNTH"; }
        virtual const char *barNumber() const = 0;
        virtual float barNumberX() const { return 204; }

        // the parameters, 0..1 on the panel
        virtual int params() const = 0;
        virtual std::string paramName(int id) const = 0;        // "OSC_CUTOFF": the overlay's targets are "p.NAME"
        virtual float paramDefault(int id) const = 0;           // a double click
        virtual std::string paramText(int id, float v) const = 0; // the value under a knob
        virtual int paramSteps(int) const { return 0; }          // > 1: a stepped one (the wheel steps, a drag snaps)

        // the presets: the factory's, then the person's (<data>/<id>/presets)
        virtual std::vector<PresetEntry> presets() = 0;
        virtual bool loadPreset(const PresetEntry &e, std::vector<float> &values, std::string &name) = 0;
        virtual bool savePreset(const std::string &name, const std::vector<float> &values, std::string &path) = 0;

        // the controllers' factory maps, and the targets of the product's own (beyond "p.NAME", @preset±, @overlay)
        virtual std::vector<ccov::Map> controllers() const = 0;
        virtual ccov::Target target(const std::string &) { return {}; }

        virtual Engine &engine() = 0;

        // the layout: which background the values ask for (an engine, a page), every one there is (rasterized ahead),
        // the controls, and the background's SVG under the bar (the SDK draws the bar's own)
        virtual std::string layoutKey(const std::vector<float> &values) const = 0;
        virtual std::vector<std::string> layoutKeys() const { return {}; }
        virtual std::vector<Control> layout(std::vector<float> &values) = 0;
        virtual std::string bodySvg(const panelkit::Theme &theme, const std::string &key) = 0;

        // drawing: what is the product's beyond the controls (displays, titles, its part of the bar); a control it
        // draws itself (true); a value drawn somewhere besides its controls (those areas; one W x H: the whole panel)
        virtual void drawBody(panelkit::Canvas &cv, const std::vector<float> &values, const Clip &clip) = 0;
        virtual bool drawControl(panelkit::Canvas &, const Control &, float) { return false; }
        virtual std::vector<SDL_FRect> damageOf(int, const std::vector<float> &) { return {}; }
        // what moves by itself (meters, positions): the areas to draw again now ({} nothing)
        virtual std::vector<SDL_FRect> live() { return {}; }
        // a click on the bar outside the SDK's parts (a page's tab): true when it was the product's
        virtual bool clickBar(float, float, std::vector<float> &) { return false; }
        // a value from the host or the controller that changes more than itself (a list): the whole panel next
        virtual bool wantsWhole() { return false; }
        // what the status line adds ("6 VOICES")
        virtual std::string status() { return std::to_string(engine().sounding()) + " VOICES"; }
        // a product without parameters (its own values): changed since its preset was loaded (the bar's •)
        virtual bool modified() { return false; }
        // every frame, after the input: what the product does by itself (its values to its engine, the host's)
        virtual void tick() {}
        // a key of its own (the pads): true when it was
        virtual bool key(SDL_Scancode, bool) { return false; }
        // a control pressed (its group: a pad's column chosen)
        virtual void pressed(const Control &) {}
        // the overlay's names for a control (its own, and what it is when chosen: "pad.sel.TUNE")
        virtual std::vector<std::string> aliases(const std::string &id) { return {id}; }

        // a product that starts on a state of its own, not on its first preset: that state's name ("START")
        virtual const char *startName() const { return nullptr; }
        // in a host (--remote), the panel units left out on the left now: a column the host's window has no room for
        virtual float crop() const { return 0; }
        // the preset's name in the bar clicked: true when the product shows something of its own (a list); else the next
        virtual bool clickTitle() { return false; }
        // a layer of the product's over the controls and the bar (handles, a source dragged, a list it opened): drawn
        // after them; a press goes to it first (true: taken), and the release after one; the wheel too (true: taken)
        virtual void drawOver(panelkit::Canvas &, const Clip &) {}
        virtual bool press(float, float, int) { return false; }
        virtual void release(float, float) {}
        virtual bool wheel(float, float, float) { return false; }
        // a list of its open (no overlay badges under it): Escape closes it (closeOver)
        virtual bool overOpen() const { return false; }
        virtual void closeOver() {}
    };

    // the program: the window, the loop, until it closes. argv: --remote LINK, --parent HWND, --preset NAME|N,
    // --cc, --setup. F2: the CC overlay (Shift F2: the next map).
    int run(Product &product, int argc, char **argv);
} // namespace synth::panel
