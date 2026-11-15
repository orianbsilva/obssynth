#include "launchkey_mk4.hpp"
#include "../utils/platform_midi_reader.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

namespace synth
{
    LaunchkeyMK4::LaunchkeyMK4()
        : MidiDevice("Launchkey MK4", DeviceType::LAUNCHKEY_MK4,
                     static_cast<uint32_t>(DeviceCapability::RGB_LEDS) |
                         static_cast<uint32_t>(DeviceCapability::VELOCITY_SENSITIVE) |
                         static_cast<uint32_t>(DeviceCapability::FADERS) |
                         static_cast<uint32_t>(DeviceCapability::ROTARY_ENCODERS) |
                         static_cast<uint32_t>(DeviceCapability::NAVIGATION)),
          daw_mode_(false), current_pad_mode_(LaunchkeyMK4Constants::PAD_MODE_DRUM)
    {
        // Initialize color arrays to off
        pad_colors_.fill(LaunchkeyMK4Constants::COLOR_OFF);
        fader_values_.fill(0);
        knob_values_.fill(64); // Center position for knobs
    }

    LaunchkeyMK4::~LaunchkeyMK4()
    {
        if (is_connected_)
        {
            disconnect();
        }
    }

    bool LaunchkeyMK4::connect(uint8_t device_id)
    {
        if (is_connected_)
        {
            return true;
        }

        // Create MIDI reader if not exists
        if (!midi_reader_)
        {
            midi_reader_ = std::make_shared<MidiInterface>();
        }

        // Initialize MIDI
        if (midi_reader_->init() != 0)
        {
            std::cerr << "Failed to initialize MIDI for Launchkey MK4" << std::endl;
            return false;
        }

        // Set up MIDI callbacks to route to our device processing
        midi_reader_->setNoteCallback([this](const MidiNote &note)
                                      { processMidiMessage(note.channel, note.is_note_on ? MIDI_NOTE_ON : MIDI_NOTE_OFF,
                                                           note.note, note.velocity); });

        midi_reader_->setPolyphonicAftertouchCallback([this](uint8_t channel, uint8_t note, uint8_t pressure)
                                                      { processMidiMessage(channel, MIDI_POLYPHONIC_AFTERTOUCH, note, pressure); });

        midi_reader_->setControlCallback([this](uint8_t channel, uint8_t controller, uint8_t value)
                                         { processMidiMessage(channel, MIDI_CONTROL, controller, value); });

        midi_reader_->setSysExCallback([this](const std::vector<uint8_t> &data)
                                       { onSysExReceived(data); });

        // Try to open the device for input
        int result = midi_reader_->open(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open Launchkey MK4 device for input " << static_cast<int>(device_id) << std::endl;
            return false;
        }

        // Try to open the device for output
        result = midi_reader_->openOutput(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open Launchkey MK4 device for output " << static_cast<int>(device_id) << std::endl;
            midi_reader_->close();
            return false;
        }

        device_id_ = device_id;
        is_connected_ = true;

        // Initialize device to known state
        reset();
        enterProgrammerMode();

        std::cout << "Launchkey MK4 connected successfully on device " << static_cast<int>(device_id) << std::endl;
        return true;
    }

    void LaunchkeyMK4::disconnect()
    {
        if (!is_connected_)
        {
            return;
        }

        // Reset device before disconnecting
        reset();

        if (midi_reader_)
        {
            midi_reader_->close();
            midi_reader_->closeOutput();
        }

        is_connected_ = false;
        std::cout << "Launchkey MK4 disconnected" << std::endl;
    }

    bool LaunchkeyMK4::setPadColor(const PadPosition &position, const RGBColor &color)
    {
        if (!is_connected_ || position.x >= LaunchkeyMK4Constants::GRID_WIDTH ||
            position.y >= LaunchkeyMK4Constants::GRID_HEIGHT)
        {
            return false;
        }

        uint8_t pad_id = position.y * LaunchkeyMK4Constants::GRID_WIDTH + position.x;
        return setPadColor(pad_id, color);
    }

