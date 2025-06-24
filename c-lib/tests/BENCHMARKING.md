# JSON Tools Benchmarking System

This document describes the benchmarking system for the JSON Tools project.

## Overview

The benchmarking system measures the performance of:
1. JSON flattening
2. JSON schema generation

Each operation is tested with:
- Single-threaded processing
- Multi-threaded processing (using 4 threads by default)

## Test Files

The benchmarking system uses test files of various sizes:

| File Name | Objects | Approximate Size | Description |
|-----------|---------|------------------|-------------|
| small_test.json | 50-100 | 50KB | Small test file for quick testing |
| medium_test.json | 200-1,000 | 200KB-1MB | Medium-sized test file |
| large_test.json | 1,000-10,000 | 1MB-10MB | Large test file |
| huge_test.json | 50,000+ | 50MB+ | Huge test file for stress testing |

## Generating Test Files

Test files can be generated using the `generate_test_data` utility:

```bash
# Compile the test data generator
make

# Generate a test file with 1,000 objects and max depth of 4
./generate_test_data output_file.json 1000 4
```

For convenience, you can use the `generate_large_test.sh` script to generate a set of test files:

```bash
./generate_large_test.sh
```

This will create:
- large_test.json (1,000 objects)
- very_large_test.json (10,000 objects)
- huge_test.json (50,000 objects)

## Running Benchmarks

### Quick Benchmarks

For a quick demonstration of the benchmarking system:

```bash
./run_mini_benchmarks.sh
```

This will:
1. Generate small test files (50 and 200 objects)
2. Run benchmarks on these files
3. Generate a benchmark report in `benchmark_results.md`

### Full Benchmarks

For comprehensive benchmarks:

```bash
# First generate large test files
./generate_large_test.sh

# Then run the full benchmarks
./run_benchmarks.sh
```

This will:
1. Run benchmarks on all test files
2. Generate a detailed benchmark report in `benchmark_results.md`

## Visualizing Results

To visualize the benchmark results:

```bash
python visualize_benchmarks.py
```

This will:
1. Parse the benchmark results from `benchmark_results.md`
2. Generate charts showing:
   - Performance comparison between single-threaded and multi-threaded processing
   - Performance improvement percentages
3. Save the charts to `benchmark_charts.png`

## Interpreting Results

The benchmark results show:

1. **Execution Time**: Lower is better
2. **Improvement Percentage**: Higher is better
   - Positive values indicate multi-threading is faster
   - Negative values indicate single-threading is faster

## Performance Factors

Several factors affect performance:

1. **File Size**: Larger files benefit more from multi-threading
2. **Object Complexity**: More complex objects take longer to process
3. **CPU Cores**: More cores can improve multi-threaded performance
4. **Memory**: Limited memory can bottleneck performance

## Customizing Benchmarks

You can customize the benchmarks by:

1. Modifying the test file generation parameters
2. Changing the number of threads used for multi-threading
3. Adding new test cases or operations

## Troubleshooting

If you encounter issues:

1. **Slow Performance**: Check system resources (CPU, memory)
2. **Failed Tests**: Verify the JSON Tools binary is compiled correctly
3. **Visualization Errors**: Ensure matplotlib and numpy are installed