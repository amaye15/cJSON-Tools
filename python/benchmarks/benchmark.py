#!/usr/bin/env python3
"""
Comprehensive benchmarks for the JSON Alchemy Python bindings.

This script benchmarks the performance of the JSON Alchemy Python bindings
for various operations and data sizes, comparing single-threaded and
multi-threaded performance.
"""

import json
import time
import os
import sys
import argparse
import random
import string
import multiprocessing
from tabulate import tabulate
import matplotlib.pyplot as plt
import numpy as np

# Add the parent directory to the path so we can import json_alchemy
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from json_alchemy import (
    flatten_json,
    flatten_json_batch,
    generate_schema,
    generate_schema_batch,
    __version__
)

def generate_random_string(length=10):
    """Generate a random string of fixed length."""
    return ''.join(random.choice(string.ascii_letters) for _ in range(length))

def generate_random_json(depth=3, max_children=5, max_array_items=5):
    """Generate a random JSON object with the specified depth and complexity."""
    if depth <= 0:
        # Leaf node - return a primitive value
        value_type = random.choice(['string', 'number', 'boolean', 'null'])
        if value_type == 'string':
            return generate_random_string(random.randint(5, 20))
        elif value_type == 'number':
            return random.randint(1, 1000)
        elif value_type == 'boolean':
            return random.choice([True, False])
        else:
            return None
    
    # Non-leaf node - create an object or array
    node_type = random.choice(['object', 'array'])
    
    if node_type == 'object':
        obj = {}
        num_children = random.randint(1, max_children)
        for _ in range(num_children):
            key = generate_random_string(random.randint(3, 10))
            obj[key] = generate_random_json(depth - 1, max_children, max_array_items)
        return obj
    else:  # array
        arr = []
        num_items = random.randint(1, max_array_items)
        for _ in range(num_items):
            arr.append(generate_random_json(depth - 1, max_children, max_array_items))
        return arr

def generate_test_data(num_objects, min_depth=2, max_depth=5):
    """Generate a list of random JSON objects for testing."""
    json_objects = []
    for _ in range(num_objects):
        depth = random.randint(min_depth, max_depth)
        obj = generate_random_json(depth)
        json_objects.append(json.dumps(obj))
    return json_objects

def benchmark_flatten_single(json_objects, num_runs=5):
    """Benchmark flattening single JSON objects."""
    total_time = 0
    for _ in range(num_runs):
        start_time = time.time()
        for json_obj in json_objects:
            flatten_json(json_obj)
        total_time += time.time() - start_time
    
    return total_time / num_runs

def benchmark_flatten_batch(json_objects, use_threads=False, num_threads=0, num_runs=5):
    """Benchmark flattening a batch of JSON objects."""
    total_time = 0
    for _ in range(num_runs):
        start_time = time.time()
        flatten_json_batch(json_objects, use_threads=use_threads, num_threads=num_threads)
        total_time += time.time() - start_time
    
    return total_time / num_runs

def benchmark_schema_single(json_objects, num_runs=5):
    """Benchmark generating schemas for single JSON objects."""
    total_time = 0
    for _ in range(num_runs):
        start_time = time.time()
        for json_obj in json_objects:
            generate_schema(json_obj)
        total_time += time.time() - start_time
    
    return total_time / num_runs

def benchmark_schema_batch(json_objects, use_threads=False, num_threads=0, num_runs=5):
    """Benchmark generating a schema from a batch of JSON objects."""
    total_time = 0
    for _ in range(num_runs):
        start_time = time.time()
        generate_schema_batch(json_objects, use_threads=use_threads, num_threads=num_threads)
        total_time += time.time() - start_time
    
    return total_time / num_runs

