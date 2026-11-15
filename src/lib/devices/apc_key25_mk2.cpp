#include "apc_key25_mk2.hpp"
#include "../utils/platform_midi_reader.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

namespace synth
{
    APCKey25Mk2::APCKey25Mk2()
        : MidiDevice(APCKey25Mk2Constants::DEVICE_NAME, DeviceType::KEYBOARD_CONTROLLER,
                    static_cast<uint32_t>(DeviceCapability::RGB_LEDS) |
                    static_cast<uint32_t>(DeviceCapability::VELOCITY_SENSITIVE) |
                    static_cast<uint32_t>(DeviceCapability::FADERS) |
                    static_cast<uint32_t>(DeviceCapability::ROTARY_ENCODERS))
        , shift_pressed_(false)
    {
        // Initialize state arrays
        pad_colors_.fill(APCKey25Mk2Constants::PadColor::OFF);
        scene_button_states_.fill(APCKey25Mk2Constants::ButtonLED::OFF);
        transport_button_states_.fill(APCKey25Mk2Constants::ButtonLED::OFF);
        fader_values_.fill(0);
        encoder_values_.fill(64); // Center position for encoders
    }

    APCKey25Mk2::~APCKey25Mk2()
    {
        if (is_connected_)
        {
            disconnect();
        }
    }

