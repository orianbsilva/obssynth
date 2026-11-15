#include "launchpadx.hpp"
#include "../utils/platform_midi_reader.hpp"
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

namespace synth
{
    LaunchpadX::LaunchpadX()
        : MidiDevice("Launchpad X", DeviceType::LAUNCHPAD_X,
                    static_cast<uint32_t>(DeviceCapability::RGB_LEDS) |
                    static_cast<uint32_t>(DeviceCapability::VELOCITY_SENSITIVE) |
                    static_cast<uint32_t>(DeviceCapability::AFTERTOUCH) |
                    static_cast<uint32_t>(DeviceCapability::SIDE_BUTTONS) |
                    static_cast<uint32_t>(DeviceCapability::TOP_BUTTONS) |
                    static_cast<uint32_t>(DeviceCapability::NAVIGATION))
        , programmer_mode_(false)
        , session_mode_(false)
        , current_layout_(LaunchpadXConstants::LAYOUT_SESSION)
    {
        // Initialize color arrays to off
        pad_colors_.fill(LaunchpadXConstants::COLOR_OFF);
        side_button_colors_.fill(LaunchpadXConstants::COLOR_OFF);
        top_button_colors_.fill(LaunchpadXConstants::COLOR_OFF);
    }

    LaunchpadX::~LaunchpadX()
    {
        if (is_connected_)
        {
            disconnect();
        }
    }

