# Synth Performance Benchmarks

This directory contains performance benchmarks designed for cross-platform comparison.

## Purpose

These benchmarks help measure and compare:
- SIMD optimization effectiveness
- Lock overhead on different platforms
- Real-time audio processing capability
- CPU usage at various buffer sizes

## Running Benchmarks

```bash
# Build and run all benchmarks
cmake --build build --target run_benchmarks

# Run via CTest with benchmark label
ctest --test-dir build -V -L benchmark

# Run individual benchmarks
./build/bin/Debug/bench_effects_simd.exe
./build/bin/Debug/bench_oscillator_performance.exe
./build/bin/Debug/bench_lock_overhead.exe
```

## Benchmark Files

| File | Description | Measures |
|------|-------------|----------|
| [bench_effects_simd.cpp](bench_effects_simd.cpp) | SIMD effects benchmark | Gain, Overdrive, Mixer processing |
| [bench_oscillator_performance.cpp](bench_oscillator_performance.cpp) | Oscillator performance | Waveform generation speed |
| [bench_lock_overhead.cpp](bench_lock_overhead.cpp) | Lock overhead | Mutex vs lock-free performance |

## Cross-Platform Comparison

### Expected Output Format

Each benchmark produces standardized output for comparison:

```
Platform: [Windows|Linux|macOS]
SIMD: [SSE2|AVX|NEON|None]
Buffer Size: N samples
Iterations: M
---
[Results...]
```

### Collecting Results

Run benchmarks on different platforms and compare:

1. **Windows (MSVC)**
   ```cmd
   cmake --build build --config Release --target run_benchmarks
   ```

2. **Linux (GCC/Clang)**
   ```bash
   cmake --build build --config Release --target run_benchmarks
   ```

3. **macOS (Clang)**
   ```bash
   cmake --build build --config Release --target run_benchmarks
   ```

### Interpreting Results

| CPU Usage | Rating | Notes |
|-----------|--------|-------|
| < 5% | Excellent | Very efficient for real-time |
| 5-10% | Good | Suitable for real-time |
| 10-25% | Moderate | May need optimization |
| > 25% | Poor | Too CPU-intensive |

## Adding New Benchmarks

1. Create `bench_<name>.cpp` in this directory
2. Include standard platform detection macros
3. Output results in consistent format
4. Add to `BENCHMARK_SOURCES` in `CMakeLists.txt`

## Related

- **Unit Tests**: See `../tests/` for functional tests
- **Original Tests**: Some benchmarks were derived from `../src/examples/test_*.cpp`