    bool LaunchkeyMK4::setPadColor(uint8_t pad_id, const RGBColor &color)
    {
        if (!is_connected_ || pad_id >= LaunchkeyMK4Constants::TOTAL_PADS)
        {
            return false;
        }

        uint8_t palette_color = rgbToPaletteColor(color);
        pad_colors_[pad_id] = palette_color;

        // Convert pad ID to MIDI note
        uint8_t note = LaunchkeyMK4Constants::PAD_NOTE_MIN + pad_id;

        // Send SysEx command to set LED color
        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_SET_LED,
            note,
            palette_color,
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);
        return true;
    }

    void LaunchkeyMK4::clearAllPads()
    {
        setAllPads(RGBColor::OFF);
    }

    void LaunchkeyMK4::setAllPads(const RGBColor &color)
    {
        for (uint8_t i = 0; i < LaunchkeyMK4Constants::TOTAL_PADS; ++i)
        {
            setPadColor(i, color);
        }
    }

    bool LaunchkeyMK4::setSideButtonColor(uint8_t button_id, const RGBColor &color)
    {
        (void)button_id;
        (void)color;
        // Launchkey MK4 doesn't have traditional side buttons like Launchpad
        // This is a no-op to satisfy the interface
        return false;
    }

    bool LaunchkeyMK4::setTopButtonColor(uint8_t button_id, const RGBColor &color)
    {
        (void)button_id;
        (void)color;
        // Launchkey MK4 doesn't have traditional top buttons like Launchpad
        // This is a no-op to satisfy the interface
        return false;
    }

    void LaunchkeyMK4::enterProgrammerMode()
    {
        if (!is_connected_)
        {
            return;
        }

        // Send SysEx command to enter DAW mode for full control
        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_SET_DAW_MODE,
            0x01, // Enable DAW mode
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);
        daw_mode_ = true;

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Launchkey MK4 entered programmer/DAW mode" << std::endl;
#endif
    }

    void LaunchkeyMK4::exitProgrammerMode()
    {
        if (!is_connected_)
        {
            return;
        }

        // Send SysEx command to exit DAW mode
        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_SET_DAW_MODE,
            0x00, // Disable DAW mode
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);
        daw_mode_ = false;

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Launchkey MK4 exited programmer/DAW mode" << std::endl;
#endif
    }

    void LaunchkeyMK4::reset()
    {
        if (!is_connected_)
        {
            return;
        }

        // Send SysEx reset command
        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_RESET,
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);

        // Reset internal state
        clearAllPads();
        fader_values_.fill(0);
        knob_values_.fill(64);
        daw_mode_ = false;
        current_pad_mode_ = LaunchkeyMK4Constants::PAD_MODE_DRUM;

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Launchkey MK4 reset" << std::endl;
#endif
    }

    void LaunchkeyMK4::sendSysEx(const std::vector<uint8_t> &data)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        int result = midi_reader_->sendSysEx(data);
        if (result != 0)
        {
            std::cerr << "Failed to send SysEx to Launchkey MK4: " << result << std::endl;
        }

#ifdef DEBUG_LAUNCHKEY
        std::cout << "SysEx sent: ";
        for (uint8_t byte : data)
        {
            std::cout << std::hex << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << std::endl;
#endif
    }

    void LaunchkeyMK4::sendNoteOn(uint8_t note, uint8_t velocity)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Note On message (channel 0)
        int result = midi_reader_->sendNoteOn(0, note, velocity);
        if (result != 0)
        {
            std::cerr << "Failed to send Note On to Launchkey MK4: " << result << std::endl;
        }

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Note ON sent: " << static_cast<int>(note) << " vel: " << static_cast<int>(velocity) << std::endl;
#endif
    }

    void LaunchkeyMK4::sendNoteOff(uint8_t note)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Note Off message (channel 0, velocity 0)
        int result = midi_reader_->sendNoteOff(0, note, 0);
        if (result != 0)
        {
            std::cerr << "Failed to send Note Off to Launchkey MK4: " << result << std::endl;
        }

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Note OFF sent: " << static_cast<int>(note) << std::endl;
#endif
    }

    void LaunchkeyMK4::sendControlChange(uint8_t controller, uint8_t value)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Control Change message (channel 0)
        int result = midi_reader_->sendControlChange(0, controller, value);
        if (result != 0)
        {
            std::cerr << "Failed to send Control Change to Launchkey MK4: " << result << std::endl;
        }

