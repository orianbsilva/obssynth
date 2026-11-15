/**
 * @file test_launchkey_mk4_device.cpp
 * @brief Test program for Launchkey MK4 device implementation
 *
 * This program demonstrates the Launchkey MK4 device functionality including:
 * - Device connection and initialization
 * - Drum pad color control
 * - Transport and navigation controls
 * - Fader and knob value monitoring
 * - Pattern display capabilities
 */

#include "../lib/devices/launchkey_mk4.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace synth;

int main()
{
    std::cout << "Launchkey MK4 Device Test" << std::endl;
    std::cout << "=========================" << std::endl;

    // Create Launchkey MK4 device
    auto device = std::make_unique<LaunchkeyMK4>();
    LaunchkeyMK4 *launchkey = device.get();

    // Display device information
    std::cout << "\nDevice: " << device->getDeviceName() << std::endl;
    std::cout << "Type: " << static_cast<int>(device->getDeviceType()) << std::endl;
    std::cout << "Grid: " << static_cast<int>(device->getGridWidth()) << "x"
              << static_cast<int>(device->getGridHeight()) << std::endl;

    // Display capabilities
    std::cout << "\nCapabilities:" << std::endl;
    std::cout << "  RGB LEDs: " << (device->hasCapability(DeviceCapability::RGB_LEDS) ? "Yes" : "No") << std::endl;
    std::cout << "  Velocity Sensitive: " << (device->hasCapability(DeviceCapability::VELOCITY_SENSITIVE) ? "Yes" : "No") << std::endl;
    std::cout << "  Faders: " << (device->hasCapability(DeviceCapability::FADERS) ? "Yes" : "No") << std::endl;
    std::cout << "  Rotary Encoders: " << (device->hasCapability(DeviceCapability::ROTARY_ENCODERS) ? "Yes" : "No") << std::endl;
    std::cout << "  Navigation: " << (device->hasCapability(DeviceCapability::NAVIGATION) ? "Yes" : "No") << std::endl;

    // List available MIDI devices
    std::cout << "\nSearching for MIDI devices..." << std::endl;

    // In a real implementation, we would use MidiInterface to scan for devices
    // For now, let's simulate device scanning
    std::vector<std::string> mock_devices = {
        "USB MIDI Device",
        "Launchkey MK4 MIDIIN",
        "Built-in Audio"};

    int launchkey_device_id = -1;
    for (size_t i = 0; i < mock_devices.size(); ++i)
    {
        std::cout << "  " << i << ": " << mock_devices[i] << std::endl;
        if (mock_devices[i].find("Launchkey") != std::string::npos)
        {
            launchkey_device_id = static_cast<int>(i);
        }
    }

    // Set up callbacks to demonstrate device interaction
    std::cout << "\nSetting up device callbacks..." << std::endl;

    device->setPadCallback([](uint8_t channel, const PadPosition &position, uint8_t velocity, bool is_press)
                           { std::cout << "Pad " << (is_press ? "pressed" : "released")
                                       << " at (" << static_cast<int>(position.x) << "," << static_cast<int>(position.y)
                                       << ") with velocity " << static_cast<int>(velocity) << std::endl; });

    device->setSideButtonCallback([](uint8_t channel, uint8_t button_id, bool is_press)
                                  { std::cout << "Transport control " << static_cast<int>(button_id)
                                              << (is_press ? " pressed" : " released") << std::endl; });

    device->setNavigationCallback([](uint8_t channel, NavigationDirection direction)
                                  {
        std::string dir_name;
        switch (direction)
        {
            case NavigationDirection::UP: dir_name = "UP"; break;
            case NavigationDirection::DOWN: dir_name = "DOWN"; break;
            case NavigationDirection::LEFT: dir_name = "LEFT"; break;
            case NavigationDirection::RIGHT: dir_name = "RIGHT"; break;
        }
        std::cout << "Navigation: " << dir_name << std::endl; });

    // Try to connect to device
    if (launchkey_device_id >= 0)
    {
        std::cout << "\nAttempting to connect to Launchkey MK4..." << std::endl;

        if (device->connect(static_cast<uint8_t>(launchkey_device_id)))
        {
            std::cout << "Connected successfully!" << std::endl;

            // Test device features
            std::cout << "\nTesting device features..." << std::endl;

            // Test pad colors
            std::cout << "Setting pad colors..." << std::endl;
            launchkey->setPadColor(PadPosition(0, 0), RGBColor::RED);
            launchkey->setPadColor(PadPosition(1, 0), RGBColor::GREEN);
            launchkey->setPadColor(PadPosition(2, 0), RGBColor::BLUE);
            launchkey->setPadColor(PadPosition(3, 0), RGBColor::YELLOW);

            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Test rainbow pattern
            std::cout << "Displaying rainbow pattern..." << std::endl;
            launchkey->rainbowPattern();

            std::this_thread::sleep_for(std::chrono::seconds(3));

            // Test checkerboard pattern
            std::cout << "Displaying checkerboard pattern..." << std::endl;
            launchkey->checkerboardPattern(RGBColor::RED, RGBColor::GREEN);

            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Test all pads
            std::cout << "Testing all pads..." << std::endl;
            launchkey->testAllPads();

            // Test DAW mode
            std::cout << "Testing DAW mode..." << std::endl;
            launchkey->enableDAWMode();
            std::cout << "DAW Mode: " << (launchkey->isDAWMode() ? "Enabled" : "Disabled") << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(1));

            launchkey->disableDAWMode();
            std::cout << "DAW Mode: " << (launchkey->isDAWMode() ? "Enabled" : "Disabled") << std::endl;

            // Test pad modes
            std::cout << "Testing pad modes..." << std::endl;
            launchkey->setPadMode(LaunchkeyMK4Constants::PAD_MODE_SESSION);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            launchkey->setPadMode(LaunchkeyMK4Constants::PAD_MODE_DRUM);

            // Display current fader and knob values
            std::cout << "\nCurrent fader values:" << std::endl;
            for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS; ++i)
            {
                std::cout << "  Fader " << static_cast<int>(i) << ": "
                          << static_cast<int>(launchkey->getFaderValue(i)) << std::endl;
            }

            std::cout << "\nCurrent knob values:" << std::endl;
            for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS; ++i)
            {
                std::cout << "  Knob " << static_cast<int>(i) << ": "
                          << static_cast<int>(launchkey->getKnobValue(i)) << std::endl;
            }

            // Simulate some device interaction
            std::cout << "\nSimulating device interaction for 10 seconds..." << std::endl;
            std::cout << "Press pads, move faders, or use transport controls..." << std::endl;

            for (int i = 0; i < 100; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Simulate some MIDI messages for demonstration
                if (i == 20)
                {
                    launchkey->processMidiMessage(0, MIDI_NOTE_ON, 36, 64); // Bottom-left pad
                }
                else if (i == 40)
                {
                    launchkey->processMidiMessage(0, MIDI_NOTE_OFF, 36, 0);
                }
                else if (i == 60)
                {
                    launchkey->processMidiMessage(0, MIDI_CONTROL, LaunchkeyMK4Constants::CC_PLAY, 127); // Play button
                }
                else if (i == 80)
                {
                    launchkey->processMidiMessage(0, MIDI_CONTROL, LaunchkeyMK4Constants::FADER_CC_START, 100); // Fader 1
                }
            }

            std::cout << "\nCleaning up..." << std::endl;
            device->disconnect();
        }
        else
        {
            std::cout << "Failed to connect to Launchkey MK4." << std::endl;
        }
    }
    else
    {
        std::cout << "No Launchkey MK4 device found. Running in simulation mode..." << std::endl;

        // Still demonstrate the visual features
        std::cout << "\nDemonstrating pattern capabilities in simulation mode..." << std::endl;

        // Create a mock display of the rainbow pattern
        std::cout << "\nRainbow Pattern (simulation):" << std::endl;
        for (uint8_t y = 0; y < LaunchkeyMK4Constants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchkeyMK4Constants::GRID_WIDTH; ++x)
            {
                uint8_t pad_id = y * LaunchkeyMK4Constants::GRID_WIDTH + x;
                char color_char = "RGBYCMW"[pad_id % 7]; // R=Red, G=Green, B=Blue, Y=Yellow, C=Cyan, M=Magenta, W=White
                std::cout << color_char << " ";
            }
            std::cout << std::endl;
        }

        // Show checkerboard pattern
        std::cout << "\nCheckerboard Pattern (simulation):" << std::endl;
        for (uint8_t y = 0; y < LaunchkeyMK4Constants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchkeyMK4Constants::GRID_WIDTH; ++x)
            {
                bool use_r = ((x + y) % 2) == 0;
                std::cout << (use_r ? "R" : "G") << " ";
            }
            std::cout << std::endl;
        }

        // Demonstrate note mapping
        std::cout << "\nNote Mapping (simulation):" << std::endl;
        for (uint8_t y = 0; y < LaunchkeyMK4Constants::GRID_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < LaunchkeyMK4Constants::GRID_WIDTH; ++x)
            {
                uint8_t note = LaunchkeyMK4Constants::PAD_NOTE_MIN + (y * LaunchkeyMK4Constants::GRID_WIDTH + x);
                std::cout << static_cast<int>(note) << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\nTest completed." << std::endl;
    return 0;
}
