/**
 * @file apc_key25_mk2.hpp
 * @brief APC Key 25 mk2 MIDI Controller Device Implementation
 *
 * Implementation for Akai APC Key 25 mk2 USB MIDI controller.
 * Supports 25-key keyboard, 8 velocity-sensitive RGB pads,
 * faders, rotary encoders, and various control buttons.
 *
 * DEVICE SPECIFICATIONS:
 * - 25-key mini keyboard (C to C, 2 octaves)
 * - 8 velocity-sensitive RGB pads (2x4 grid)
 * - 8 rotary encoders with LED rings
 * - 8 channel faders
 * - Transport controls (play, stop, record, etc.)
 * - Scene launch buttons
 * - Shift button for secondary functions
 *
 * MIDI COMMUNICATION:
 * - Standard MIDI Note On/Off for keys and pads
 * - Control Change messages for faders and encoders
 * - LED control via Note On velocity values
 * - RGB pad colors via velocity mapping
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

    // APC Key 25 mk2 specific constants
    namespace APCKey25Mk2Constants
    {
        // Device identification
        constexpr const char* DEVICE_NAME = "APC Key 25 mk2";

        // Keyboard specifications
        constexpr uint8_t KEYBOARD_KEYS = 25;
        constexpr uint8_t KEYBOARD_START_NOTE = 48; // C3
        constexpr uint8_t KEYBOARD_END_NOTE = 72;   // C5

        // Pad grid specifications
        constexpr uint8_t PAD_COLUMNS = 2;
        constexpr uint8_t PAD_ROWS = 4;
        constexpr uint8_t TOTAL_PADS = PAD_COLUMNS * PAD_ROWS; // 8 pads

        // Pad MIDI note mapping (bottom-left to top-right)
        constexpr uint8_t PAD_NOTES[TOTAL_PADS] = {
            // Column 1 (left)
            40, // Pad 1 (bottom-left)
            41, // Pad 2
            42, // Pad 3
            43, // Pad 4 (top-left)
            // Column 2 (right)
            44, // Pad 5 (bottom-right)
            45, // Pad 6
            46, // Pad 7
            47  // Pad 8 (top-right)
        };

        // Control elements
        constexpr uint8_t NUM_FADERS = 8;
        constexpr uint8_t NUM_ENCODERS = 8;
        constexpr uint8_t NUM_SCENE_BUTTONS = 5;

        // MIDI CC mappings for faders (channel faders)
        constexpr uint8_t FADER_CC_START = 48;
        constexpr uint8_t FADER_CC_END = 55;

        // MIDI CC mappings for rotary encoders
        constexpr uint8_t ENCODER_CC_START = 56;
        constexpr uint8_t ENCODER_CC_END = 63;

        // Transport control notes
        constexpr uint8_t TRANSPORT_PLAY = 91;
        constexpr uint8_t TRANSPORT_STOP = 92;
        constexpr uint8_t TRANSPORT_RECORD = 93;
        constexpr uint8_t TRANSPORT_SHIFT = 98;

        // Scene launch button notes
        constexpr uint8_t SCENE_BUTTON_1 = 82; // Scene 1
        constexpr uint8_t SCENE_BUTTON_2 = 83; // Scene 2
        constexpr uint8_t SCENE_BUTTON_3 = 84; // Scene 3
        constexpr uint8_t SCENE_BUTTON_4 = 85; // Scene 4
        constexpr uint8_t SCENE_BUTTON_5 = 86; // Scene 5

        // LED Control via Note On velocity
        // Pad RGB colors (velocity values for Note On to pad notes)
        struct PadColor
        {
            static constexpr uint8_t OFF = 0;
            static constexpr uint8_t RED_LOW = 1;
            static constexpr uint8_t RED_FULL = 3;
            static constexpr uint8_t GREEN_LOW = 16;
            static constexpr uint8_t GREEN_FULL = 21;
            static constexpr uint8_t BLUE_LOW = 37;
            static constexpr uint8_t BLUE_FULL = 45;
            static constexpr uint8_t YELLOW = 13;
            static constexpr uint8_t CYAN = 33;
            static constexpr uint8_t MAGENTA = 53;
            static constexpr uint8_t WHITE = 118;
            static constexpr uint8_t ORANGE = 9;
            static constexpr uint8_t PURPLE = 49;
            static constexpr uint8_t PINK = 57;
        };

        // Button LED states (for transport and scene buttons)
        struct ButtonLED
        {
            static constexpr uint8_t OFF = 0;
            static constexpr uint8_t ON = 127;
            static constexpr uint8_t BLINK = 64;
        };

        // MIDI Channel
        constexpr uint8_t MIDI_CHANNEL = 0; // Channel 1 (0-indexed)
    }

    /**
     * @brief APC Key 25 mk2 MIDI Controller Device
     *
     * Provides interface for controlling and receiving events from
     * the Akai APC Key 25 mk2 USB MIDI controller. Supports keyboard,
     * velocity-sensitive RGB pads, faders, encoders, and transport controls.
     */
    class APCKey25Mk2 : public MidiDevice
    {
    private:
        std::shared_ptr<MidiInterface> midi_reader_;

        // LED state tracking
        std::array<uint8_t, APCKey25Mk2Constants::TOTAL_PADS> pad_colors_;
        std::array<uint8_t, APCKey25Mk2Constants::NUM_SCENE_BUTTONS> scene_button_states_;
        std::array<uint8_t, 3> transport_button_states_; // play, stop, record

        // Current control values
        std::array<uint8_t, APCKey25Mk2Constants::NUM_FADERS> fader_values_;
        std::array<uint8_t, APCKey25Mk2Constants::NUM_ENCODERS> encoder_values_;

        // Shift state
        bool shift_pressed_;

        // MIDI output for sending LED commands
        void sendNoteOn(uint8_t note, uint8_t velocity);
        void sendNoteOff(uint8_t note);
        void sendControlChange(uint8_t controller, uint8_t value);

        // Internal helper methods
        bool isKeyboardNote(uint8_t note) const;
        bool isPadNote(uint8_t note) const;
        bool isTransportNote(uint8_t note) const;
        bool isSceneNote(uint8_t note) const;
        uint8_t rgbToVelocity(const RGBColor& color) const;

        // Event processing
        void processNoteEvent(uint8_t channel, uint8_t note, uint8_t velocity, bool is_press);
        void processControlChange(uint8_t channel, uint8_t controller, uint8_t value);

    public:
        /**
         * @brief Construct a new APC Key 25 mk2 device
         */
        APCKey25Mk2();

        /**
         * @brief Destructor
         */
        ~APCKey25Mk2() override;

        // MidiDevice interface implementation
        bool connect(uint8_t device_id) override;
        void disconnect() override;
        bool setPadColor(const PadPosition& position, const RGBColor& color) override;
        bool setPadColor(uint8_t pad_id, const RGBColor& color) override;
        void clearAllPads() override;
        void setAllPads(const RGBColor& color) override;
        bool setSideButtonColor(uint8_t button_id, const RGBColor& color) override;
        bool setTopButtonColor(uint8_t button_id, const RGBColor& color) override;
        void enterProgrammerMode() override;
        void exitProgrammerMode() override;
        void reset() override;
        void processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2) override;
        PadPosition noteToPadPosition(uint8_t note) const override;
        uint8_t padPositionToNote(const PadPosition& position) const override;
        uint8_t getGridWidth() const override { return APCKey25Mk2Constants::PAD_COLUMNS; }
        uint8_t getGridHeight() const override { return APCKey25Mk2Constants::PAD_ROWS; }

        // APC Key 25 mk2 specific methods
        void setTransportButtonLED(uint8_t button, uint8_t state);

        /**
         * @brief Set scene button LED state
         * @param scene_index Scene button index (0-4)
         * @param state LED state (off/on/blink)
         */
        void setSceneButtonLED(uint8_t scene_index, uint8_t state);

        /**
         * @brief Test all pads with cycling colors
         */
        void testAllPads();

        /**
         * @brief Create rainbow pattern on pads
         */
        void rainbowPattern();

        /**
         * @brief Flash a specific pad
         * @param pad_index Pad index to flash
         * @param color Color to flash
         * @param duration_ms Flash duration in milliseconds
         */
        void flashPad(uint8_t pad_index, const RGBColor& color, uint32_t duration_ms = 200);

        /**
         * @brief Get current fader value
         * @param fader_index Fader index (0-7)
         * @return Current fader value (0-127)
         */
        uint8_t getFaderValue(uint8_t fader_index) const;

        /**
         * @brief Get current encoder value
         * @param encoder_index Encoder index (0-7)
         * @return Current encoder value (0-127)
         */
        uint8_t getEncoderValue(uint8_t encoder_index) const;

        /**
         * @brief Check if shift button is currently pressed
         * @return True if shift is pressed
         */
        bool isShiftPressed() const { return shift_pressed_; }

        // Callback setters for device-specific events
        using KeyboardCallback = std::function<void(uint8_t channel, uint8_t note, uint8_t velocity, bool is_press)>;
        using FaderCallback = std::function<void(uint8_t channel, uint8_t fader_index, uint8_t value)>;
        using EncoderCallback = std::function<void(uint8_t channel, uint8_t encoder_index, uint8_t value)>;
        using TransportCallback = std::function<void(uint8_t channel, uint8_t button, bool is_press)>;
        using SceneCallback = std::function<void(uint8_t channel, uint8_t scene_index, bool is_press)>;

        /**
         * @brief Set keyboard event callback
         * @param callback Function to call on keyboard events
         */
        void setKeyboardCallback(KeyboardCallback callback) { keyboard_callback_ = callback; }

        /**
         * @brief Set fader change callback
         * @param callback Function to call on fader changes
         */
        void setFaderCallback(FaderCallback callback) { fader_callback_ = callback; }

        /**
         * @brief Set encoder change callback
         * @param callback Function to call on encoder changes
         */
        void setEncoderCallback(EncoderCallback callback) { encoder_callback_ = callback; }

        /**
         * @brief Set transport button callback
         * @param callback Function to call on transport button events
         */
        void setTransportCallback(TransportCallback callback) { transport_callback_ = callback; }

        /**
         * @brief Set scene button callback
         * @param callback Function to call on scene button events
         */
        void setSceneCallback(SceneCallback callback) { scene_callback_ = callback; }

    private:
        // Event callbacks
        KeyboardCallback keyboard_callback_;
        FaderCallback fader_callback_;
        EncoderCallback encoder_callback_;
        TransportCallback transport_callback_;
        SceneCallback scene_callback_;
    };

    /**
     * @brief Factory function to create APC Key 25 mk2 device
     * @param device_name Device name (unused for this device)
     * @return Unique pointer to APC Key 25 mk2 device
     */
    std::unique_ptr<MidiDevice> createAPCKey25Mk2(const std::string& device_name = "");

} // namespace synth
