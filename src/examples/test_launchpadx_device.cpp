// Enhanced Launchpad X test using the new device abstraction
#include "../lib/devices/launchpadx.hpp"
#include "../lib/devices/midi_device.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

using namespace synth;

void demonstrateLaunchpadFeatures(LaunchpadX& launchpad)
{
    std::cout << "\n=== Launchpad X Feature Demonstration ===" << std::endl;

    // Test 1: Individual pad colors
    std::cout << "1. Setting individual pad colors..." << std::endl;
    launchpad.setPadColor(PadPosition(0, 0), RGBColor::RED);
    launchpad.setPadColor(PadPosition(1, 0), RGBColor::GREEN);
    launchpad.setPadColor(PadPosition(2, 0), RGBColor::BLUE);
    launchpad.setPadColor(PadPosition(3, 0), RGBColor::YELLOW);
    launchpad.setPadColor(PadPosition(4, 0), RGBColor::CYAN);
    launchpad.setPadColor(PadPosition(5, 0), RGBColor::MAGENTA);
    launchpad.setPadColor(PadPosition(6, 0), RGBColor::WHITE);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 2: Clear all pads
    std::cout << "2. Clearing all pads..." << std::endl;
    launchpad.clearAllPads();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Test 3: Checkerboard pattern
    std::cout << "3. Creating checkerboard pattern..." << std::endl;
    launchpad.checkerboardPattern(RGBColor::RED, RGBColor::GREEN);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 4: Rainbow pattern
    std::cout << "4. Creating rainbow pattern..." << std::endl;
    launchpad.rainbowPattern();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 5: Side button colors
    std::cout << "5. Setting side button colors..." << std::endl;
    for (uint8_t i = 0; i < 8; ++i)
    {
        RGBColor color = (i % 2 == 0) ? RGBColor::BLUE : RGBColor::CYAN;
        launchpad.setSideButtonColor(i, color);
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 6: Top button colors
    std::cout << "6. Setting top button colors..." << std::endl;
    for (uint8_t i = 0; i < 8; ++i)
    {
        RGBColor color = (i % 2 == 0) ? RGBColor::YELLOW : RGBColor::MAGENTA;
        launchpad.setTopButtonColor(i, color);
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 7: All pads test
    std::cout << "7. Testing all pads sequentially..." << std::endl;
    launchpad.testAllPads();

    // Test 8: Flash and pulse effects
    std::cout << "8. Flash and pulse effects..." << std::endl;
    launchpad.clearAllPads();
    launchpad.flashPad(PadPosition(3, 3), RGBColor::WHITE, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    launchpad.pulsePad(PadPosition(4, 4), RGBColor::RED);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    launchpad.clearAllPads();
}

int main()
{
    std::cout << "Launchpad X Device Abstraction Test" << std::endl;
    std::cout << "===================================" << std::endl;

    // Create Launchpad X device using the factory
    auto device = DeviceFactory::createDevice("Launchpad X");
    if (!device)
    {
        std::cerr << "Failed to create Launchpad X device!" << std::endl;
        return 1;
    }

    // Cast to LaunchpadX for specific features
    LaunchpadX* launchpad = dynamic_cast<LaunchpadX*>(device.get());
    if (!launchpad)
    {
        std::cerr << "Device is not a Launchpad X!" << std::endl;
        return 1;
    }

    std::cout << "Device Name: " << device->getDeviceName() << std::endl;
    std::cout << "Device Type: " << static_cast<int>(device->getDeviceType()) << std::endl;
    std::cout << "Grid Size: " << static_cast<int>(device->getGridWidth())
              << "x" << static_cast<int>(device->getGridHeight()) << std::endl;

    // Check capabilities
    std::cout << "Capabilities:" << std::endl;
    std::cout << "  RGB LEDs: " << (device->hasCapability(DeviceCapability::RGB_LEDS) ? "Yes" : "No") << std::endl;
    std::cout << "  Velocity Sensitive: " << (device->hasCapability(DeviceCapability::VELOCITY_SENSITIVE) ? "Yes" : "No") << std::endl;
    std::cout << "  Aftertouch: " << (device->hasCapability(DeviceCapability::AFTERTOUCH) ? "Yes" : "No") << std::endl;
    std::cout << "  Side Buttons: " << (device->hasCapability(DeviceCapability::SIDE_BUTTONS) ? "Yes" : "No") << std::endl;
    std::cout << "  Top Buttons: " << (device->hasCapability(DeviceCapability::TOP_BUTTONS) ? "Yes" : "No") << std::endl;

    // List available MIDI devices
    std::cout << "\nSearching for MIDI devices..." << std::endl;

    // In a real implementation, we would use MidiInterface to scan for devices
    // For now, let's simulate device scanning
    std::vector<std::string> mock_devices = {
        "USB MIDI Device",
        "Launchpad X MIDIIN",
        "Built-in Audio"
    };

    int launchpad_device_id = -1;
    for (size_t i = 0; i < mock_devices.size(); ++i)
    {
        std::cout << "  " << i << ": " << mock_devices[i] << std::endl;
        if (mock_devices[i].find("Launchpad") != std::string::npos)
        {
            launchpad_device_id = static_cast<int>(i);
        }
    }

    if (launchpad_device_id == -1)
    {
        std::cout << "\nNo Launchpad X found! Using simulation mode..." << std::endl;
        launchpad_device_id = 1; // Use a mock device
    }

    // Set up event callbacks
    device->setPadCallback([](uint8_t channel, const PadPosition& pos, uint8_t velocity, bool is_press) {
        std::cout << "Pad " << (is_press ? "PRESSED" : "RELEASED")
                  << " at (" << static_cast<int>(pos.x) << ", " << static_cast<int>(pos.y)
                  << ") with velocity " << static_cast<int>(velocity)
                  << " on channel " << static_cast<int>(channel) << std::endl;
    });

    device->setSideButtonCallback([](uint8_t channel, uint8_t button_id, bool is_press) {
        std::cout << "Side button " << static_cast<int>(button_id)
                  << " " << (is_press ? "PRESSED" : "RELEASED")
                  << " on channel " << static_cast<int>(channel) << std::endl;
    });

    device->setTopButtonCallback([](uint8_t channel, uint8_t button_id, bool is_press) {
        std::cout << "Top button " << static_cast<int>(button_id)
                  << " " << (is_press ? "PRESSED" : "RELEASED")
                  << " on channel " << static_cast<int>(channel) << std::endl;
    });

    device->setAftertouchCallback([](uint8_t channel, const PadPosition& pos, uint8_t pressure) {
        std::cout << "Aftertouch at (" << static_cast<int>(pos.x) << ", " << static_cast<int>(pos.y)
                  << ") pressure: " << static_cast<int>(pressure)
                  << " on channel " << static_cast<int>(channel) << std::endl;
    });

    // Try to connect to the device
    std::cout << "\nAttempting to connect to Launchpad X..." << std::endl;
    bool connected = device->connect(static_cast<uint8_t>(launchpad_device_id));

    if (connected)
    {
        std::cout << "Successfully connected to Launchpad X!" << std::endl;

        // Demonstrate features
        demonstrateLaunchpadFeatures(*launchpad);

        // Wait for user input
        std::cout << "\nPress Enter to start listening for MIDI input (or Ctrl+C to exit)..." << std::endl;
        std::cin.get();

        std::cout << "Listening for MIDI input. Press pads, side buttons, or top buttons..." << std::endl;
        std::cout << "Press Enter again to exit." << std::endl;

        // Simulate listening for a few seconds
        for (int i = 0; i < 10; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Simulate some MIDI input for demonstration
            if (i == 2)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_ON, 11, 64); // Bottom-left pad
            }
            else if (i == 4)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_OFF, 11, 0);
            }
            else if (i == 6)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_ON, 91, 64); // Top button 1
            }
            else if (i == 8)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_ON, 89, 64); // Side button 1
            }
        }

        std::cout << "\nCleaning up..." << std::endl;
        device->disconnect();
    }
    else
    {
        std::cout << "Failed to connect to Launchpad X. Running in simulation mode..." << std::endl;

        // Still demonstrate the visual features
        demonstrateLaunchpadFeatures(*launchpad);
    }

    std::cout << "\nTest completed." << std::endl;
    return 0;
}
