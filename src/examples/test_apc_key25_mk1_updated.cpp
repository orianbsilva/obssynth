/**
 * @file test_apc_key25_mk1_updated.cpp
 * @brief Test the updated APC Key 25 Mk1 implementation with correct mappings
 *
 * This program tests the updated APC Key 25 Mk1 device implementation
 * with the correct MIDI mappings discovered from your hardware.
 */

#include "../lib/devices/apc_key25_mk1.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace synth;
using namespace examples;

static std::atomic<bool> running(true);

void signal_handler(int)
{
    running = false;
    exit(0);
}

int main()
{
    signal(SIGINT, signal_handler);

    PlatformUtils::initialize();

    PlatformUtils::printHeader("APC Key 25 Mk1 Updated Test",
                             "Testing updated implementation with correct MIDI mappings");

    std::cout << "Testing the new grid pad system and control mappings!\n\n";

    APCKey25Mk1 apc_device;

    // Set up all the callbacks to test the new mappings
    apc_device.setGridPadCallback([&apc_device](uint8_t channel, uint8_t note, uint8_t velocity, bool is_press) {
        std::cout << "🟩 Grid Pad " << static_cast<int>(note)
                  << (is_press ? " pressed" : " released")
                  << " (velocity: " << static_cast<int>(velocity)
                  << ", channel: " << static_cast<int>(channel) << ")\n";
        apc_device.setPadColor(note, is_press ? APCKey25Mk1Constants::PadColor::GREEN : APCKey25Mk1Constants::PadColor::OFF);
    });

    apc_device.setNavigationCallback([](uint8_t channel, uint8_t direction, bool is_press) {
        const char* dirs[] = {"UP", "DOWN", "LEFT", "RIGHT"};
        std::cout << "🧭 Navigation " << dirs[direction]
                  << (is_press ? " pressed" : " released")
                  << " (channel: " << static_cast<int>(channel) << ")\n";
    });

    apc_device.setFunctionCallback([](uint8_t channel, uint8_t function, bool is_press) {
        const char* funcs[] = {"VOLUME", "PAN", "SEND", "DEVICE"};
        std::cout << "⚙️  Function " << funcs[function]
                  << (is_press ? " pressed" : " released")
                  << " (channel: " << static_cast<int>(channel) << ")\n";
    });

    apc_device.setShiftCallback([](uint8_t channel, bool is_pressed) {
        std::cout << "⇧ Shift " << (is_pressed ? "pressed" : "released")
                  << " (channel: " << static_cast<int>(channel) << ")\n";
    });

    apc_device.setTransportCallback([](uint8_t channel, uint8_t button, bool is_press) {
        const char* buttons[] = {"PLAY", "STOP", "RECORD", "STOP_ALL_CLIPS"};
        std::cout << "⏯️  Transport " << buttons[button]
                  << (is_press ? " pressed" : " released")
                  << " (channel: " << static_cast<int>(channel) << ")\n";
    });

    apc_device.setFaderCallback([](uint8_t channel, uint8_t fader_index, uint8_t value) {
        std::cout << "🎚️  Fader " << static_cast<int>(fader_index)
                  << " = " << static_cast<int>(value)
                  << " (channel: " << static_cast<int>(channel) << ")\n";
    });

    apc_device.setSceneCallback([](uint8_t channel, uint8_t scene_index, bool is_press) {
        std::cout << "🎬 Scene " << static_cast<int>(scene_index)
                  << (is_press ? " pressed" : " released")
                  << " (channel: " << static_cast<int>(channel) << ")\n";
    });


    apc_device.setKeyboardCallback([](uint8_t channel, uint8_t key, uint8_t velocity, bool is_press) {
        std::cout << "⌨️  Keyboard " << static_cast<int>(key)
                  << " (velocity: " << static_cast<int>(velocity) << ")"
                  << " (channel: " << static_cast<int>(channel) << ")"
                  << (is_press ? " pressed" : " released") << "\n";
    });

    apc_device.setTopButtonCallback([](uint8_t channel, uint8_t button_index, bool is_press) {
        std::cout << "🔝 Top Button " << static_cast<int>(button_index)
                  << (is_press ? " pressed" : " released")
                  << " (channel: " << static_cast<int>(channel) << ")\n";
    });

    // Try to connect to device
    std::cout << "Searching for APC Key 25 Mk1...\n";
    if (!apc_device.connect(0))
    {
        std::cerr << "Failed to connect to APC Key 25 Mk1\n";
        return 1;
    }

    std::cout << "Connected successfully!\n\n";

 // Clear all pads first
    apc_device.clearAllPads();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // // Demonstrate different colors using enum values
    // std::cout << "Setting pad 0 to GREEN...\n";
    // apc_device.setPadColor(0, APCKey25Mk1Constants::PadColor::GREEN);
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    // std::cout << "Setting pad 1 to RED...\n";
    // apc_device.setPadColor(1, APCKey25Mk1Constants::PadColor::RED);
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    // std::cout << "Setting pad 2 to AMBER...\n";
    // apc_device.setPadColor(2, APCKey25Mk1Constants::PadColor::AMBER);
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    // std::cout << "Setting pad 3 to GREEN_BLINK...\n";
    // apc_device.setPadColor(3, APCKey25Mk1Constants::PadColor::GREEN_BLINK);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // std::cout << "Setting pad 4 to RED_BLINK...\n";
    // apc_device.setPadColor(4, APCKey25Mk1Constants::PadColor::RED_BLINK);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // std::cout << "Setting pad 5 to AMBER_BLINK...\n";
    // apc_device.setPadColor(5, APCKey25Mk1Constants::PadColor::AMBER_BLINK);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // // Demonstrate setAllGridPads
    // std::cout << "Setting all grid pads to GREEN...\n";
    // apc_device.setAllGridPads(APCKey25Mk1Constants::PadColor::GREEN);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    //     std::cout << "Setting all grid pads to GREEN_BLINK...\n";
    // apc_device.setAllGridPads(APCKey25Mk1Constants::PadColor::GREEN_BLINK);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // std::cout << "Setting all grid pads to RED...\n";
    // apc_device.setAllGridPads(APCKey25Mk1Constants::PadColor::RED);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // std::cout << "Setting all grid pads to RED_BLINK...\n";
    // apc_device.setAllGridPads(APCKey25Mk1Constants::PadColor::RED_BLINK);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // std::cout << "Setting all grid pads to AMBER...\n";
    // apc_device.setAllGridPads(APCKey25Mk1Constants::PadColor::AMBER);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // std::cout << "Setting all grid pads to AMBER_BLINK...\n";
    // apc_device.setAllGridPads(APCKey25Mk1Constants::PadColor::AMBER_BLINK);
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // // Clear all pads
    // std::cout << "Clearing all pads...\n";
    // apc_device.clearAllPads();

    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // std::cout << "Clearing all grid pads...\n";
    // apc_device.clearAllGridPads();

    std::cout << "\nNow test your controls!\n";
    std::cout << "Expected mappings:\n";
    std::cout << "- Grid pads: Notes 0-39 (40 pads total)\n";
    std::cout << "- Faders: CC 48-55\n";
    std::cout << "- Shift: CC 64\n";
    std::cout << "- Navigation: Notes 64-67 (Up/Down/Left/Right)\n";
    std::cout << "- Functions: Notes 68-71 (Volume/Pan/Send/Device)\n";
    std::cout << "- Stop All Clips: Note 81\n";
    std::cout << "- Scene buttons: Notes 82-86\n";
    std::cout << "- Transport: Play=91, Stop=92, Record=93\n\n";

    std::cout << "Press controls on your APC Key 25 to test the mappings!\n";
    std::cout << "Press Ctrl+C to exit.\n\n";

    // Main event loop
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\nDisconnecting...\n";
    apc_device.disconnect();

    PlatformUtils::cleanup();
    return 0;
}
