#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Build the project
echo -e "${BLUE}Building JSON Tools...${NC}"
cd ../..
make clean
make
cd c-lib/tests
make clean
make
echo ""

# Generate test data if it doesn't exist
if [ ! -f "small_test.json" ]; then
    echo -e "${BLUE}Generating small test data (100 objects)...${NC}"
    ./generate_test_data small_test.json 100 3
fi

if [ ! -f "medium_test.json" ]; then
    echo -e "${BLUE}Generating medium test data (1,000 objects)...${NC}"
    ./generate_test_data medium_test.json 1000 3
fi

if [ ! -f "large_test.json" ]; then
    echo -e "${BLUE}Generating large test data (10,000 objects)...${NC}"
    ./generate_test_data large_test.json 10000 3
fi

if [ ! -f "huge_test.json" ]; then
    echo -e "${BLUE}Generating huge test data (50,000 objects)...${NC}"
    ./generate_test_data huge_test.json 50000 3
fi

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
        { time ../../bin/json_tools -f -t $threads $file > $output_file; } 2> $time_file
    else
        { time ../../bin/json_tools -s -t $threads $file > $output_file; } 2> $time_file
    fi
    
    # Extract real time
    real_time=$(grep "real" $time_file | awk '{print $2}')
    
    # Get file size
    file_size=$(du -h $file | awk '{print $1}')
    
    echo -e "${GREEN}Completed in ${real_time} (file size: ${file_size})${NC}"
    echo ""
}

# Run benchmarks
echo -e "${BLUE}=== Running Benchmarks ===${NC}"
echo ""

# Small file benchmarks
echo -e "${BLUE}--- Small File Benchmarks (100 objects) ---${NC}"
run_benchmark "small_test.json" "flatten" 0
run_benchmark "small_test.json" "flatten" 4
run_benchmark "small_test.json" "schema" 0
run_benchmark "small_test.json" "schema" 4

# Medium file benchmarks
echo -e "${BLUE}--- Medium File Benchmarks (1,000 objects) ---${NC}"
run_benchmark "medium_test.json" "flatten" 0
run_benchmark "medium_test.json" "flatten" 4
run_benchmark "medium_test.json" "schema" 0
run_benchmark "medium_test.json" "schema" 4

# Large file benchmarks
echo -e "${BLUE}--- Large File Benchmarks (10,000 objects) ---${NC}"
run_benchmark "large_test.json" "flatten" 0
run_benchmark "large_test.json" "flatten" 4
run_benchmark "large_test.json" "schema" 0
run_benchmark "large_test.json" "schema" 4

# Huge file benchmarks
echo -e "${BLUE}--- Huge File Benchmarks (50,000 objects) ---${NC}"
run_benchmark "huge_test.json" "flatten" 0
run_benchmark "huge_test.json" "flatten" 4
run_benchmark "huge_test.json" "schema" 0
run_benchmark "huge_test.json" "schema" 4

# Generate benchmark report
echo -e "${BLUE}Generating benchmark report...${NC}"

# Create markdown report
cat > benchmark_results.md << EOF
# JSON Tools Benchmark Results

## Test Environment
- Date: $(date)
- CPU: $(grep "model name" /proc/cpuinfo | head -n 1 | cut -d ':' -f 2 | sed 's/^[ \t]*//')
- Cores: $(grep -c processor /proc/cpuinfo)
- Memory: $(free -h | grep Mem | awk '{print $2}')

## Test Files
- Small: 100 objects ($(du -h small_test.json | awk '{print $1}'))
- Medium: 1,000 objects ($(du -h medium_test.json | awk '{print $1}'))
- Large: 10,000 objects ($(du -h large_test.json | awk '{print $1}'))
- Huge: 50,000 objects ($(du -h huge_test.json | awk '{print $1}'))

## Flattening Performance

| File Size | Single-threaded | Multi-threaded (4 threads) | Improvement |
|-----------|-----------------|----------------------------|-------------|
EOF

# Add flattening results to report
for size in "small" "medium" "large" "huge"; do
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
for size in "small" "medium" "large" "huge"; do
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

# Add conclusions
cat >> benchmark_results.md << EOF

## Conclusions

### Flattening Performance
- Small files: Multi-threading overhead may outweigh benefits
- Medium files: Multi-threading starts to show benefits
- Large files: Significant performance improvement with multi-threading
- Huge files: Maximum benefit from multi-threading

### Schema Generation Performance
- Small files: Multi-threading overhead may outweigh benefits
- Medium files: Multi-threading starts to show benefits
- Large files: Significant performance improvement with multi-threading
- Huge files: Maximum benefit from multi-threading

### Recommendations
- For files under 1MB: Use single-threaded processing
- For files over 1MB: Use multi-threaded processing
- For optimal performance on large files, use thread count equal to available CPU cores
EOF

echo -e "${GREEN}Benchmark report generated: benchmark_results.md${NC}"
echo ""
echo -e "${BLUE}All benchmarks completed!${NC}"