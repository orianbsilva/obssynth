/**
 * @file test_apc_key25_mk1.cpp
 * @brief Test application for APC Key 25 Mk1 device
 *
 * This program demonstrates the capabilities of the APC Key 25 Mk1
 * implementation and allows you to test the correct MIDI note mappings.
 * Use this to verify if the pad notes are correct for your Mk1 device.
 */

#include "../lib/devices/apc_key25_mk1.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace synth;

static std::atomic<bool> running(true);

void signal_handler(int)
{
    running = false;
    std::cout << "Exiting...\n";
    exit(0);
}


void print_usage()
{
    std::cout << "\nAPC Key 25 Mk1 Device Test Application\n";
    std::cout << "=====================================\n\n";
    std::cout << "This application tests the APC Key 25 Mk1 implementation.\n";
    std::cout << "It will attempt to connect to your device and demonstrate\n";
    std::cout << "various features including pad lighting and button control.\n\n";
    std::cout << "Expected pad note mapping for Mk1:\n";
    std::cout << "Pad 1: Note 36, Pad 2: Note 37, Pad 3: Note 38, Pad 4: Note 39\n";
    std::cout << "Pad 5: Note 40, Pad 6: Note 41, Pad 7: Note 42, Pad 8: Note 43\n\n";
    std::cout << "If the pads don't light up correctly, the note mapping may be wrong.\n";
    std::cout << "Use the discovery tool to find the correct note numbers.\n\n";
}

int main()
{
    signal(SIGINT, signal_handler);
    print_usage();

    // Find APC Key 25 device
    auto midi_reader = std::make_shared<MidiInterface>();
    if (midi_reader->init() != 0)
    {
        std::cerr << "Failed to initialize MIDI interface" << std::endl;
        return 1;
    }

    std::cout << "Searching for APC Key 25 Mk1 devices...\n";
    auto devices = midi_reader->getDeviceList();

    std::cout << "Available MIDI devices:\n";
    for (size_t i = 0; i < devices.size(); i++)
    {
        std::cout << "  " << i << ": " << devices[i] << std::endl;
    }

    int device_id = -1;
    for (size_t i = 0; i < devices.size(); i++)
    {
        const std::string& device_name = devices[i];
        if (device_name.find("APC Key 25") != std::string::npos ||
            device_name.find("APC") != std::string::npos)
        {
            device_id = static_cast<int>(i);
            std::cout << "Found APC Key 25 Mk1 at device " << device_id << ": " << device_name << std::endl;
            break;
        }
    }

    if (device_id == -1)
    {
        std::cout << "APC Key 25 device not automatically detected.\n";
        std::cout << "Please enter device ID manually (0-" << (devices.size() - 1) << "): ";
        std::cin >> device_id;

        if (device_id < 0 || device_id >= static_cast<int>(devices.size()))
        {
            std::cerr << "Invalid device ID" << std::endl;
            return 1;
        }
    }

    // Connect to device
    APCKey25Mk1 apc_device;
    if (!apc_device.connect(device_id))
    {
        std::cerr << "Failed to connect to APC Key 25 Mk1" << std::endl;
        return 1;
    }

    std::cout << "Successfully connected to APC Key 25 Mk1!\n\n";

    std::cout << "=== APC Key 25 Mk1 Feature Demonstrations ===\n";
    std::cout << "Press Ctrl+C to exit at any time.\n\n";

    // 1. Pad Lighting Test
    std::cout << "1. Pad Lighting Test\n";
    std::cout << "--------------------\n";
    std::cout << "Testing individual pad colors...\n";

    std::cout << "Clearing all pads...\n";
    apc_device.clearAllPads();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Pad lighting test complete.\n\n";

    // 2. Scene Button Test
    std::cout << "2. Scene Button Test\n";
    std::cout << "--------------------\n";
    std::cout << "Testing scene buttons (these should already work)...\n";

    for (uint8_t i = 0; i < 5; i++)
    {
        std::cout << "Lighting scene button " << static_cast<int>(i + 1) << std::endl;
        apc_device.setSceneButtonLED(i, APCKey25Mk1Constants::ButtonLED::ON);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        apc_device.setSceneButtonLED(i, APCKey25Mk1Constants::ButtonLED::OFF);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Scene button test complete.\n\n";

    // 3. Transport Button Test
    std::cout << "3. Transport Button Test\n";
    std::cout << "------------------------\n";
    std::cout << "Testing transport buttons...\n";

    apc_device.setTransportButtonLED(APCKey25Mk1Constants::TRANSPORT_PLAY, APCKey25Mk1Constants::ButtonLED::ON);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    apc_device.setTransportButtonLED(APCKey25Mk1Constants::TRANSPORT_PLAY, APCKey25Mk1Constants::ButtonLED::OFF);

    apc_device.setTransportButtonLED(APCKey25Mk1Constants::TRANSPORT_STOP, APCKey25Mk1Constants::ButtonLED::ON);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    apc_device.setTransportButtonLED(APCKey25Mk1Constants::TRANSPORT_STOP, APCKey25Mk1Constants::ButtonLED::OFF);

    apc_device.setTransportButtonLED(APCKey25Mk1Constants::TRANSPORT_RECORD, APCKey25Mk1Constants::ButtonLED::ON);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    apc_device.setTransportButtonLED(APCKey25Mk1Constants::TRANSPORT_RECORD, APCKey25Mk1Constants::ButtonLED::OFF);

    std::cout << "Transport button test complete.\n\n";

    // 4. Interactive Mode
    std::cout << "4. Interactive Mode\n";
    std::cout << "-------------------\n";
    std::cout << "Play the keyboard and pads - they will light up!\n";
    std::cout << "Move faders and encoders to see their values.\n";
    std::cout << "Press transport buttons to see their status.\n";
    std::cout << "Press Ctrl+C to exit interactive mode.\n\n";

    std::cout << "IMPORTANT: If pads don't light up when pressed, the pad note\n";
    std::cout << "mapping is incorrect for your Mk1 device. Use the discovery\n";
    std::cout << "tool to find the correct note numbers.\n\n";

    // Interactive event processing loop
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\nShutting down APC Key 25 Mk1 test...\n";
    apc_device.disconnect();

    return 0;
}
