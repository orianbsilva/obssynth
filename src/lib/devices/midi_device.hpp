/**
 * @file midi_device.hpp
 * @brief MIDI device abstraction layer for hardware controllers
 *
 * Provides unified interface for various MIDI hardware controllers including:
 * - Novation Launchpad X, Launchkey MK4, APC Key 25
 * - Generic MIDI keyboards and control surfaces
 * - RGB LED control and feedback
 * - Velocity-sensitive pads and aftertouch support
 *
 * Supports device-specific features like session modes, LED patterns,
 * and control surface layouts while maintaining a consistent API.
 */

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace synth
{
    // Forward declarations
    struct MidiNote;
    class MidiInterface;

    /**
     * @brief RGB color structure for devices with colored LEDs
     *
     * Represents RGB color values for LED-equipped MIDI controllers.
     * Includes predefined color constants for common use cases.
     *
     * @code
     * RGBColor red(255, 0, 0);
     * RGBColor green = RGBColor::GREEN;
     * @endcode
     */
    struct RGBColor
    {
        uint8_t red;   ///< Red component (0-255)
        uint8_t green; ///< Green component (0-255)
        uint8_t blue;  ///< Blue component (0-255)

        /**
         * @brief Default constructor - creates black color (0,0,0)
         */
        RGBColor() : red(0), green(0), blue(0) {}

        /**
         * @brief Construct RGB color with specific values
         * @param r Red component (0-255)
         * @param g Green component (0-255)
         * @param b Blue component (0-255)
         */
        RGBColor(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}

        // Predefined colors for common LED patterns
        static const RGBColor OFF;     ///< Black/off (0,0,0)
        static const RGBColor RED;     ///< Pure red (255,0,0)
        static const RGBColor GREEN;   ///< Pure green (0,255,0)
        static const RGBColor BLUE;    ///< Pure blue (0,0,255)
        static const RGBColor YELLOW;  ///< Yellow (255,255,0)
        static const RGBColor CYAN;    ///< Cyan (0,255,255)
        static const RGBColor MAGENTA; ///< Magenta (255,0,255)
        static const RGBColor WHITE;   ///< White (255,255,255)
    };

    /**
     * @brief Button/pad position structure for grid-based controllers
     *
     * Represents physical position of pads/buttons on grid controllers
     * like Launchpad X (8x8 grid) or APC Key 25 (5x8 grid).
     *
     * @code
     * PadPosition top_left(0, 0);
     * PadPosition center(4, 4);  // Center of 8x8 grid
     * @endcode
     */
    struct PadPosition
    {
        uint8_t x; ///< X coordinate (column) on controller grid
        uint8_t y; ///< Y coordinate (row) on controller grid

        /**
         * @brief Default constructor - creates position (0,0)
         */
        PadPosition() : x(0), y(0) {}

        /**
         * @brief Construct pad position with coordinates
         * @param x_pos X coordinate (column, 0-based)
         * @param y_pos Y coordinate (row, 0-based)
         */
        PadPosition(uint8_t x_pos, uint8_t y_pos) : x(x_pos), y(y_pos) {}
    };

    /**
     * @brief Device capabilities flags for MIDI controller features
     *
     * Bitfield enumeration describing hardware capabilities of MIDI devices.
     * Used to enable device-specific features and optimize performance.
     *
     * @code
     * DeviceCapability caps = DeviceCapability::RGB_LEDS | DeviceCapability::VELOCITY_SENSITIVE;
     * if (caps & DeviceCapability::RGB_LEDS) {
     *     // Enable LED feedback
     * }
     * @endcode
     */
    enum class DeviceCapability : uint32_t
    {
        NONE = 0,                    ///< No special capabilities
        RGB_LEDS = 1 << 0,           ///< Support for RGB LED feedback
        VELOCITY_SENSITIVE = 1 << 1, ///< Velocity-sensitive pads/keys
        AFTERTOUCH = 1 << 2,         ///< Polyphonic or channel aftertouch
        SIDE_BUTTONS = 1 << 3,       ///< Side navigation/function buttons
        TOP_BUTTONS = 1 << 4,        ///< Top row function buttons
        FADERS = 1 << 5,             ///< Volume/parameter faders
        ROTARY_ENCODERS = 1 << 6,    ///< Rotary encoder knobs
        DISPLAYS = 1 << 7,           ///< LCD/LED displays
        NAVIGATION = 1 << 8          ///< Navigation pad/joystick
    };

    /**
     * @brief Supported MIDI device types
     *
     * Enumeration of specific MIDI controller models with known
     * communication protocols and feature sets.
     */
    enum class DeviceType
    {
        UNKNOWN,                 ///< Unknown or unsupported device
        LAUNCHPAD_X,             ///< Novation Launchpad X (8x8 RGB grid)
        LAUNCHPAD_PRO,           ///< Novation Launchpad Pro
        LAUNCHPAD_MINI,          ///< Novation Launchpad Mini
        APC_KEY_25_MK1,          ///< Akai APC Key 25 Mk1 (5x8 grid)
        APC_KEY_25_MK2,          ///< Akai APC Key 25 Mk2
        LAUNCHKEY_MK4,           ///< Novation Launchkey MK4 (keyboard + pads)
        GENERIC_GRID_CONTROLLER, ///< Generic grid-based controller
        KEYBOARD_CONTROLLER      ///< Generic keyboard controller
    };

    /**
     * @brief Navigation directions for directional controls
     */
    enum class NavigationDirection
    {
        UP,   ///< Upward navigation
        DOWN, ///< Downward navigation
        LEFT, ///< Left navigation
        RIGHT ///< Right navigation
    };

    // Callback function types for device-specific events

    /// Callback for pad press/release events with position and velocity
    using PadPressCallback = std::function<void(uint8_t channel, const PadPosition &position, uint8_t velocity, bool is_press)>;

    /// Callback for side button press/release events
    using SideButtonCallback = std::function<void(uint8_t channel, uint8_t button_id, bool is_press)>;

    /// Callback for top button press/release events
    using TopButtonCallback = std::function<void(uint8_t channel, uint8_t button_id, bool is_press)>;

    /// Callback for aftertouch pressure events
    using AftertouchCallback = std::function<void(uint8_t channel, const PadPosition &position, uint8_t pressure)>;

    /// Callback for navigation control events
    using NavigationCallback = std::function<void(uint8_t channel, NavigationDirection direction)>;

    /**
     * @brief Abstract base class for MIDI grid controllers and keyboards
     *
     * Provides unified interface for various MIDI hardware controllers with
     * support for device-specific features like RGB LEDs, velocity sensitivity,
     * and specialized control layouts.
     *
     * Key features:
     * - Device auto-detection and connection management
     * - Event callbacks for pads, buttons, and controls
     * - LED feedback and visual patterns
     * - Session mode support for compatible devices
     * - Cross-platform MIDI communication
     *
     * Subclasses implement device-specific protocols for:
     * - Novation Launchpad series (X, Pro, Mini)
     * - Akai APC Key 25 series
     * - Novation Launchkey MK4
     * - Generic MIDI controllers
     *
     * @code
     * auto device = std::make_unique<LaunchpadX>("Launchpad X");
     * device->connect(0);
     * device->setPadCallback([](uint8_t ch, const PadPosition& pos, uint8_t vel, bool press) {
     *     std::cout << "Pad " << (int)pos.x << "," << (int)pos.y << " velocity " << (int)vel << std::endl;
     * });
     * device->setLED(PadPosition(0, 0), RGBColor::RED);
     * @endcode
     *
     * @warning Device must be connected before setting LEDs or receiving events
     * @see LaunchpadX, APCKey25, LaunchkeyMK4 for concrete implementations
     */
    class MidiDevice
    {
    protected:
        std::shared_ptr<MidiInterface> midi_reader_;
        std::string device_name_;
        DeviceType device_type_;
        uint32_t capabilities_;
        uint8_t device_id_;
        bool is_connected_;

        // Callbacks
        PadPressCallback pad_callback_;
        SideButtonCallback side_button_callback_;
        TopButtonCallback top_button_callback_;
        AftertouchCallback aftertouch_callback_;
        NavigationCallback navigation_callback_;

    public:
        /**
         * @brief Construct a new MIDI Device
         * @param name Device name for identification
         * @param type Device type from DeviceType enum
         * @param capabilities Bitfield of supported features
         */
        MidiDevice(const std::string &name, DeviceType type, uint32_t capabilities);

        /**
         * @brief Virtual destructor for proper cleanup
         */
        virtual ~MidiDevice() = default;

        // Device management

        /**
         * @brief Connect to MIDI device
         * @param device_id MIDI device ID to connect to
         * @return true if connection successful, false otherwise
         */
        virtual bool connect(uint8_t device_id) = 0;

        /**
         * @brief Disconnect from MIDI device
         */
        virtual void disconnect() = 0;

        /**
         * @brief Check if device is currently connected
         * @return true if connected, false otherwise
         */
        virtual bool isConnected() const { return is_connected_; }

        // Device information

        /**
         * @brief Get device name
         * @return Device name string
         */
        const std::string &getDeviceName() const { return device_name_; }

        /**
         * @brief Get device type
         * @return DeviceType enumeration value
         */
        DeviceType getDeviceType() const { return device_type_; }

        /**
         * @brief Get device capabilities bitfield
         * @return Capabilities as uint32_t bitfield
         */
        uint32_t getCapabilities() const { return capabilities_; }

        /**
         * @brief Check if device has specific capability
         * @param cap Capability to check
         * @return true if capability is supported
         */
        bool hasCapability(DeviceCapability cap) const;

        // Grid control (if supported)

        /**
         * @brief Set LED color for pad at specific position
         * @param position Grid position of pad
         * @param color RGB color to set
         * @return true if successful, false if position invalid or not supported
         */
        virtual bool setPadColor(const PadPosition &position, const RGBColor &color) = 0;

        /**
         * @brief Set LED color for pad by ID
         * @param pad_id Pad ID number
         * @param color RGB color to set
         * @return true if successful, false if ID invalid or not supported
         */
        virtual bool setPadColor(uint8_t pad_id, const RGBColor &color) = 0;

        /**
         * @brief Clear all pad LEDs (set to off/black)
         */
        virtual void clearAllPads() = 0;

        /**
         * @brief Set all pads to same color
         * @param color RGB color for all pads
         */
        virtual void setAllPads(const RGBColor &color) = 0;

        // Side/top button control (if supported)

        /**
         * @brief Set LED color for side button
         * @param button_id Side button ID
         * @param color RGB color to set
         * @return true if successful, false if not supported
         */
        virtual bool setSideButtonColor(uint8_t button_id, const RGBColor &color) = 0;

        /**
         * @brief Set LED color for top button
         * @param button_id Top button ID
         * @param color RGB color to set
         * @return true if successful, false if not supported
         */
        virtual bool setTopButtonColor(uint8_t button_id, const RGBColor &color) = 0;

        // Device-specific features

        /**
         * @brief Enter programmer mode for direct control
         */
        virtual void enterProgrammerMode() = 0;

        /**
         * @brief Exit programmer mode, return to normal operation
         */
        virtual void exitProgrammerMode() = 0;

        /**
         * @brief Reset device to default state
         */
        virtual void reset() = 0;

        // Callback setters

        /**
         * @brief Set callback for pad press/release events
         * @param callback Function to call on pad events
         */
        void setPadCallback(PadPressCallback callback) { pad_callback_ = callback; }

        /**
         * @brief Set callback for side button events
         * @param callback Function to call on side button events
         */
        void setSideButtonCallback(SideButtonCallback callback) { side_button_callback_ = callback; }

        /**
         * @brief Set callback for top button events
         * @param callback Function to call on top button events
         */
        void setTopButtonCallback(TopButtonCallback callback) { top_button_callback_ = callback; }

        /**
         * @brief Set callback for aftertouch events
         * @param callback Function to call on aftertouch events
         */
        void setAftertouchCallback(AftertouchCallback callback) { aftertouch_callback_ = callback; }

        /**
         * @brief Set callback for navigation events
         * @param callback Function to call on navigation events
         */
        void setNavigationCallback(NavigationCallback callback) { navigation_callback_ = callback; }

        // MIDI message processing

        /**
         * @brief Process incoming MIDI message
         * @param channel MIDI channel (0-15)
         * @param status Status byte
         * @param data1 First data byte
         * @param data2 Second data byte
         */
        virtual void processMidiMessage(uint8_t channel, uint8_t status, uint8_t data1, uint8_t data2) = 0;

        // Grid utilities

        /**
         * @brief Convert MIDI note to grid position
         * @param note MIDI note number
         * @return Grid position for the note
         */
        virtual PadPosition noteToPadPosition(uint8_t note) const = 0;

        /**
         * @brief Convert grid position to MIDI note
         * @param position Grid position
         * @return MIDI note number for the position
         */
        virtual uint8_t padPositionToNote(const PadPosition &position) const = 0;

        /**
         * @brief Get grid width (number of columns)
         * @return Width in pad columns
         */
        virtual uint8_t getGridWidth() const = 0;

        /**
         * @brief Get grid height (number of rows)
         * @return Height in pad rows
         */
        virtual uint8_t getGridHeight() const = 0;

    protected:
        // Helper methods for derived classes

        /**
         * @brief Trigger pad callback with event data
         * @param channel MIDI channel
         * @param position Pad position
         * @param velocity Velocity value
         * @param is_press true for press, false for release
         */
        void triggerPadCallback(uint8_t channel, const PadPosition &position, uint8_t velocity, bool is_press);

        /**
         * @brief Trigger side button callback with event data
         * @param channel MIDI channel
         * @param button_id Button ID
         * @param is_press true for press, false for release
         */
        void triggerSideButtonCallback(uint8_t channel, uint8_t button_id, bool is_press);

        /**
         * @brief Trigger top button callback with event data
         * @param channel MIDI channel
         * @param button_id Button ID
         * @param is_press true for press, false for release
         */
        void triggerTopButtonCallback(uint8_t channel, uint8_t button_id, bool is_press);

        /**
         * @brief Trigger aftertouch callback with pressure data
         * @param channel MIDI channel
         * @param position Pad position
         * @param pressure Pressure value (0-127)
         */
        void triggerAftertouchCallback(uint8_t channel, const PadPosition &position, uint8_t pressure);

        /**
         * @brief Trigger navigation callback with direction
         * @param channel MIDI channel
         * @param direction Navigation direction
         */
        void triggerNavigationCallback(uint8_t channel, NavigationDirection direction);
    };

    /**
     * @brief Factory class for creating MIDI device instances
     *
     * Provides device auto-detection and instantiation based on device names.
     * Supports all known MIDI controller types with automatic capability detection.
     *
     * @code
     * auto device = DeviceFactory::createDevice("Launchpad X");
     * if (device) {
     *     device->connect(0);
     * }
     * @endcode
     */
    class DeviceFactory
    {
    public:
        /**
         * @brief Create device instance from device name
         * @param device_name Name of the MIDI device to create
         * @return Unique pointer to device instance, or nullptr if unsupported
         */
        static std::unique_ptr<MidiDevice> createDevice(const std::string &device_name);

        /**
         * @brief Get list of all supported device names
         * @return Vector of supported device name strings
         */
        static std::vector<std::string> getSupportedDevices();

        /**
         * @brief Detect device type from device name
         * @param device_name Name of the MIDI device
         * @return DeviceType enumeration value, UNKNOWN if not recognized
         */
        static DeviceType detectDeviceType(const std::string &device_name);
    };

} // namespace synth
