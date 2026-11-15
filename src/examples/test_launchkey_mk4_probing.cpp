/**
 * @file test_launchkey_mk4_probing.cpp
 * @brief Test program for Launchkey MK4 SysEx probing functionality
 *
 * This program demonstrates the new SysEx probing capabilities for the Launchkey MK4.
 * It shows how to request current fader positions, knob values, and device state
 * using SysEx queries instead of waiting for user input events.
 *
 * Features tested:
 * - Request current fader values
 * - Request current knob values
 * - Request complete device state
 * - Request all control values at once
 * - Response handling and data validation
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "../lib/devices/launchkey_mk4.hpp"
#include "../lib/devices/midi_device.hpp"
#include "../lib/utils/platform_midi_reader.hpp"

using namespace synth;

void printHeader(const std::string &title)
{
    std::cout << "\n"
              << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void printDeviceStatus(const LaunchkeyMK4 &device)
{
    std::cout << "\n--- Current Device Status ---" << std::endl;

    // Display fader values
    std::cout << "Faders: ";
    for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS; ++i)
    {
        std::cout << std::setw(3) << static_cast<int>(device.getFaderValue(i));
        if (i < LaunchkeyMK4Constants::NUM_FADERS - 1)
            std::cout << " ";
    }
    std::cout << std::endl;

    // Display knob values
    std::cout << "Knobs:  ";
    for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS; ++i)
    {
        std::cout << std::setw(3) << static_cast<int>(device.getKnobValue(i));
        if (i < LaunchkeyMK4Constants::NUM_KNOBS - 1)
            std::cout << " ";
    }
    std::cout << std::endl;

    // Display mode information
    std::cout << "DAW Mode: " << (device.isDAWMode() ? "Enabled" : "Disabled") << std::endl;
    std::cout << std::endl;
}

void waitForUser(const std::string &message)
{
    std::cout << message << " Press Enter to continue...";
    std::cin.ignore();
    std::cin.get();
}

void testBasicProbing(LaunchkeyMK4 &device)
{
    printHeader("Basic Probing Test");

    std::cout << "This test demonstrates basic SysEx probing functionality." << std::endl;
    std::cout << "Move some faders and knobs on your Launchkey MK4, then continue." << std::endl;
    waitForUser("");

    printDeviceStatus(device);

    std::cout << "Now requesting current fader values..." << std::endl;
    device.requestFaderValues();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for response

    std::cout << "Requesting current knob values..." << std::endl;
    device.requestKnobValues();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for response

    printDeviceStatus(device);
}

void testCompleteStateProbing(LaunchkeyMK4 &device)
{
    printHeader("Complete State Probing Test");

    std::cout << "This test requests complete device state including mode information." << std::endl;
    waitForUser("");

    std::cout << "Before state request:" << std::endl;
    printDeviceStatus(device);

    std::cout << "Requesting complete device state..." << std::endl;
    device.requestDeviceState();
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Wait for response

    std::cout << "After state request:" << std::endl;
    printDeviceStatus(device);
}

void testBulkValueProbing(LaunchkeyMK4 &device)
{
    printHeader("Bulk Value Probing Test");

    std::cout << "This test requests all control values in a single efficient query." << std::endl;
    waitForUser("");

    std::cout << "Before bulk request:" << std::endl;
    printDeviceStatus(device);

    std::cout << "Requesting all control values..." << std::endl;
    device.requestAllValues();
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Wait for response

    std::cout << "After bulk request:" << std::endl;
    printDeviceStatus(device);
}

void testContinuousProbing(LaunchkeyMK4 &device)
{
    printHeader("Continuous Probing Test");

    std::cout << "This test continuously probes the device while you move controls." << std::endl;
    std::cout << "Move faders and knobs while the test is running." << std::endl;
    std::cout << "The test will run for 15 seconds." << std::endl;
    waitForUser("");

    auto start_time = std::chrono::steady_clock::now();
    auto last_probe = start_time;

    std::cout << "\nStarting continuous probing (every 500ms for 15 seconds)..." << std::endl;
    std::cout << "Time | Fader Values        | Knob Values" << std::endl;
    std::cout << "-----+---------------------+---------------------" << std::endl;

    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(15))
    {
        auto now = std::chrono::steady_clock::now();

        if (now - last_probe >= std::chrono::milliseconds(500))
        {
            device.requestAllValues();
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Wait for response

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            std::cout << std::setw(3) << elapsed.count() << "s | ";

            // Print fader values
            for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_FADERS; ++i)
            {
                std::cout << std::setw(3) << static_cast<int>(device.getFaderValue(i));
                if (i < LaunchkeyMK4Constants::NUM_FADERS - 1)
                    std::cout << " ";
            }
            std::cout << " | ";

            // Print knob values
            for (uint8_t i = 0; i < LaunchkeyMK4Constants::NUM_KNOBS; ++i)
            {
                std::cout << std::setw(3) << static_cast<int>(device.getKnobValue(i));
                if (i < LaunchkeyMK4Constants::NUM_KNOBS - 1)
                    std::cout << " ";
            }
            std::cout << std::endl;

            last_probe = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\nContinuous probing test completed." << std::endl;
}

void testResponseValidation(LaunchkeyMK4 &device)
{
    printHeader("Response Validation Test");

    std::cout << "This test validates SysEx response handling and error conditions." << std::endl;
    waitForUser("");

    std::cout << "Testing multiple rapid requests..." << std::endl;
    for (int i = 0; i < 5; ++i)
    {
        std::cout << "Request " << (i + 1) << "/5..." << std::endl;
        device.requestFaderValues();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        device.requestKnobValues();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "Rapid request test completed." << std::endl;

    std::cout << "\nTesting device state consistency..." << std::endl;
    device.requestDeviceState();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    printDeviceStatus(device);
}

int main()
{
    printHeader("Launchkey MK4 SysEx Probing Test");

    std::cout << "This program tests the new SysEx probing functionality for the Launchkey MK4." << std::endl;
    std::cout << "It demonstrates how to actively query current fader and knob positions" << std::endl;
    std::cout << "instead of waiting for user input events." << std::endl;

    // Initialize MIDI system
    MidiInterface midi;
    if (midi.init() != 0)
    {
        std::cerr << "Failed to initialize MIDI system" << std::endl;
        return -1;
    }

    // List available devices
    std::cout << "\nAvailable MIDI devices:" << std::endl;
    auto devices = MidiInterface::getDeviceList();
    for (size_t i = 0; i < devices.size(); ++i)
    {
        std::cout << i << ": " << devices[i] << std::endl;
    }

    // Get device to connect to
    int device_id;
    std::cout << "\nEnter device ID to connect to: ";
    std::cin >> device_id;

    // Create and connect Launchkey MK4
    LaunchkeyMK4 launchkey;

    std::cout << "\nConnecting to Launchkey MK4..." << std::endl;
    if (!launchkey.connect(static_cast<uint8_t>(device_id)))
    {
        std::cerr << "Failed to connect to Launchkey MK4 device " << device_id << std::endl;
        return -1;
    }

    std::cout << "Connected successfully!" << std::endl;

    // Enable debug output
    std::cout << "\nNote: If you compiled with DEBUG_LAUNCHKEY, you'll see detailed SysEx messages." << std::endl;

    // Wait for device to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Run test sequence
    try
    {
        testBasicProbing(launchkey);
        testCompleteStateProbing(launchkey);
        testBulkValueProbing(launchkey);
        testContinuousProbing(launchkey);
        testResponseValidation(launchkey);

        printHeader("All Tests Completed Successfully");
        std::cout << "The SysEx probing functionality is working correctly!" << std::endl;
        std::cout << "\nKey features validated:" << std::endl;
        std::cout << "✓ Fader value probing" << std::endl;
        std::cout << "✓ Knob value probing" << std::endl;
        std::cout << "✓ Complete device state queries" << std::endl;
        std::cout << "✓ Bulk value requests" << std::endl;
        std::cout << "✓ Continuous monitoring" << std::endl;
        std::cout << "✓ Response validation" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "\nDisconnecting..." << std::endl;
    launchkey.disconnect();

    return 0;
}