#ifdef DEBUG_LAUNCHKEY
        std::cout << "CC sent: " << static_cast<int>(controller) << " val: " << static_cast<int>(value) << std::endl;
#endif
    }

    uint8_t LaunchkeyMK4::rgbToPaletteColor(const RGBColor &color) const
    {
        // Simple RGB to palette mapping
        if (color.red == 0 && color.green == 0 && color.blue == 0)
        {
            return LaunchkeyMK4Constants::COLOR_OFF;
        }
        else if (color.red > color.green && color.red > color.blue)
        {
            return color.red > 128 ? LaunchkeyMK4Constants::COLOR_RED_FULL : LaunchkeyMK4Constants::COLOR_RED_LOW;
        }
        else if (color.green > color.red && color.green > color.blue)
        {
            return color.green > 128 ? LaunchkeyMK4Constants::COLOR_GREEN_FULL : LaunchkeyMK4Constants::COLOR_GREEN_LOW;
        }
        else if (color.blue > color.red && color.blue > color.green)
        {
            return color.blue > 128 ? LaunchkeyMK4Constants::COLOR_BLUE_FULL : LaunchkeyMK4Constants::COLOR_BLUE_LOW;
        }
        else if (color.red > 128 && color.green > 128 && color.blue < 64)
        {
            return LaunchkeyMK4Constants::COLOR_YELLOW;
        }
        else if (color.green > 128 && color.blue > 128 && color.red < 64)
        {
            return LaunchkeyMK4Constants::COLOR_CYAN;
        }
        else if (color.red > 128 && color.blue > 128 && color.green < 64)
        {
            return LaunchkeyMK4Constants::COLOR_MAGENTA;
        }
        else if (color.red > 200 && color.green > 200 && color.blue > 200)
        {
            return LaunchkeyMK4Constants::COLOR_WHITE;
        }

        return LaunchkeyMK4Constants::COLOR_WHITE; // Default fallback
    }

    RGBColor LaunchkeyMK4::paletteColorToRGB(uint8_t palette_color) const
    {
        switch (palette_color)
        {
        case LaunchkeyMK4Constants::COLOR_OFF:
            return RGBColor(0, 0, 0);
        case LaunchkeyMK4Constants::COLOR_RED_LOW:
            return RGBColor(64, 0, 0);
        case LaunchkeyMK4Constants::COLOR_RED_FULL:
            return RGBColor(255, 0, 0);
        case LaunchkeyMK4Constants::COLOR_GREEN_LOW:
            return RGBColor(0, 64, 0);
        case LaunchkeyMK4Constants::COLOR_GREEN_FULL:
            return RGBColor(0, 255, 0);
        case LaunchkeyMK4Constants::COLOR_BLUE_LOW:
            return RGBColor(0, 0, 64);
        case LaunchkeyMK4Constants::COLOR_BLUE_FULL:
            return RGBColor(0, 0, 255);
        case LaunchkeyMK4Constants::COLOR_YELLOW:
            return RGBColor(255, 255, 0);
        case LaunchkeyMK4Constants::COLOR_CYAN:
            return RGBColor(0, 255, 255);
        case LaunchkeyMK4Constants::COLOR_MAGENTA:
            return RGBColor(255, 0, 255);
        case LaunchkeyMK4Constants::COLOR_WHITE:
            return RGBColor(255, 255, 255);
        default:
            return RGBColor(255, 255, 255);
        }
    }

    void LaunchkeyMK4::processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2)
    {
        uint8_t msg_type = status & 0xF0;

        switch (msg_type)
        {
        case MIDI_NOTE_ON:
        case MIDI_NOTE_OFF:
        {
            bool is_press = (msg_type == MIDI_NOTE_ON) && (data2 > 0);

            if (isPadNote(data1))
            {
                PadPosition pos = noteToPadPosition(data1);
                triggerPadCallback(channel, pos, data2, is_press);
            }
            // Keyboard notes would be handled here for full keyboard functionality
            break;
        }

        case MIDI_POLYPHONIC_AFTERTOUCH:
        {
            if (isPadNote(data1))
            {
                PadPosition pos = noteToPadPosition(data1);
                triggerAftertouchCallback(channel, pos, data2);
            }
            break;
        }

        case MIDI_CONTROL:
        {
            // Handle faders
            if (isFaderControl(data1))
            {
                uint8_t fader_id = data1 - LaunchkeyMK4Constants::FADER_CC_START;
                if (fader_id < LaunchkeyMK4Constants::NUM_FADERS)
                {
                    fader_values_[fader_id] = data2;
                }
            }
            // Handle knobs
            else if (isKnobControl(data1))
            {
                uint8_t knob_id = data1 - LaunchkeyMK4Constants::KNOB_CC_START;
                if (knob_id < LaunchkeyMK4Constants::NUM_KNOBS)
                {
                    knob_values_[knob_id] = data2;
                }
            }
            // Handle transport controls
            else if (isTransportControl(data1))
            {
                // Transport controls are handled as button presses
                // Convert to side button callback for consistency
                triggerSideButtonCallback(channel, data1, data2 > 0);
            }
            // Handle navigation controls
            else if (isNavigationControl(data1))
            {
                if (data2 > 0) // Button press
                {
                    NavigationDirection direction;
                    switch (data1)
                    {
                    case LaunchkeyMK4Constants::CC_UP:
                        direction = NavigationDirection::UP;
                        break;
                    case LaunchkeyMK4Constants::CC_DOWN:
                        direction = NavigationDirection::DOWN;
                        break;
                    case LaunchkeyMK4Constants::CC_LEFT:
                        direction = NavigationDirection::LEFT;
                        break;
                    case LaunchkeyMK4Constants::CC_RIGHT:
                        direction = NavigationDirection::RIGHT;
                        break;
                    default:
                        return;
                    }
                    triggerNavigationCallback(channel, direction);
                }
            }
            break;
        }
        }
    }

    PadPosition LaunchkeyMK4::noteToPadPosition(uint8_t note) const
    {
        // Launchkey MK4 pad mapping: note 36 = (0,3), note 39 = (3,3), etc.
        if (!isPadNote(note))
        {
            return PadPosition(0, 0);
        }

        uint8_t pad_id = note - LaunchkeyMK4Constants::PAD_NOTE_MIN;
        uint8_t col = pad_id % LaunchkeyMK4Constants::GRID_WIDTH;
        uint8_t row = pad_id / LaunchkeyMK4Constants::GRID_WIDTH;

        return PadPosition(col, LaunchkeyMK4Constants::GRID_HEIGHT - 1 - row); // Flip Y to match visual layout
    }

    uint8_t LaunchkeyMK4::padPositionToNote(const PadPosition &position) const
    {
        if (position.x >= LaunchkeyMK4Constants::GRID_WIDTH || position.y >= LaunchkeyMK4Constants::GRID_HEIGHT)
        {
            return 0;
        }

        uint8_t row = LaunchkeyMK4Constants::GRID_HEIGHT - 1 - position.y;
        uint8_t pad_id = row * LaunchkeyMK4Constants::GRID_WIDTH + position.x;
        return LaunchkeyMK4Constants::PAD_NOTE_MIN + pad_id;
    }

    bool LaunchkeyMK4::isPadNote(uint8_t note) const
    {
        return note >= LaunchkeyMK4Constants::PAD_NOTE_MIN && note <= LaunchkeyMK4Constants::PAD_NOTE_MAX;
    }

    bool LaunchkeyMK4::isTransportControl(uint8_t cc) const
    {
        return cc == LaunchkeyMK4Constants::CC_PLAY ||
               cc == LaunchkeyMK4Constants::CC_STOP ||
               cc == LaunchkeyMK4Constants::CC_RECORD ||
               cc == LaunchkeyMK4Constants::CC_LOOP ||
               cc == LaunchkeyMK4Constants::CC_REWIND ||
               cc == LaunchkeyMK4Constants::CC_FAST_FORWARD;
    }

    bool LaunchkeyMK4::isNavigationControl(uint8_t cc) const
    {
        return cc == LaunchkeyMK4Constants::CC_UP ||
               cc == LaunchkeyMK4Constants::CC_DOWN ||
               cc == LaunchkeyMK4Constants::CC_LEFT ||
               cc == LaunchkeyMK4Constants::CC_RIGHT;
    }

    bool LaunchkeyMK4::isFaderControl(uint8_t cc) const
    {
        return cc >= LaunchkeyMK4Constants::FADER_CC_START &&
               cc < (LaunchkeyMK4Constants::FADER_CC_START + LaunchkeyMK4Constants::NUM_FADERS);
    }

    bool LaunchkeyMK4::isKnobControl(uint8_t cc) const
    {
        return cc >= LaunchkeyMK4Constants::KNOB_CC_START &&
               cc < (LaunchkeyMK4Constants::KNOB_CC_START + LaunchkeyMK4Constants::NUM_KNOBS);
    }

    // Launchkey MK4 specific features
    void LaunchkeyMK4::setPadMode(uint8_t mode)
    {
        if (!is_connected_)
        {
            return;
        }

        current_pad_mode_ = mode;

        // Send SysEx command to set pad mode
        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_SET_PAD_MODE,
            mode,
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);
    }

    void LaunchkeyMK4::enableDAWMode()
    {
        enterProgrammerMode();
    }

    void LaunchkeyMK4::disableDAWMode()
    {
        exitProgrammerMode();
    }

    bool LaunchkeyMK4::isDAWMode() const
    {
        return daw_mode_;
    }

    uint8_t LaunchkeyMK4::getFaderValue(uint8_t fader_id) const
    {
        if (fader_id >= LaunchkeyMK4Constants::NUM_FADERS)
        {
            return 0;
        }
        return fader_values_[fader_id];
    }

    uint8_t LaunchkeyMK4::getKnobValue(uint8_t knob_id) const
    {
        if (knob_id >= LaunchkeyMK4Constants::NUM_KNOBS)
        {
            return 64; // Center position
        }
        return knob_values_[knob_id];
    }

    void LaunchkeyMK4::setFaderValues(const std::array<uint8_t, LaunchkeyMK4Constants::NUM_FADERS> &values)
    {
        fader_values_ = values;

        // Optionally send the values back to the device (if supported)
        for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS; ++i)
        {
            sendControlChange(LaunchkeyMK4Constants::FADER_CC_START + i, values[i]);
        }
    }

    void LaunchkeyMK4::setKnobValues(const std::array<uint8_t, LaunchkeyMK4Constants::NUM_KNOBS> &values)
    {
        knob_values_ = values;

        // Optionally send the values back to the device (if supported)
        for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS; ++i)
        {
            sendControlChange(LaunchkeyMK4Constants::KNOB_CC_START + i, values[i]);
        }
    }

    // Utility functions
    void LaunchkeyMK4::testAllPads()
    {
        if (!is_connected_)
        {
            return;
        }

        std::cout << "Testing all Launchkey MK4 pads..." << std::endl;

        // Cycle through all pads with different colors
        for (uint8_t i = 0; i < LaunchkeyMK4Constants::TOTAL_PADS; ++i)
        {
            RGBColor test_color;
            switch (i % 7)
            {
            case 0:
                test_color = RGBColor::RED;
                break;
            case 1:
                test_color = RGBColor::GREEN;
                break;
            case 2:
                test_color = RGBColor::BLUE;
                break;
            case 3:
                test_color = RGBColor::YELLOW;
                break;
            case 4:
                test_color = RGBColor::CYAN;
                break;
            case 5:
                test_color = RGBColor::MAGENTA;
                break;
            case 6:
                test_color = RGBColor::WHITE;
                break;
            }

            setPadColor(i, test_color);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        clearAllPads();
        std::cout << "Pad test complete" << std::endl;
    }

    void LaunchkeyMK4::rainbowPattern()
    {
        if (!is_connected_)
        {
            return;
        }

        for (uint8_t y = 0; y < LaunchkeyMK4Constants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchkeyMK4Constants::GRID_WIDTH; ++x)
            {
                float hue = (static_cast<float>(x + y * LaunchkeyMK4Constants::GRID_WIDTH) /
                             static_cast<float>(LaunchkeyMK4Constants::TOTAL_PADS)) *
                            360.0f;

                // Simple HSV to RGB conversion
                float r, g, b;
                float h = hue / 60.0f;
                float c = 1.0f;
                float x_val = c * (1.0f - std::abs(fmodf(h, 2.0f) - 1.0f));

                if (h < 1.0f)
                {
                    r = c;
                    g = x_val;
                    b = 0.0f;
                }
                else if (h < 2.0f)
                {
                    r = x_val;
                    g = c;
                    b = 0.0f;
                }
                else if (h < 3.0f)
                {
                    r = 0.0f;
                    g = c;
                    b = x_val;
                }
                else if (h < 4.0f)
                {
                    r = 0.0f;
                    g = x_val;
                    b = c;
                }
                else if (h < 5.0f)
                {
                    r = x_val;
                    g = 0.0f;
                    b = c;
                }
                else
                {
                    r = c;
                    g = 0.0f;
                    b = x_val;
                }

                RGBColor color(
                    static_cast<uint8_t>(r * 255.0f),
                    static_cast<uint8_t>(g * 255.0f),
                    static_cast<uint8_t>(b * 255.0f));

                setPadColor(PadPosition(x, y), color);
            }
        }
    }

    void LaunchkeyMK4::checkerboardPattern(const RGBColor &color1, const RGBColor &color2)
    {
        for (uint8_t y = 0; y < LaunchkeyMK4Constants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchkeyMK4Constants::GRID_WIDTH; ++x)
            {
                bool use_color1 = ((x + y) % 2) == 0;
                setPadColor(PadPosition(x, y), use_color1 ? color1 : color2);
            }
        }
    }

    void LaunchkeyMK4::flashPad(const PadPosition &position, const RGBColor &color, uint16_t duration_ms)
    {
        uint8_t pad_id = position.y * LaunchkeyMK4Constants::GRID_WIDTH + position.x;
        RGBColor original_color = paletteColorToRGB(pad_colors_[pad_id]);

        setPadColor(position, color);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        setPadColor(position, original_color);
    }

    void LaunchkeyMK4::pulsePad(const PadPosition &position, const RGBColor &color)
    {
        // Simple pulse effect - flash twice
        flashPad(position, color, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        flashPad(position, color, 100);
    }

    // Device state probing methods
    void LaunchkeyMK4::requestFaderValues()
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_GET_FADER_VALUES,
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Requested fader values from device" << std::endl;
#endif
    }

    void LaunchkeyMK4::requestKnobValues()
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_GET_KNOB_VALUES,
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Requested knob values from device" << std::endl;
#endif
    }

    void LaunchkeyMK4::requestDeviceState()
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_GET_DEVICE_STATE,
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Requested device state from device" << std::endl;
#endif
    }

    void LaunchkeyMK4::requestAllValues()
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        std::vector<uint8_t> sysex_data = {
            LaunchkeyMK4Constants::SYSEX_HEADER[0],
            LaunchkeyMK4Constants::SYSEX_HEADER[1],
            LaunchkeyMK4Constants::SYSEX_HEADER[2],
            LaunchkeyMK4Constants::SYSEX_HEADER[3],
            LaunchkeyMK4Constants::SYSEX_HEADER[4],
            LaunchkeyMK4Constants::SYSEX_HEADER[5],
            LaunchkeyMK4Constants::CMD_GET_ALL_VALUES,
            LaunchkeyMK4Constants::SYSEX_FOOTER};

        sendSysEx(sysex_data);

