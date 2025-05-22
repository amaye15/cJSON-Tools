#!/bin/bash

# Test script for optimized multi-threading implementation

echo "Creating test data..."
# Create a small test file (single object)
cat > test_small.json << EOF
{
  "name": "John Doe",
  "age": 30,
  "address": {
    "street": "123 Main St",
    "city": "Anytown",
    "state": "CA",
    "zip": "12345"
  },
  "phones": [
    {
      "type": "home",
      "number": "555-1234"
    },
    {
      "type": "work",
      "number": "555-5678"
    }
  ],
  "active": true
}
EOF

# Create a medium test file (batch of objects)
cat > test_medium.json << EOF
[
  {
    "id": 1,
    "name": "John Doe",
    "email": "john@example.com",
    "active": true,
    "tags": ["admin", "user"]
  },
  {
    "id": 2,
    "name": "Jane Smith",
    "email": "jane@example.com",
    "active": false
  },
  {
    "id": 3,
    "name": "Bob Johnson",
    "email": "bob@example.com",
    "active": true,
    "department": {
      "id": 101,
      "name": "Engineering"
    }
  }
]
EOF

# Generate a larger test file with 1000 objects
echo "[" > test_large.json
for i in {1..1000}; do
  echo "  {
    \"id\": $i,
    \"name\": \"User $i\",
    \"email\": \"user$i@example.com\",
    \"active\": $([ $((i % 2)) -eq 0 ] && echo "true" || echo "false"),
    \"created\": \"2025-01-$((i % 28 + 1))\",
    \"profile\": {
      \"age\": $((20 + i % 50)),
      \"location\": \"City $((i % 100))\",
      \"interests\": [\"hobby$((i % 5 + 1))\", \"hobby$((i % 5 + 2))\"]
    }
  }$([ $i -lt 1000 ] && echo ",")" >> test_large.json
done
echo "]" >> test_large.json

echo "Testing single-threaded flattening (small file)..."
time ../bin/json_tools -f test_small.json -o flattened_small_st.json

echo "Testing multi-threaded flattening (small file)..."
time ../bin/json_tools -f -t 4 test_small.json -o flattened_small_mt.json

echo "Testing single-threaded flattening (medium file)..."
time ../bin/json_tools -f test_medium.json -o flattened_medium_st.json

echo "Testing multi-threaded flattening (medium file)..."
time ../bin/json_tools -f -t 4 test_medium.json -o flattened_medium_mt.json

echo "Testing single-threaded flattening (large file)..."
time ../bin/json_tools -f test_large.json -o flattened_large_st.json

echo "Testing multi-threaded flattening (large file)..."
time ../bin/json_tools -f -t 4 test_large.json -o flattened_large_mt.json

echo "Testing single-threaded schema generation (small file)..."
time ../bin/json_tools -s test_small.json -o schema_small_st.json

echo "Testing multi-threaded schema generation (small file)..."
time ../bin/json_tools -s -t 4 test_small.json -o schema_small_mt.json

echo "Testing single-threaded schema generation (medium file)..."
time ../bin/json_tools -s test_medium.json -o schema_medium_st.json

echo "Testing multi-threaded schema generation (medium file)..."
time ../bin/json_tools -s -t 4 test_medium.json -o schema_medium_mt.json

echo "Testing single-threaded schema generation (large file)..."
time ../bin/json_tools -s test_large.json -o schema_large_st.json

echo "Testing multi-threaded schema generation (large file)..."
time ../bin/json_tools -s -t 4 test_large.json -o schema_large_mt.json

echo "Verifying results..."
# Compare single-threaded and multi-threaded outputs
diff flattened_small_st.json flattened_small_mt.json
diff flattened_medium_st.json flattened_medium_mt.json
diff flattened_large_st.json flattened_large_mt.json
diff schema_small_st.json schema_small_mt.json
diff schema_medium_st.json schema_medium_mt.json
diff schema_large_st.json schema_large_mt.json

echo "Tests completed."