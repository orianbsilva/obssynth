/**
 * @brief Simple test to verify Launchkey MK4 device factory integration
 */

#include "devices/midi_device.hpp"
#include <iostream>

using namespace synth;

int main()
{
    std::cout << "Device Factory Test - Launchkey MK4" << std::endl;
    std::cout << "===================================" << std::endl;

    // Test device creation via factory
    auto device = DeviceFactory::createDevice("Launchkey MK4");

    if (!device)
    {
        std::cout << "ERROR: Failed to create Launchkey MK4 device!" << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: Created device: " << device->getDeviceName() << std::endl;
    std::cout << "Device Type: " << static_cast<int>(device->getDeviceType()) << std::endl;
    std::cout << "Grid Size: " << static_cast<int>(device->getGridWidth()) << "x"
              << static_cast<int>(device->getGridHeight()) << std::endl;

    // Test capabilities
    std::cout << "\nDevice Capabilities:" << std::endl;
    std::cout << "  RGB LEDs: " << (device->hasCapability(DeviceCapability::RGB_LEDS) ? "Yes" : "No") << std::endl;
    std::cout << "  Velocity Sensitive: " << (device->hasCapability(DeviceCapability::VELOCITY_SENSITIVE) ? "Yes" : "No") << std::endl;
    std::cout << "  Faders: " << (device->hasCapability(DeviceCapability::FADERS) ? "Yes" : "No") << std::endl;
    std::cout << "  Rotary Encoders: " << (device->hasCapability(DeviceCapability::ROTARY_ENCODERS) ? "Yes" : "No") << std::endl;
    std::cout << "  Navigation: " << (device->hasCapability(DeviceCapability::NAVIGATION) ? "Yes" : "No") << std::endl;

    // Test supported devices list
    std::cout << "\nSupported Devices:" << std::endl;
    auto supported = DeviceFactory::getSupportedDevices();
    for (const auto &dev : supported)
    {
        std::cout << "  - " << dev << std::endl;
    }

    // Test device type detection
    std::cout << "\nDevice Type Detection:" << std::endl;

    std::vector<std::string> test_names = {
        "Launchkey MK4",
        "Launchkey MK4 MIDIIN",
        "Novation Launchkey MK4",
        "Launchpad X",
        "Unknown Device"};

    for (const auto &name : test_names)
    {
        auto type = DeviceFactory::detectDeviceType(name);
        std::cout << "  \"" << name << "\" -> " << static_cast<int>(type) << std::endl;
    }

    std::cout << "\nDevice factory integration test completed successfully!" << std::endl;
    return 0;
}
