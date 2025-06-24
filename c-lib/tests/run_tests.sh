#!/bin/bash

# Build the project
echo "Building JSON Tools..."
cd ../..
make clean
make
echo ""

# Test JSON flattening
echo "=== Testing JSON Flattening ==="
echo ""

echo "1. Flattening a single object:"
./bin/json_tools -f c-lib/tests/test_object.json > c-lib/tests/flattened_object.json
echo "Output saved to c-lib/tests/flattened_object.json"
echo ""

echo "2. Flattening a batch of objects:"
./bin/json_tools -f c-lib/tests/test_batch.json > c-lib/tests/flattened_batch.json
echo "Output saved to c-lib/tests/flattened_batch.json"
echo ""

echo "3. Flattening with multi-threading:"
./bin/json_tools -f -t 4 c-lib/tests/test_batch.json > c-lib/tests/flattened_batch_mt.json
echo "Output saved to c-lib/tests/flattened_batch_mt.json"
echo ""

# Test JSON schema generation
echo "=== Testing JSON Schema Generation ==="
echo ""

echo "1. Generating schema from a single object:"
./bin/json_tools -s c-lib/tests/test_object.json > c-lib/tests/schema_object.json
echo "Output saved to c-lib/tests/schema_object.json"
echo ""

echo "2. Generating schema from a batch of objects:"
./bin/json_tools -s c-lib/tests/test_batch.json > c-lib/tests/schema_batch.json
echo "Output saved to c-lib/tests/schema_batch.json"
echo ""

echo "3. Generating schema with multi-threading:"
./bin/json_tools -s -t 4 c-lib/tests/test_batch.json > c-lib/tests/schema_batch_mt.json
echo "Output saved to c-lib/tests/schema_batch_mt.json"
echo ""

echo "4. Generating schema from mixed types:"
./bin/json_tools -s c-lib/tests/test_mixed.json > c-lib/tests/schema_mixed.json
echo "Output saved to c-lib/tests/schema_mixed.json"
echo ""

# Compare results
echo "=== Comparing Results ==="
echo ""

echo "1. Comparing single-threaded vs multi-threaded flattening:"
diff c-lib/tests/flattened_batch.json c-lib/tests/flattened_batch_mt.json > /dev/null
if [ $? -eq 0 ]; then
    echo "Results match! ✓"
else
    echo "Results differ! ✗"
fi
echo ""

echo "2. Comparing single-threaded vs multi-threaded schema generation:"
diff c-lib/tests/schema_batch.json c-lib/tests/schema_batch_mt.json > /dev/null
if [ $? -eq 0 ]; then
    echo "Results match! ✓"
else
    echo "Results differ! ✗"
fi
echo ""

echo "All tests completed!"