#include "apc_key25_mk1.hpp"
#include "../utils/platform_midi_reader.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

namespace synth
{
    APCKey25Mk1::APCKey25Mk1()
        : MidiDevice(APCKey25Mk1Constants::DEVICE_NAME, DeviceType::KEYBOARD_CONTROLLER,
                    static_cast<uint32_t>(DeviceCapability::RGB_LEDS) |
                    static_cast<uint32_t>(DeviceCapability::VELOCITY_SENSITIVE) |
                    static_cast<uint32_t>(DeviceCapability::FADERS) |
                    static_cast<uint32_t>(DeviceCapability::ROTARY_ENCODERS) |
                    static_cast<uint32_t>(DeviceCapability::TOP_BUTTONS) |
                    static_cast<uint32_t>(DeviceCapability::SIDE_BUTTONS) |
                    static_cast<uint32_t>(DeviceCapability::NAVIGATION))
        , shift_pressed_(false)
    {
        // Initialize state arrays
        pad_colors_.fill(static_cast<uint8_t>(APCKey25Mk1Constants::PadColor::OFF));
        scene_button_states_.fill(APCKey25Mk1Constants::ButtonLED::OFF);
        transport_button_states_.fill(APCKey25Mk1Constants::ButtonLED::OFF);
        fader_values_.fill(0);
        encoder_values_.fill(64); // Center position for encoders
        nav_button_states_.fill(false);
        func_button_states_.fill(false);
    }

    APCKey25Mk1::~APCKey25Mk1()
    {
        if (is_connected_)
        {
            disconnect();
        }
    }

    bool APCKey25Mk1::connect(uint8_t device_id)
    {
        if (is_connected_)
        {
            return true;
        }

        // Create MIDI interface if not exists
        if (!midi_reader_)
        {
            midi_reader_ = std::make_shared<MidiInterface>();
        }

        // Initialize MIDI
        if (midi_reader_->init() != 0)
        {
            std::cerr << "Failed to initialize MIDI for APC Key 25" << std::endl;
            return false;
        }

        // Set up MIDI callbacks
        midi_reader_->setNoteCallback([this](const MidiNote& note) {
            uint8_t status = (note.is_note_on ? 0x90 : 0x80) | note.channel;
            processMidiMessage(note.channel, status, note.note, note.velocity);
        });

        midi_reader_->setControlCallback([this](uint8_t channel, uint8_t controller, uint8_t value) {
            uint8_t status = 0xB0 | channel;
            processMidiMessage(channel, status, controller, value);
        });

        // Try to open the device for input
        int result = midi_reader_->open(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open APC Key 25 input device " << static_cast<int>(device_id) << std::endl;
            return false;
        }

        // Try to open the device for output (LED control)
        result = midi_reader_->openOutput(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open APC Key 25 output device " << static_cast<int>(device_id) << std::endl;
            // Continue anyway - input might still work
        }

        device_id_ = device_id;
        is_connected_ = true;

        // Initialize device
        enterProgrammerMode();
        reset();

        std::cout << "APC Key 25 connected successfully on device " << static_cast<int>(device_id) << std::endl;
        return true;
    }

    void APCKey25Mk1::disconnect()
    {
        if (!is_connected_)
        {
            return;
        }

        // Clear all LEDs
        clearAllPads();

        // Turn off transport and scene buttons
        for (uint8_t i = 0; i < 3; ++i)
        {
            setTransportButtonLED(i, APCKey25Mk1Constants::ButtonLED::OFF);
        }
        for (uint8_t i = 0; i < APCKey25Mk1Constants::NUM_SCENE_BUTTONS; ++i)
        {
            setSceneButtonLED(i, APCKey25Mk1Constants::ButtonLED::OFF);
        }

        exitProgrammerMode();

        if (midi_reader_)
        {
            midi_reader_->close();
            midi_reader_->closeOutput();
        }

        is_connected_ = false;
        std::cout << "APC Key 25 disconnected" << std::endl;
    }

    // RGB interface implementations (for base class compatibility)
    bool APCKey25Mk1::setPadColor(const PadPosition& position, const RGBColor& color)
    {
        // Convert RGB to enum and use enum version
        APCKey25Mk1Constants::PadColor apc_color = rgbToEnum(color);
        return setPadColor(position, apc_color);
    }

