# LogManager Singleton Class

## Overview

The `LogManager` class is a thread-safe singleton that provides global access to logging functionality throughout the synthesizer application. It wraps the `Logger` class and provides convenient static methods for logging.

## Features

### 🔧 **Core Functionality**
- **Singleton Pattern**: Single global instance accessible throughout the application
- **Thread-Safe**: All operations are protected with mutexes for concurrent access
- **Global Static Methods**: Easy-to-use static logging functions
- **Initialization Control**: Explicit initialization with stdout or file output
- **Shutdown Management**: Proper cleanup and resource management

### 📊 **Logging Capabilities**
- **Multiple Log Levels**: DEBUG, INFO, WARNING, ERROR, CRITICAL
- **Dynamic Level Control**: Change minimum log level at runtime
- **Timestamp Control**: Enable/disable timestamp prefixes
- **Output Flexibility**: Support for both stdout and file logging
- **High Performance**: Asynchronous logging with background thread

### 🛡️ **Safety Features**
- **Initialization Checking**: Prevents usage before initialization
- **Exception Safety**: Comprehensive error handling
- **Double Initialization Protection**: Prevents multiple initializations
- **Resource Management**: Automatic cleanup on shutdown

## API Reference

### Static Logging Methods
```cpp
synth::LogManager::Debug("Debug message");
synth::LogManager::Info("Info message");
synth::LogManager::Warning("Warning message");
synth::LogManager::Error("Error message");
synth::LogManager::Critical("Critical message");
```

### Convenience Macros
```cpp
LOG_DEBUG("Debug message");
LOG_INFO("Info message");
LOG_WARNING("Warning message");
LOG_ERROR("Error message");
LOG_CRITICAL("Critical message");
```

### Configuration Methods
```cpp
// Set minimum log level
synth::LogManager::SetMinLevel(synth::Logger::LogLevel::WARNING);

// Get current minimum log level
auto level = synth::LogManager::GetMinLevel();

// Control timestamps
synth::LogManager::SetTimestampEnabled(false);
bool enabled = synth::LogManager::IsTimestampEnabled();

// Force flush all pending messages
synth::LogManager::Flush();
```

### Initialization and Lifecycle
```cpp
// Initialize with stdout logging
synth::LogManager::getInstance().initialize(synth::Logger::LogLevel::INFO);

// Initialize with file logging
synth::LogManager::getInstance().initialize("app.log", synth::Logger::LogLevel::DEBUG);

// Check if initialized
bool initialized = synth::LogManager::getInstance().isInitialized();

// Shutdown (optional - happens automatically at program exit)
synth::LogManager::getInstance().shutdown();
```

## Usage Examples

### Basic Application Logging
```cpp
int main() {
    try {
        // Initialize LogManager
        synth::LogManager::getInstance().initialize("app.log", synth::Logger::LogLevel::INFO);
        
        LOG_INFO("Application started");
        
        // Your application code here
        LOG_DEBUG("Processing data...");
        LOG_WARNING("Memory usage high");
        
        LOG_INFO("Application completed");
        
        // Automatic cleanup on exit
        return 0;
    } catch (const std::exception& e) {
        LOG_CRITICAL("Fatal error: " + std::string(e.what()));
        return 1;
    }
}
```

### Multi-threaded Logging
```cpp
// Thread 1
std::thread worker1([]() {
    for (int i = 0; i < 10; ++i) {
        LOG_INFO("Worker 1 - Task " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
});

// Thread 2
std::thread worker2([]() {
    for (int i = 0; i < 10; ++i) {
        LOG_WARNING("Worker 2 - Warning " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
});

worker1.join();
worker2.join();
```

### Dynamic Log Level Control
```cpp
LOG_INFO("Starting application");

// Set to only show errors and critical messages
synth::LogManager::SetMinLevel(synth::Logger::LogLevel::ERROR);

LOG_DEBUG("This won't appear");    // Filtered out
LOG_INFO("This won't appear");     // Filtered out
LOG_WARNING("This won't appear");  // Filtered out
LOG_ERROR("This will appear");     // Shown
LOG_CRITICAL("This will appear");  // Shown

// Reset to show all messages
synth::LogManager::SetMinLevel(synth::Logger::LogLevel::DEBUG);
LOG_INFO("All messages visible again");
```

## Files

1. **`src/utils/log_manager.hpp`** - Header file with class declaration
2. **`src/utils/log_manager.cpp`** - Implementation file
3. **`examples/test_log_manager.cpp`** - Comprehensive test suite
4. **`examples/simple_audio_with_log_manager.cpp`** - Real-world usage example

## Integration with Synthesizer Project

The LogManager integrates seamlessly with the existing synthesizer project:

- **No Dependencies**: Only depends on the Logger class already in the project
- **Thread-Safe**: Perfect for real-time audio applications with multiple threads
- **Low Overhead**: Asynchronous logging doesn't block audio processing
- **Configurable**: Can be set to different verbosity levels for development vs. production
- **Global Access**: Available throughout the entire codebase without passing references

## Performance Characteristics

- **Initialization**: One-time setup cost
- **Logging**: Minimal overhead due to asynchronous processing
- **Memory**: Constant memory usage with bounded message queue
- **Thread Safety**: Efficient mutex usage with minimal contention
- **Shutdown**: Clean resource cleanup with message flushing

The LogManager provides a robust, enterprise-grade logging solution for the synthesizer project while maintaining simplicity and ease of use!