def run_benchmarks(sizes, num_runs=3):
    """Run all benchmarks for various data sizes."""
    results = []
    cpu_count = multiprocessing.cpu_count()
    
    for size in sizes:
        print(f"Generating {size} random JSON objects...")
        json_objects = generate_test_data(size)
        
        print(f"Running benchmarks for {size} objects...")
        
        # Flatten single
        flatten_single_time = benchmark_flatten_single(json_objects, num_runs)
        
        # Flatten batch (single-threaded)
        flatten_batch_st_time = benchmark_flatten_batch(json_objects, use_threads=False, num_runs=num_runs)
        
        # Flatten batch (multi-threaded, auto)
        flatten_batch_mt_auto_time = benchmark_flatten_batch(json_objects, use_threads=True, num_runs=num_runs)
        
        # Flatten batch (multi-threaded, specific)
        flatten_batch_mt_spec_time = benchmark_flatten_batch(json_objects, use_threads=True, num_threads=cpu_count, num_runs=num_runs)
        
        # Schema single
        schema_single_time = benchmark_schema_single(json_objects, num_runs)
        
        # Schema batch (single-threaded)
        schema_batch_st_time = benchmark_schema_batch(json_objects, use_threads=False, num_runs=num_runs)
        
        # Schema batch (multi-threaded, auto)
        schema_batch_mt_auto_time = benchmark_schema_batch(json_objects, use_threads=True, num_runs=num_runs)
        
        # Schema batch (multi-threaded, specific)
        schema_batch_mt_spec_time = benchmark_schema_batch(json_objects, use_threads=True, num_threads=cpu_count, num_runs=num_runs)
        
        # Calculate speedups
        flatten_batch_st_speedup = flatten_single_time / flatten_batch_st_time
        flatten_batch_mt_auto_speedup = flatten_single_time / flatten_batch_mt_auto_time
        flatten_batch_mt_spec_speedup = flatten_single_time / flatten_batch_mt_spec_time
        
        schema_batch_st_speedup = schema_single_time / schema_batch_st_time
        schema_batch_mt_auto_speedup = schema_single_time / schema_batch_mt_auto_time
        schema_batch_mt_spec_speedup = schema_single_time / schema_batch_mt_spec_time
        
        # Add to results
        results.append({
            'size': size,
            'flatten_single': flatten_single_time,
            'flatten_batch_st': flatten_batch_st_time,
            'flatten_batch_mt_auto': flatten_batch_mt_auto_time,
            'flatten_batch_mt_spec': flatten_batch_mt_spec_time,
            'schema_single': schema_single_time,
            'schema_batch_st': schema_batch_st_time,
            'schema_batch_mt_auto': schema_batch_mt_auto_time,
            'schema_batch_mt_spec': schema_batch_mt_spec_time,
            'flatten_batch_st_speedup': flatten_batch_st_speedup,
            'flatten_batch_mt_auto_speedup': flatten_batch_mt_auto_speedup,
            'flatten_batch_mt_spec_speedup': flatten_batch_mt_spec_speedup,
            'schema_batch_st_speedup': schema_batch_st_speedup,
            'schema_batch_mt_auto_speedup': schema_batch_mt_auto_speedup,
            'schema_batch_mt_spec_speedup': schema_batch_mt_spec_speedup,
        })
    
    return results

def print_results(results):
    """Print benchmark results in a table."""
    headers = [
        'Size',
        'Flatten Single (s)',
        'Flatten Batch ST (s)',
        'Flatten Batch MT Auto (s)',
        'Flatten Batch MT Spec (s)',
        'Schema Single (s)',
        'Schema Batch ST (s)',
        'Schema Batch MT Auto (s)',
        'Schema Batch MT Spec (s)',
        'Flatten Batch ST Speedup',
        'Flatten Batch MT Auto Speedup',
        'Flatten Batch MT Spec Speedup',
        'Schema Batch ST Speedup',
        'Schema Batch MT Auto Speedup',
        'Schema Batch MT Spec Speedup',
    ]
    
    table_data = []
    for result in results:
        table_data.append([
            result['size'],
            f"{result['flatten_single']:.4f}",
            f"{result['flatten_batch_st']:.4f}",
            f"{result['flatten_batch_mt_auto']:.4f}",
            f"{result['flatten_batch_mt_spec']:.4f}",
            f"{result['schema_single']:.4f}",
            f"{result['schema_batch_st']:.4f}",
            f"{result['schema_batch_mt_auto']:.4f}",
            f"{result['schema_batch_mt_spec']:.4f}",
            f"{result['flatten_batch_st_speedup']:.2f}x",
            f"{result['flatten_batch_mt_auto_speedup']:.2f}x",
            f"{result['flatten_batch_mt_spec_speedup']:.2f}x",
            f"{result['schema_batch_st_speedup']:.2f}x",
            f"{result['schema_batch_mt_auto_speedup']:.2f}x",
            f"{result['schema_batch_mt_spec_speedup']:.2f}x",
        ])
    
    print(tabulate(table_data, headers=headers, tablefmt='grid'))

