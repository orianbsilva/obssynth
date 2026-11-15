/**
 * @file apc_key25_mk1.hpp
 * @brief APC Key 25 mk1 MIDI Controller Device Implementation
 *
 * Implementation for Akai APC Key 25 mk1 USB MIDI controller.
 * Supports 25-key keyboard, 40 velocity-sensitive grid pads (5x8),
 * faders, rotary encoders, and various control buttons.
 *
 * DEVICE SPECIFICATIONS:
 * - 25-key mini keyboard (C to C, 2 octaves)
 * - 40 velocity-sensitive grid pads (5x8 grid)
 * - 8 rotary encoders with LED rings
 * - 8 channel faders
 * - Transport controls (play, stop, record, etc.)
 * - Scene launch buttons
 * - Shift button for secondary functions
 *
 * MIDI COMMUNICATION:
 * - Channel 0: Pads, clips, transport, scene buttons, faders, encoders
 * - Channel 1: 25-key keyboard only
 * - Standard MIDI Note On/Off for keys and pads
 * - Control Change messages for faders and encoders
 * - LED control via Note On velocity values
 * - Shift functionality: When pressed, notes 64-67 behave as navigation buttons
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
    namespace APCKey25Mk1Constants
    {
        // Device identification
        constexpr const char* DEVICE_NAME = "APC Key 25";

        // Keyboard specifications
        constexpr uint8_t KEYBOARD_KEYS = 25;
        constexpr uint8_t KEYBOARD_START_NOTE = 48; // C3
        constexpr uint8_t KEYBOARD_END_NOTE = 72;   // C5

        // Grid pad specifications (5x8 grid = 40 pads total)
        constexpr uint8_t PAD_COLUMNS = 8;
        constexpr uint8_t PAD_ROWS = 5;
        constexpr uint8_t TOTAL_PADS = 40; // Notes 0-39 are grid pads

        // Grid pad MIDI note mapping (notes 0-39)
        constexpr uint8_t GRID_PAD_START_NOTE = 0;
        constexpr uint8_t GRID_PAD_END_NOTE = 39;

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

        // Shift key (CC 64)
        constexpr uint8_t SHIFT_CC = 64;

        // Navigation arrow keys (notes 64-67)
        // NOTE: When SHIFT is pressed, notes 64-67 behave as navigation buttons
        constexpr uint8_t NAV_UP = 64;
        constexpr uint8_t NAV_DOWN = 65;
        constexpr uint8_t NAV_LEFT = 66;
        constexpr uint8_t NAV_RIGHT = 67;

        // Function keys (notes 68-71)
        // NOTE: Function keys are disabled when SHIFT is pressed
        constexpr uint8_t FUNC_VOLUME = 68;
        constexpr uint8_t FUNC_PAN = 69;
        constexpr uint8_t FUNC_SEND = 70;
        constexpr uint8_t FUNC_DEVICE = 71;

        // Transport control notes
        constexpr uint8_t TRANSPORT_PLAY = 91;
        constexpr uint8_t TRANSPORT_STOP = 92;
        constexpr uint8_t TRANSPORT_RECORD = 93;
        constexpr uint8_t TRANSPORT_SHIFT = 98;  // Legacy shift button
        constexpr uint8_t STOP_ALL_CLIPS = 81;

        // Scene launch button notes
        constexpr uint8_t SCENE_BUTTON_1 = 82; // Scene 1
        constexpr uint8_t SCENE_BUTTON_2 = 83; // Scene 2
        constexpr uint8_t SCENE_BUTTON_3 = 84; // Scene 3
        constexpr uint8_t SCENE_BUTTON_4 = 85; // Scene 4
        constexpr uint8_t SCENE_BUTTON_5 = 86; // Scene 5

        // LED Control via Note On velocity - APC Key 25 Mk1 predefined colors
        // Based on official APC color scheme from Ableton Remote Scripts
        enum class PadColor : uint8_t
        {
            OFF = 0,          // Pad off
            GREEN = 1,        // Green solid
            GREEN_BLINK = 2,  // Green blinking
            RED = 3,          // Red solid
            RED_BLINK = 4,    // Red blinking
            AMBER = 5,        // Amber/Yellow solid
            AMBER_BLINK = 6   // Amber/Yellow blinking
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
     * @brief APC Key 25 mk1 MIDI Controller Device
     *
     * Provides interface for controlling and receiving events from
     * the Akai APC Key 25 mk1 USB MIDI controller. Supports keyboard,
     * 40 velocity-sensitive grid pads, faders, encoders, and transport controls.
     *
     * Channel Configuration:
     * - Channel 0: Pads, clips, transport, scene buttons, faders, encoders
     * - Channel 1: 25-key keyboard only
     *
     * Shift Functionality:
     * - When shift is pressed, notes 64-67 behave as navigation buttons
     * - Function buttons (68-71) are disabled when shift is pressed
     */
    class APCKey25Mk1 : public MidiDevice
    {
    private:
        std::shared_ptr<MidiInterface> midi_reader_;

        // LED state tracking
        std::array<uint8_t, APCKey25Mk1Constants::TOTAL_PADS> pad_colors_;
        std::array<uint8_t, APCKey25Mk1Constants::NUM_SCENE_BUTTONS> scene_button_states_;
        std::array<uint8_t, 3> transport_button_states_; // play, stop, record

        // Current control values
        std::array<uint8_t, APCKey25Mk1Constants::NUM_FADERS> fader_values_;
        std::array<uint8_t, APCKey25Mk1Constants::NUM_ENCODERS> encoder_values_;

        // Navigation and function button states
        std::array<bool, 4> nav_button_states_; // up, down, left, right
        std::array<bool, 4> func_button_states_; // volume, pan, send, device

        // Shift state (both CC64 and note 98)
        bool shift_pressed_;

        // MIDI output for sending LED commands
        void sendNoteOn(uint8_t note, uint8_t velocity);
        void sendNoteOff(uint8_t note);
        void sendControlChange(uint8_t controller, uint8_t value);

        // Internal helper methods
        bool isKeyboardNote(uint8_t note) const;
        bool isPadNote(uint8_t note) const;
        bool isGridPadNote(uint8_t note) const;
        bool isNavigationNote(uint8_t note) const;
        bool isFunctionNote(uint8_t note) const;
        bool isTransportNote(uint8_t note) const;
        bool isSceneNote(uint8_t note) const;
        APCKey25Mk1Constants::PadColor rgbToEnum(const RGBColor& color) const;

        // Event processing
        void processNoteEvent(uint8_t channel, uint8_t note, uint8_t velocity, bool is_press);
        void processControlChange(uint8_t channel, uint8_t controller, uint8_t value);

    public:
        /**
         * @brief Construct a new APC Key 25 mk1 device
         */
        APCKey25Mk1();

        /**
         * @brief Destructor
         */
        ~APCKey25Mk1() override;

        // MidiDevice interface implementation
        bool connect(uint8_t device_id) override;
        void disconnect() override;

        // RGB interface for base class compatibility
        bool setPadColor(const PadPosition& position, const RGBColor& color) override;
        bool setPadColor(uint8_t pad_id, const RGBColor& color) override;
        void setAllPads(const RGBColor& color) override;
        bool setSideButtonColor(uint8_t button_id, const RGBColor& color) override;
        bool setTopButtonColor(uint8_t button_id, const RGBColor& color) override;

        // Enum-based color interface (preferred for APC)
        bool setPadColor(const PadPosition& position, APCKey25Mk1Constants::PadColor color);
        bool setPadColor(uint8_t pad_id, APCKey25Mk1Constants::PadColor color);
        void setAllPads(APCKey25Mk1Constants::PadColor color);
        bool setSideButtonColor(uint8_t button_id, APCKey25Mk1Constants::PadColor color);
        bool setTopButtonColor(uint8_t button_id, APCKey25Mk1Constants::PadColor color);

        void clearAllPads() override;
        void enterProgrammerMode() override;
        void exitProgrammerMode() override;
        void reset() override;
        void processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2) override;
        PadPosition noteToPadPosition(uint8_t note) const override;
        uint8_t padPositionToNote(const PadPosition& position) const override;
        uint8_t getGridWidth() const override { return APCKey25Mk1Constants::PAD_COLUMNS; }
        uint8_t getGridHeight() const override { return APCKey25Mk1Constants::PAD_ROWS; }

        // APC Key 25 mk1 specific methods
        void setTransportButtonLED(uint8_t button, uint8_t state);

        /**
         * @brief Set scene button LED state
         * @param scene_index Scene button index (0-4)
         * @param state LED state (off/on/blink)
         */
        void setSceneButtonLED(uint8_t scene_index, uint8_t state);

        /**
         * @brief Set all grid pads to the same APC predefined color
         * @param color APC predefined color (use APCKey25Mk1Constants::PadColor)
         */
        void setAllGridPads(APCKey25Mk1Constants::PadColor color);

        /**
         * @brief Set a specific grid pad color by note number using APC predefined color
         * @param note Grid pad note (0-39)
         * @param color APC predefined color (use APCKey25Mk1Constants::PadColor)
         * @return True if successful, false if invalid note
         */
        bool setGridPadColor(uint8_t note, APCKey25Mk1Constants::PadColor color);

        /**
         * @brief Clear all grid pads (turn off LEDs)
         */
        void clearAllGridPads();

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
        void flashPad(uint8_t pad_index, APCKey25Mk1Constants::PadColor color, uint32_t duration_ms = 200);

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
        using GridPadCallback = std::function<void(uint8_t channel, uint8_t note, uint8_t velocity, bool is_press)>;
        using FaderCallback = std::function<void(uint8_t channel, uint8_t fader_index, uint8_t value)>;
        using EncoderCallback = std::function<void(uint8_t channel, uint8_t encoder_index, uint8_t value)>;
        using TransportCallback = std::function<void(uint8_t channel, uint8_t button, bool is_press)>;
        using SceneCallback = std::function<void(uint8_t channel, uint8_t scene_index, bool is_press)>;
        using NavigationCallback = std::function<void(uint8_t channel, uint8_t direction, bool is_press)>; // 0=up, 1=down, 2=left, 3=right
        using FunctionCallback = std::function<void(uint8_t channel, uint8_t function, bool is_press)>; // 0=volume, 1=pan, 2=send, 3=device
        using ShiftCallback = std::function<void(uint8_t channel, bool is_pressed)>;

        /**
         * @brief Set keyboard event callback
         * @param callback Function to call on keyboard events
         */
        void setKeyboardCallback(KeyboardCallback callback) { keyboard_callback_ = callback; }

        /**
         * @brief Set grid pad event callback
         * @param callback Function to call on grid pad events (notes 0-39)
         */
        void setGridPadCallback(GridPadCallback callback) { grid_pad_callback_ = callback; }

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

        /**
         * @brief Set navigation button callback
         * @param callback Function to call on navigation button events
         */
        void setNavigationCallback(NavigationCallback callback) { navigation_callback_ = callback; }

        /**
         * @brief Set function button callback
         * @param callback Function to call on function button events
         */
        void setFunctionCallback(FunctionCallback callback) { function_callback_ = callback; }

        /**
         * @brief Set shift state callback
         * @param callback Function to call on shift state changes
         */
        void setShiftCallback(ShiftCallback callback) { shift_callback_ = callback; }

    private:
        // Event callbacks
        KeyboardCallback keyboard_callback_;
        GridPadCallback grid_pad_callback_;
        FaderCallback fader_callback_;
        EncoderCallback encoder_callback_;
        TransportCallback transport_callback_;
        SceneCallback scene_callback_;
        NavigationCallback navigation_callback_;
        FunctionCallback function_callback_;
        ShiftCallback shift_callback_;
    };

    /**
     * @brief Factory function to create APC Key 25 mk1 device
     * @param device_name Device name (unused for this device)
     * @return Unique pointer to APC Key 25 mk1 device
     */
    std::unique_ptr<MidiDevice> createAPCKey25Mk1(const std::string& device_name = "");

} // namespace synth
