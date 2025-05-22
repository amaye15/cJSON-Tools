#!/bin/bash

# Build the project
echo "Building JSON Tools..."
cd ..
make clean
make
echo ""

# Test JSON flattening
echo "=== Testing JSON Flattening ==="
echo ""

echo "1. Flattening a single object:"
./bin/json_tools -f test/test_object.json > test/flattened_object.json
echo "Output saved to test/flattened_object.json"
echo ""

echo "2. Flattening a batch of objects:"
./bin/json_tools -f test/test_batch.json > test/flattened_batch.json
echo "Output saved to test/flattened_batch.json"
echo ""

echo "3. Flattening with multi-threading:"
./bin/json_tools -f -t 4 test/test_batch.json > test/flattened_batch_mt.json
echo "Output saved to test/flattened_batch_mt.json"
echo ""

# Test JSON schema generation
echo "=== Testing JSON Schema Generation ==="
echo ""

echo "1. Generating schema from a single object:"
./bin/json_tools -s test/test_object.json > test/schema_object.json
echo "Output saved to test/schema_object.json"
echo ""

echo "2. Generating schema from a batch of objects:"
./bin/json_tools -s test/test_batch.json > test/schema_batch.json
echo "Output saved to test/schema_batch.json"
echo ""

echo "3. Generating schema with multi-threading:"
./bin/json_tools -s -t 4 test/test_batch.json > test/schema_batch_mt.json
echo "Output saved to test/schema_batch_mt.json"
echo ""

echo "4. Generating schema from mixed types:"
./bin/json_tools -s test/test_mixed.json > test/schema_mixed.json
echo "Output saved to test/schema_mixed.json"
echo ""

# Compare results
echo "=== Comparing Results ==="
echo ""

echo "1. Comparing single-threaded vs multi-threaded flattening:"
diff test/flattened_batch.json test/flattened_batch_mt.json > /dev/null
if [ $? -eq 0 ]; then
    echo "Results match! ✓"
else
    echo "Results differ! ✗"
fi
echo ""

echo "2. Comparing single-threaded vs multi-threaded schema generation:"
diff test/schema_batch.json test/schema_batch_mt.json > /dev/null
if [ $? -eq 0 ]; then
    echo "Results match! ✓"
else
    echo "Results differ! ✗"
fi
echo ""

echo "All tests completed!"