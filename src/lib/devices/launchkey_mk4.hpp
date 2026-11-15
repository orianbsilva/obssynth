/**
 * @file launchkey_mk4.hpp
 * @brief Novation Launchkey MK4 MIDI controller device implementation
 *
 * This file provides support for the Novation Launchkey MK4 series MIDI controllers.
 * The Launchkey MK4 features velocity-sensitive RGB drum pads, faders, rotary encoders,
 * and comprehensive DAW integration capabilities.
 */

#pragma once

#include "midi_device.hpp"
#include <cstdint>
#include <array>
#include <memory>

namespace synth
{
    // Forward declaration
    class MidiInterface;

    // Launchkey MK4 specific constants
    namespace LaunchkeyMK4Constants
    {
        // Drum pad grid dimensions
        constexpr uint8_t GRID_WIDTH = 4;
        constexpr uint8_t GRID_HEIGHT = 4;
        constexpr uint8_t TOTAL_PADS = GRID_WIDTH * GRID_HEIGHT;

        // MIDI note ranges for drum pads
        constexpr uint8_t PAD_NOTE_MIN = 36; // C2 - bottom-left pad
        constexpr uint8_t PAD_NOTE_MAX = 51; // D#3 - top-right pad

        // Control Change (CC) numbers for faders and knobs
        constexpr uint8_t FADER_CC_START = 41; // Faders 1-8: CC 41-48
        constexpr uint8_t KNOB_CC_START = 21;  // Knobs 1-8: CC 21-28

        // Transport and navigation controls
        constexpr uint8_t CC_PLAY = 115;
        constexpr uint8_t CC_STOP = 116;
        constexpr uint8_t CC_RECORD = 117;
        constexpr uint8_t CC_LOOP = 118;
        constexpr uint8_t CC_REWIND = 119;
        constexpr uint8_t CC_FAST_FORWARD = 120;

        // Navigation controls
        constexpr uint8_t CC_UP = 106;
        constexpr uint8_t CC_DOWN = 107;
        constexpr uint8_t CC_LEFT = 108;
        constexpr uint8_t CC_RIGHT = 109;

        // Scene launch buttons
        constexpr uint8_t CC_SCENE_1 = 112;
        constexpr uint8_t CC_SCENE_2 = 113;

        // Mode buttons
        constexpr uint8_t CC_SHIFT = 108;
        constexpr uint8_t CC_CLICK = 109;
        constexpr uint8_t CC_UNDO = 110;

        // SysEx commands for advanced features
        constexpr uint8_t SYSEX_HEADER[] = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0F};
        constexpr uint8_t SYSEX_FOOTER = 0xF7;

        // Mode commands
        constexpr uint8_t CMD_SET_PAD_MODE = 0x01;
        constexpr uint8_t CMD_SET_LED = 0x02;
        constexpr uint8_t CMD_SET_DAW_MODE = 0x03;
        constexpr uint8_t CMD_RESET = 0x04;

        // Query commands for probing device state
        constexpr uint8_t CMD_GET_FADER_VALUES = 0x05;
        constexpr uint8_t CMD_GET_KNOB_VALUES = 0x06;
        constexpr uint8_t CMD_GET_DEVICE_STATE = 0x07;
        constexpr uint8_t CMD_GET_ALL_VALUES = 0x08;

        // Pad modes
        constexpr uint8_t PAD_MODE_DRUM = 0x00;
        constexpr uint8_t PAD_MODE_SESSION = 0x01;
        constexpr uint8_t PAD_MODE_CUSTOM = 0x02;

        // Color palette (simplified RGB mapping)
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
        constexpr uint8_t COLOR_WHITE = 127;