def save_results_to_markdown(results, filename):
    """Save benchmark results to a Markdown file."""
    with open(filename, 'w') as f:
        f.write("# JSON Alchemy Python Bindings Benchmark Results\n\n")
        f.write(f"JSON Alchemy version: {__version__}\n\n")
        f.write(f"CPU count: {multiprocessing.cpu_count()}\n\n")
        
        f.write("## Execution Times (seconds)\n\n")
        
        headers = [
            'Size',
            'Flatten Single',
            'Flatten Batch ST',
            'Flatten Batch MT Auto',
            'Flatten Batch MT Spec',
            'Schema Single',
            'Schema Batch ST',
            'Schema Batch MT Auto',
            'Schema Batch MT Spec',
        ]
        
        f.write("| " + " | ".join(headers) + " |\n")
        f.write("| " + " | ".join(["---"] * len(headers)) + " |\n")
        
        for result in results:
            row = [
                str(result['size']),
                f"{result['flatten_single']:.4f}",
                f"{result['flatten_batch_st']:.4f}",
                f"{result['flatten_batch_mt_auto']:.4f}",
                f"{result['flatten_batch_mt_spec']:.4f}",
                f"{result['schema_single']:.4f}",
                f"{result['schema_batch_st']:.4f}",
                f"{result['schema_batch_mt_auto']:.4f}",
                f"{result['schema_batch_mt_spec']:.4f}",
            ]
            f.write("| " + " | ".join(row) + " |\n")
        
        f.write("\n## Speedups (relative to single-object processing)\n\n")
        
        headers = [
            'Size',
            'Flatten Batch ST',
            'Flatten Batch MT Auto',
            'Flatten Batch MT Spec',
            'Schema Batch ST',
            'Schema Batch MT Auto',
            'Schema Batch MT Spec',
        ]
        
        f.write("| " + " | ".join(headers) + " |\n")
        f.write("| " + " | ".join(["---"] * len(headers)) + " |\n")
        
        for result in results:
            row = [
                str(result['size']),
                f"{result['flatten_batch_st_speedup']:.2f}x",
                f"{result['flatten_batch_mt_auto_speedup']:.2f}x",
                f"{result['flatten_batch_mt_spec_speedup']:.2f}x",
                f"{result['schema_batch_st_speedup']:.2f}x",
                f"{result['schema_batch_mt_auto_speedup']:.2f}x",
                f"{result['schema_batch_mt_spec_speedup']:.2f}x",
            ]
            f.write("| " + " | ".join(row) + " |\n")
        
        f.write("\n## Analysis\n\n")
        
        # Calculate average speedups
        avg_flatten_batch_st_speedup = sum(r['flatten_batch_st_speedup'] for r in results) / len(results)
        avg_flatten_batch_mt_auto_speedup = sum(r['flatten_batch_mt_auto_speedup'] for r in results) / len(results)
        avg_flatten_batch_mt_spec_speedup = sum(r['flatten_batch_mt_spec_speedup'] for r in results) / len(results)
        avg_schema_batch_st_speedup = sum(r['schema_batch_st_speedup'] for r in results) / len(results)
        avg_schema_batch_mt_auto_speedup = sum(r['schema_batch_mt_auto_speedup'] for r in results) / len(results)
        avg_schema_batch_mt_spec_speedup = sum(r['schema_batch_mt_spec_speedup'] for r in results) / len(results)
        
        f.write("### Average Speedups\n\n")
        f.write(f"- Flatten Batch (Single-Threaded): {avg_flatten_batch_st_speedup:.2f}x\n")
        f.write(f"- Flatten Batch (Multi-Threaded, Auto): {avg_flatten_batch_mt_auto_speedup:.2f}x\n")
        f.write(f"- Flatten Batch (Multi-Threaded, Specific): {avg_flatten_batch_mt_spec_speedup:.2f}x\n")
        f.write(f"- Schema Batch (Single-Threaded): {avg_schema_batch_st_speedup:.2f}x\n")
        f.write(f"- Schema Batch (Multi-Threaded, Auto): {avg_schema_batch_mt_auto_speedup:.2f}x\n")
        f.write(f"- Schema Batch (Multi-Threaded, Specific): {avg_schema_batch_mt_spec_speedup:.2f}x\n\n")
        
        f.write("### Key Findings\n\n")
        
        # Determine if batch processing is beneficial
        if avg_flatten_batch_st_speedup > 1:
            f.write("- Batch processing is beneficial for flattening operations, with an average speedup of "
                   f"{avg_flatten_batch_st_speedup:.2f}x over single-object processing.\n")
        else:
            f.write("- Batch processing does not provide significant benefits for flattening operations.\n")
        
        if avg_schema_batch_st_speedup > 1:
            f.write("- Batch processing is beneficial for schema generation, with an average speedup of "
                   f"{avg_schema_batch_st_speedup:.2f}x over single-object processing.\n")
        else:
            f.write("- Batch processing does not provide significant benefits for schema generation.\n")
        
        # Determine if multi-threading is beneficial
        if avg_flatten_batch_mt_auto_speedup > avg_flatten_batch_st_speedup:
            f.write("- Multi-threading (auto) improves flattening performance by "
                   f"{(avg_flatten_batch_mt_auto_speedup / avg_flatten_batch_st_speedup - 1) * 100:.1f}% "
                   f"compared to single-threaded batch processing.\n")
        else:
            f.write("- Multi-threading (auto) does not improve flattening performance compared to "
                   "single-threaded batch processing.\n")
        
        if avg_flatten_batch_mt_spec_speedup > avg_flatten_batch_st_speedup:
            f.write("- Multi-threading (specific) improves flattening performance by "
                   f"{(avg_flatten_batch_mt_spec_speedup / avg_flatten_batch_st_speedup - 1) * 100:.1f}% "
                   f"compared to single-threaded batch processing.\n")
        else:
            f.write("- Multi-threading (specific) does not improve flattening performance compared to "
                   "single-threaded batch processing.\n")
        
        if avg_schema_batch_mt_auto_speedup > avg_schema_batch_st_speedup:
            f.write("- Multi-threading (auto) improves schema generation performance by "
                   f"{(avg_schema_batch_mt_auto_speedup / avg_schema_batch_st_speedup - 1) * 100:.1f}% "
                   f"compared to single-threaded batch processing.\n")
        else:
            f.write("- Multi-threading (auto) does not improve schema generation performance compared to "
                   "single-threaded batch processing.\n")
        
        if avg_schema_batch_mt_spec_speedup > avg_schema_batch_st_speedup:
            f.write("- Multi-threading (specific) improves schema generation performance by "
                   f"{(avg_schema_batch_mt_spec_speedup / avg_schema_batch_st_speedup - 1) * 100:.1f}% "
                   f"compared to single-threaded batch processing.\n")
        else:
            f.write("- Multi-threading (specific) does not improve schema generation performance compared to "
                   "single-threaded batch processing.\n")
        
        # Determine optimal thread count
        if avg_flatten_batch_mt_spec_speedup > avg_flatten_batch_mt_auto_speedup:
            f.write(f"- Using a specific thread count ({multiprocessing.cpu_count()}) is more efficient than "
                   f"auto-detection for flattening operations.\n")
        else:
            f.write("- Auto-detection of thread count is more efficient than using a specific thread count "
                   "for flattening operations.\n")
        
        if avg_schema_batch_mt_spec_speedup > avg_schema_batch_mt_auto_speedup:
            f.write(f"- Using a specific thread count ({multiprocessing.cpu_count()}) is more efficient than "
                   f"auto-detection for schema generation.\n")
        else:
            f.write("- Auto-detection of thread count is more efficient than using a specific thread count "
                   "for schema generation.\n")
        
        # Determine optimal batch size
        best_flatten_size = max(results, key=lambda r: r['flatten_batch_mt_auto_speedup'])['size']
        best_schema_size = max(results, key=lambda r: r['schema_batch_mt_auto_speedup'])['size']
        
        f.write(f"- The optimal batch size for flattening operations is around {best_flatten_size} objects.\n")
        f.write(f"- The optimal batch size for schema generation is around {best_schema_size} objects.\n")

