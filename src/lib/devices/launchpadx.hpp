#pragma once

#include "midi_device.hpp"
#include <cstdint>
#include <array>
#include <memory>

namespace synth
{
    // Forward declaration
    class MidiInterface;

    // Launchpad X specific constants
    namespace LaunchpadXConstants
    {
        // Grid dimensions
        constexpr uint8_t GRID_WIDTH = 8;
        constexpr uint8_t GRID_HEIGHT = 8;
        constexpr uint8_t TOTAL_PADS = GRID_WIDTH * GRID_HEIGHT;

        // MIDI note ranges
        constexpr uint8_t GRID_NOTE_MIN = 11;  // Bottom-left pad
        constexpr uint8_t GRID_NOTE_MAX = 89;  // Top-right pad

        // Side button notes (right side)
        constexpr uint8_t SIDE_BUTTON_1 = 89;  // Top
        constexpr uint8_t SIDE_BUTTON_2 = 79;
        constexpr uint8_t SIDE_BUTTON_3 = 69;
        constexpr uint8_t SIDE_BUTTON_4 = 59;
        constexpr uint8_t SIDE_BUTTON_5 = 49;
        constexpr uint8_t SIDE_BUTTON_6 = 39;
        constexpr uint8_t SIDE_BUTTON_7 = 29;
        constexpr uint8_t SIDE_BUTTON_8 = 19;  // Bottom

        // Top button notes
        constexpr uint8_t TOP_BUTTON_1 = 91;  // Left
        constexpr uint8_t TOP_BUTTON_2 = 92;
        constexpr uint8_t TOP_BUTTON_3 = 93;
        constexpr uint8_t TOP_BUTTON_4 = 94;
        constexpr uint8_t TOP_BUTTON_5 = 95;
        constexpr uint8_t TOP_BUTTON_6 = 96;
        constexpr uint8_t TOP_BUTTON_7 = 97;
        constexpr uint8_t TOP_BUTTON_8 = 98;  // Right

        // Logo button
        constexpr uint8_t LOGO = 99;

        // SysEx commands
        constexpr uint8_t SYSEX_HEADER[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0C};
        constexpr uint8_t SYSEX_FOOTER = 0xF7;

        // Mode commands
        constexpr uint8_t CMD_SET_PROGRAMMER_MODE = 0x0E;
        constexpr uint8_t CMD_SET_DAW_MODE = 0x10;
        constexpr uint8_t CMD_SET_SESSION_MODE = 0x00; // Session/Standalone mode
        constexpr uint8_t CMD_SET_LED = 0x03;
        constexpr uint8_t CMD_SET_ALL_LEDS = 0x0E;
        constexpr uint8_t CMD_RESET = 0x0A;

        // Session mode layouts
        constexpr uint8_t LAYOUT_SESSION = 0x00;      // Standard session layout
        constexpr uint8_t LAYOUT_DRUM_RACK = 0x01;    // Drum rack layout
        constexpr uint8_t LAYOUT_CHROMATIC = 0x02;    // Chromatic note layout
        constexpr uint8_t LAYOUT_USER = 0x03;         // User custom layout

        // Color palette (127 predefined colors)
        constexpr uint8_t COLOR_OFF = 0;
        constexpr uint8_t COLOR_RED_LOW = 1;
        constexpr uint8_t COLOR_RED_FULL = 3;
        constexpr uint8_t COLOR_GREEN_LOW = 16;
        constexpr uint8_t COLOR_GREEN_FULL = 21;
        constexpr uint8_t COLOR_BLUE_LOW = 37;
        constexpr uint8_t COLOR_BLUE_FULL = 45;
        constexpr uint8_t COLOR_YELLOW = 13;
        constexpr uint8_t COLOR_CYAN = 33;
        constexpr uint8_t COLOR_MAGENTA = 53;
        constexpr uint8_t COLOR_WHITE = 118;
    }

