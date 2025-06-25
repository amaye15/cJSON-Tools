#!/bin/bash

# Dynamic Test Runner for cJSON-Tools
# Generates test data on-the-fly and runs comprehensive tests

set -e  # Exit on any error

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Test configuration
TEMP_DIR="temp_test_data"
TEST_SIZES=(10 100 1000 10000)  # Small to medium test sizes

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to cleanup temporary files
cleanup() {
    print_status "Cleaning up temporary files..."
    rm -rf "$TEMP_DIR"
    print_success "Cleanup completed"
}

# Set up cleanup trap
trap cleanup EXIT

# Function to generate test data
generate_test_data() {
    local size=$1
    local filename=$2
    local depth=${3:-3}

    print_status "Generating test data: $size objects, depth $depth -> $filename"

    # For large datasets, reduce depth to avoid excessive memory usage
    if [ "$size" -gt 10000 ]; then
        depth=2
        print_status "Using reduced depth ($depth) for large dataset"
    fi

    start_time=$(date +%s.%N)
    ./generate_test_data "$filename" "$size" "$depth"
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc -l)

    file_size=$(du -h "$filename" | cut -f1)
    print_success "Generated $size objects in ${duration}s (file size: $file_size)"
}

# Function to run a test and measure time
run_test() {
    local test_name="$1"
    local command="$2"
    local input_file="$3"
    local output_file="$4"
    
    print_status "Running test: $test_name"
    
    # Measure execution time
    start_time=$(date +%s.%N)
    
    if eval "$command \"$input_file\" > \"$output_file\""; then
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc -l)
        print_success "Test completed in ${duration}s: $test_name"
        return 0
    else
        print_error "Test failed: $test_name"
        return 1
    fi
}

# Function to compare two JSON files
compare_results() {
    local file1="$1"
    local file2="$2"
    local test_name="$3"
    
    if diff "$file1" "$file2" > /dev/null; then
        print_success "Results match: $test_name"
        return 0
    else
        print_warning "Results differ: $test_name"
        return 1
    fi
}

