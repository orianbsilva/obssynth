# Synth Unit Tests

This directory contains unit tests using the [doctest](https://github.com/doctest/doctest) testing framework.

## Running Tests

```bash
# Build and run all tests
cmake --build build --target run_tests

# Run via CTest with verbose output
ctest --test-dir build -V -C Debug

# Run specific test suites by pattern
ctest --test-dir build -R "Oscillator"
ctest --test-dir build -R "BiquadFilter"
```

## Test Files

| File | Description | Test Cases |
|------|-------------|------------|
| [test_main.cpp](test_main.cpp) | doctest entry point | - |
| [test_synth_core.cpp](test_synth_core.cpp) | Core DSP components | BiquadFilter, ModMatrix, OscillatorChain, EffectSlot |
| [test_oscillator.cpp](test_oscillator.cpp) | Oscillator continuity | Phase continuity, waveform generation |

## Test Suites

### BiquadFilter (5 tests)
- Construction with defaults
- Lowpass frequency attenuation
- Highpass frequency attenuation
- Filter mode switching
- Parameter smoothing

### ModMatrix (8 tests)
- Construction and clearing
- Route addition and removal
- Multiple routes to same destination
- Bipolar modulation
- Modulation depth

### OscillatorChain (6 tests)
- Note on/off lifecycle
- Filter integration
- Stereo output
- Modulation routing

### Oscillator (9 tests)
- Sine wave continuity
- Fractional frequency handling
- All wave types border continuity
- Multi-block sequence
- Output range limits
- Phase reset behavior
- Frequency change
- Wave type switching
- Amplitude control

### EffectSlot (3 tests)
- Bypass functionality
- Dry/wet mix
- Effect processing

## Adding New Tests

1. Create a new test file: `test_component.cpp`
2. Include doctest: `#include <doctest/doctest.h>`
3. Use `TEST_SUITE` and `TEST_CASE` macros
4. Add the file to `CMakeLists.txt`

Example:
```cpp
#include <doctest/doctest.h>
#include "component.hpp"

TEST_SUITE("Component") {
    TEST_CASE("basic test") {
        CHECK(1 == 1);
    }
}
```

## Related

- **Benchmarks**: See `../benchmarks/` for performance tests
- **Examples**: See `../src/examples/` for interactive demos
