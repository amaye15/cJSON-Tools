#!/usr/bin/env python3
import os

import matplotlib.pyplot as plt
import numpy as np

# Data from our comprehensive benchmarks
file_sizes = [
    "Test Object",
    "Test Batch",
    "Test Mixed",
    "Small",
    "Medium",
    "Large",
    "Very Large",
]
file_size_bytes = [560, 787, 485, 36 * 1024, 140 * 1024, 692 * 1024, 6.8 * 1024 * 1024]

# Flattening times (in seconds)
flatten_single = [0.001, 0.002, 0.001, 0.002, 0.006, 0.022, 0.957]
flatten_multi = [0.001, 0.001, 0.001, 0.003, 0.006, 0.022, 1.025]

# Schema generation times (in seconds)
schema_single = [0.001, 0.001, 0.002, 0.007, 0.008, 0.028, 1.050]
schema_multi = [0.001, 0.001, 0.001, 0.007, 0.007, 0.030, 1.132]


# Calculate improvement percentages
def calculate_improvement(single, multi):
    return [(s - m) / s * 100 if s > 0 else 0 for s, m in zip(single, multi)]


flatten_improvement = calculate_improvement(flatten_single, flatten_multi)
schema_improvement = calculate_improvement(schema_single, schema_multi)

# Create figure with multiple subplots
plt.figure(figsize=(15, 12))

# Plot 1: Flattening Performance (Linear scale)
plt.subplot(2, 2, 1)
x = np.arange(len(file_sizes))
width = 0.35
plt.bar(x - width / 2, flatten_single, width, label="Single-threaded")
plt.bar(x + width / 2, flatten_multi, width, label="Multi-threaded (4 threads)")
plt.xlabel("File Size")
plt.ylabel("Time (seconds)")
plt.title("JSON Flattening Performance")
plt.xticks(x, file_sizes, rotation=45)
plt.legend()
plt.grid(axis="y", linestyle="--", alpha=0.7)

# Plot 2: Schema Generation Performance (Linear scale)
plt.subplot(2, 2, 2)
plt.bar(x - width / 2, schema_single, width, label="Single-threaded")
plt.bar(x + width / 2, schema_multi, width, label="Multi-threaded (4 threads)")
plt.xlabel("File Size")
plt.ylabel("Time (seconds)")
plt.title("JSON Schema Generation Performance")
plt.xticks(x, file_sizes, rotation=45)
plt.legend()
plt.grid(axis="y", linestyle="--", alpha=0.7)

# Plot 3: Flattening Performance (Log scale)
plt.subplot(2, 2, 3)
plt.bar(x - width / 2, flatten_single, width, label="Single-threaded")
plt.bar(x + width / 2, flatten_multi, width, label="Multi-threaded (4 threads)")
plt.xlabel("File Size")
plt.ylabel("Time (seconds)")
plt.title("JSON Flattening Performance (Log Scale)")
plt.xticks(x, file_sizes, rotation=45)
plt.yscale("log")
plt.legend()
plt.grid(axis="y", linestyle="--", alpha=0.7)

# Plot 4: Performance Improvement
plt.subplot(2, 2, 4)
plt.bar(x - width / 2, flatten_improvement, width, label="Flattening")
plt.bar(x + width / 2, schema_improvement, width, label="Schema Generation")
plt.xlabel("File Size")
plt.ylabel("Improvement (%)")
plt.title("Multi-threading Performance Improvement")
plt.xticks(x, file_sizes, rotation=45)
plt.axhline(y=0, color="r", linestyle="-", alpha=0.3)
plt.legend()
plt.grid(axis="y", linestyle="--", alpha=0.7)

# Add a title for the entire figure
plt.suptitle("JSON Alchemy Performance Benchmarks", fontsize=16)

# Adjust layout
plt.tight_layout(rect=[0, 0, 1, 0.95])

# Save the figure
plt.savefig("comprehensive_benchmark_charts.png", dpi=300)
print(
    f"Benchmark visualization saved to: {os.path.abspath('comprehensive_benchmark_charts.png')}"
)

# Show the figure
plt.show()