    bool APCKey25Mk1::setPadColor(uint8_t pad_id, const RGBColor& color)
    {
        // Convert RGB to enum and use enum version
        APCKey25Mk1Constants::PadColor apc_color = rgbToEnum(color);
        return setPadColor(pad_id, apc_color);
    }

    void APCKey25Mk1::setAllPads(const RGBColor& color)
    {
        // Convert RGB to enum and use enum version
        APCKey25Mk1Constants::PadColor apc_color = rgbToEnum(color);
        setAllPads(apc_color);
    }

    bool APCKey25Mk1::setSideButtonColor(uint8_t button_id, const RGBColor& color)
    {
        // Convert RGB to enum and use enum version
        APCKey25Mk1Constants::PadColor apc_color = rgbToEnum(color);
        return setSideButtonColor(button_id, apc_color);
    }

    bool APCKey25Mk1::setTopButtonColor(uint8_t button_id, const RGBColor& color)
    {
        // Convert RGB to enum and use enum version
        APCKey25Mk1Constants::PadColor apc_color = rgbToEnum(color);
        return setTopButtonColor(button_id, apc_color);
    }

    // Enum-based implementations (preferred interface)
    bool APCKey25Mk1::setPadColor(const PadPosition& position, APCKey25Mk1Constants::PadColor color)
    {
        if (position.x >= APCKey25Mk1Constants::PAD_COLUMNS ||
            position.y >= APCKey25Mk1Constants::PAD_ROWS)
        {
            return false;
        }

        uint8_t pad_index = position.y * APCKey25Mk1Constants::PAD_COLUMNS + position.x;
        return setPadColor(pad_index, color);
    }

    bool APCKey25Mk1::setPadColor(uint8_t pad_id, APCKey25Mk1Constants::PadColor color)
    {
        if (pad_id >= APCKey25Mk1Constants::TOTAL_PADS)
        {
            return false;
        }

        // Use the enum value directly as velocity
        uint8_t velocity = static_cast<uint8_t>(color);
        pad_colors_[pad_id] = velocity;

        // Send MIDI Note On with velocity for color - grid pads use notes 0-39
        uint8_t note = APCKey25Mk1Constants::GRID_PAD_START_NOTE + pad_id;
        sendNoteOn(note, velocity);

        return true;
    }

    void APCKey25Mk1::setAllPads(APCKey25Mk1Constants::PadColor color)
    {
        for (uint8_t i = 0; i < APCKey25Mk1Constants::TOTAL_PADS; ++i)
        {
            setPadColor(i, color);
        }
    }

    bool APCKey25Mk1::setSideButtonColor(uint8_t button_id, APCKey25Mk1Constants::PadColor color)
    {
        // APC Key 25 doesn't have traditional side buttons like Launchpad
        // Scene buttons can be considered as side buttons
        if (button_id >= APCKey25Mk1Constants::NUM_SCENE_BUTTONS)
        {
            return false;
        }

        uint8_t state = (color != APCKey25Mk1Constants::PadColor::OFF) ?
                       APCKey25Mk1Constants::ButtonLED::ON : APCKey25Mk1Constants::ButtonLED::OFF;
        setSceneButtonLED(button_id, state);
        return true;
    }

    bool APCKey25Mk1::setTopButtonColor(uint8_t button_id, APCKey25Mk1Constants::PadColor color)
    {
        // Transport buttons can be considered as top buttons
        if (button_id >= 3) // Only 3 transport buttons
        {
            return false;
        }

        uint8_t state = (color != APCKey25Mk1Constants::PadColor::OFF) ?
                       APCKey25Mk1Constants::ButtonLED::ON : APCKey25Mk1Constants::ButtonLED::OFF;
        setTransportButtonLED(button_id, state);
        return true;
    }

    bool APCKey25Mk1::setGridPadColor(uint8_t note, APCKey25Mk1Constants::PadColor color)
    {
        if (!isGridPadNote(note))
        {
            return false;
        }

        // Convert note to pad index
        uint8_t pad_id = note - APCKey25Mk1Constants::GRID_PAD_START_NOTE;
        return setPadColor(pad_id, color);
    }

