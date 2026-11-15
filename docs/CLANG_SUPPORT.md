# Compiler Support for Synth Project

## Overview
The synth project uses **Clang as the default compiler** and also supports building with GCC without breaking compatibility.

## Changes Made

### 1. CMakeLists.txt Updates
Added compiler-specific configuration to handle Clang's requirements:

```cmake
# Compiler-specific flags for float-only precision
if(MSVC)
    add_compile_options(/fp:fast /W4)
    # Disable specific warnings that might interfere with our code
    add_compile_options(/wd4100) # unreferenced formal parameter
else()
    add_compile_options(-Wdouble-promotion -Wfloat-conversion)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-ffloat-store)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Clang-specific flags - use libc++ instead of libstdc++
        add_compile_options(-stdlib=libc++)
        add_link_options(-stdlib=libc++)
    endif()
    add_compile_options(-Wall -Wextra -pedantic)
endif()
```

### 2. float_only.h Compatibility Fix
Modified `src/lib/float_only.h` to detect libc++ and avoid conflicts:

```cpp
// Convenience macro for files that need float-only math
// Define SYNTH_USE_FLOAT_MATH before including this header to get local using declarations
// Note: Skip using declarations when using libc++ as it already provides float overloads
#if defined(SYNTH_USE_FLOAT_MATH) && !defined(_LIBCPP_VERSION)
using synth::float_math::sin;
using synth::float_math::cos;
// ... other math functions
#endif
```

**Explanation**: libc++ (Clang's standard library) already provides float overloads for math functions, so our custom using declarations would conflict. The `_LIBCPP_VERSION` macro is defined when using libc++.

### 3. OpenGL Double Promotion Fix
Fixed `src/app/synth_view/gl_renderer.cpp` to handle strict double promotion warnings:

```cpp
glOrtho(static_cast<GLdouble>(r.xMin - padX), static_cast<GLdouble>(r.xMax + padX),
        static_cast<GLdouble>(r.yMin - padY), static_cast<GLdouble>(r.yMax + padY), -1.0, 1.0);
```

**Explanation**: `glOrtho` expects `GLdouble` parameters, but Clang treats implicit float-to-double conversion as an error with our strict compiler flags.

## Build Instructions

## Build Instructions

### Quick Build (Recommended)
```bash
# Use the provided build script (Clang by default)
./build.sh

# Or with explicit compiler choice
CC=gcc CXX=g++ ./build.sh      # Use GCC
CC=clang CXX=clang++ ./build.sh # Use Clang explicitly
```

### Manual Build with Clang (Default)
```bash
cd /path/to/synth
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Manual Build with GCC (Alternative)
```bash
cd /path/to/synth
mkdir build && cd build
CC=gcc CXX=g++ cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Requirements

### For Clang (Default)
- Clang 10+ with C++17 support
- libc++-dev and libc++abi-dev packages:
  ```bash
  sudo apt install libc++-dev libc++abi-dev
  ```

### For GCC (Alternative)
- GCC 8+ with C++17 support
- libstdc++-dev (usually included with build-essential)

## Validation

Both builds produce identical functionality:
- ✅ All unit tests pass
- ✅ All example programs work correctly
- ✅ Audio synthesis operates identically
- ✅ Effects processing maintains float-only precision

## Notes

1. **Standard Library Choice**:
   - GCC uses libstdc++ (GNU's C++ standard library)
   - Clang uses libc++ (LLVM's C++ standard library)

2. **Warnings**: Both compilers may show warnings about unused parameters in example code, which is normal and doesn't affect functionality.

3. **Performance**: Both compilers produce optimized code with similar performance characteristics when built in Release mode.

4. **Cross-Compatibility**: Code written for one compiler will work with the other, thanks to the compatibility layer in `float_only.h`.