    bool LaunchpadX::connect(uint8_t device_id)
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
            std::cerr << "Failed to initialize MIDI for Launchpad X" << std::endl;
            return false;
        }

        // Set up MIDI callbacks to route to our device processing
        midi_reader_->setNoteCallback([this](const MidiNote& note) {
            processMidiMessage(note.channel, note.is_note_on ? MIDI_NOTE_ON : MIDI_NOTE_OFF,
                             note.note, note.velocity);
        });

        midi_reader_->setPolyphonicAftertouchCallback([this](uint8_t channel, uint8_t note, uint8_t pressure) {
            processMidiMessage(channel, MIDI_POLYPHONIC_AFTERTOUCH, note, pressure);
        });

        midi_reader_->setControlCallback([this](uint8_t channel, uint8_t controller, uint8_t value) {
            processMidiMessage(channel, MIDI_CONTROL, controller, value);
        });

        // Try to open the device for input
        int result = midi_reader_->open(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open Launchpad X device for input " << static_cast<int>(device_id) << std::endl;
            return false;
        }

        // Try to open the device for output
        result = midi_reader_->openOutput(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open Launchpad X device for output " << static_cast<int>(device_id) << std::endl;
            // Close input if output failed
            midi_reader_->close();
            return false;
        }

        device_id_ = device_id;
        is_connected_ = true;

        // Enter programmer mode and reset
        enterProgrammerMode();
        reset();

        std::cout << "Launchpad X connected successfully on device " << static_cast<int>(device_id) << std::endl;
        return true;
    }

    void LaunchpadX::disconnect()
    {
        if (!is_connected_)
        {
            return;
        }

        // Exit programmer mode and clear all LEDs
        exitProgrammerMode();

        if (midi_reader_)
        {
            midi_reader_->closeOutput();
            midi_reader_->close();
        }

        is_connected_ = false;
        std::cout << "Launchpad X disconnected" << std::endl;
    }

    void LaunchpadX::sendSysEx(const std::vector<uint8_t>& data)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Construct full SysEx message
        std::vector<uint8_t> sysex_msg;

        // Add header
        for (uint8_t byte : LaunchpadXConstants::SYSEX_HEADER)
        {
            sysex_msg.push_back(byte);
        }

        // Add data
        for (uint8_t byte : data)
        {
            sysex_msg.push_back(byte);
        }

        // Add footer
        sysex_msg.push_back(LaunchpadXConstants::SYSEX_FOOTER);

        // Send via MIDI interface
        int result = midi_reader_->sendSysEx(sysex_msg);
        if (result != 0)
        {
            std::cerr << "Failed to send SysEx message to Launchpad X: " << result << std::endl;
        }

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "SysEx sent: ";
        for (uint8_t byte : sysex_msg)
        {
            std::cout << std::hex << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << std::endl;
        #endif
    }

    void LaunchpadX::sendNoteOn(uint8_t note, uint8_t velocity)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Note On message (channel 0)
        int result = midi_reader_->sendNoteOn(0, note, velocity);
        if (result != 0)
        {
            std::cerr << "Failed to send Note On to Launchpad X: " << result << std::endl;
        }

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "Note ON sent: " << static_cast<int>(note) << " vel: " << static_cast<int>(velocity) << std::endl;
        #endif
    }

    void LaunchpadX::sendNoteOff(uint8_t note)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Note Off message (channel 0, velocity 0)
        int result = midi_reader_->sendNoteOff(0, note, 0);
        if (result != 0)
        {
            std::cerr << "Failed to send Note Off to Launchpad X: " << result << std::endl;
        }

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "Note OFF sent: " << static_cast<int>(note) << std::endl;
        #endif
    }

    void LaunchpadX::sendControlChange(uint8_t controller, uint8_t value)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Control Change message (channel 0)
        int result = midi_reader_->sendControlChange(0, controller, value);
        if (result != 0)
        {
            std::cerr << "Failed to send Control Change to Launchpad X: " << result << std::endl;
        }

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "CC sent: " << static_cast<int>(controller) << " val: " << static_cast<int>(value) << std::endl;
        #endif
    }

    uint8_t LaunchpadX::rgbToPaletteColor(const RGBColor& color) const
    {
        // Simple RGB to Launchpad palette mapping
        if (color.red == 0 && color.green == 0 && color.blue == 0)
            return LaunchpadXConstants::COLOR_OFF;

        // Find closest predefined color
        if (color.red > 200 && color.green < 50 && color.blue < 50)
            return LaunchpadXConstants::COLOR_RED_FULL;
        else if (color.red < 50 && color.green > 200 && color.blue < 50)
            return LaunchpadXConstants::COLOR_GREEN_FULL;
        else if (color.red < 50 && color.green < 50 && color.blue > 200)
            return LaunchpadXConstants::COLOR_BLUE_FULL;
        else if (color.red > 200 && color.green > 200 && color.blue < 50)
            return LaunchpadXConstants::COLOR_YELLOW;
        else if (color.red < 50 && color.green > 200 && color.blue > 200)
            return LaunchpadXConstants::COLOR_CYAN;
        else if (color.red > 200 && color.green < 50 && color.blue > 200)
            return LaunchpadXConstants::COLOR_MAGENTA;
        else if (color.red > 200 && color.green > 200 && color.blue > 200)
            return LaunchpadXConstants::COLOR_WHITE;

        // Default to dim white for unrecognized colors
        return 1;
    }

    RGBColor LaunchpadX::paletteColorToRGB(uint8_t palette_color) const
    {
        // Basic palette to RGB conversion
        switch (palette_color)
        {
            case LaunchpadXConstants::COLOR_OFF: return RGBColor::OFF;
            case LaunchpadXConstants::COLOR_RED_FULL: return RGBColor::RED;
            case LaunchpadXConstants::COLOR_GREEN_FULL: return RGBColor::GREEN;
            case LaunchpadXConstants::COLOR_BLUE_FULL: return RGBColor::BLUE;
            case LaunchpadXConstants::COLOR_YELLOW: return RGBColor::YELLOW;
            case LaunchpadXConstants::COLOR_CYAN: return RGBColor::CYAN;
            case LaunchpadXConstants::COLOR_MAGENTA: return RGBColor::MAGENTA;
            case LaunchpadXConstants::COLOR_WHITE: return RGBColor::WHITE;
            default: return RGBColor(64, 64, 64); // Dim white
        }
    }

    bool LaunchpadX::setPadColor(const PadPosition& position, const RGBColor& color)
    {
        if (position.x >= LaunchpadXConstants::GRID_WIDTH ||
            position.y >= LaunchpadXConstants::GRID_HEIGHT)
        {
            return false;
        }

        uint8_t note = padPositionToNote(position);
        return setPadColor(note, color);
    }

    bool LaunchpadX::setPadColor(uint8_t pad_id, const RGBColor& color)
    {
        if (!isGridNote(pad_id))
        {
            return false;
        }

        uint8_t palette_color = rgbToPaletteColor(color);

        // Store color
        PadPosition pos = noteToPadPosition(pad_id);
        size_t index = pos.y * LaunchpadXConstants::GRID_WIDTH + pos.x;
        if (index < pad_colors_.size())
        {
            pad_colors_[index] = palette_color;
        }

        // Send MIDI note to set color
        sendNoteOn(pad_id, palette_color);

        return true;
    }

    void LaunchpadX::clearAllPads()
    {
        setAllPads(RGBColor::OFF);
    }

    void LaunchpadX::setAllPads(const RGBColor& color)
    {
        uint8_t palette_color = rgbToPaletteColor(color);

        // Set all grid pads
        for (uint8_t y = 0; y < LaunchpadXConstants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchpadXConstants::GRID_WIDTH; ++x)
            {
                PadPosition pos(x, y);
                uint8_t note = padPositionToNote(pos);
                sendNoteOn(note, palette_color);

                size_t index = y * LaunchpadXConstants::GRID_WIDTH + x;
                if (index < pad_colors_.size())
                {
                    pad_colors_[index] = palette_color;
                }
            }
        }
    }

    bool LaunchpadX::setSideButtonColor(uint8_t button_id, const RGBColor& color)
    {
        if (button_id >= 8)
        {
            return false;
        }

        uint8_t palette_color = rgbToPaletteColor(color);
        side_button_colors_[button_id] = palette_color;

        // Map button_id to MIDI note
        uint8_t note = LaunchpadXConstants::SIDE_BUTTON_1 - (button_id * 10);
        sendNoteOn(note, palette_color);

        return true;
    }

    bool LaunchpadX::setTopButtonColor(uint8_t button_id, const RGBColor& color)
    {
        if (button_id >= 8)
        {
            return false;
        }

        uint8_t palette_color = rgbToPaletteColor(color);
        top_button_colors_[button_id] = palette_color;

        // Map button_id to MIDI note
        uint8_t note = LaunchpadXConstants::TOP_BUTTON_1 + button_id;
        sendNoteOn(note, palette_color);

        return true;
    }

    bool LaunchpadX::setLogoColor(const RGBColor& color)
    {
        uint8_t palette_color = rgbToPaletteColor(color);
        sendNoteOn(LaunchpadXConstants::LOGO, palette_color);
        return true;
    }

    void LaunchpadX::enterProgrammerMode()
    {
        /**
         * @brief Enter programmer mode for full software control
         *
         * Programmer mode gives the software complete control over
         * all LEDs and device behavior. This is the default mode
         * for custom applications.
         */
        std::vector<uint8_t> cmd = {LaunchpadXConstants::CMD_SET_PROGRAMMER_MODE, 0x01};
        sendSysEx(cmd);
        programmer_mode_ = true;
        session_mode_ = false;  // Exit session mode when entering programmer mode

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "Launchpad X: Entered programmer mode" << std::endl;
        #endif
    }

    void LaunchpadX::exitProgrammerMode()
    {
        /**
         * @brief Exit programmer mode and return to DAW mode
         *
         * DAW mode returns control to the connected DAW software.
         * This is typically used when integrating with music production software.
         */
        std::vector<uint8_t> cmd = {LaunchpadXConstants::CMD_SET_DAW_MODE};
        sendSysEx(cmd);
        programmer_mode_ = false;
        session_mode_ = false;  // Ensure session mode is also disabled

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "Launchpad X: Entered DAW mode" << std::endl;
        #endif
    }

    void LaunchpadX::reset()
    {
        /**
         * @brief Reset the Launchpad X to default state
         *
         * Sends a reset command to the device and clears all local state.
         * The device will return to its default mode and all LEDs will be turned off.
         */
        std::vector<uint8_t> cmd = {LaunchpadXConstants::CMD_RESET};
        sendSysEx(cmd);

        // Clear local state
        pad_colors_.fill(LaunchpadXConstants::COLOR_OFF);
        side_button_colors_.fill(LaunchpadXConstants::COLOR_OFF);
        top_button_colors_.fill(LaunchpadXConstants::COLOR_OFF);

        // Reset mode states
        programmer_mode_ = false;
        session_mode_ = false;
        current_layout_ = LaunchpadXConstants::LAYOUT_SESSION;

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "Launchpad X: Device reset" << std::endl;
        #endif
    }

    void LaunchpadX::processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2)
    {
        uint8_t msg_type = status & 0xF0;

        switch (msg_type)
        {
            case MIDI_NOTE_ON:
            case MIDI_NOTE_OFF:
            {
                bool is_press = (msg_type == MIDI_NOTE_ON) && (data2 > 0);

                if (isGridNote(data1))
                {
                    PadPosition pos = noteToPadPosition(data1);
                    triggerPadCallback(channel, pos, data2, is_press);
                }
                else if (isSideButton(data1))
                {
                    uint8_t button_id = getSideButtonIndex(data1);
                    triggerSideButtonCallback(channel, button_id, is_press);
                }
                else if (isTopButton(data1))
                {
                    uint8_t button_id = getTopButtonIndex(data1);
                    triggerTopButtonCallback(channel, button_id, is_press);
                }
                break;
            }

            case MIDI_POLYPHONIC_AFTERTOUCH:
            {
                if (isGridNote(data1))
                {
                    PadPosition pos = noteToPadPosition(data1);
                    triggerAftertouchCallback(channel, pos, data2);
                }
                break;
            }

            case MIDI_CONTROL:
            {
                // Handle control changes if needed
                break;
            }
        }
    }

    PadPosition LaunchpadX::noteToPadPosition(uint8_t note) const
    {
        // Launchpad X grid mapping: note 11 = (0,0), note 18 = (7,0), etc.
        if (!isGridNote(note))
        {
            return PadPosition(0, 0);
        }

        // Convert note to grid position
        uint8_t row = (note - 11) / 10;
        uint8_t col = (note - 11) % 10;

        return PadPosition(col, 7 - row); // Flip Y to match visual layout
    }

    uint8_t LaunchpadX::padPositionToNote(const PadPosition& position) const
    {
        if (position.x >= LaunchpadXConstants::GRID_WIDTH ||
            position.y >= LaunchpadXConstants::GRID_HEIGHT)
        {
            return 0;
        }

        // Convert grid position to note
        uint8_t row = 7 - position.y; // Flip Y to match MIDI layout
        uint8_t col = position.x;

        return 11 + (row * 10) + col;
    }

    bool LaunchpadX::isGridNote(uint8_t note) const
    {
        // Check if note is in the 8x8 grid range
        if (note < 11 || note > 88)
        {
            return false;
        }

        uint8_t row = (note - 11) / 10;
        uint8_t col = (note - 11) % 10;

        return (row < 8) && (col < 8);
    }

    bool LaunchpadX::isSideButton(uint8_t note) const
    {
        return (note == LaunchpadXConstants::SIDE_BUTTON_1 ||
                note == LaunchpadXConstants::SIDE_BUTTON_2 ||
                note == LaunchpadXConstants::SIDE_BUTTON_3 ||
                note == LaunchpadXConstants::SIDE_BUTTON_4 ||
                note == LaunchpadXConstants::SIDE_BUTTON_5 ||
                note == LaunchpadXConstants::SIDE_BUTTON_6 ||
                note == LaunchpadXConstants::SIDE_BUTTON_7 ||
                note == LaunchpadXConstants::SIDE_BUTTON_8);
    }

    bool LaunchpadX::isTopButton(uint8_t note) const
    {
        return (note >= LaunchpadXConstants::TOP_BUTTON_1 &&
                note <= LaunchpadXConstants::TOP_BUTTON_8);
    }

    uint8_t LaunchpadX::getSideButtonIndex(uint8_t note) const
    {
        switch (note)
        {
            case LaunchpadXConstants::SIDE_BUTTON_1: return 0;
            case LaunchpadXConstants::SIDE_BUTTON_2: return 1;
            case LaunchpadXConstants::SIDE_BUTTON_3: return 2;
            case LaunchpadXConstants::SIDE_BUTTON_4: return 3;
            case LaunchpadXConstants::SIDE_BUTTON_5: return 4;
            case LaunchpadXConstants::SIDE_BUTTON_6: return 5;
            case LaunchpadXConstants::SIDE_BUTTON_7: return 6;
            case LaunchpadXConstants::SIDE_BUTTON_8: return 7;
            default: return 0;
        }
    }

    uint8_t LaunchpadX::getTopButtonIndex(uint8_t note) const
    {
        if (note >= LaunchpadXConstants::TOP_BUTTON_1 &&
            note <= LaunchpadXConstants::TOP_BUTTON_8)
        {
            return note - LaunchpadXConstants::TOP_BUTTON_1;
        }
        return 0;
    }

    // Utility functions
    void LaunchpadX::testAllPads()
    {
        std::cout << "Testing all Launchpad X pads..." << std::endl;

        // Light up all pads in sequence
        for (uint8_t y = 0; y < LaunchpadXConstants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchpadXConstants::GRID_WIDTH; ++x)
            {
                setPadColor(PadPosition(x, y), RGBColor::RED);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        clearAllPads();
    }

    void LaunchpadX::rainbowPattern()
    {
        std::cout << "Setting rainbow pattern..." << std::endl;

        for (uint8_t y = 0; y < LaunchpadXConstants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchpadXConstants::GRID_WIDTH; ++x)
            {
                // Create rainbow based on position
                float hue = ((x + y) * 45.0f) / 360.0f; // Convert to 0-1 range

                RGBColor color;
                if (hue < 0.16f) color = RGBColor::RED;
                else if (hue < 0.33f) color = RGBColor::YELLOW;
                else if (hue < 0.5f) color = RGBColor::GREEN;
                else if (hue < 0.66f) color = RGBColor::CYAN;
                else if (hue < 0.83f) color = RGBColor::BLUE;
                else color = RGBColor::MAGENTA;

                setPadColor(PadPosition(x, y), color);
            }
        }
    }

    void LaunchpadX::checkerboardPattern(const RGBColor& color1, const RGBColor& color2)
    {
        std::cout << "Setting checkerboard pattern..." << std::endl;

        for (uint8_t y = 0; y < LaunchpadXConstants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchpadXConstants::GRID_WIDTH; ++x)
            {
                bool is_even = ((x + y) % 2) == 0;
                setPadColor(PadPosition(x, y), is_even ? color1 : color2);
            }
        }
    }

    void LaunchpadX::setLayout(uint8_t layout)
    {
        /**
         * @brief Set the layout for session mode
         * @param layout Layout ID (LAYOUT_SESSION, LAYOUT_DRUM_RACK, etc.)
         *
         * Changes the layout used in session mode. This affects how
         * the pads and buttons behave when in session mode.
         */
        if (layout <= LaunchpadXConstants::LAYOUT_USER)
        {
            current_layout_ = layout;

            // If currently in session mode, apply the layout immediately
            if (session_mode_)
            {
                std::vector<uint8_t> cmd = {LaunchpadXConstants::CMD_SET_SESSION_MODE, layout};
                sendSysEx(cmd);
            }

            #ifdef DEBUG_LAUNCHPAD
            std::cout << "Launchpad X: Layout set to " << static_cast<int>(layout) << std::endl;
            #endif
        }
    }

    void LaunchpadX::enableLiveMode()
    {
        exitProgrammerMode();
    }

    void LaunchpadX::enableProgrammerMode()
    {
        enterProgrammerMode();
    }

    void LaunchpadX::enableSessionMode()
    {
        enterSessionMode();
    }

    void LaunchpadX::enterSessionMode()
    {
        /**
         * @brief Enter session mode for standalone operation
         *
         * Session mode allows the Launchpad X to operate independently
         * without constant software control. In this mode:
         * - Pads can trigger clips or samples
         * - Side buttons control scene launching
         * - Top buttons control track operations
         * - Device operates with default layout and behaviors
         */
        std::vector<uint8_t> cmd = {LaunchpadXConstants::CMD_SET_SESSION_MODE, current_layout_};
        sendSysEx(cmd);

        session_mode_ = true;
        programmer_mode_ = false;

        #ifdef DEBUG_LAUNCHPAD
        std::cout << "Launchpad X: Entered session mode with layout " << static_cast<int>(current_layout_) << std::endl;
        #endif
    }

    void LaunchpadX::exitSessionMode()
    {
        /**
         * @brief Exit session mode and return to programmer mode
         *
         * Exits session mode and returns the device to programmer mode
         * for full software control over LEDs and behaviors.
         */
        if (session_mode_)
        {
            enterProgrammerMode();
            session_mode_ = false;

            #ifdef DEBUG_LAUNCHPAD
            std::cout << "Launchpad X: Exited session mode" << std::endl;
            #endif
        }
    }

    bool LaunchpadX::isSessionMode() const
    {
        return session_mode_;
    }

    void LaunchpadX::setPattern(const std::vector<std::vector<RGBColor>>& pattern)
    {
        if (pattern.size() != LaunchpadXConstants::GRID_HEIGHT)
        {
            return;
        }

        for (uint8_t y = 0; y < LaunchpadXConstants::GRID_HEIGHT; ++y)
        {
            if (pattern[y].size() != LaunchpadXConstants::GRID_WIDTH)
            {
                continue;
            }

            for (uint8_t x = 0; x < LaunchpadXConstants::GRID_WIDTH; ++x)
            {
                setPadColor(PadPosition(x, y), pattern[y][x]);
            }
        }
    }

    void LaunchpadX::flashPad(const PadPosition& position, const RGBColor& color, uint16_t duration_ms)
    {
        RGBColor original_color = paletteColorToRGB(pad_colors_[position.y * LaunchpadXConstants::GRID_WIDTH + position.x]);

        setPadColor(position, color);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        setPadColor(position, original_color);
    }

    void LaunchpadX::pulsePad(const PadPosition& position, const RGBColor& color)
    {
        // Simple pulse effect - flash twice
        flashPad(position, color, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        flashPad(position, color, 100);
    }

    // Factory function
    std::unique_ptr<MidiDevice> createLaunchpadX(const std::string& device_name)
    {
        (void)device_name; // Device name not used in constructor
        return std::make_unique<LaunchpadX>();
    }

} // namespace synth