def plot_results(results, filename):
    """Plot benchmark results and save to a file."""
    sizes = [r['size'] for r in results]
    
    # Create a figure with multiple subplots
    fig, axs = plt.subplots(2, 2, figsize=(15, 12))
    
    # Plot flattening execution times
    axs[0, 0].plot(sizes, [r['flatten_single'] for r in results], 'o-', label='Single')
    axs[0, 0].plot(sizes, [r['flatten_batch_st'] for r in results], 's-', label='Batch ST')
    axs[0, 0].plot(sizes, [r['flatten_batch_mt_auto'] for r in results], '^-', label='Batch MT Auto')
    axs[0, 0].plot(sizes, [r['flatten_batch_mt_spec'] for r in results], 'd-', label='Batch MT Spec')
    axs[0, 0].set_title('Flattening Execution Times')
    axs[0, 0].set_xlabel('Number of Objects')
    axs[0, 0].set_ylabel('Time (seconds)')
    axs[0, 0].legend()
    axs[0, 0].grid(True)
    
    # Plot schema generation execution times
    axs[0, 1].plot(sizes, [r['schema_single'] for r in results], 'o-', label='Single')
    axs[0, 1].plot(sizes, [r['schema_batch_st'] for r in results], 's-', label='Batch ST')
    axs[0, 1].plot(sizes, [r['schema_batch_mt_auto'] for r in results], '^-', label='Batch MT Auto')
    axs[0, 1].plot(sizes, [r['schema_batch_mt_spec'] for r in results], 'd-', label='Batch MT Spec')
    axs[0, 1].set_title('Schema Generation Execution Times')
    axs[0, 1].set_xlabel('Number of Objects')
    axs[0, 1].set_ylabel('Time (seconds)')
    axs[0, 1].legend()
    axs[0, 1].grid(True)
    
    # Plot flattening speedups
    axs[1, 0].plot(sizes, [r['flatten_batch_st_speedup'] for r in results], 's-', label='Batch ST')
    axs[1, 0].plot(sizes, [r['flatten_batch_mt_auto_speedup'] for r in results], '^-', label='Batch MT Auto')
    axs[1, 0].plot(sizes, [r['flatten_batch_mt_spec_speedup'] for r in results], 'd-', label='Batch MT Spec')
    axs[1, 0].axhline(y=1, color='r', linestyle='--', label='Baseline (Single)')
    axs[1, 0].set_title('Flattening Speedups')
    axs[1, 0].set_xlabel('Number of Objects')
    axs[1, 0].set_ylabel('Speedup (x)')
    axs[1, 0].legend()
    axs[1, 0].grid(True)
    
    # Plot schema generation speedups
    axs[1, 1].plot(sizes, [r['schema_batch_st_speedup'] for r in results], 's-', label='Batch ST')
    axs[1, 1].plot(sizes, [r['schema_batch_mt_auto_speedup'] for r in results], '^-', label='Batch MT Auto')
    axs[1, 1].plot(sizes, [r['schema_batch_mt_spec_speedup'] for r in results], 'd-', label='Batch MT Spec')
    axs[1, 1].axhline(y=1, color='r', linestyle='--', label='Baseline (Single)')
    axs[1, 1].set_title('Schema Generation Speedups')
    axs[1, 1].set_xlabel('Number of Objects')
    axs[1, 1].set_ylabel('Speedup (x)')
    axs[1, 1].legend()
    axs[1, 1].grid(True)
    
    plt.tight_layout()
    plt.savefig(filename)
    plt.close()