    void APCKey25Mk1::clearAllPads()
    {
        for (uint8_t i = 0; i < APCKey25Mk1Constants::TOTAL_PADS; ++i)
        {
            setPadColor(i, APCKey25Mk1Constants::PadColor::OFF); // Turn off all pads
        }
    }

    void APCKey25Mk1::setAllGridPads(APCKey25Mk1Constants::PadColor color)
    {
        // Set all 40 grid pads (notes 0-39) to the same color
        for (uint8_t note = APCKey25Mk1Constants::GRID_PAD_START_NOTE;
             note <= APCKey25Mk1Constants::GRID_PAD_END_NOTE; ++note)
        {
            setGridPadColor(note, color);
        }
    }

    void APCKey25Mk1::clearAllGridPads()
    {
        // Turn off all grid pad LEDs
        setAllGridPads(APCKey25Mk1Constants::PadColor::OFF);
    }

    // Helper method to convert RGB to APC enum
    APCKey25Mk1Constants::PadColor APCKey25Mk1::rgbToEnum(const RGBColor& color) const
    {
        // Convert RGB to APC Key 25 enum value
        if (color.red == 0 && color.green == 0 && color.blue == 0)
        {
            return APCKey25Mk1Constants::PadColor::OFF;
        }

        // Determine dominant color
        if (color.red > color.green && color.red > color.blue)
        {
            return APCKey25Mk1Constants::PadColor::RED;
        }
        else if (color.green > color.red && color.green > color.blue)
        {
            return APCKey25Mk1Constants::PadColor::GREEN;
        }
        else if (color.red > 0 && color.green > 0 && color.blue == 0)
        {
            return APCKey25Mk1Constants::PadColor::AMBER;
        }

        return APCKey25Mk1Constants::PadColor::GREEN; // Default
    }

    void APCKey25Mk1::enterProgrammerMode()
    {
        // APC Key 25 doesn't have a specific programmer mode like Launchpad
        // This is a no-op for compatibility
    }

    void APCKey25Mk1::exitProgrammerMode()
    {
        // APC Key 25 APCKey25Mk1doesn't have a specific programmer mode like Launchpad
        // This is a no-op for compatibility
    }

    void APCKey25Mk1::reset()
    {
        // Clear all LEDs and reset state
        clearAllPads();

        // Reset button states
        for (uint8_t i = 0; i < 3; ++i)
        {
            transport_button_states_[i] = APCKey25Mk1Constants::ButtonLED::OFF;
        }
        for (uint8_t i = 0; i < APCKey25Mk1Constants::NUM_SCENE_BUTTONS; ++i)
        {
            scene_button_states_[i] = APCKey25Mk1Constants::ButtonLED::OFF;
        }

        shift_pressed_ = false;
    }