    class LaunchpadX : public MidiDevice
    {
    private:
        std::shared_ptr<MidiInterface> midi_reader_;
        std::array<uint8_t, LaunchpadXConstants::TOTAL_PADS> pad_colors_;
        std::array<uint8_t, 8> side_button_colors_;
        std::array<uint8_t, 8> top_button_colors_;
        bool programmer_mode_;
        bool session_mode_;
        uint8_t current_layout_;

        // MIDI output for sending commands
        void sendSysEx(const std::vector<uint8_t>& data);
        void sendNoteOn(uint8_t note, uint8_t velocity);
        void sendNoteOff(uint8_t note);
        void sendControlChange(uint8_t controller, uint8_t value);

        // Color conversion
        uint8_t rgbToPaletteColor(const RGBColor& color) const;
        RGBColor paletteColorToRGB(uint8_t palette_color) const;

        // Note mapping utilities
        bool isGridNote(uint8_t note) const;
        bool isSideButton(uint8_t note) const;
        bool isTopButton(uint8_t note) const;
        uint8_t getSideButtonIndex(uint8_t note) const;
        uint8_t getTopButtonIndex(uint8_t note) const;

    public:
        LaunchpadX();
        virtual ~LaunchpadX();

        // MidiDevice interface implementation
        bool connect(uint8_t device_id) override;
        void disconnect() override;

        // Grid control
        bool setPadColor(const PadPosition& position, const RGBColor& color) override;
        bool setPadColor(uint8_t pad_id, const RGBColor& color) override;
        void clearAllPads() override;
        void setAllPads(const RGBColor& color) override;

        // Side/top button control
        bool setSideButtonColor(uint8_t button_id, const RGBColor& color) override;
        bool setTopButtonColor(uint8_t button_id, const RGBColor& color) override;

        // Device-specific features
        void enterProgrammerMode() override;
        void exitProgrammerMode() override;
        void reset() override;

        // MIDI message processing
        void processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2) override;

        // Grid utilities
        PadPosition noteToPadPosition(uint8_t note) const override;
        uint8_t padPositionToNote(const PadPosition& position) const override;
        uint8_t getGridWidth() const override { return LaunchpadXConstants::GRID_WIDTH; }
        uint8_t getGridHeight() const override { return LaunchpadXConstants::GRID_HEIGHT; }

        // Launchpad X specific features
        /**
         * @brief Set the layout for session mode
         * @param layout Layout ID (LAYOUT_SESSION, LAYOUT_DRUM_RACK, LAYOUT_CHROMATIC, LAYOUT_USER)
         */
        void setLayout(uint8_t layout);

        /**
         * @brief Enable live mode (alias for exitProgrammerMode)
         */
        void enableLiveMode();

        /**
         * @brief Enable programmer mode (alias for enterProgrammerMode)
         */
        void enableProgrammerMode();

        /**
         * @brief Enable session mode for standalone operation
         */
        void enableSessionMode();

        /**
         * @brief Enter session mode with current layout
         */
        void enterSessionMode();

        /**
         * @brief Exit session mode and return to programmer mode
         */
        void exitSessionMode();

        /**
         * @brief Check if device is currently in session mode
         * @return True if in session mode, false otherwise
         */
        bool isSessionMode() const;

        // Pattern and animation helpers
        void setPattern(const std::vector<std::vector<RGBColor>>& pattern);
        void flashPad(const PadPosition& position, const RGBColor& color, uint16_t duration_ms);
        void pulsePad(const PadPosition& position, const RGBColor& color);

        // Utility functions
        void testAllPads();
        void rainbowPattern();
        void checkerboardPattern(const RGBColor& color1, const RGBColor& color2);

        bool setLogoColor(const RGBColor& color);
    };

    // Factory function declaration (to be used by DeviceFactory)
    std::unique_ptr<MidiDevice> createLaunchpadX(const std::string& device_name);

} // namespace synth