def main():
    """Main function."""
    parser = argparse.ArgumentParser(description='Benchmark JSON Alchemy Python bindings')
    parser.add_argument('--sizes', type=int, nargs='+', default=[10, 100, 1000, 10000],
                        help='Sizes of test data (number of JSON objects)')
    parser.add_argument('--runs', type=int, default=3,
                        help='Number of runs for each benchmark')
    parser.add_argument('--output', type=str, default='benchmark_results.md',
                        help='Output file for benchmark results')
    parser.add_argument('--plot', type=str, default='benchmark_results.png',
                        help='Output file for benchmark plots')
    
    args = parser.parse_args()
    
    print(f"JSON Alchemy version: {__version__}")
    print(f"CPU count: {multiprocessing.cpu_count()}")
    print(f"Running benchmarks with sizes: {args.sizes}")
    print(f"Number of runs per benchmark: {args.runs}")
    
    # Create the benchmarks directory if it doesn't exist
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    
    # Run benchmarks
    results = run_benchmarks(args.sizes, args.runs)
    
    # Print results
    print_results(results)
    
    # Save results to file
    save_results_to_markdown(results, args.output)
    print(f"Results saved to {args.output}")
    
    # Plot results
    try:
        plot_results(results, args.plot)
        print(f"Plots saved to {args.plot}")
    except Exception as e:
        print(f"Error creating plots: {e}")
    
    print("Benchmarks completed successfully!")

if __name__ == "__main__":
    main()