    bool APCKey25Mk2::connect(uint8_t device_id)
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
            std::cerr << "Failed to initialize MIDI for APC Key 25 mk2" << std::endl;
            return false;
        }

        // Set up MIDI callbacks
        midi_reader_->setNoteCallback([this](const MidiNote& note) {
            processMidiMessage(note.channel, note.is_note_on ? 0x90 : 0x80, note.note, note.velocity);
        });

        midi_reader_->setControlCallback([this](uint8_t channel, uint8_t controller, uint8_t value) {
            processMidiMessage(channel, 0xB0, controller, value);
        });

        // Try to open the device for input
        int result = midi_reader_->open(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open APC Key 25 mk2 input device " << static_cast<int>(device_id) << std::endl;
            return false;
        }

        // Try to open the device for output (LED control)
        result = midi_reader_->openOutput(device_id);
        if (result != 0)
        {
            std::cerr << "Failed to open APC Key 25 mk2 output device " << static_cast<int>(device_id) << std::endl;
            // Continue anyway - input might still work
        }

        device_id_ = device_id;
        is_connected_ = true;

        // Initialize device
        enterProgrammerMode();
        reset();

        std::cout << "APC Key 25 mk2 connected successfully on device " << static_cast<int>(device_id) << std::endl;
        return true;
    }

    void APCKey25Mk2::disconnect()
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
            setTransportButtonLED(i, APCKey25Mk2Constants::ButtonLED::OFF);
        }
        for (uint8_t i = 0; i < APCKey25Mk2Constants::NUM_SCENE_BUTTONS; ++i)
        {
            setSceneButtonLED(i, APCKey25Mk2Constants::ButtonLED::OFF);
        }

        exitProgrammerMode();

        if (midi_reader_)
        {
            midi_reader_->close();
            midi_reader_->closeOutput();
        }

        is_connected_ = false;
        std::cout << "APC Key 25 mk2 disconnected" << std::endl;
    }

    bool APCKey25Mk2::setPadColor(const PadPosition& position, const RGBColor& color)
    {
        if (position.x >= APCKey25Mk2Constants::PAD_COLUMNS ||
            position.y >= APCKey25Mk2Constants::PAD_ROWS)
        {
            return false;
        }

        uint8_t pad_index = position.y * APCKey25Mk2Constants::PAD_COLUMNS + position.x;
        return setPadColor(pad_index, color);
    }

    bool APCKey25Mk2::setPadColor(uint8_t pad_id, const RGBColor& color)
    {
        if (pad_id >= APCKey25Mk2Constants::TOTAL_PADS)
        {
            return false;
        }

        // Convert RGB to APC velocity value
        uint8_t velocity = rgbToVelocity(color);
        pad_colors_[pad_id] = velocity;

        // Send MIDI Note On with velocity for color
        uint8_t note = APCKey25Mk2Constants::PAD_NOTES[pad_id];
        sendNoteOn(note, velocity);

        return true;
    }

    void APCKey25Mk2::clearAllPads()
    {
        for (uint8_t i = 0; i < APCKey25Mk2Constants::TOTAL_PADS; ++i)
        {
            setPadColor(i, RGBColor::OFF);
        }
    }

    void APCKey25Mk2::setAllPads(const RGBColor& color)
    {
        for (uint8_t i = 0; i < APCKey25Mk2Constants::TOTAL_PADS; ++i)
        {
            setPadColor(i, color);
        }
    }

    bool APCKey25Mk2::setSideButtonColor(uint8_t button_id, const RGBColor& color)
    {
        // APC Key 25 mk2 doesn't have traditional side buttons like Launchpad
        // Scene buttons can be considered as side buttons
        if (button_id >= APCKey25Mk2Constants::NUM_SCENE_BUTTONS)
        {
            return false;
        }

        uint8_t state = (color.red > 0 || color.green > 0 || color.blue > 0) ?
                       APCKey25Mk2Constants::ButtonLED::ON : APCKey25Mk2Constants::ButtonLED::OFF;
        setSceneButtonLED(button_id, state);
        return true;
    }

    bool APCKey25Mk2::setTopButtonColor(uint8_t button_id, const RGBColor& color)
    {
        // Transport buttons can be considered as top buttons
        if (button_id >= 3) // Only 3 transport buttons
        {
            return false;
        }

        uint8_t state = (color.red > 0 || color.green > 0 || color.blue > 0) ?
                       APCKey25Mk2Constants::ButtonLED::ON : APCKey25Mk2Constants::ButtonLED::OFF;
        setTransportButtonLED(button_id, state);
        return true;
    }

    void APCKey25Mk2::enterProgrammerMode()
    {
        // APC Key 25 mk2 doesn't have a specific programmer mode like Launchpad
        // This is a no-op for compatibility
    }

    void APCKey25Mk2::exitProgrammerMode()
    {
        // APC Key 25 mk2 doesn't have a specific programmer mode like Launchpad
        // This is a no-op for compatibility
    }

    void APCKey25Mk2::reset()
    {
        // Clear all LEDs and reset state
        clearAllPads();

        // Reset button states
        for (uint8_t i = 0; i < 3; ++i)
        {
            transport_button_states_[i] = APCKey25Mk2Constants::ButtonLED::OFF;
        }
        for (uint8_t i = 0; i < APCKey25Mk2Constants::NUM_SCENE_BUTTONS; ++i)
        {
            scene_button_states_[i] = APCKey25Mk2Constants::ButtonLED::OFF;
        }

        shift_pressed_ = false;
    }

    void APCKey25Mk2::processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2)
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

    PadPosition APCKey25Mk2::noteToPadPosition(uint8_t note) const
    {
        for (uint8_t i = 0; i < APCKey25Mk2Constants::TOTAL_PADS; ++i)
        {
            if (APCKey25Mk2Constants::PAD_NOTES[i] == note)
            {
                uint8_t x = i % APCKey25Mk2Constants::PAD_COLUMNS;
                uint8_t y = i / APCKey25Mk2Constants::PAD_COLUMNS;
                return PadPosition(x, y);
            }
        }
        return PadPosition(0, 0); // Default if not found
    }

    uint8_t APCKey25Mk2::padPositionToNote(const PadPosition& position) const
    {
        if (position.x >= APCKey25Mk2Constants::PAD_COLUMNS ||
            position.y >= APCKey25Mk2Constants::PAD_ROWS)
        {
            return 0;
        }

        uint8_t pad_index = position.y * APCKey25Mk2Constants::PAD_COLUMNS + position.x;
        return APCKey25Mk2Constants::PAD_NOTES[pad_index];
    }

    void APCKey25Mk2::setTransportButtonLED(uint8_t button, uint8_t state)
    {
        if (button >= 3) return;

        transport_button_states_[button] = state;

        uint8_t note = 0;
        switch (button)
        {
        case 0: note = APCKey25Mk2Constants::TRANSPORT_PLAY; break;
        case 1: note = APCKey25Mk2Constants::TRANSPORT_STOP; break;
        case 2: note = APCKey25Mk2Constants::TRANSPORT_RECORD; break;
        }

        sendNoteOn(note, state);
    }

    void APCKey25Mk2::setSceneButtonLED(uint8_t scene_index, uint8_t state)
    {
        if (scene_index >= APCKey25Mk2Constants::NUM_SCENE_BUTTONS) return;

        scene_button_states_[scene_index] = state;
        uint8_t note = APCKey25Mk2Constants::SCENE_BUTTON_1 + scene_index;
        sendNoteOn(note, state);
    }

    void APCKey25Mk2::testAllPads()
    {
        std::cout << "Testing all APC Key 25 mk2 pads..." << std::endl;

        // Define test colors
        RGBColor colors[] = {
            RGBColor::RED, RGBColor::GREEN, RGBColor::BLUE, RGBColor::YELLOW,
            RGBColor::CYAN, RGBColor::MAGENTA, RGBColor::WHITE, RGBColor::OFF
        };

        for (uint8_t i = 0; i < APCKey25Mk2Constants::TOTAL_PADS; ++i)
        {
            RGBColor color = colors[i % (sizeof(colors) / sizeof(colors[0]))];
            setPadColor(i, color);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // Clear all pads after test
        std::this_thread::sleep_for(std::chrono::seconds(1));
        clearAllPads();
    }

    void APCKey25Mk2::rainbowPattern()
    {
        std::cout << "Creating rainbow pattern on APC Key 25 mk2 pads..." << std::endl;

        RGBColor rainbow_colors[] = {
            RGBColor::RED, RGBColor(255, 127, 0), // Orange
            RGBColor::YELLOW, RGBColor::GREEN,
            RGBColor::CYAN, RGBColor::BLUE,
            RGBColor::MAGENTA, RGBColor(255, 0, 127) // Pink
        };

        for (uint8_t i = 0; i < APCKey25Mk2Constants::TOTAL_PADS; ++i)
        {
            RGBColor color = rainbow_colors[i % (sizeof(rainbow_colors) / sizeof(rainbow_colors[0]))];
            setPadColor(i, color);
        }
    }

    void APCKey25Mk2::flashPad(uint8_t pad_index, const RGBColor& color, uint32_t duration_ms)
    {
        if (pad_index >= APCKey25Mk2Constants::TOTAL_PADS) return;

        // Save current color
        uint8_t original_color = pad_colors_[pad_index];

        // Flash to new color
        setPadColor(pad_index, color);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));

        // Restore original color
        uint8_t velocity = original_color;
        sendNoteOn(APCKey25Mk2Constants::PAD_NOTES[pad_index], velocity);
    }

    uint8_t APCKey25Mk2::getFaderValue(uint8_t fader_index) const
    {
        if (fader_index >= APCKey25Mk2Constants::NUM_FADERS)
        {
            return 0;
        }
        return fader_values_[fader_index];
    }

    uint8_t APCKey25Mk2::getEncoderValue(uint8_t encoder_index) const
    {
        if (encoder_index >= APCKey25Mk2Constants::NUM_ENCODERS)
        {
            return 64; // Center position
        }
        return encoder_values_[encoder_index];
    }

    // Private helper methods
    void APCKey25Mk2::sendNoteOn(uint8_t note, uint8_t velocity)
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

    void APCKey25Mk2::sendNoteOff(uint8_t note)
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

    void APCKey25Mk2::sendControlChange(uint8_t controller, uint8_t value)
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

    bool APCKey25Mk2::isKeyboardNote(uint8_t note) const
    {
        return note >= APCKey25Mk2Constants::KEYBOARD_START_NOTE &&
               note <= APCKey25Mk2Constants::KEYBOARD_END_NOTE;
    }

    bool APCKey25Mk2::isPadNote(uint8_t note) const
    {
        for (uint8_t pad_note : APCKey25Mk2Constants::PAD_NOTES)
        {
            if (note == pad_note) return true;
        }
        return false;
    }

    bool APCKey25Mk2::isTransportNote(uint8_t note) const
    {
        return note == APCKey25Mk2Constants::TRANSPORT_PLAY ||
               note == APCKey25Mk2Constants::TRANSPORT_STOP ||
               note == APCKey25Mk2Constants::TRANSPORT_RECORD ||
               note == APCKey25Mk2Constants::TRANSPORT_SHIFT;
    }

    bool APCKey25Mk2::isSceneNote(uint8_t note) const
    {
        return note >= APCKey25Mk2Constants::SCENE_BUTTON_1 &&
               note <= APCKey25Mk2Constants::SCENE_BUTTON_5;
    }

    void APCKey25Mk2::processNoteEvent(uint8_t channel, uint8_t note, uint8_t velocity, bool is_press)
    {
        if (isKeyboardNote(note))
        {
            if (keyboard_callback_)
            {
                keyboard_callback_(channel, note, velocity, is_press);
            }
        }
        else if (isPadNote(note))
        {
            PadPosition position = noteToPadPosition(note);
            if (pad_callback_)
            {
                pad_callback_(channel, position, velocity, is_press);
            }
        }
        else if (isTransportNote(note))
        {
            if (note == APCKey25Mk2Constants::TRANSPORT_SHIFT)
            {
                shift_pressed_ = is_press;
            }

            uint8_t button_id = 0;
            if (note == APCKey25Mk2Constants::TRANSPORT_PLAY) button_id = 0;
            else if (note == APCKey25Mk2Constants::TRANSPORT_STOP) button_id = 1;
            else if (note == APCKey25Mk2Constants::TRANSPORT_RECORD) button_id = 2;

            if (transport_callback_)
            {
                transport_callback_(channel, button_id, is_press);
            }
        }
        else if (isSceneNote(note))
        {
            uint8_t scene_id = note - APCKey25Mk2Constants::SCENE_BUTTON_1;
            if (scene_callback_)
            {
                scene_callback_(channel, scene_id, is_press);
            }
        }
    }

    void APCKey25Mk2::processControlChange(uint8_t channel, uint8_t controller, uint8_t value)
    {
        if (controller >= APCKey25Mk2Constants::FADER_CC_START &&
            controller <= APCKey25Mk2Constants::FADER_CC_END)
        {
            uint8_t fader_index = controller - APCKey25Mk2Constants::FADER_CC_START;
            fader_values_[fader_index] = value;

            if (fader_callback_)
            {
                fader_callback_(channel, fader_index, value);
            }
        }
        else if (controller >= APCKey25Mk2Constants::ENCODER_CC_START &&
                 controller <= APCKey25Mk2Constants::ENCODER_CC_END)
        {
            uint8_t encoder_index = controller - APCKey25Mk2Constants::ENCODER_CC_START;
            encoder_values_[encoder_index] = value;

            if (encoder_callback_)
            {
                encoder_callback_(channel, encoder_index, value);
            }
        }
    }

    uint8_t APCKey25Mk2::rgbToVelocity(const RGBColor& color) const
    {
        // Convert RGB to APC Key 25 mk2 velocity value
        // This is a simplified mapping - real device may have more complex color mapping

        if (color.red == 0 && color.green == 0 && color.blue == 0)
        {
            return APCKey25Mk2Constants::PadColor::OFF;
        }

        // Determine dominant color
        if (color.red > color.green && color.red > color.blue)
        {
            return (color.red > 127) ? APCKey25Mk2Constants::PadColor::RED_FULL :
                                      APCKey25Mk2Constants::PadColor::RED_LOW;
        }
        else if (color.green > color.red && color.green > color.blue)
        {
            return (color.green > 127) ? APCKey25Mk2Constants::PadColor::GREEN_FULL :
                                        APCKey25Mk2Constants::PadColor::GREEN_LOW;
        }
        else if (color.blue > color.red && color.blue > color.green)
        {
            return (color.blue > 127) ? APCKey25Mk2Constants::PadColor::BLUE_FULL :
                                       APCKey25Mk2Constants::PadColor::BLUE_LOW;
        }
        else if (color.red > 0 && color.green > 0 && color.blue == 0)
        {
            return APCKey25Mk2Constants::PadColor::YELLOW;
        }
        else if (color.red == 0 && color.green > 0 && color.blue > 0)
        {
            return APCKey25Mk2Constants::PadColor::CYAN;
        }
        else if (color.red > 0 && color.green == 0 && color.blue > 0)
        {
            return APCKey25Mk2Constants::PadColor::MAGENTA;
        }
        else if (color.red > 0 && color.green > 0 && color.blue > 0)
        {
            return APCKey25Mk2Constants::PadColor::WHITE;
        }

        return APCKey25Mk2Constants::PadColor::WHITE; // Default
    }

    // Factory function
    std::unique_ptr<MidiDevice> createAPCKey25Mk2(const std::string& device_name)
    {
        (void)device_name; // Device name not used in constructor
        return std::make_unique<APCKey25Mk2>();
    }

} // namespace synth
