# Effects Module Coding Standards

## Header/Implementation Separation

**MANDATORY**: All effects in this module must follow strict header/implementation separation:

### Header Files (.hpp)
- **ONLY declarations** are allowed in header files
- No function implementations (except simple inline getters)
- Include necessary forward declarations and includes
- Use proper documentation comments
- Follow the pattern established by `gain.hpp` and `overdrive.hpp`

### Implementation Files (.cpp)
- **ALL implementations** must be in .cpp files
- Include the corresponding header file first
- Include all necessary system headers
- Follow the namespace structure

## Coding Standards

1. **Float Only**: Use `float` instead of `double` for all floating-point operations
2. **Private Members**: All private member variables must end with underscore (`_`)
3. **Namespace**: All code must be in the `synth` namespace (lowercase)
4. **Include Guards**: Use `#pragma once` in headers
5. **Documentation**: Use Doxygen-style comments for **ALL** public methods and constructors
6. **Method Documentation**: Every public method must have `@brief` and parameter documentation
7. **Error Handling**: Clamp parameters to valid ranges in setters

## Example Structure

```cpp
// effect_name.hpp
#pragma once

#include "../float_only.h"

namespace synth {

/**
 * @brief Description of the effect
 */
class EffectName {
private:
    float parameter_;

public:
    /**
     * @brief Construct a new EffectName
     * @param parameter Initial parameter value (0.0f to 1.0f)
     */
    EffectName(float parameter = 1.0f);

    /**
     * @brief Set the effect parameter
     * @param parameter Parameter value (0.0f to 1.0f)
     */
    void set_parameter(float parameter);

    /**
     * @brief Get the current parameter value
     * @return Current parameter value
     */
    float get_parameter() const { return parameter_; }

    /**
     * @brief Reset the effect state
     */
    void reset();

    /**
     * @brief Process audio samples through the effect
     * @param input Input audio buffer
     * @param output Output audio buffer
     * @param sample_count Number of samples to process
     */
    void process(const float* input, float* output, size_t sample_count);
};

} // namespace synth
``````cpp
// effect_name.cpp
#include "effect_name.hpp"
#include <algorithm>

namespace synth {

EffectName::EffectName(float parameter) : parameter_(parameter) {
    set_parameter(parameter);
}

void EffectName::set_parameter(float parameter) {
    parameter_ = std::max(0.0f, std::min(1.0f, parameter));
}

void EffectName::reset() {
    // Reset internal state
}

void EffectName::process(const float* input, float* output, size_t sample_count) {
    for (size_t i = 0; i < sample_count; ++i) {
        output[i] = input[i] * parameter_;
    }
}

} // namespace synth
```

## Documentation Compliance

**ALL** public methods in effect headers must have complete Doxygen documentation:

### Required Documentation Elements
- `@brief` - Short description of what the method does
- `@param` - Description of each parameter (name and purpose)
- `@return` - Description of return value (for non-void methods)

### Documentation Examples

```cpp
/**
 * @brief Set the gain level
 * @param gain Gain multiplier value
 */
void set_gain(float gain);

/**
 * @brief Get the current gain level
 * @return Current gain multiplier value
 */
float get_gain() const;

/**
 * @brief Process audio samples through the effect
 * @param input Input audio buffer
 * @param output Output audio buffer
 * @param sample_count Number of samples to process
 */
void process(const float* input, float* output, size_t sample_count);
```

## Files Following Standard
- ✅ `gain.hpp` / `gain.cpp` - **FULLY DOCUMENTED**
- ✅ `overdrive.hpp` / `overdrive.cpp` - **FULLY DOCUMENTED**
- ✅ `filter.hpp` / `filter.cpp` - **FULLY DOCUMENTED**
- ✅ `mixer.hpp` / `mixer.cpp` - **FULLY DOCUMENTED**
- ✅ `reverb.hpp` / `reverb.cpp` - **DOCUMENTED** (needs implementation refactoring)


## Implementation Guidelines

When creating new effects or refactoring existing ones:

1. Start with the header file - declare the interface only
2. Create the .cpp file with all implementations
3. Use parameter validation in setter methods
4. Provide reset() method for clearing internal state
5. Use const correctness where appropriate
6. Follow the established naming conventions