    void APCKey25Mk1::processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2)
    {
        uint8_t message_type = status & 0xF0;

        switch (message_type)
        {
        case 0x90: // Note On
            if (data2 > 0) // Velocity > 0 means note on
            {
                processNoteEvent(channel, data1, data2, true);
            }
            else // Velocity 0 means note off
            {
                processNoteEvent(channel, data1, 0, false);
            }
            break;

        case 0x80: // Note Off
            processNoteEvent(channel, data1, data2, false);
            break;

        case 0xB0: // Control Change
            processControlChange(channel, data1, data2);
            break;

        default:
            // Ignore other message types
            break;
        }
    }

    PadPosition APCKey25Mk1::noteToPadPosition(uint8_t note) const
    {
        // For grid pads (notes 0-39), calculate position directly
        if (isGridPadNote(note))
        {
            uint8_t pad_index = note - APCKey25Mk1Constants::GRID_PAD_START_NOTE;
            uint8_t x = pad_index % APCKey25Mk1Constants::PAD_COLUMNS;
            uint8_t y = pad_index / APCKey25Mk1Constants::PAD_COLUMNS;
            return PadPosition(x, y);
        }

        // For legacy compatibility, return default
        return PadPosition(0, 0);
    }

    uint8_t APCKey25Mk1::padPositionToNote(const PadPosition& position) const
    {
        if (position.x >= APCKey25Mk1Constants::PAD_COLUMNS ||
            position.y >= APCKey25Mk1Constants::PAD_ROWS)
        {
            return 0;
        }

        uint8_t pad_index = position.y * APCKey25Mk1Constants::PAD_COLUMNS + position.x;
        return APCKey25Mk1Constants::GRID_PAD_START_NOTE + pad_index;
    }

    void APCKey25Mk1::setTransportButtonLED(uint8_t button, uint8_t state)
    {
        if (button >= 3) return;

        transport_button_states_[button] = state;

        uint8_t note = 0;
        switch (button)
        {
        case 0: note = APCKey25Mk1Constants::TRANSPORT_PLAY; break;
        case 1: note = APCKey25Mk1Constants::TRANSPORT_STOP; break;
        case 2: note = APCKey25Mk1Constants::TRANSPORT_RECORD; break;
        }

        sendNoteOn(note, state);
    }

    void APCKey25Mk1::setSceneButtonLED(uint8_t scene_index, uint8_t state)
    {
        if (scene_index >= APCKey25Mk1Constants::NUM_SCENE_BUTTONS) return;

        scene_button_states_[scene_index] = state;
        uint8_t note = APCKey25Mk1Constants::SCENE_BUTTON_1 + scene_index;
        sendNoteOn(note, state);
    }

    void APCKey25Mk1::flashPad(uint8_t pad_index, APCKey25Mk1Constants::PadColor color, uint32_t duration_ms)
    {
        if (pad_index >= APCKey25Mk1Constants::TOTAL_PADS) return;

        // Save current color
        uint8_t original_color = pad_colors_[pad_index];

        // Flash to new color
        setPadColor(pad_index, color);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));

        // Restore original color - use grid pad note mapping
        uint8_t velocity = original_color;
        uint8_t note = APCKey25Mk1Constants::GRID_PAD_START_NOTE + pad_index;
        sendNoteOn(note, velocity);
    }

    uint8_t APCKey25Mk1::getFaderValue(uint8_t fader_index) const
    {
        if (fader_index >= APCKey25Mk1Constants::NUM_FADERS)
        {
            return 0;
        }
        return fader_values_[fader_index];
    }

    uint8_t APCKey25Mk1::getEncoderValue(uint8_t encoder_index) const
    {
        if (encoder_index >= APCKey25Mk1Constants::NUM_ENCODERS)
        {
            return 64; // Center position
        }
        return encoder_values_[encoder_index];
    }

    // Private helper methods
    void APCKey25Mk1::sendNoteOn(uint8_t note, uint8_t velocity)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Note On message on channel 0
        [[maybe_unused]] int result = midi_reader_->sendNoteOn(0, note, velocity);

        #ifdef DEBUG_APC_KEY25_MK2
        if (result == 0)
        {
            std::cout << "MIDI Out: Note On " << static_cast<int>(note)
                      << " velocity " << static_cast<int>(velocity) << std::endl;
        }
        else
        {
            std::cout << "MIDI Out Failed: Note On " << static_cast<int>(note)
                      << " velocity " << static_cast<int>(velocity) << std::endl;
        }
        #endif
    }

    void APCKey25Mk1::sendNoteOff(uint8_t note)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Note Off message on channel 0 with velocity 0
        [[maybe_unused]] int result = midi_reader_->sendNoteOff(0, note, 0);

        #ifdef DEBUG_APC_KEY25_MK2
        if (result == 0)
        {
            std::cout << "MIDI Out: Note Off " << static_cast<int>(note) << std::endl;
        }
        else
        {
            std::cout << "MIDI Out Failed: Note Off " << static_cast<int>(note) << std::endl;
        }
        #endif
    }

    void APCKey25Mk1::sendControlChange(uint8_t controller, uint8_t value)
    {
        if (!is_connected_ || !midi_reader_)
        {
            return;
        }

        // Send MIDI Control Change message on channel 0
        [[maybe_unused]] int result = midi_reader_->sendControlChange(0, controller, value);

        #ifdef DEBUG_APC_KEY25_MK2
        if (result == 0)
        {
            std::cout << "MIDI Out: CC " << static_cast<int>(controller)
                      << " value " << static_cast<int>(value) << std::endl;
        }
        else
        {
            std::cout << "MIDI Out Failed: CC " << static_cast<int>(controller)
                      << " value " << static_cast<int>(value) << std::endl;
        }
        #endif
    }

    bool APCKey25Mk1::isKeyboardNote(uint8_t note) const
    {
        return note >= APCKey25Mk1Constants::KEYBOARD_START_NOTE &&
               note <= APCKey25Mk1Constants::KEYBOARD_END_NOTE;
    }

    bool APCKey25Mk1::isPadNote(uint8_t note) const
    {
        // Legacy method - now redirects to grid pad check
        return isGridPadNote(note);
    }

    bool APCKey25Mk1::isGridPadNote(uint8_t note) const
    {
        return note >= APCKey25Mk1Constants::GRID_PAD_START_NOTE &&
               note <= APCKey25Mk1Constants::GRID_PAD_END_NOTE;
    }

    bool APCKey25Mk1::isNavigationNote(uint8_t note) const
    {
        return note == APCKey25Mk1Constants::NAV_UP ||
               note == APCKey25Mk1Constants::NAV_DOWN ||
               note == APCKey25Mk1Constants::NAV_LEFT ||
               note == APCKey25Mk1Constants::NAV_RIGHT;
    }

    bool APCKey25Mk1::isFunctionNote(uint8_t note) const
    {
        return note == APCKey25Mk1Constants::FUNC_VOLUME ||
               note == APCKey25Mk1Constants::FUNC_PAN ||
               note == APCKey25Mk1Constants::FUNC_SEND ||
               note == APCKey25Mk1Constants::FUNC_DEVICE;
    }

    bool APCKey25Mk1::isTransportNote(uint8_t note) const
    {
        return note == APCKey25Mk1Constants::TRANSPORT_PLAY ||
               note == APCKey25Mk1Constants::TRANSPORT_STOP ||
               note == APCKey25Mk1Constants::TRANSPORT_RECORD ||
               note == APCKey25Mk1Constants::TRANSPORT_SHIFT ||
               note == APCKey25Mk1Constants::STOP_ALL_CLIPS;
    }

    bool APCKey25Mk1::isSceneNote(uint8_t note) const
    {
        return note >= APCKey25Mk1Constants::SCENE_BUTTON_1 &&
               note <= APCKey25Mk1Constants::SCENE_BUTTON_5;
    }

    void APCKey25Mk1::processNoteEvent(uint8_t channel, uint8_t note, uint8_t velocity, bool is_press)
    {
        // Channel-specific processing:
        // Channel 0: Pads and clip controls
        // Channel 1: Keyboard

        // Priority 1: Check for shift navigation functionality first (notes 64-67 when shift is pressed)
        if (shift_pressed_ && (note >= 64 && note <= 67) && channel == 0)
        {
            // Handle shifted buttons 64-67 as navigation
            uint8_t nav_index = 0;
            if (note == 64) nav_index = 0;      // Up
            else if (note == 65) nav_index = 1; // Down
            else if (note == 66) nav_index = 2; // Left
            else if (note == 67) nav_index = 3; // Right

            nav_button_states_[nav_index] = is_press;

            if (navigation_callback_)
            {
                navigation_callback_(channel, nav_index, is_press);
            }
            return; // Exit early to prevent further processing
        }

        else if (!shift_pressed_ && (note >= 64 && note <= 71) && channel == 0)
        {
            if (top_button_callback_)
            {
                uint8_t top_index = note - 64; // Map 64-71 to 0-7
                top_button_callback_(channel, top_index, is_press);
            }
            return; // Exit early to prevent further processing
        }

        // Priority 3: Keyboard events (only processed on channel 1, and not if it's navigation notes 64-67)
        else if (isKeyboardNote(note) && channel == 1)
        {
            // Skip notes 64-67 on keyboard channel if shift is pressed (they should be navigation on channel 0)
            if (!(shift_pressed_ && (note >= 64 && note <= 67)) && keyboard_callback_)
            {
                keyboard_callback_(channel, note, velocity, is_press);
            }
        }

        // Priority 4: Grid pad events (only processed on channel 0)
        else if (isGridPadNote(note) && channel == 0)
        {
            if (grid_pad_callback_)
            {
                grid_pad_callback_(channel, note, velocity, is_press);
            }
        }

        // Priority 5: Legacy pad handling for compatibility (channel 0 only)
        else if (isPadNote(note) && channel == 0)
        {
            PadPosition position = noteToPadPosition(note);
            if (pad_callback_)
            {
                triggerPadCallback(channel, position, velocity, is_press);
            }
        }

        // Priority 6: Function buttons (68-71) only when shift is pressed and on channel 0
        else if (isFunctionNote(note) && shift_pressed_ && channel == 0)
        {
            uint8_t func_index = 0;
            if (note == APCKey25Mk1Constants::FUNC_VOLUME) func_index = 0;
            else if (note == APCKey25Mk1Constants::FUNC_PAN) func_index = 1;
            else if (note == APCKey25Mk1Constants::FUNC_SEND) func_index = 2;
            else if (note == APCKey25Mk1Constants::FUNC_DEVICE) func_index = 3;

            func_button_states_[func_index] = is_press;

            if (function_callback_)
            {
                function_callback_(channel, func_index, is_press);
            }
        }

        // Priority 7: Transport controls (only processed on channel 0)
        else if (isTransportNote(note) && channel == 0)
        {
            if (note == APCKey25Mk1Constants::TRANSPORT_SHIFT)
            {
                shift_pressed_ = is_press;
                if (shift_callback_)
                {
                    shift_callback_(channel, is_press);
                    return;
                }
            }

            uint8_t button_id = 0;
            if (note == APCKey25Mk1Constants::TRANSPORT_PLAY) button_id = 0;
            else if (note == APCKey25Mk1Constants::TRANSPORT_STOP) button_id = 1;
            else if (note == APCKey25Mk1Constants::TRANSPORT_RECORD) button_id = 2;
            else if (note == APCKey25Mk1Constants::STOP_ALL_CLIPS) button_id = 3;

            if (transport_callback_)
            {
                transport_callback_(channel, button_id, is_press);
            }
        }

        // Priority 8: Scene buttons (only processed on channel 0)
        else if (isSceneNote(note) && channel == 0)
        {
            uint8_t scene_id = note - APCKey25Mk1Constants::SCENE_BUTTON_1;
            if (scene_callback_)
            {
                scene_callback_(channel, scene_id, is_press);
            }
        }
    }

    void APCKey25Mk1::processControlChange(uint8_t channel, uint8_t controller, uint8_t value)
    {
        // Control changes (faders, encoders, shift) only processed on channel 0
        if (channel != 0)
        {
            return;
        }

        if (controller >= APCKey25Mk1Constants::FADER_CC_START &&
            controller <= APCKey25Mk1Constants::FADER_CC_END)
        {
            uint8_t fader_index = controller - APCKey25Mk1Constants::FADER_CC_START;
            fader_values_[fader_index] = value;

            if (fader_callback_)
            {
                fader_callback_(channel, fader_index, value);
            }
        }
        else if (controller >= APCKey25Mk1Constants::ENCODER_CC_START &&
                 controller <= APCKey25Mk1Constants::ENCODER_CC_END)
        {
            uint8_t encoder_index = controller - APCKey25Mk1Constants::ENCODER_CC_START;
            encoder_values_[encoder_index] = value;

            if (encoder_callback_)
            {
                encoder_callback_(channel, encoder_index, value);
            }
        }
        else if (controller == APCKey25Mk1Constants::SHIFT_CC)
        {
            // Handle shift key via CC 64
            bool is_pressed = value > 0;
            shift_pressed_ = is_pressed;

            if (shift_callback_)
            {
                shift_callback_(channel, is_pressed);
            }
        }
    }

    // Factory function
    std::unique_ptr<synth::MidiDevice> createAPCKey25Mk1(const std::string& device_name)
    {
        (void)device_name; // Device name not used in constructor
        return std::make_unique<synth::APCKey25Mk1>();
    }

} // namespace synth