#ifdef DEBUG_LAUNCHKEY
        std::cout << "Requested all control values from device" << std::endl;
#endif
    }

    void LaunchkeyMK4::onSysExReceived(const std::vector<uint8_t> &data)
    {
        // Validate SysEx message format
        if (data.size() < 7)
        {
            return; // Too short to be valid
        }

        // Check if this is a Launchkey MK4 SysEx message
        if (data[0] != LaunchkeyMK4Constants::SYSEX_HEADER[0] ||
            data[1] != LaunchkeyMK4Constants::SYSEX_HEADER[1] ||
            data[2] != LaunchkeyMK4Constants::SYSEX_HEADER[2] ||
            data[3] != LaunchkeyMK4Constants::SYSEX_HEADER[3] ||
            data[4] != LaunchkeyMK4Constants::SYSEX_HEADER[4] ||
            data[5] != LaunchkeyMK4Constants::SYSEX_HEADER[5])
        {
            return; // Not a Launchkey MK4 message
        }

        if (data.back() != LaunchkeyMK4Constants::SYSEX_FOOTER)
        {
            return; // Invalid SysEx footer
        }

        // Process the response
        processSysExResponse(data);
    }

    void LaunchkeyMK4::processSysExResponse(const std::vector<uint8_t> &data)
    {
        if (data.size() < 8)
        {
            return; // Not enough data
        }

        uint8_t command = data[6]; // Command byte after header

        switch (command)
        {
        case LaunchkeyMK4Constants::CMD_GET_FADER_VALUES:
            // Response format: Header + CMD + 8 fader values + Footer
            if (data.size() >= 16) // 6 (header) + 1 (cmd) + 8 (values) + 1 (footer)
            {
                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS && i < 8; ++i)
                {
                    fader_values_[i] = data[7 + i];
                }
#ifdef DEBUG_LAUNCHKEY
                std::cout << "Received fader values: ";
                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS; ++i)
                {
                    std::cout << static_cast<int>(fader_values_[i]) << " ";
                }
                std::cout << std::endl;
#endif
            }
            break;

        case LaunchkeyMK4Constants::CMD_GET_KNOB_VALUES:
            // Response format: Header + CMD + 8 knob values + Footer
            if (data.size() >= 16) // 6 (header) + 1 (cmd) + 8 (values) + 1 (footer)
            {
                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS && i < 8; ++i)
                {
                    knob_values_[i] = data[7 + i];
                }