        // Number of faders and knobs
        constexpr uint8_t NUM_FADERS = 8;
        constexpr uint8_t NUM_KNOBS = 8;
    }

    /**
     * @brief Novation Launchkey MK4 MIDI controller device implementation
     *
     * This class provides comprehensive support for the Launchkey MK4 series,
     * including 16 velocity-sensitive RGB drum pads, 8 faders, 8 rotary encoders,
     * transport controls, and DAW integration features.
     */
    class LaunchkeyMK4 : public MidiDevice
    {
    private:
        std::shared_ptr<MidiInterface> midi_reader_;
        std::array<uint8_t, LaunchkeyMK4Constants::TOTAL_PADS> pad_colors_;
        std::array<uint8_t, LaunchkeyMK4Constants::NUM_FADERS> fader_values_;
        std::array<uint8_t, LaunchkeyMK4Constants::NUM_KNOBS> knob_values_;
        bool daw_mode_;
        uint8_t current_pad_mode_;

        // MIDI output for sending commands
        void sendSysEx(const std::vector<uint8_t> &data);
        void sendNoteOn(uint8_t note, uint8_t velocity);
        void sendNoteOff(uint8_t note);
        void sendControlChange(uint8_t controller, uint8_t value);

        // SysEx response processing
        void processSysExResponse(const std::vector<uint8_t> &data);
        void onSysExReceived(const std::vector<uint8_t> &data);

        // Color conversion
        uint8_t rgbToPaletteColor(const RGBColor &color) const;
        RGBColor paletteColorToRGB(uint8_t palette_color) const;

        // Note mapping utilities
        bool isPadNote(uint8_t note) const;
        bool isTransportControl(uint8_t cc) const;
        bool isNavigationControl(uint8_t cc) const;
        bool isFaderControl(uint8_t cc) const;
        bool isKnobControl(uint8_t cc) const;

    public:
        LaunchkeyMK4();
        virtual ~LaunchkeyMK4();

        // MidiDevice interface implementation
        bool connect(uint8_t device_id) override;
        void disconnect() override;

        // Grid control (drum pads)
        bool setPadColor(const PadPosition &position, const RGBColor &color) override;
        bool setPadColor(uint8_t pad_id, const RGBColor &color) override;
        void clearAllPads() override;
        void setAllPads(const RGBColor &color) override;

        // Side/top button control (not applicable for Launchkey)
        bool setSideButtonColor(uint8_t button_id, const RGBColor &color) override;
        bool setTopButtonColor(uint8_t button_id, const RGBColor &color) override;

        // Device-specific features
        void enterProgrammerMode() override;
        void exitProgrammerMode() override;
        void reset() override;

        // MIDI message processing
        void processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2) override;

        // Grid utilities
        PadPosition noteToPadPosition(uint8_t note) const override;
        uint8_t padPositionToNote(const PadPosition &position) const override;
        uint8_t getGridWidth() const override { return LaunchkeyMK4Constants::GRID_WIDTH; }
        uint8_t getGridHeight() const override { return LaunchkeyMK4Constants::GRID_HEIGHT; }

        // Launchkey MK4 specific features

        /**
         * @brief Set the pad mode for drum pads
         * @param mode Pad mode (PAD_MODE_DRUM, PAD_MODE_SESSION, PAD_MODE_CUSTOM)
         */
        void setPadMode(uint8_t mode);

        /**
         * @brief Enable DAW mode for enhanced integration
         */
        void enableDAWMode();

        /**
         * @brief Disable DAW mode and return to standalone operation
         */
        void disableDAWMode();

        /**
         * @brief Check if device is currently in DAW mode
         * @return True if in DAW mode, false otherwise
         */
        bool isDAWMode() const;

        /**
         * @brief Get the current value of a fader
         * @param fader_id Fader index (0-7)
         * @return Current fader value (0-127)
         */
        uint8_t getFaderValue(uint8_t fader_id) const;

        /**
         * @brief Get the current value of a knob
         * @param knob_id Knob index (0-7)
         * @return Current knob value (0-127)
         */
        uint8_t getKnobValue(uint8_t knob_id) const;

        /**
         * @brief Set all faders to specified values
         * @param values Array of 8 fader values (0-127)
         */
        void setFaderValues(const std::array<uint8_t, LaunchkeyMK4Constants::NUM_FADERS> &values);

        /**
         * @brief Set all knobs to specified values
         * @param values Array of 8 knob values (0-127)
         */
        void setKnobValues(const std::array<uint8_t, LaunchkeyMK4Constants::NUM_KNOBS> &values);

        // Device state probing methods

        /**
         * @brief Request current fader values from the device
         * This sends a SysEx query to the device to get current fader positions
         */
        void requestFaderValues();

        /**
         * @brief Request current knob values from the device
         * This sends a SysEx query to the device to get current knob positions
         */
        void requestKnobValues();

        /**
         * @brief Request complete device state (faders, knobs, pad mode, etc.)
         * This sends a SysEx query to get all current device state information
         */
        void requestDeviceState();

        /**
         * @brief Request all control values in a single query
         * This sends a SysEx query to get both fader and knob values efficiently
         */
        void requestAllValues();

        // Pattern and animation helpers
        void flashPad(const PadPosition &position, const RGBColor &color, uint16_t duration_ms);
        void pulsePad(const PadPosition &position, const RGBColor &color);

        // Utility functions
        void testAllPads();
        void rainbowPattern();
        void checkerboardPattern(const RGBColor &color1, const RGBColor &color2);
    };

    // Factory function declaration (to be used by DeviceFactory)
    std::unique_ptr<MidiDevice> createLaunchkeyMK4(const std::string &device_name);

} // namespace synth
