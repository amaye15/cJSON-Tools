#!/usr/bin/env python3
"""
Comprehensive Performance Testing Script for cJSON-Tools
Runs performance tests across different build tiers and configurations
"""

import os
import sys
import time
import json
import subprocess
import statistics
import argparse
from pathlib import Path

def run_command(cmd, cwd=None, timeout=60):
    """Run a command and return the result"""
    try:
        result = subprocess.run(
            cmd, shell=True, cwd=cwd, capture_output=True, 
            text=True, timeout=timeout
        )
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Command timed out"

def build_binary(tier, clean=True):
    """Build the binary with specified tier"""
    print(f"🔨 Building with tier {tier}...")
    
    if clean:
        success, _, _ = run_command("make clean")
        if not success:
            print("❌ Failed to clean")
            return False
    
    success, stdout, stderr = run_command(f"make tier{tier}")
    if not success:
        print(f"❌ Failed to build tier {tier}")
        print(f"Error: {stderr}")
        return False
    
    if not os.path.exists("bin/json_tools"):
        print("❌ Binary not found after build")
        return False
    
    print(f"✅ Built tier {tier} successfully")
    return True

def generate_test_data(size, depth=3):
    """Generate test data of specified size"""
    test_data = []
    for i in range(size):
        obj = {
            "id": i,
            "name": f"Object_{i}",
            "active": i % 2 == 0,
            "score": round(i * 0.1, 2),
            "metadata": {
                "created": f"2023-{(i % 12) + 1:02d}-01",
                "tags": [f"tag_{i}", f"category_{i % 10}"],
                "nested": {
                    "deep": {
                        "value": f"deep_value_{i}",
                        "count": i % 100,
                        "array": [i, i*2, i*3]
                    }
                } if depth >= 3 else {}
            } if depth >= 2 else {}
        }
        test_data.append(obj)
    return test_data

def benchmark_operation(operation, input_file, iterations=3):
    """Benchmark a specific operation"""
    times = []
    
    for _ in range(iterations):
        start_time = time.time()
        success, _, _ = run_command(f"./bin/json_tools {operation} {input_file} > /dev/null")
        end_time = time.time()
        
        if not success:
            return None
        
        times.append(end_time - start_time)
    
    return {
        'avg_time': statistics.mean(times),
        'min_time': min(times),
        'max_time': max(times),
        'std_dev': statistics.stdev(times) if len(times) > 1 else 0
    }

def run_performance_tests(tiers, sizes, iterations=3):
    """Run comprehensive performance tests"""
    results = {}
    
    for tier in tiers:
        print(f"\n🚀 Testing Build Tier {tier}")
        print("=" * 50)
        
        if not build_binary(tier):
            continue
        
        tier_results = {}
        
        for size in sizes:
            print(f"\n📊 Testing with {size} objects...")
            
            # Generate test data
            test_data = generate_test_data(size)
            test_file = f"test_data_{size}.json"
            
            with open(test_file, 'w') as f:
                json.dump(test_data, f)
            
            # Test flattening
            flatten_result = benchmark_operation("-f", test_file, iterations)
            if flatten_result:
                throughput = size / flatten_result['avg_time']
                print(f"  Flatten: {flatten_result['avg_time']:.4f}s ({throughput:.0f} obj/s)")
            
            # Test schema generation
            schema_result = benchmark_operation("-s", test_file, iterations)
            if schema_result:
                throughput = size / schema_result['avg_time']
                print(f"  Schema:  {schema_result['avg_time']:.4f}s ({throughput:.0f} obj/s)")
            
            tier_results[size] = {
                'flatten': flatten_result,
                'schema': schema_result
            }
            
            # Cleanup
            os.remove(test_file)
        
        results[tier] = tier_results
    
    return results

def generate_report(results, output_file=None):
    """Generate a performance report"""
    report = []
    report.append("# 🚀 cJSON-Tools Performance Report")
    report.append("")
    report.append(f"Generated on: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    report.append("")
    
    # Summary table
    report.append("## Performance Summary")
    report.append("")
    report.append("| Tier | Size | Flatten (obj/s) | Schema (obj/s) |")
    report.append("|------|------|-----------------|----------------|")
    
    for tier, tier_results in results.items():
        for size, size_results in tier_results.items():
            flatten_throughput = ""
            schema_throughput = ""
            
            if size_results['flatten']:
                flatten_throughput = f"{size / size_results['flatten']['avg_time']:.0f}"
            
            if size_results['schema']:
                schema_throughput = f"{size / size_results['schema']['avg_time']:.0f}"
            
            report.append(f"| {tier} | {size} | {flatten_throughput} | {schema_throughput} |")
    
    report.append("")
    
    # Detailed results
    report.append("## Detailed Results")
    report.append("")
    
    for tier, tier_results in results.items():
        report.append(f"### Build Tier {tier}")
        report.append("")
        
        for size, size_results in tier_results.items():
            report.append(f"#### {size} Objects")
            report.append("")
            
            if size_results['flatten']:
                fr = size_results['flatten']
                report.append(f"**Flattening:**")
                report.append(f"- Average: {fr['avg_time']:.4f}s")
                report.append(f"- Throughput: {size / fr['avg_time']:.0f} obj/s")
                report.append(f"- Std Dev: {fr['std_dev']:.4f}s")
                report.append("")
            
            if size_results['schema']:
                sr = size_results['schema']
                report.append(f"**Schema Generation:**")
                report.append(f"- Average: {sr['avg_time']:.4f}s")
                report.append(f"- Throughput: {size / sr['avg_time']:.0f} obj/s")
                report.append(f"- Std Dev: {sr['std_dev']:.4f}s")
                report.append("")
    
    report_text = "\n".join(report)
    
    if output_file:
        with open(output_file, 'w') as f:
            f.write(report_text)
        print(f"📊 Report saved to {output_file}")
    else:
        print(report_text)

def main():
    parser = argparse.ArgumentParser(description="Performance testing for cJSON-Tools")
    parser.add_argument("--tiers", default="1,2,3", help="Build tiers to test (comma-separated)")
    parser.add_argument("--sizes", default="100,1000,5000", help="Test sizes (comma-separated)")
    parser.add_argument("--iterations", type=int, default=3, help="Number of iterations per test")
    parser.add_argument("--output", help="Output file for report")
    
    args = parser.parse_args()
    
    tiers = [int(t.strip()) for t in args.tiers.split(",")]
    sizes = [int(s.strip()) for s in args.sizes.split(",")]
    
    print("🎯 cJSON-Tools Performance Testing")
    print("=" * 50)
    print(f"Tiers: {tiers}")
    print(f"Sizes: {sizes}")
    print(f"Iterations: {args.iterations}")
    print("")
    
    # Change to project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    os.chdir(project_root)
    
    results = run_performance_tests(tiers, sizes, args.iterations)
    generate_report(results, args.output)

if __name__ == "__main__":
    main()