# Main test execution
main() {
    print_status "Starting dynamic tests for cJSON-Tools"
    
    # Build the main project
    print_status "Building cJSON-Tools..."
    cd ../..
    make clean
    make
    cd c-lib/tests
    
    # Build test data generator
    print_status "Building test data generator..."
    make clean
    make
    
    # Create temporary directory for test data
    mkdir -p "$TEMP_DIR"
    
    # Generate basic test objects
    print_status "Generating basic test objects..."
    
    # Create a simple object for basic tests
    cat > "$TEMP_DIR/test_object.json" << 'EOF'
{
    "name": "John Doe",
    "age": 30,
    "email": "john@example.com",
    "address": {
        "street": "123 Main St",
        "city": "Anytown",
        "zip": "12345"
    },
    "tags": ["developer", "tester"],
    "active": true,
    "score": 95.5,
    "metadata": null
}
EOF

    # Create a mixed types test
    cat > "$TEMP_DIR/test_mixed.json" << 'EOF'
[
    {
        "id": 1,
        "name": "Alice",
        "active": true,
        "score": 85.5
    },
    {
        "id": 2,
        "name": "Bob",
        "active": false,
        "score": null,
        "tags": ["admin"]
    },
    {
        "id": 3,
        "name": "Charlie",
        "active": true,
        "score": 92.0,
        "tags": ["user", "premium"],
        "metadata": {
            "created": "2023-01-01",
            "updated": "2023-12-01"
        }
    }
]
EOF

    # Generate dynamic test data for different sizes
    for size in "${TEST_SIZES[@]}"; do
        generate_test_data "$size" "$TEMP_DIR/test_${size}.json" 3
    done
    
    print_status "=== Running JSON Flattening Tests ==="
    
    # Test 1: Flatten single object
    run_test "Flatten single object" \
        "../../bin/json_tools -f" \
        "$TEMP_DIR/test_object.json" \
        "$TEMP_DIR/flattened_object.json"
    
    # Test 2: Flatten different sized batches
    for size in "${TEST_SIZES[@]}"; do
        print_status "Testing flattening with $size objects..."

        # Single-threaded
        run_test "Flatten $size objects (single-threaded)" \
            "../../bin/json_tools -f" \
            "$TEMP_DIR/test_${size}.json" \
            "$TEMP_DIR/flattened_${size}_st.json"

        # Multi-threaded
        run_test "Flatten $size objects (multi-threaded)" \
            "../../bin/json_tools -f -t 4" \
            "$TEMP_DIR/test_${size}.json" \
            "$TEMP_DIR/flattened_${size}_mt.json"

        # For very large datasets, skip detailed comparison to save time
        if [ "$size" -le 10000 ]; then
            compare_results \
                "$TEMP_DIR/flattened_${size}_st.json" \
                "$TEMP_DIR/flattened_${size}_mt.json" \
                "Flatten $size objects (ST vs MT)"
        else
            print_status "Skipping detailed comparison for large dataset ($size objects)"
        fi
    done
    
    print_status "=== Running JSON Schema Generation Tests ==="
    
    # Test 3: Generate schema from single object
    run_test "Generate schema from single object" \
        "../../bin/json_tools -s" \
        "$TEMP_DIR/test_object.json" \
        "$TEMP_DIR/schema_object.json"
    
    # Test 4: Generate schema from mixed types
    run_test "Generate schema from mixed types" \
        "../../bin/json_tools -s" \
        "$TEMP_DIR/test_mixed.json" \
        "$TEMP_DIR/schema_mixed.json"
    
    # Test 5: Generate schema from different sized batches
    for size in "${TEST_SIZES[@]}"; do
        print_status "Testing schema generation with $size objects..."

        # Single-threaded
        run_test "Generate schema from $size objects (single-threaded)" \
            "../../bin/json_tools -s" \
            "$TEMP_DIR/test_${size}.json" \
            "$TEMP_DIR/schema_${size}_st.json"

        # Multi-threaded
        run_test "Generate schema from $size objects (multi-threaded)" \
            "../../bin/json_tools -s -t 4" \
            "$TEMP_DIR/test_${size}.json" \
            "$TEMP_DIR/schema_${size}_mt.json"

        # For very large datasets, skip detailed comparison to save time
        if [ "$size" -le 10000 ]; then
            compare_results \
                "$TEMP_DIR/schema_${size}_st.json" \
                "$TEMP_DIR/schema_${size}_mt.json" \
                "Schema generation $size objects (ST vs MT)"
        else
            print_status "Skipping detailed comparison for large dataset ($size objects)"
        fi
    done
    
    print_status "=== Running Performance Tests ==="

    # Performance comparison for multiple dataset sizes
    print_status "Performance comparison across different dataset sizes:"

    for size in 1000 10000; do
        if [ -f "$TEMP_DIR/test_${size}.json" ]; then
            print_status "Performance test with $size objects..."

            # Measure flattening performance
            start_time=$(date +%s.%N)
            ../../bin/json_tools -f "$TEMP_DIR/test_${size}.json" > /dev/null
            st_time=$(echo "$(date +%s.%N) - $start_time" | bc -l)

            start_time=$(date +%s.%N)
            ../../bin/json_tools -f -t 4 "$TEMP_DIR/test_${size}.json" > /dev/null
            mt_time=$(echo "$(date +%s.%N) - $start_time" | bc -l)

            # Calculate throughput (objects per second) with zero-time protection
            if (( $(echo "$st_time > 0.000001" | bc -l) )); then
                st_throughput=$(echo "scale=0; $size / $st_time" | bc -l)
            else
                st_throughput="∞"
                st_time="<0.000001"
            fi

            if (( $(echo "$mt_time > 0.000001" | bc -l) )); then
                mt_throughput=$(echo "scale=0; $size / $mt_time" | bc -l)
            else
                mt_throughput="∞"
                mt_time="<0.000001"
            fi

            print_success "  $size objects - Flattening:"
            print_success "    Single-threaded: ${st_time}s (${st_throughput} obj/s)"
            print_success "    Multi-threaded:  ${mt_time}s (${mt_throughput} obj/s)"

            # Only calculate speedup if both times are measurable
            if (( $(echo "$st_time > 0.000001" | bc -l) )) && (( $(echo "$mt_time > 0.000001" | bc -l) )); then
                if (( $(echo "$st_time > $mt_time" | bc -l) )); then
                    speedup=$(echo "scale=2; $st_time / $mt_time" | bc -l)
                    print_success "    Speedup: ${speedup}x"
                else
                    slowdown=$(echo "scale=2; $mt_time / $st_time" | bc -l)
                    print_warning "    Multi-threading slower by: ${slowdown}x"
                fi
            else
                print_status "    Performance too fast to measure accurately"
            fi
            echo ""
        fi
    done
    
    print_success "All dynamic tests completed successfully!"
    
    # Show generated file sizes
    print_status "Generated test data summary:"
    for size in "${TEST_SIZES[@]}"; do
        if [ -f "$TEMP_DIR/test_${size}.json" ]; then
            file_size=$(du -h "$TEMP_DIR/test_${size}.json" | cut -f1)
            print_status "  $size objects: $file_size"
        fi
    done
}

# Run main function
main "$@"
