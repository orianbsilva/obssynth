# Synth Project - Code Formatting Rules and Style Guide

This document defines the coding standards and formatting rules for the Synth audio synthesizer project, based on analysis of the existing codebase.

## Table of Contents

1. [General Principles](#general-principles)
2. [File Organization](#file-organization)
3. [Naming Conventions](#naming-conventions)
4. [Indentation and Spacing](#indentation-and-spacing)
5. [Braces and Brackets](#braces-and-brackets)
6. [Comments and Documentation](#comments-and-documentation)
7. [Data Types and Declarations](#data-types-and-declarations)
8. [Function Definitions](#function-definitions)
9. [Class Definitions](#class-definitions)
10. [Namespace Usage](#namespace-usage)
11. [Include Organization](#include-organization)
12. [Control Structures](#control-structures)
13. [Compiler and Build Standards](#compiler-and-build-standards)

---

## General Principles

- **Language**: Modern C++17 standards and features only
- **Precision**: Use `float` exclusively for audio processing (never `double`)
- **Platform**: Cross-platform compatibility (Windows/Linux focus)
- **Performance**: Real-time audio performance optimized
- **Consistency**: Follow established patterns throughout codebase

---

## File Organization

### Header Files (.hpp)
```cpp
/**
 * @file filename.hpp
 * @brief Brief description of the file's purpose
 *
 * Detailed description of what this file provides,
 * including usage patterns and architectural notes.
 */

#pragma once

#include "dependencies.hpp"
#include <system_headers>

namespace synth
{
    // Declarations only - no implementations
}
```

### Implementation Files (.cpp)
```cpp
#include "corresponding_header.hpp"
#include "../other/project_headers.hpp"
#include <system_headers>

namespace synth
{
    // Implementations
}
```

### Effects Module Standard
- **Headers (.hpp)**: ONLY declarations - no implementations
- **Source (.cpp)**: ALL implementations must be in corresponding .cpp files
- Follow pattern: declaration in .hpp, implementation in .cpp

---

## Naming Conventions

### Variables
- **Local variables**: `snake_case`
  ```cpp
  float sample_rate = 44100.0f;
  int buffer_size = 512;
  ```

- **Private members**: `snake_case_` (trailing underscore)
  ```cpp
  class Oscillator {
  private:
      float frequency_;
      float phase_;
      WaveType wave_type_;
  };
  ```

- **Public members**: `snake_case` (no underscore)
  ```cpp
  struct AudioConfig {
      float sample_rate;
      int buffer_size;
  };
  ```

### Functions and Methods
- **Function names**: `camelCase`
  ```cpp
  void setFrequency(float frequency);
  float generateSample();
  bool initializeAudio();
  ```

### Classes and Types
- **Class names**: `PascalCase`
  ```cpp
  class PolyphonicADSROscillator;
  class SchroederReverb;
  class AudioSink;
  ```

- **Enum classes**: `PascalCase` with `UPPER_CASE` values
  ```cpp
  enum class WaveType {
      SINE,
      SAW,
      SQUARE
  };
  ```

### Constants
- **Global constants**: `UPPER_CASE`
  ```cpp
  static const float MAX_FREQUENCY = 20000.0f;
  static const int DEFAULT_BUFFER_SIZE = 512;
  ```

### Namespaces
- **Namespace names**: `lowercase`
  ```cpp
  namespace synth {
      // All project code
  }

  namespace examples {
      // Example and test code
  }
  ```

---

## Indentation and Spacing

### Indentation
- **Use 4 spaces** (no tabs)
- **Consistent depth** for nested structures

### Horizontal Spacing
```cpp
// Function calls - space after commas, around operators
float result = calculateFrequency(note, octave, detune);

// Assignments - spaces around operators
amplitude_ = new_amplitude * gain_factor;

// Conditionals - spaces around operators and parentheses
if (frequency > MIN_FREQUENCY && frequency < MAX_FREQUENCY) {
    // code
}

// Function parameters - space after commas
void processAudio(float* buffer, int buffer_size, float sample_rate);
```

### Vertical Spacing
```cpp
// Single blank line between method implementations
void Oscillator::setFrequency(float frequency) {
    // implementation
}

void Oscillator::setWaveType(WaveType wave_type) {
    // implementation
}

// Two blank lines between class definitions
class Oscillator {
    // class content
};


class LFO {
    // class content
};
```

---

## Braces and Brackets

### Function Braces
```cpp
// Opening brace on same line
void functionName(float parameter) {
    // function body
}

// Constructor initialization lists
Oscillator::Oscillator(float frequency, WaveType wave_type, float sample_rate)
    : frequency_(frequency), phase_(0.0f), sample_rate_(sample_rate), wave_type_(wave_type), amplitude_(0.5f)
      ,
      phase_increment_(0.0f), samples_per_buffer_(0), base_frequency_(frequency), frequency_mod_amount_(0.0f)
{
    // constructor body
}
```

### Class Braces
```cpp
class ClassName {
private:
    float member_variable_;

public:
    void publicMethod();
};
```

### Control Structure Braces
```cpp
// Always use braces, even for single statements
if (condition) {
    singleStatement();
}

// Switch statements
switch (wave_type) {
case WaveType::SINE:
    wave_generator_ = sine_wave;
    break;
case WaveType::SAW:
    wave_generator_ = saw_wave;
    break;
default:
    wave_generator_ = sine_wave;
    break;
}
```

---

## Comments and Documentation

### Doxygen Documentation (Required for ALL public methods)
```cpp
/**
 * @brief Short description of what the method does
 * @param parameter_name Description of parameter purpose and expected values
 * @return Description of return value and meaning
 */
float calculateFrequency(int note, int octave = 4);
```

### Implementation Comments
```cpp
// Single line comments for brief explanations
float phase_increment = frequency / sample_rate;

/* Multi-line comments for complex explanations
 * that require more detailed description
 * of the algorithm or approach being used
 */
```

### Section Comments
```cpp
// ===========================================
// Audio processing section
// ===========================================

// Global variables for MIDI control
static std::unique_ptr<PolyphonicADSROscillator> g_oscillator = nullptr;
```

---

## Data Types and Declarations

### Float-Only Policy
```cpp
// ✅ Correct - use float exclusively
float frequency = 440.0f;
float amplitude = 0.5f;
float sample_rate = 44100.0f;

// ❌ Incorrect - never use double
double frequency = 440.0;  // FORBIDDEN
```

### Variable Declarations
```cpp
// Declare variables close to first use
void processBuffer() {
    int buffer_size = getBufferSize();
    float* audio_buffer = allocateBuffer(buffer_size);

    // Use the variables...
}

// Initialize member variables in initializer lists
Oscillator::Oscillator(float frequency)
    : frequency_(frequency)
    , phase_(0.0f)
    , amplitude_(1.0f)
{
}
```

### Pointer and Reference Declarations
```cpp
// Pointer and reference symbols attached to type
float* buffer;
const float& reference;
std::unique_ptr<AudioSink> audio_sink;
```

---

## Function Definitions

### Function Signatures
```cpp
// Return type on same line, parameters with descriptive names
bool initializeAudioSink(float sample_rate, int buffer_size);

// Long parameter lists - break after each parameter
void configureOscillator(
    float frequency,
    WaveType wave_type,
    float amplitude,
    float sample_rate
);
```

### Function Implementations
```cpp
void Oscillator::generateBuffer(float* buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) {
        return;
    }

    for (int i = 0; i < buffer_size; ++i) {
        buffer[i] = generateSample();
    }
}
```

---

## Class Definitions

### Class Structure Order
```cpp
class ExampleClass {
private:
    // Private member variables first
    float private_variable_;

    // Private methods
    void privateMethod();

public:
    // Constructors and destructor
    ExampleClass();
    ~ExampleClass() = default;

    // Public methods
    void publicMethod();

    // Getters and setters last
    float getValue() const;
    void setValue(float value);
};
```

### Access Modifiers
```cpp
class AudioProcessor {
private:
    // 4-space indentation after access modifier
    float sample_rate_;

public:
    // Public interface clearly separated
    void processAudio(float* buffer, int size);
};
```

---

## Namespace Usage

### Namespace Declaration
```cpp
// Lowercase namespace names
namespace synth {

// No additional indentation inside namespace
class Oscillator {
    // class content
};

} // namespace synth
```

### Using Statements
```cpp
// In .cpp files only, after includes
#include "headers.hpp"

using namespace synth;
using namespace examples;

// Never in header files
```

---

## Include Organization

### Include Order
```cpp
// 1. Corresponding header (for .cpp files)
#include "oscillator.hpp"

// 2. Project headers (relative paths)
#include "../math/synt_math.hpp"
#include "../audio/audio_constants.hpp"

// 3. System headers (alphabetical)
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
```

### Include Guards
```cpp
// Use #pragma once (preferred)
#pragma once

// Content of header file
```

---

## Control Structures

### Conditional Statements
```cpp
// Always use braces
if (condition) {
    statement();
}

// Multi-condition formatting
if (frequency > MIN_FREQUENCY &&
    frequency < MAX_FREQUENCY &&
    amplitude >= 0.0f) {
    processAudio();
}

// Ternary operator for simple cases
float result = (value > threshold) ? value : threshold;
```

### Loops
```cpp
// Range-based for loops (preferred)
for (const auto& sample : buffer) {
    processample(sample);
}

// Traditional for loops
for (int i = 0; i < buffer_size; ++i) {
    buffer[i] = generateSample();
}

// While loops
while (condition) {
    updateState();
}
```

---

## Compiler and Build Standards

### Compiler Flags
```cmake
# GCC/Clang specific flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion -Wfloat-conversion -ffloat-store")

# MSVC specific flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /W4")
```

### C++ Standard
```cmake
# Modern C++17 only
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

### Float Enforcement
- Compiler flags will ERROR on double usage - this is intentional
- Use `float` literals with `f` suffix: `440.0f`, `1.0f`, `0.5f`
- Use float math functions: `sinf()`, `cosf()`, `powf()`

---

## Code Examples

### Complete Class Example
```cpp
/**
 * @file example_processor.hpp
 * @brief Example audio processor demonstrating coding standards
 */

#pragma once

#include "../math/synt_math.hpp"
#include <memory>

namespace synth {

/**
 * @brief Example audio processor class
 *
 * Demonstrates proper coding style and documentation
 * for the Synth project standards.
 */
class ExampleProcessor {
private:
    float sample_rate_;
    float gain_;
    std::unique_ptr<float[]> buffer_;

public:
    /**
     * @brief Construct a new Example Processor
     * @param sample_rate Sample rate in Hz (typically 44100.0f)
     * @param gain Initial gain value (0.0f to 1.0f)
     */
    ExampleProcessor(float sample_rate, float gain);

    /**
     * @brief Destructor
     */
    ~ExampleProcessor() = default;

    /**
     * @brief Process audio buffer
     * @param input Input audio buffer
     * @param output Output audio buffer
     * @param buffer_size Number of samples to process
     * @return True if processing successful, false otherwise
     */
    bool processBuffer(const float* input, float* output, int buffer_size);

    /**
     * @brief Set the gain value
     * @param gain New gain value (0.0f to 1.0f)
     */
    void setGain(float gain);
};

} // namespace synth
```

### Implementation Example
```cpp
#include "example_processor.hpp"
#include <algorithm>

namespace synth {

ExampleProcessor::ExampleProcessor(float sample_rate, float gain)
    : sample_rate_(sample_rate)
    , gain_(gain)
    , buffer_(std::make_unique<float[]>(1024))
{
    // Validate parameters
    if (gain_ < 0.0f) {
        gain_ = 0.0f;
    }
    if (gain_ > 1.0f) {
        gain_ = 1.0f;
    }
}

bool ExampleProcessor::processBuffer(const float* input, float* output, int buffer_size) {
    if (!input || !output || buffer_size <= 0) {
        return false;
    }

    for (int i = 0; i < buffer_size; ++i) {
        output[i] = input[i] * gain_;
    }

    return true;
}

void ExampleProcessor::setGain(float gain) {
    // Clamp gain to valid range
    gain_ = std::clamp(gain, 0.0f, 1.0f);
}

} // namespace synth
```

---

## Enforcement Tools

### Clang-Format Configuration
Create `.clang-format` file in project root:
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
UseTab: Never
ColumnLimit: 100
BreakBeforeBraces: Attach
SpaceBeforeParens: Never
PointerAlignment: Left
AlignConsecutiveAssignments: false
```

### CMake Integration
```cmake
# Add format target
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i src/**/*.cpp src/**/*.hpp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
```

---

## Summary

This style guide ensures:
- **Consistency** across the entire codebase
- **Readability** for maintainable code
- **Performance** with float-only audio processing
- **Documentation** with comprehensive Doxygen comments
- **Compliance** with modern C++17 standards

All contributors must follow these standards to maintain code quality and project consistency.
