#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Build the test data generator
echo -e "${BLUE}Building test data generator...${NC}"
make clean
make
echo ""

# Generate large test files
echo -e "${BLUE}Generating large test files...${NC}"

echo -e "${YELLOW}Generating large test file (1,000 objects)...${NC}"
./generate_test_data large_test.json 1000 4
echo -e "${GREEN}Generated large_test.json ($(du -h large_test.json | awk '{print $1}'))${NC}"
echo ""

echo -e "${YELLOW}Generating very large test file (10,000 objects)...${NC}"
./generate_test_data very_large_test.json 10000 4
echo -e "${GREEN}Generated very_large_test.json ($(du -h very_large_test.json | awk '{print $1}'))${NC}"
echo ""

echo -e "${YELLOW}Generating huge test file (50,000 objects)...${NC}"
./generate_test_data huge_test.json 50000 4
echo -e "${GREEN}Generated huge_test.json ($(du -h huge_test.json | awk '{print $1}'))${NC}"
echo ""

echo -e "${BLUE}All test files generated successfully!${NC}"
echo -e "${BLUE}You can now run the full benchmarks with:${NC}"
echo -e "${GREEN}./run_benchmarks.sh${NC}"