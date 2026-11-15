/**
 * @file test_launchpadx_session_mode.cpp
 * @brief Test program for Launchpad X session mode functionality
 *
 * This program demonstrates the new session mode features of the Launchpad X,
 * including entering/exiting session mode, changing layouts, and testing
 * standalone operation capabilities.
 */

#include "../lib/devices/launchpadx.hpp"
#include "../lib/devices/midi_device.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

using namespace synth;

void demonstrateSessionMode(LaunchpadX& launchpad)
{
    std::cout << "\n=== Launchpad X Session Mode Demonstration ===" << std::endl;

    // Test 1: Enter session mode with default layout
    std::cout << "1. Entering session mode with default layout..." << std::endl;
    launchpad.enterSessionMode();
    std::cout << "   Session mode active: " << (launchpad.isSessionMode() ? "Yes" : "No") << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 2: Change to drum rack layout
    std::cout << "2. Switching to drum rack layout..." << std::endl;
    launchpad.setLayout(LaunchpadXConstants::LAYOUT_DRUM_RACK);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 3: Change to chromatic layout
    std::cout << "3. Switching to chromatic layout..." << std::endl;
    launchpad.setLayout(LaunchpadXConstants::LAYOUT_CHROMATIC);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 4: Return to session layout
    std::cout << "4. Returning to session layout..." << std::endl;
    launchpad.setLayout(LaunchpadXConstants::LAYOUT_SESSION);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 5: Exit session mode
    std::cout << "5. Exiting session mode..." << std::endl;
    launchpad.exitSessionMode();
    std::cout << "   Session mode active: " << (launchpad.isSessionMode() ? "Yes" : "No") << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Test 6: Try programmer mode operations after session mode
    std::cout << "6. Testing programmer mode after session mode..." << std::endl;
    launchpad.enterProgrammerMode();

    // Light up corners in different colors to show programmer mode is active
    launchpad.setPadColor(PadPosition(0, 0), RGBColor::RED);    // Bottom-left
    launchpad.setPadColor(PadPosition(7, 0), RGBColor::GREEN);  // Bottom-right
    launchpad.setPadColor(PadPosition(0, 7), RGBColor::BLUE);   // Top-left
    launchpad.setPadColor(PadPosition(7, 7), RGBColor::YELLOW); // Top-right

    std::cout << "   Corner pads should be lit in different colors." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test 7: Re-enter session mode
    std::cout << "7. Re-entering session mode..." << std::endl;
    launchpad.enableSessionMode();
    std::cout << "   Session mode active: " << (launchpad.isSessionMode() ? "Yes" : "No") << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 8: Reset device
    std::cout << "8. Resetting device..." << std::endl;
    launchpad.reset();
    std::cout << "   Session mode active after reset: " << (launchpad.isSessionMode() ? "Yes" : "No") << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\nSession mode demonstration completed!" << std::endl;
}

int main()
{
    std::cout << "Launchpad X Session Mode Test" << std::endl;
    std::cout << "=============================" << std::endl;

    // Create Launchpad X device
    auto device = std::make_unique<LaunchpadX>();
    LaunchpadX* launchpad = device.get();

    // Display device information
    std::cout << "\nDevice: " << device->getDeviceName() << std::endl;
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

    // Set up event callbacks for session mode testing
    device->setPadCallback([](uint8_t channel, const PadPosition& pos, uint8_t velocity, bool is_press) {
        std::cout << "Session Pad " << (is_press ? "PRESSED" : "RELEASED")
                  << " at (" << static_cast<int>(pos.x) << ", " << static_cast<int>(pos.y)
                  << ") with velocity " << static_cast<int>(velocity)
                  << " on channel " << static_cast<int>(channel) << std::endl;
    });

    device->setSideButtonCallback([](uint8_t channel, uint8_t button_id, bool is_press) {
        std::cout << "Session Side button " << static_cast<int>(button_id)
                  << " " << (is_press ? "PRESSED" : "RELEASED")
                  << " on channel " << static_cast<int>(channel) << std::endl;
    });

    device->setTopButtonCallback([](uint8_t channel, uint8_t button_id, bool is_press) {
        std::cout << "Session Top button " << static_cast<int>(button_id)
                  << " " << (is_press ? "PRESSED" : "RELEASED")
                  << " on channel " << static_cast<int>(channel) << std::endl;
    });

    // Attempt to connect
    std::cout << "\nAttempting to connect to Launchpad X..." << std::endl;
    bool connected = device->connect(static_cast<uint8_t>(launchpad_device_id));

    if (connected)
    {
        std::cout << "Successfully connected to Launchpad X!" << std::endl;

        // Demonstrate session mode features
        demonstrateSessionMode(*launchpad);

        // Wait for user input to test session mode interactivity
        std::cout << "\nPress Enter to start listening for MIDI input in session mode..." << std::endl;
        std::cin.get();

        // Enter session mode for interactive testing
        launchpad->enterSessionMode();
        std::cout << "Now in session mode. Press pads and buttons to see session mode behavior." << std::endl;
        std::cout << "Available layouts:" << std::endl;
        std::cout << "  0 = Session layout (default)" << std::endl;
        std::cout << "  1 = Drum rack layout" << std::endl;
        std::cout << "  2 = Chromatic note layout" << std::endl;
        std::cout << "  3 = User custom layout" << std::endl;
        std::cout << "Press Enter again to exit." << std::endl;

        // Simulate listening for a few seconds
        for (int i = 0; i < 20; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Simulate some session mode MIDI input for demonstration
            if (i == 3)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_ON, 11, 64); // Bottom-left pad
            }
            else if (i == 6)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_OFF, 11, 0);
            }
            else if (i == 9)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_ON, 91, 64); // Top button 1
            }
            else if (i == 12)
            {
                launchpad->processMidiMessage(0, MIDI_NOTE_ON, 89, 64); // Side button 1
            }
            else if (i == 15)
            {
                std::cout << "Switching to drum rack layout in session mode..." << std::endl;
                launchpad->setLayout(LaunchpadXConstants::LAYOUT_DRUM_RACK);
            }
        }

        std::cout << "\nCleaning up..." << std::endl;
        device->disconnect();
    }
    else
    {
        std::cout << "Failed to connect to Launchpad X. Running in simulation mode..." << std::endl;

        // Still demonstrate the session mode features
        demonstrateSessionMode(*launchpad);
    }

    std::cout << "\nSession mode test completed." << std::endl;
    return 0;
}
