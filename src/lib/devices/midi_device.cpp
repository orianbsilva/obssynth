#include "midi_device.hpp"
#include "launchpadx.hpp"
#include "launchkey_mk4.hpp"
#include "../utils/platform_midi_reader.hpp"
#include <algorithm>
#include <cctype>

namespace synth
{
    // Predefined colors
    const RGBColor RGBColor::OFF(0, 0, 0);
    const RGBColor RGBColor::RED(255, 0, 0);
    const RGBColor RGBColor::GREEN(0, 255, 0);
    const RGBColor RGBColor::BLUE(0, 0, 255);
    const RGBColor RGBColor::YELLOW(255, 255, 0);
    const RGBColor RGBColor::CYAN(0, 255, 255);
    const RGBColor RGBColor::MAGENTA(255, 0, 255);
    const RGBColor RGBColor::WHITE(255, 255, 255);

    MidiDevice::MidiDevice(const std::string &name, DeviceType type, uint32_t capabilities)
        : device_name_(name), device_type_(type), capabilities_(capabilities), device_id_(0), is_connected_(false)
    {
    }

    bool MidiDevice::hasCapability(DeviceCapability cap) const
    {
        return (capabilities_ & static_cast<uint32_t>(cap)) != 0;
    }

    void MidiDevice::triggerPadCallback(uint8_t channel, const PadPosition &position, uint8_t velocity, bool is_press)
    {
        if (pad_callback_)
        {
            pad_callback_(channel, position, velocity, is_press);
        }
    }

    void MidiDevice::triggerSideButtonCallback(uint8_t channel, uint8_t button_id, bool is_press)
    {
        if (side_button_callback_)
        {
            side_button_callback_(channel, button_id, is_press);
        }
    }

    void MidiDevice::triggerTopButtonCallback(uint8_t channel, uint8_t button_id, bool is_press)
    {
        if (top_button_callback_)
        {
            top_button_callback_(channel, button_id, is_press);
        }
    }

    void MidiDevice::triggerAftertouchCallback(uint8_t channel, const PadPosition &position, uint8_t pressure)
    {
        if (aftertouch_callback_)
        {
            aftertouch_callback_(channel, position, pressure);
        }
    }

    void MidiDevice::triggerNavigationCallback(uint8_t channel, NavigationDirection direction)
    {
        if (navigation_callback_)
        {
            navigation_callback_(channel, direction);
        }
    }

    // Device Factory Implementation
    std::unique_ptr<MidiDevice> DeviceFactory::createDevice(const std::string &device_name)
    {
        DeviceType type = detectDeviceType(device_name);

        switch (type)
        {
        case DeviceType::LAUNCHPAD_X:
            // Forward declaration - will be implemented in launchpadx.cpp
            return createLaunchpadX(device_name);

        case DeviceType::LAUNCHKEY_MK4:
            // Forward declaration - will be implemented in launchkey_mk4.cpp
            return createLaunchkeyMK4(device_name);

        case DeviceType::LAUNCHPAD_PRO:
        case DeviceType::LAUNCHPAD_MINI:
        case DeviceType::GENERIC_GRID_CONTROLLER:
        case DeviceType::KEYBOARD_CONTROLLER:
        case DeviceType::UNKNOWN:
        default:
            return nullptr; // Not implemented yet
        }
    }

    std::vector<std::string> DeviceFactory::getSupportedDevices()
    {
        return {
            "Launchpad X",
            "Launchpad Pro",
            "Launchpad Mini",
            "Launchkey MK4"};
    }

    DeviceType DeviceFactory::detectDeviceType(const std::string &device_name)
    {
        std::string name_lower = device_name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        if (name_lower.find("launchpad x") != std::string::npos ||
            name_lower.find("launchpadx") != std::string::npos)
        {
            return DeviceType::LAUNCHPAD_X;
        }
        else if (name_lower.find("launchpad pro") != std::string::npos)
        {
            return DeviceType::LAUNCHPAD_PRO;
        }
        else if (name_lower.find("launchpad mini") != std::string::npos)
        {
            return DeviceType::LAUNCHPAD_MINI;
        }
        else if (name_lower.find("launchpad") != std::string::npos)
        {
            return DeviceType::LAUNCHPAD_X; // Default to X for unknown Launchpads
        }
        else if (name_lower.find("launchkey") != std::string::npos &&
                 name_lower.find("mk4") != std::string::npos)
        {
            return DeviceType::LAUNCHKEY_MK4;
        }
        else if (name_lower.find("launchkey") != std::string::npos)
        {
            return DeviceType::LAUNCHKEY_MK4; // Default to MK4 for unknown Launchkeys
        }
        else if (name_lower.find("apc") != std::string::npos &&
                 name_lower.find("key") != std::string::npos)
        {
            if (name_lower.find("mk2") != std::string::npos)
            {
                return DeviceType::APC_KEY_25_MK2;
            }
            else
            {
                return DeviceType::APC_KEY_25_MK1;
            }
        }

        return DeviceType::UNKNOWN;
    }

} // namespace synth
