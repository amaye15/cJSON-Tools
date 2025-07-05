# ============================================================================
# cJSON-Tools Main Makefile
# ============================================================================

# Default compiler
CC ?= gcc

# Include build configuration system
include build-config.mk

SRC_DIR = c-lib/src
OBJ_DIR = obj
BIN_DIR = bin

# Source files (including new optimization modules)
SRCS = $(SRC_DIR)/json_tools.c \
       $(SRC_DIR)/json_flattener.c \
       $(SRC_DIR)/json_schema_generator.c \
       $(SRC_DIR)/json_tools_builder.c \
       $(SRC_DIR)/json_utils.c \
       $(SRC_DIR)/thread_pool.c \
       $(SRC_DIR)/cJSON.c \
       $(SRC_DIR)/memory_pool.c \
       $(SRC_DIR)/json_parser_simd.c \
       $(SRC_DIR)/lockfree_queue.c \
       $(SRC_DIR)/portable_string.c \
       $(SRC_DIR)/common.c \
       $(SRC_DIR)/cpu_features.c \
       $(SRC_DIR)/regex_engine.c

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Executable
TARGET = $(BIN_DIR)/json_tools

# Default target
all: show-build-config directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files
$(TARGET): $(OBJS) | directories
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Install
install: all
	cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/json_tools

# Profile-guided optimization build
pgo: clean
	$(CC) $(CFLAGS_PGO) -o $(TARGET) $(SRCS) $(LIBS)
	@echo "Running profile generation..."
	@echo '{"test": {"nested": {"value": 123}}}' | ./$(TARGET) -f > /dev/null || true
	$(CC) $(CFLAGS_PGO_USE) -o $(TARGET) $(SRCS) $(LIBS)
	@echo "PGO build complete"

# ============================================================================
# Build Tier Targets
# ============================================================================

# Tier 1: Conservative (CI/containers)
tier1:
	$(MAKE) BUILD_TIER=1 all

# Tier 2: Moderate (general use)
tier2:
	$(MAKE) BUILD_TIER=2 all

# Tier 3: Aggressive (native builds)
tier3:
	$(MAKE) BUILD_TIER=3 all

# Tier 4: Maximum (PGO builds)
tier4:
	$(MAKE) BUILD_TIER=4 all

# Debug build
debug:
	$(MAKE) DEBUG=1 all

# CI-friendly build (alias for tier1)
ci: tier1

# Native optimized build (alias for tier3)
native: tier3

.PHONY: all directories clean install uninstall pgo debug format format-check lint dev-install setup-hooks
.PHONY: tier1 tier2 tier3 tier4 ci native show-build-config

# Python code formatting targets
format:
	@echo "🎨 Formatting Python code with Black..."
	cd py-lib && python -m black .
	@echo "📦 Sorting imports with isort..."
	cd py-lib && python -m isort . --profile black
	@echo "✅ Code formatting complete!"

format-check:
	@echo "🔍 Checking Python code formatting..."
	cd py-lib && python -m black --check .
	cd py-lib && python -m isort --check-only . --profile black
	@echo "✅ Code formatting check passed!"

lint:
	@echo "🔍 Running linting checks..."
	cd py-lib && python -m flake8 . --max-line-length=88 --extend-ignore=E203,W503
	@echo "✅ Linting checks passed!"

dev-install:
	@echo "🛠️ Installing development environment..."
	pip install black isort flake8 pytest pre-commit
	cd py-lib && pip install -e .
	@echo "✅ Development installation complete!"

setup-hooks:
	@echo "🪝 Installing pre-commit hooks..."
	pre-commit install
	@echo "✅ Pre-commit hooks installed!"

# ============================================================================
# Profile-Guided Optimization (PGO) Targets
# ============================================================================

pgo-generate:
	@echo "🎯 Building with profile generation..."
	$(MAKE) clean
	$(MAKE) BUILD_TIER=4 PGO_GENERATE=1
	@echo "✅ PGO instrumented build complete!"

pgo-use:
	@echo "🚀 Building with profile-guided optimization..."
	$(MAKE) clean
	$(MAKE) BUILD_TIER=4 PGO_USE=1
	@echo "✅ PGO optimized build complete!"

pgo-full: pgo-generate
	@echo "📊 Running training workload for PGO..."
	cd c-lib/tests && ./run_dynamic_tests.sh --sizes "1000,10000" --quick
	$(MAKE) pgo-use
	@echo "🎯 Full PGO optimization complete!"

# Clean PGO data
pgo-clean:
	@echo "🧹 Cleaning PGO profile data..."
	find . -name "*.gcda" -delete
	find . -name "*.gcno" -delete
	@echo "✅ PGO data cleaned!"

.PHONY: pgo-generate pgo-use pgo-full pgo-clean