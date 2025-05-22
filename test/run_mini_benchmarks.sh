#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Build the project
echo -e "${BLUE}Building JSON Tools...${NC}"
cd ..
make clean
make
cd test
make clean
make
echo ""

# Generate smaller test data for quick demonstration
echo -e "${BLUE}Generating mini test data (50 objects)...${NC}"
./generate_test_data small_test.json 50 3

echo -e "${BLUE}Generating small test data (200 objects)...${NC}"
./generate_test_data medium_test.json 200 3

# Function to run benchmark
run_benchmark() {
    local file=$1
    local operation=$2
    local threads=$3
    local output_file="benchmark_${operation}_${file%.*}_${threads}threads.json"
    local time_file="time_${operation}_${file%.*}_${threads}threads.txt"
    
    echo -e "${YELLOW}Running benchmark: ${operation} on ${file} with ${threads} threads${NC}"
    
    # Run the benchmark with time measurement
    if [ "$operation" == "flatten" ]; then
        { time ../bin/json_tools -f -t $threads $file > $output_file; } 2> $time_file
    else
        { time ../bin/json_tools -s -t $threads $file > $output_file; } 2> $time_file
    fi
    
    # Extract real time
    real_time=$(grep "real" $time_file | awk '{print $2}')
    
    # Get file size
    file_size=$(du -h $file | awk '{print $1}')
    
    echo -e "${GREEN}Completed in ${real_time} (file size: ${file_size})${NC}"
    echo ""
}

# Run benchmarks
echo -e "${BLUE}=== Running Mini Benchmarks ===${NC}"
echo ""

# Small file benchmarks
echo -e "${BLUE}--- Mini File Benchmarks (50 objects) ---${NC}"
run_benchmark "small_test.json" "flatten" 0
run_benchmark "small_test.json" "flatten" 4
run_benchmark "small_test.json" "schema" 0
run_benchmark "small_test.json" "schema" 4

# Medium file benchmarks
echo -e "${BLUE}--- Small File Benchmarks (200 objects) ---${NC}"
run_benchmark "medium_test.json" "flatten" 0
run_benchmark "medium_test.json" "flatten" 4
run_benchmark "medium_test.json" "schema" 0
run_benchmark "medium_test.json" "schema" 4

# Generate benchmark report
echo -e "${BLUE}Generating benchmark report...${NC}"

# Create markdown report
cat > benchmark_results.md << EOF
# JSON Tools Benchmark Results (Mini Demo)

## Test Environment
- Date: $(date)
- CPU: $(grep "model name" /proc/cpuinfo | head -n 1 | cut -d ':' -f 2 | sed 's/^[ \t]*//')
- Cores: $(grep -c processor /proc/cpuinfo)
- Memory: $(free -h | grep Mem | awk '{print $2}')

## Test Files
- Small: 50 objects ($(du -h small_test.json | awk '{print $1}'))
- Medium: 200 objects ($(du -h medium_test.json | awk '{print $1}'))

## Flattening Performance

| File Size | Single-threaded | Multi-threaded (4 threads) | Improvement |
|-----------|-----------------|----------------------------|-------------|
EOF

# Add flattening results to report
for size in "small" "medium"; do
    single_time=$(grep "real" time_flatten_${size}_test_0threads.txt | awk '{print $2}')
    multi_time=$(grep "real" time_flatten_${size}_test_4threads.txt | awk '{print $2}')
    
    # Convert time to seconds for calculation
    single_seconds=$(echo $single_time | awk -F'[ms]' '{print $1*60 + $2}')
    multi_seconds=$(echo $multi_time | awk -F'[ms]' '{print $1*60 + $2}')
    
    # Calculate improvement percentage
    if (( $(echo "$single_seconds > 0" | bc -l) )); then
        improvement=$(echo "scale=2; (($single_seconds - $multi_seconds) / $single_seconds) * 100" | bc)
        improvement="${improvement}%"
    else
        improvement="N/A"
    fi
    
    echo "| ${size^} | ${single_time} | ${multi_time} | ${improvement} |" >> benchmark_results.md
done

# Add schema generation results
cat >> benchmark_results.md << EOF

## Schema Generation Performance

| File Size | Single-threaded | Multi-threaded (4 threads) | Improvement |
|-----------|-----------------|----------------------------|-------------|
EOF

# Add schema results to report
for size in "small" "medium"; do
    single_time=$(grep "real" time_schema_${size}_test_0threads.txt | awk '{print $2}')
    multi_time=$(grep "real" time_schema_${size}_test_4threads.txt | awk '{print $2}')
    
    # Convert time to seconds for calculation
    single_seconds=$(echo $single_time | awk -F'[ms]' '{print $1*60 + $2}')
    multi_seconds=$(echo $multi_time | awk -F'[ms]' '{print $1*60 + $2}')
    
    # Calculate improvement percentage
    if (( $(echo "$single_seconds > 0" | bc -l) )); then
        improvement=$(echo "scale=2; (($single_seconds - $multi_seconds) / $single_seconds) * 100" | bc)
        improvement="${improvement}%"
    else
        improvement="N/A"
    fi
    
    echo "| ${size^} | ${single_time} | ${multi_time} | ${improvement} |" >> benchmark_results.md
done

# Add note about full benchmarks
cat >> benchmark_results.md << EOF

## Note

This is a mini benchmark demonstration. For more comprehensive benchmarks with larger datasets:

1. Run the full benchmark script: \`./run_benchmarks.sh\`
2. Generate visualization: \`./visualize_benchmarks.py\`

The full benchmarks include tests with:
- Small: 100 objects
- Medium: 1,000 objects
- Large: 10,000 objects
- Huge: 50,000 objects

## Conclusions

For these small test files, the multi-threading overhead may outweigh the benefits.
The performance improvements become more significant with larger datasets.
EOF

echo -e "${GREEN}Mini benchmark report generated: benchmark_results.md${NC}"
echo ""
echo -e "${BLUE}Mini benchmarks completed!${NC}"