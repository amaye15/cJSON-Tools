# JSON Alchemy Python Bindings Benchmark Results

JSON Alchemy version: 1.2.0

CPU count: 4

## Execution Times (seconds)

| Size | Flatten Single | Flatten Batch ST | Flatten Batch MT Auto | Flatten Batch MT Spec | Schema Single | Schema Batch ST | Schema Batch MT Auto | Schema Batch MT Spec |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 10 | 0.0011 | 0.0010 | 0.0010 | 0.0009 | 0.0004 | 0.0003 | 0.0003 | 0.0003 |
| 100 | 0.0081 | 0.0079 | 0.0072 | 0.0079 | 0.0047 | 0.0024 | 0.0049 | 0.0024 |
| 1000 | 0.0903 | 0.0896 | 0.0604 | 0.0571 | 0.0470 | 0.0282 | 0.0259 | 0.0255 |

## Speedups (relative to single-object processing)

| Size | Flatten Batch ST | Flatten Batch MT Auto | Flatten Batch MT Spec | Schema Batch ST | Schema Batch MT Auto | Schema Batch MT Spec |
| --- | --- | --- | --- | --- | --- | --- |
| 10 | 1.10x | 1.11x | 1.17x | 1.38x | 1.54x | 1.61x |
| 100 | 1.03x | 1.13x | 1.03x | 2.01x | 0.97x | 1.98x |
| 1000 | 1.01x | 1.50x | 1.58x | 1.67x | 1.82x | 1.85x |

## Analysis

### Average Speedups

- Flatten Batch (Single-Threaded): 1.05x
- Flatten Batch (Multi-Threaded, Auto): 1.25x
- Flatten Batch (Multi-Threaded, Specific): 1.26x
- Schema Batch (Single-Threaded): 1.68x
- Schema Batch (Multi-Threaded, Auto): 1.44x
- Schema Batch (Multi-Threaded, Specific): 1.81x

### Key Findings

- Batch processing is beneficial for flattening operations, with an average speedup of 1.05x over single-object processing.
- Batch processing is beneficial for schema generation, with an average speedup of 1.68x over single-object processing.
- Multi-threading (auto) improves flattening performance by 18.8% compared to single-threaded batch processing.
- Multi-threading (specific) improves flattening performance by 20.2% compared to single-threaded batch processing.
- Multi-threading (auto) does not improve schema generation performance compared to single-threaded batch processing.
- Multi-threading (specific) improves schema generation performance by 7.5% compared to single-threaded batch processing.
- Using a specific thread count (4) is more efficient than auto-detection for flattening operations.
- Using a specific thread count (4) is more efficient than auto-detection for schema generation.
- The optimal batch size for flattening operations is around 1000 objects.
- The optimal batch size for schema generation is around 1000 objects.
