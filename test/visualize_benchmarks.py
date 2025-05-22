#!/usr/bin/env python3
import re
import matplotlib.pyplot as plt
import numpy as np
import os

def parse_time(time_str):
    """Convert time string (e.g., '0m1.234s') to seconds."""
    match = re.match(r'(\d+)m(\d+\.\d+)s', time_str)
    if match:
        minutes, seconds = match.groups()
        return int(minutes) * 60 + float(seconds)
    return 0

def read_benchmark_results(file_path):
    """Read benchmark results from markdown file."""
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Determine if it's a mini benchmark or full benchmark
    is_mini = "Mini Demo" in content
    
    # Set sizes based on benchmark type
    if is_mini:
        sizes = ['Small', 'Medium']
        num_sizes = 2
    else:
        sizes = ['Small', 'Medium', 'Large', 'Huge']
        num_sizes = 4
    
    # Extract flattening results - skip header row
    flatten_lines = re.findall(r'\| (.*?) \| (.*?) \| (.*?) \| (.*?) \|', 
                              re.search(r'## Flattening Performance.*?## Schema', content, re.DOTALL).group(0))
    
    # Extract schema results - skip header row
    schema_section = re.search(r'## Schema Generation Performance.*?##', content, re.DOTALL)
    if not schema_section:
        schema_section = re.search(r'## Schema Generation Performance.*', content, re.DOTALL)
    
    schema_lines = re.findall(r'\| (.*?) \| (.*?) \| (.*?) \| (.*?) \|', schema_section.group(0))
    
    if len(flatten_lines) < num_sizes + 1 or len(schema_lines) < num_sizes + 1:
        print(f"Could not parse benchmark results. Found {len(flatten_lines)} flatten lines and {len(schema_lines)} schema lines")
        return None
    
    # Process flattening results (skip header row)
    flatten_results = {
        'sizes': sizes,
        'single_threaded': [],
        'multi_threaded': [],
        'improvement': []
    }
    
    for i in range(1, num_sizes + 1):  # Skip header row
        if i < len(flatten_lines):
            line = flatten_lines[i]
            # Extract times for each file size
            size_name, single_time, multi_time, improvement = line
            
            # Convert to seconds
            flatten_results['single_threaded'].append(parse_time(single_time))
            flatten_results['multi_threaded'].append(parse_time(multi_time))
            
            # Extract improvement percentage
            if improvement != 'N/A':
                flatten_results['improvement'].append(float(improvement.strip('%')))
            else:
                flatten_results['improvement'].append(0)
    
    # Process schema results (skip header row)
    schema_results = {
        'sizes': sizes,
        'single_threaded': [],
        'multi_threaded': [],
        'improvement': []
    }
    
    for i in range(1, num_sizes + 1):  # Skip header row
        if i < len(schema_lines):
            line = schema_lines[i]
            # Extract times for each file size
            size_name, single_time, multi_time, improvement = line
            
            # Convert to seconds
            schema_results['single_threaded'].append(parse_time(single_time))
            schema_results['multi_threaded'].append(parse_time(multi_time))
            
            # Extract improvement percentage
            if improvement != 'N/A':
                schema_results['improvement'].append(float(improvement.strip('%')))
            else:
                schema_results['improvement'].append(0)
    
    return flatten_results, schema_results

def create_charts(flatten_results, schema_results):
    """Create charts from benchmark results."""
    # Set up the figure
    plt.figure(figsize=(15, 10))
    
    # Bar width
    width = 0.35
    
    # Set positions for bars
    x = np.arange(len(flatten_results['sizes']))
    
    # Create subplots
    plt.subplot(2, 2, 1)
    plt.bar(x - width/2, flatten_results['single_threaded'], width, label='Single-threaded')
    plt.bar(x + width/2, flatten_results['multi_threaded'], width, label='Multi-threaded')
    plt.xlabel('File Size')
    plt.ylabel('Time (seconds)')
    plt.title('JSON Flattening Performance')
    plt.xticks(x, flatten_results['sizes'])
    plt.legend()
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    
    plt.subplot(2, 2, 2)
    plt.bar(x, flatten_results['improvement'], width, color='green')
    plt.xlabel('File Size')
    plt.ylabel('Improvement (%)')
    plt.title('JSON Flattening Performance Improvement')
    plt.xticks(x, flatten_results['sizes'])
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    
    plt.subplot(2, 2, 3)
    plt.bar(x - width/2, schema_results['single_threaded'], width, label='Single-threaded')
    plt.bar(x + width/2, schema_results['multi_threaded'], width, label='Multi-threaded')
    plt.xlabel('File Size')
    plt.ylabel('Time (seconds)')
    plt.title('JSON Schema Generation Performance')
    plt.xticks(x, schema_results['sizes'])
    plt.legend()
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    
    plt.subplot(2, 2, 4)
    plt.bar(x, schema_results['improvement'], width, color='green')
    plt.xlabel('File Size')
    plt.ylabel('Improvement (%)')
    plt.title('JSON Schema Generation Performance Improvement')
    plt.xticks(x, schema_results['sizes'])
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    
    plt.tight_layout()
    plt.savefig('benchmark_charts.png')
    print("Charts saved to benchmark_charts.png")

def main():
    if not os.path.exists('benchmark_results.md'):
        print("Benchmark results file not found. Run run_benchmarks.sh first.")
        return
    
    results = read_benchmark_results('benchmark_results.md')
    if results:
        flatten_results, schema_results = results
        create_charts(flatten_results, schema_results)

if __name__ == "__main__":
    main()