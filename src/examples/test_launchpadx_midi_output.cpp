/**
 * @file test_launchpadx_midi_output.cpp
 * @brief Test program specifically for Launchpad X MIDI output functionality
 *
 * This program demonstrates the real MIDI output implementation by sending
 * various MIDI commands to the Launchpad X and showing visual feedback.
 */

#include "../lib/devices/launchpadx.hpp"
#include "../lib/devices/midi_device.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

using namespace synth;

void testMidiOutput(LaunchpadX& launchpad)
{
    std::cout << "\n=== Launchpad X MIDI Output Test ===" << std::endl;

    // Test 1: SysEx Commands (Mode Switching)
    std::cout << "1. Testing SysEx commands..." << std::endl;
    std::cout << "   Entering programmer mode..." << std::endl;
    launchpad.enterProgrammerMode();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "   Resetting device..." << std::endl;
    launchpad.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    launchpad.setLogoColor(RGBColor::MAGENTA);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Test 2: Note On/Off Commands (LED Control)
    std::cout << "2. Testing Note On/Off commands (LED control)..." << std::endl;

    // Light up individual pads with different colors
    std::cout << "   Lighting pads with different colors..." << std::endl;



    // Test the entire 8x8 grid with different colors
    std::cout << "   Lighting up the entire grid sequentially..." << std::endl;

    // Array of different colors to use
    const RGBColor colors[] = {
      RGBColor::RED, RGBColor::GREEN, RGBColor::BLUE,
      RGBColor::YELLOW, RGBColor::CYAN, RGBColor::MAGENTA,
      RGBColor::WHITE
    };

    // Light up each pad in the grid
    for (uint8_t y = 0; y < 8; ++y) {
      for (uint8_t x = 0; x < 8; ++x) {
        // Use a different color for each row
        RGBColor color = colors[y % 8];
        launchpad.setPadColor(PadPosition(x, y), color);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }

    // Wait a moment to see the full grid lit up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Clear all pads
    std::cout << "   Clearing all pads..." << std::endl;
    launchpad.clearAllPads();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));



    // Test 3: Side Button LEDs
    std::cout << "3. Testing side button LEDs..." << std::endl;
    for (uint8_t i = 0; i < 8; ++i)
    {
        launchpad.setSideButtonColor(i, RGBColor::CYAN);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    // Test 4: Top Button LEDs
    std::cout << "4. Testing top button LEDs..." << std::endl;
    for (uint8_t i = 0; i < 9; ++i)
    {
        launchpad.setTopButtonColor(i, RGBColor::MAGENTA);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    // Test 5: Pattern Display
    std::cout << "5. Testing pattern display..." << std::endl;
    launchpad.checkerboardPattern(RGBColor::RED, RGBColor::GREEN);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    launchpad.rainbowPattern();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Test 6: All Pads Control
    std::cout << "6. Testing all pads control..." << std::endl;
    launchpad.setAllPads(RGBColor::BLUE);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    launchpad.clearAllPads();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Test 7: Session Mode SysEx
    std::cout << "7. Testing session mode SysEx..." << std::endl;
    launchpad.enterSessionMode();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    launchpad.setLayout(LaunchpadXConstants::LAYOUT_DRUM_RACK);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    launchpad.exitSessionMode();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Test 8: Flash Effect
    std::cout << "8. Testing flash effects..." << std::endl;
    launchpad.flashPad(PadPosition(3, 3), RGBColor::WHITE, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));




    launchpad.enterSessionMode();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    launchpad.setLayout(LaunchpadXConstants::LAYOUT_CHROMATIC);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    launchpad.setLogoColor(RGBColor::MAGENTA);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));


    std::cout << "\nMIDI Output test completed!" << std::endl;
    std::cout << "If you have a Launchpad X connected, you should see:" << std::endl;
    std::cout << "- LEDs lighting up in different colors" << std::endl;
    std::cout << "- Side and top buttons changing colors" << std::endl;
    std::cout << "- Pattern displays (checkerboard, rainbow)" << std::endl;
    std::cout << "- Flash effects" << std::endl;
}

int main()
{
    std::cout << "Launchpad X MIDI Output Test" << std::endl;
    std::cout << "============================" << std::endl;

    // Create Launchpad X device
    auto device = std::make_unique<LaunchpadX>();
    LaunchpadX* launchpad = device.get();

    // Display device information
    std::cout << "\nDevice: " << device->getDeviceName() << std::endl;
    std::cout << "Type: " << static_cast<int>(device->getDeviceType()) << std::endl;

    // List available MIDI devices
    std::cout << "\nSearching for MIDI devices..." << std::endl;

    // In real usage, you would get this from MidiInterface::getDeviceList()
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

    // Attempt to connect
    std::cout << "\nAttempting to connect to Launchpad X..." << std::endl;
    bool connected = device->connect(static_cast<uint8_t>(launchpad_device_id));

    if (connected)
    {
        std::cout << "Successfully connected to Launchpad X!" << std::endl;
        std::cout << "MIDI Input: ✓" << std::endl;
        std::cout << "MIDI Output: ✓" << std::endl;

        // Test MIDI output functionality
        testMidiOutput(*launchpad);

        std::cout << "\nCleaning up..." << std::endl;
        device->disconnect();
    }
    else
    {
        std::cout << "Failed to connect to Launchpad X." << std::endl;
        std::cout << "Possible reasons:" << std::endl;
        std::cout << "- Device not connected" << std::endl;
        std::cout << "- Device in use by another application" << std::endl;
        std::cout << "- Permission issues" << std::endl;
        std::cout << "- MIDI drivers not installed" << std::endl;
        return 1;
    }

    std::cout << "\nMIDI Output test completed successfully!" << std::endl;
    return 0;
}