#ifdef DEBUG_LAUNCHKEY
                std::cout << "Received knob values: ";
                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS; ++i)
                {
                    std::cout << static_cast<int>(knob_values_[i]) << " ";
                }
                std::cout << std::endl;
#endif
            }
            break;

        case LaunchkeyMK4Constants::CMD_GET_DEVICE_STATE:
            // Response format: Header + CMD + pad_mode + daw_mode + 8 faders + 8 knobs + Footer
            if (data.size() >= 25) // 6 + 1 + 1 + 1 + 8 + 8 + 1
            {
                current_pad_mode_ = data[7];
                daw_mode_ = (data[8] != 0);

                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS && i < 8; ++i)
                {
                    fader_values_[i] = data[9 + i];
                }

                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS && i < 8; ++i)
                {
                    knob_values_[i] = data[17 + i];
                }

#ifdef DEBUG_LAUNCHKEY
                std::cout << "Received device state: pad_mode=" << static_cast<int>(current_pad_mode_)
                          << " daw_mode=" << daw_mode_ << std::endl;
#endif
            }
            break;

        case LaunchkeyMK4Constants::CMD_GET_ALL_VALUES:
            // Response format: Header + CMD + 8 faders + 8 knobs + Footer
            if (data.size() >= 24) // 6 + 1 + 8 + 8 + 1
            {
                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS && i < 8; ++i)
                {
                    fader_values_[i] = data[7 + i];
                }

                for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS && i < 8; ++i)
                {
                    knob_values_[i] = data[15 + i];
                }

#ifdef DEBUG_LAUNCHKEY
                std::cout << "Received all control values" << std::endl;
#endif
            }
            break;

        default:
#ifdef DEBUG_LAUNCHKEY
            std::cout << "Unknown SysEx command: " << std::hex << static_cast<int>(command) << std::dec << std::endl;
#endif
            break;
        }
    }

    // Factory function
    std::unique_ptr<MidiDevice> createLaunchkeyMK4(const std::string &device_name)
    {
        (void)device_name; // Device name not used in constructor
        return std::make_unique<LaunchkeyMK4>();
    }

} // namespace synth
