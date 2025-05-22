# JSON Alchemy Python Bindings Benchmark Results

JSON Alchemy version: 1.2.0

CPU count: 4

## Execution Times (seconds)

| Size | Flatten Single | Flatten Batch ST | Flatten Batch MT Auto | Flatten Batch MT Spec | Schema Single | Schema Batch ST | Schema Batch MT Auto | Schema Batch MT Spec |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 10 | 0.0006 | 0.0006 | 0.0005 | 0.0006 | 0.0004 | 0.0002 | 0.0002 | 0.0002 |
| 100 | 0.0071 | 0.0067 | 0.0065 | 0.0064 | 0.0042 | 0.0020 | 0.0021 | 0.0022 |
| 1000 | 0.0730 | 0.0779 | 0.0535 | 0.0506 | 0.0421 | 0.0257 | 0.0239 | 0.0236 |
| 5000 | 0.4289 | 0.6755 | 0.4709 | 0.5008 | 0.2215 | 0.2981 | 0.3215 | 0.3205 |
| 10000 | 0.8283 | 2.5152 | 2.3001 | 2.2356 | 0.4720 | 1.1422 | 1.1888 | 1.2166 |

## Speedups (relative to single-object processing)

| Size | Flatten Batch ST | Flatten Batch MT Auto | Flatten Batch MT Spec | Schema Batch ST | Schema Batch MT Auto | Schema Batch MT Spec |
| --- | --- | --- | --- | --- | --- | --- |
| 10 | 1.07x | 1.16x | 1.12x | 2.41x | 2.82x | 2.81x |
| 100 | 1.06x | 1.10x | 1.11x | 2.12x | 2.04x | 1.94x |
| 1000 | 0.94x | 1.36x | 1.44x | 1.64x | 1.76x | 1.78x |
| 5000 | 0.63x | 0.91x | 0.86x | 0.74x | 0.69x | 0.69x |
| 10000 | 0.33x | 0.36x | 0.37x | 0.41x | 0.40x | 0.39x |

## Analysis

### Average Speedups

- Flatten Batch (Single-Threaded): 0.81x
- Flatten Batch (Multi-Threaded, Auto): 0.98x
- Flatten Batch (Multi-Threaded, Specific): 0.98x
- Schema Batch (Single-Threaded): 1.46x
- Schema Batch (Multi-Threaded, Auto): 1.54x
- Schema Batch (Multi-Threaded, Specific): 1.52x

### Key Findings

- Batch processing does not provide significant benefits for flattening operations.
- Batch processing is beneficial for schema generation, with an average speedup of 1.46x over single-object processing.
- Multi-threading (auto) improves flattening performance by 21.4% compared to single-threaded batch processing.
- Multi-threading (specific) improves flattening performance by 21.5% compared to single-threaded batch processing.
- Multi-threading (auto) improves schema generation performance by 5.2% compared to single-threaded batch processing.
- Multi-threading (specific) improves schema generation performance by 4.0% compared to single-threaded batch processing.
- Using a specific thread count (4) is more efficient than auto-detection for flattening operations.
- Auto-detection of thread count is more efficient than using a specific thread count for schema generation.
- The optimal batch size for flattening operations is around 1000 objects.
- The optimal batch size for schema generation is around 10 objects.
