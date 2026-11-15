/**
 * @file test_apc_key25_mk2.cpp
 * @brief Test application for APC Key 25 mk2 MIDI Controller
 *
 * This test application demonstrates the capabilities of the APC Key 25 mk2
 * device implementation by lighting up pads, testing transport buttons,
 * and responding to keyboard input and fader movements.
 */

#include "../lib/devices/apc_key25_mk2.hpp"
#include "../lib/devices/midi_device.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cmath>
#include <csignal>

using namespace synth;
using namespace examples;

// Global state for the test
std::atomic<bool> g_running{true};
std::unique_ptr<APCKey25Mk2> g_apc_device = nullptr;

// Demo functions
void demonstratePadLighting(APCKey25Mk2& device);
void demonstrateTransportButtons(APCKey25Mk2& device);
void demonstrateInteractiveMode(APCKey25Mk2& device);
void setupCallbacks(APCKey25Mk2& device);

void signalHandler(int /* signal */)
{
    std::cout << "\nShutting down APC Key 25 mk2 test..." << std::endl;
    g_running.store(false);
    exit(0);
}

int main()
{
    std::cout << "APC Key 25 mk2 Device Test Application" << std::endl;
    std::cout << "=====================================" << std::endl;

    // Set up signal handling for clean shutdown
    signal(SIGINT, signalHandler);

    // Create APC Key 25 mk2 device
    g_apc_device = std::make_unique<APCKey25Mk2>();

    // Try to find and connect to APC Key 25 mk2
    std::cout << "Searching for APC Key 25 mk2 devices..." << std::endl;

    auto devices = MidiInterface::getDeviceList();
    std::cout << "Available MIDI devices:" << std::endl;
    for (size_t i = 0; i < devices.size(); ++i)
    {
        std::cout << "  " << i << ": " << devices[i] << std::endl;
    }

    // Try to auto-detect APC Key 25 mk2
    int device_id = -1;
    for (size_t i = 0; i < devices.size(); ++i)
    {
        std::string device_name = devices[i];
        std::transform(device_name.begin(), device_name.end(), device_name.begin(), ::tolower);

        if (device_name.find("apc") != std::string::npos &&
            device_name.find("key") != std::string::npos)
        {
            device_id = static_cast<int>(i);
            std::cout << "Found APC Key 25 mk2 at device " << i << ": " << devices[i] << std::endl;
            break;
        }
    }

    if (device_id == -1)
    {
        std::cout << "APC Key 25 mk2 not found. Please select device manually." << std::endl;
        std::cout << "Enter device number (or -1 to simulate): ";
        std::cin >> device_id;

        if (device_id == -1)
        {
            std::cout << "Running in simulation mode (no actual device)." << std::endl;
        }
    }

    // Connect to device
    bool connected = false;
    if (device_id >= 0 && device_id < static_cast<int>(devices.size()))
    {
        connected = g_apc_device->connect(static_cast<uint8_t>(device_id));
        if (!connected)
        {
            std::cout << "Failed to connect to device. Running in simulation mode." << std::endl;
        }
    }

    if (connected)
    {
        std::cout << "Successfully connected to APC Key 25 mk2!" << std::endl;
    }
    else
    {
        std::cout << "Running demonstrations in simulation mode." << std::endl;
    }

    // Set up device callbacks
    setupCallbacks(*g_apc_device);

    std::cout << "\n=== APC Key 25 mk2 Feature Demonstrations ===" << std::endl;
    std::cout << "Press Ctrl+C to exit at any time." << std::endl;

    try
    {
        // Demo 1: Pad Lighting
        if (g_running.load())
        {
            demonstratePadLighting(*g_apc_device);
        }

        // Demo 2: Transport Buttons
        if (g_running.load())
        {
            demonstrateTransportButtons(*g_apc_device);
        }

        // Demo 3: Interactive Mode
        if (g_running.load())
        {
            demonstrateInteractiveMode(*g_apc_device);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
    }

    // Clean up
    if (g_apc_device)
    {
        g_apc_device->disconnect();
    }

    std::cout << "APC Key 25 mk2 test completed." << std::endl;
    return 0;
}

void demonstratePadLighting(APCKey25Mk2& device)
{
    std::cout << "\n1. Pad Lighting Demonstration" << std::endl;
    std::cout << "-----------------------------" << std::endl;

    // Test 1: Individual pad colors
    std::cout << "Setting individual pad colors..." << std::endl;
    device.setPadColor(0, RGBColor::RED);
    device.setPadColor(1, RGBColor::GREEN);
    device.setPadColor(2, RGBColor::BLUE);
    device.setPadColor(3, RGBColor::YELLOW);
    device.setPadColor(4, RGBColor::CYAN);
    device.setPadColor(5, RGBColor::MAGENTA);
    device.setPadColor(6, RGBColor::WHITE);
    device.setPadColor(7, RGBColor(255, 127, 0)); // Orange
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test 2: Clear all pads
    std::cout << "Clearing all pads..." << std::endl;
    device.clearAllPads();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Test 3: All pads same color
    std::cout << "Setting all pads to blue..." << std::endl;
    device.setAllPads(RGBColor::BLUE);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 4: Rainbow pattern
    std::cout << "Creating rainbow pattern..." << std::endl;
    device.rainbowPattern();
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test 5: Test all pads cycling
    std::cout << "Testing all pads with cycling colors..." << std::endl;
    device.testAllPads();

    // Test 6: Flash individual pads
    std::cout << "Flashing individual pads..." << std::endl;
    device.clearAllPads();
    for (uint8_t i = 0; i < 8; ++i)
    {
        device.flashPad(i, RGBColor::WHITE, 150);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    device.clearAllPads();
    std::cout << "Pad lighting demonstration complete." << std::endl;
}

void demonstrateTransportButtons(APCKey25Mk2& device)
{
    std::cout << "\n2. Transport Button Demonstration" << std::endl;
    std::cout << "---------------------------------" << std::endl;

    // Light up transport buttons
    std::cout << "Lighting transport buttons..." << std::endl;
    device.setTransportButtonLED(0, APCKey25Mk2Constants::ButtonLED::ON);  // Play
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    device.setTransportButtonLED(1, APCKey25Mk2Constants::ButtonLED::ON);  // Stop
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    device.setTransportButtonLED(2, APCKey25Mk2Constants::ButtonLED::ON);  // Record
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Blink effect simulation
    std::cout << "Simulating blink effect..." << std::endl;
    for (int i = 0; i < 5; ++i)
    {
        device.setTransportButtonLED(2, APCKey25Mk2Constants::ButtonLED::OFF);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        device.setTransportButtonLED(2, APCKey25Mk2Constants::ButtonLED::ON);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Test scene buttons
    std::cout << "Testing scene buttons..." << std::endl;
    for (uint8_t i = 0; i < 5; ++i)
    {
        device.setSceneButtonLED(i, APCKey25Mk2Constants::ButtonLED::ON);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Turn off all buttons
    std::cout << "Turning off all buttons..." << std::endl;
    for (uint8_t i = 0; i < 3; ++i)
    {
        device.setTransportButtonLED(i, APCKey25Mk2Constants::ButtonLED::OFF);
    }
    for (uint8_t i = 0; i < 5; ++i)
    {
        device.setSceneButtonLED(i, APCKey25Mk2Constants::ButtonLED::OFF);
    }

    std::cout << "Transport button demonstration complete." << std::endl;
}

void demonstrateInteractiveMode(APCKey25Mk2& device)
{
    std::cout << "\n3. Interactive Mode" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << "Play the keyboard and pads - they will light up!" << std::endl;
    std::cout << "Move faders and encoders to see their values." << std::endl;
    std::cout << "Press transport buttons to see their status." << std::endl;
    std::cout << "Press Ctrl+C to exit interactive mode." << std::endl;

    // Create a visual feedback system
    device.clearAllPads();

    // Set initial transport button state
    device.setTransportButtonLED(1, APCKey25Mk2Constants::ButtonLED::ON); // Stop button on

    // Interactive loop
    auto start_time = std::chrono::steady_clock::now();
    while (g_running.load())
    {
        // Create a breathing effect on pads if no interaction
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();

        if (elapsed > 5000) // After 5 seconds of no interaction
        {
            float wave = std::sin(elapsed * 0.002f) * 0.5f + 0.5f; // 0.0 to 1.0
            uint8_t intensity = static_cast<uint8_t>(wave * 255);
            RGBColor breathing_color(0, intensity / 4, intensity / 2);

            // Apply breathing effect to first pad
            device.setPadColor(0, breathing_color);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Clean up
    device.clearAllPads();
    for (uint8_t i = 0; i < 3; ++i)
    {
        device.setTransportButtonLED(i, APCKey25Mk2Constants::ButtonLED::OFF);
    }

    std::cout << "Interactive mode ended." << std::endl;
}

void setupCallbacks(APCKey25Mk2& device)
{
    // Keyboard callback - light up corresponding pad when key is pressed
    device.setKeyboardCallback([&device](uint8_t channel, uint8_t note, uint8_t velocity, bool is_press) {
        std::cout << "Keyboard: Note " << static_cast<int>(note)
                  << " velocity " << static_cast<int>(velocity)
                  << " channel " << static_cast<int>(channel)
                  << (is_press ? " pressed" : " released") << std::endl;

        // Map keyboard notes to pad colors for visual feedback
        if (is_press && velocity > 0)
        {
            uint8_t pad_index = (note - APCKey25Mk2Constants::KEYBOARD_START_NOTE) % 8;
            RGBColor color = (velocity > 100) ? RGBColor::WHITE :
                           (velocity > 80) ? RGBColor::YELLOW :
                           (velocity > 60) ? RGBColor::GREEN :
                           (velocity > 40) ? RGBColor::BLUE : RGBColor::RED;
            device.setPadColor(pad_index, color);
        }
    });

    // Pad callback - provide audio feedback and visual response
    device.setPadCallback([&device](uint8_t channel, const PadPosition& position, uint8_t velocity, bool is_press) {
        std::cout << "Pad: Position (" << static_cast<int>(position.x)
                  << ", " << static_cast<int>(position.y) << ") "
                  << "velocity " << static_cast<int>(velocity)
                  << " channel " << static_cast<int>(channel)
                  << (is_press ? " pressed" : " released") << std::endl;

        if (is_press)
        {
            // Create visual feedback based on velocity
            RGBColor color = (velocity > 100) ? RGBColor::WHITE :
                           (velocity > 80) ? RGBColor::CYAN :
                           (velocity > 60) ? RGBColor::MAGENTA :
                           (velocity > 40) ? RGBColor::YELLOW : RGBColor::GREEN;
            device.setPadColor(position, color);
        }
        else
        {
            // Fade out pad on release
            device.setPadColor(position, RGBColor::OFF);
        }
    });

    // Fader callback
    device.setFaderCallback([](uint8_t channel, uint8_t fader_index, uint8_t value) {
        std::cout << "Fader " << static_cast<int>(fader_index)
                  << ": " << static_cast<int>(value)
                  << " channel " << static_cast<int>(channel) << std::endl;
    });

    // Encoder callback
    device.setEncoderCallback([](uint8_t channel, uint8_t encoder_index, uint8_t value) {
        std::cout << "Encoder " << static_cast<int>(encoder_index)
                  << ": " << static_cast<int>(value)
                  << " channel " << static_cast<int>(channel) << std::endl;
    });

    // Transport callback
    device.setTransportCallback([&device](uint8_t channel, uint8_t button, bool is_press) {
        const char* button_names[] = {"Play", "Stop", "Record"};
        std::cout << "Transport: " << button_names[button]
                  << " channel " << static_cast<int>(channel)
                  << (is_press ? " pressed" : " released") << std::endl;

        if (is_press)
        {
            // Toggle button LED state
            uint8_t new_state = APCKey25Mk2Constants::ButtonLED::ON;
            device.setTransportButtonLED(button, new_state);

            // Turn off other transport buttons for exclusive selection
            for (uint8_t i = 0; i < 3; ++i)
            {
                if (i != button)
                {
                    device.setTransportButtonLED(i, APCKey25Mk2Constants::ButtonLED::OFF);
                }
            }
        }
    });

    // Scene callback
    device.setSceneCallback([&device](uint8_t channel, uint8_t scene_index, bool is_press) {
        std::cout << "Scene " << static_cast<int>(scene_index + 1)
                  << " channel " << static_cast<int>(channel)
                  << (is_press ? " pressed" : " released") << std::endl;

        if (is_press)
        {
            // Light up scene button
            device.setSceneButtonLED(scene_index, APCKey25Mk2Constants::ButtonLED::ON);

            // Create scene-specific pad pattern
            device.clearAllPads();
            RGBColor scene_colors[] = {
                RGBColor::RED, RGBColor::GREEN, RGBColor::BLUE,
                RGBColor::YELLOW, RGBColor::MAGENTA
            };
            RGBColor color = scene_colors[scene_index % 5];
            device.setAllPads(color);
        }
        else
        {
            device.setSceneButtonLED(scene_index, APCKey25Mk2Constants::ButtonLED::OFF);
        }
    });
}
