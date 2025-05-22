# JSON Alchemy Python Bindings Benchmarks

This directory contains benchmarks for the JSON Alchemy Python bindings.

## Running Benchmarks

To run the benchmarks, use the `benchmark.py` script:

```bash
python benchmarks/benchmark.py [options]
```

### Options

- `--sizes`: List of batch sizes to benchmark (default: 10, 100, 1000, 10000)
- `--runs`: Number of runs for each benchmark (default: 3)
- `--output`: Output file for benchmark results (default: benchmark_results.md)
- `--plot`: Output file for benchmark plots (default: benchmark_results.png)

Example:

```bash
python benchmarks/benchmark.py --sizes 10 100 1000 5000 --runs 5 --output custom_results.md --plot custom_results.png
```

## Results

The benchmark results are stored in the `results` directory. The results include:

- Execution times for various operations and batch sizes
- Speedups relative to single-object processing
- Analysis of the results
- Plots of execution times and speedups

### Key Findings

Based on the benchmark results, we can make the following observations:

1. **Batch Processing**:
   - For small batches (10-1000 objects), batch processing provides significant speedups for both flattening and schema generation.
   - For larger batches (5000+ objects), the overhead of batch processing outweighs the benefits.

2. **Multi-threading**:
   - Multi-threading provides significant speedups for flattening operations, especially for medium-sized batches (around 1000 objects).
   - For schema generation, multi-threading provides modest speedups for small to medium-sized batches.
   - For very small batches, the overhead of thread creation outweighs the benefits.

3. **Thread Count**:
   - Using a specific thread count (equal to the number of CPU cores) is generally more efficient than auto-detection for flattening operations.
   - For schema generation, auto-detection of thread count is slightly more efficient.

4. **Optimal Batch Size**:
   - The optimal batch size for flattening operations is around 1000 objects.
   - The optimal batch size for schema generation is around 10-100 objects.

## Recommendations

Based on the benchmark results, we recommend the following:

1. **For Flattening Operations**:
   - Use batch processing with multi-threading for batches of 10-1000 objects.
   - For larger batches, consider splitting them into smaller batches of around 1000 objects.
   - Use a specific thread count equal to the number of CPU cores.

2. **For Schema Generation**:
   - Use batch processing with multi-threading for batches of 10-1000 objects.
   - For larger batches, consider splitting them into smaller batches of around 100 objects.
   - Use auto-detection of thread count.

3. **General**:
   - For very small batches (< 10 objects), the overhead of batch processing and multi-threading may outweigh the benefits.
   - For very large batches (> 5000 objects), consider splitting them into smaller batches to avoid excessive memory usage and improve performance.