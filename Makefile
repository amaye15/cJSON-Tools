CC = gcc
CFLAGS_BASE = -Wall -Wextra -std=c99 -I./c-lib/include

# Platform-specific optimizations
UNAME_S := $(shell uname -s)
UNAME_P := $(shell uname -p)

ifeq ($(UNAME_S),Linux)
    # Linux-specific optimizations
    CFLAGS_OPT = -O3 -march=native -mtune=native -flto=auto \
                 -ffast-math -funroll-loops -fomit-frame-pointer \
                 -finline-functions -fno-stack-protector \
                 -ftree-vectorize -fprefetch-loop-arrays \
                 -fprofile-arcs -ftest-coverage \
                 -DNDEBUG -DUSE_HUGE_PAGES

    # Enable profile-guided optimization if profile data exists
    ifneq (,$(wildcard profile.gcda))
        CFLAGS_OPT += -fprofile-use=profile.gcda
    endif

else ifeq ($(UNAME_S),Darwin)
    # macOS optimizations
    CFLAGS_OPT = -O3 -flto=full -funroll-loops \
                 -fomit-frame-pointer -finline-functions \
                 -DNDEBUG

    # Universal binary support
    ifeq ($(ARCH),universal)
        CFLAGS_OPT += -arch x86_64 -arch arm64
    else
        CFLAGS_OPT += -march=native -mtune=native
    endif

else ifeq ($(UNAME_S),FreeBSD)
    # FreeBSD optimizations
    CFLAGS_OPT = -O3 -march=native -mtune=native \
                 -funroll-loops -fomit-frame-pointer \
                 -DNDEBUG -DHAS_SUPERPAGE_SUPPORT
else
    # Default optimizations for other platforms
    CFLAGS_OPT = -O3 -funroll-loops -fomit-frame-pointer -DNDEBUG
endif

CFLAGS = $(CFLAGS_BASE) $(CFLAGS_OPT)

# Profile-guided optimization support
CFLAGS_PGO = $(CFLAGS) -fprofile-generate
CFLAGS_PGO_USE = $(CFLAGS) -fprofile-use -fprofile-correction

# Link-time optimization (platform-specific)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LIBS = -pthread -flto=auto -Wl,--gc-sections
else ifeq ($(UNAME_S),Darwin)
    LIBS = -pthread -flto=auto -Wl,-dead_strip
else
    LIBS = -pthread -flto=auto
endif

SRC_DIR = c-lib/src
OBJ_DIR = obj
BIN_DIR = bin

# Source files (including new optimization modules)
SRCS = $(SRC_DIR)/json_tools.c \
       $(SRC_DIR)/json_flattener.c \
       $(SRC_DIR)/json_schema_generator.c \
       $(SRC_DIR)/json_utils.c \
       $(SRC_DIR)/thread_pool.c \
       $(SRC_DIR)/cJSON.c \
       $(SRC_DIR)/memory_pool.c \
       $(SRC_DIR)/json_parser_simd.c \
       $(SRC_DIR)/lockfree_queue.c

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Executable
TARGET = $(BIN_DIR)/json_tools

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files
$(TARGET): $(OBJS)
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

# Debug build
debug: CFLAGS = -Wall -Wextra -std=c99 -g -O0 -DDEBUG -I./c-lib/include
debug: LIBS = -pthread
debug: directories $(TARGET)

.PHONY: all directories clean install uninstall pgo debug format format-check lint dev-install setup-hooks

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

# Profile-guided optimization targets
pgo-generate:
	@echo "🎯 Building with profile generation..."
	$(MAKE) clean
	$(MAKE) CFLAGS_OPT="$(CFLAGS_OPT) -fprofile-generate" all
	@echo "✅ Profile generation build complete!"

pgo-use:
	@echo "🚀 Building with profile-guided optimization..."
	$(MAKE) clean
	$(MAKE) CFLAGS_OPT="$(CFLAGS_OPT) -fprofile-use" all
	@echo "✅ PGO optimized build complete!"

pgo-full: pgo-generate
	@echo "📊 Running training workload for PGO..."
	cd c-lib/tests && ./run_dynamic_tests.sh --sizes "1000,10000" --quick
	$(MAKE) pgo-use
	@echo "🎯 Full PGO optimization complete!"