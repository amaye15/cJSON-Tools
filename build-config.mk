# ============================================================================
# cJSON-Tools Build Configuration System
# ============================================================================
# 
# This file provides a multi-tier optimization system that automatically
# selects appropriate build flags based on the environment and target.
#
# Usage:
#   make BUILD_TIER=1  # Conservative (CI/containers)
#   make BUILD_TIER=2  # Moderate (general use)
#   make BUILD_TIER=3  # Aggressive (native builds)
#   make BUILD_TIER=4  # Maximum (PGO builds)
#   make               # Auto-detect best tier
#
# ============================================================================

# Detect build environment
BUILD_ENV := $(shell if [ -n "$$CI" ] || [ -n "$$GITHUB_ACTIONS" ] || [ -n "$$CONTAINER" ]; then echo "ci"; elif [ -n "$$ACT" ]; then echo "act"; else echo "native"; fi)

# Auto-detect build tier if not specified
ifndef BUILD_TIER
ifeq ($(BUILD_ENV),ci)
BUILD_TIER := 1
else ifeq ($(BUILD_ENV),act)
BUILD_TIER := 1
else
BUILD_TIER := 2
endif
endif

# Platform detection
UNAME_S := $(shell uname -s)
UNAME_P := $(shell uname -p)
UNAME_M := $(shell uname -m)

# Compiler detection
CC_VERSION := $(shell $(CC) --version 2>/dev/null | head -n1)
ifneq (,$(findstring gcc,$(CC_VERSION)))
COMPILER_TYPE := gcc
else ifneq (,$(findstring clang,$(CC_VERSION)))
COMPILER_TYPE := clang
else
COMPILER_TYPE := unknown
endif

# Architecture detection
ifeq ($(UNAME_M),x86_64)
ARCH := x86_64
else ifeq ($(UNAME_M),aarch64)
ARCH := arm64
else ifeq ($(UNAME_M),arm64)
ARCH := arm64
else
ARCH := generic
endif

# ============================================================================
# Base Compiler Flags (Common to all tiers)
# ============================================================================

CFLAGS_BASE := -Wall -Wextra -std=c99 -I./c-lib/include

# Debug vs Release
ifdef DEBUG
CFLAGS_BASE += -g -O0 -DDEBUG
BUILD_TYPE := debug
else
CFLAGS_BASE += -DNDEBUG
BUILD_TYPE := release
endif

# Threading support
ifndef THREADING_DISABLED
CFLAGS_BASE += -pthread
LIBS_BASE := -pthread
else
CFLAGS_BASE += -DTHREADING_DISABLED
LIBS_BASE :=
endif

# ============================================================================
# Tier 1: Conservative (Maximum Compatibility)
# ============================================================================
# Target: CI/CD, containers, unknown environments
# Focus: Reliability, broad compatibility, fast compilation

ifeq ($(BUILD_TIER),1)
CFLAGS_OPT := -O2
CFLAGS_ARCH :=
CFLAGS_LTO :=
LIBS_LTO :=
BUILD_TIER_NAME := Conservative
endif

# ============================================================================
# Tier 2: Moderate (Balanced Performance)
# ============================================================================
# Target: General desktop/server use
# Focus: Good performance with reasonable compatibility

ifeq ($(BUILD_TIER),2)
CFLAGS_OPT := -O3 -funroll-loops -fomit-frame-pointer

ifeq ($(ARCH),x86_64)
CFLAGS_ARCH := -msse2 -msse4.2
else ifeq ($(ARCH),arm64)
CFLAGS_ARCH := -mcpu=generic
else
CFLAGS_ARCH :=
endif

ifeq ($(COMPILER_TYPE),gcc)
CFLAGS_LTO := -flto=auto
LIBS_LTO := -flto=auto
else ifeq ($(COMPILER_TYPE),clang)
# Disable LTO on macOS due to compatibility issues
ifeq ($(UNAME_S),Darwin)
CFLAGS_LTO :=
LIBS_LTO :=
else
CFLAGS_LTO := -flto
LIBS_LTO := -flto
endif
else
CFLAGS_LTO :=
LIBS_LTO :=
endif

BUILD_TIER_NAME := Moderate
endif

# ============================================================================
# Tier 3: Aggressive (Native Optimization)
# ============================================================================
# Target: Native builds, known hardware
# Focus: Maximum performance for specific hardware

ifeq ($(BUILD_TIER),3)
CFLAGS_OPT := -O3 -funroll-loops -fomit-frame-pointer -finline-functions \
              -ffast-math -ftree-vectorize

ifeq ($(ARCH),x86_64)
CFLAGS_ARCH := -march=native -mtune=native -msse4.2 -mavx2
else ifeq ($(ARCH),arm64)
ifeq ($(UNAME_S),Darwin)
CFLAGS_ARCH := -mcpu=apple-a14
else
CFLAGS_ARCH := -march=native -mcpu=native
endif
else
CFLAGS_ARCH := -march=native -mtune=native
endif

ifeq ($(COMPILER_TYPE),gcc)
CFLAGS_LTO := -flto=auto -fno-stack-protector
LIBS_LTO := -flto=auto -Wl,--gc-sections
else ifeq ($(COMPILER_TYPE),clang)
# Disable LTO on macOS due to compatibility issues
ifeq ($(UNAME_S),Darwin)
CFLAGS_LTO := -fno-stack-protector
LIBS_LTO := -Wl,-dead_strip
else
CFLAGS_LTO := -flto -fno-stack-protector
LIBS_LTO := -flto -Wl,-dead_strip
endif
else
CFLAGS_LTO :=
LIBS_LTO :=
endif

BUILD_TIER_NAME := Aggressive
endif

# ============================================================================
# Tier 4: Maximum (Profile-Guided Optimization)
# ============================================================================
# Target: Production builds with profiling data
# Focus: Absolute maximum performance

ifeq ($(BUILD_TIER),4)
CFLAGS_OPT := -O3 -funroll-loops -fomit-frame-pointer -finline-functions \
              -ffast-math -ftree-vectorize -fprefetch-loop-arrays

ifeq ($(ARCH),x86_64)
CFLAGS_ARCH := -march=native -mtune=native -msse4.2 -mavx2
else ifeq ($(ARCH),arm64)
ifeq ($(UNAME_S),Darwin)
CFLAGS_ARCH := -mcpu=apple-a14
else
CFLAGS_ARCH := -march=native -mcpu=native
endif
else
CFLAGS_ARCH := -march=native -mtune=native
endif

# PGO flags (will be set by PGO targets)
ifdef PGO_GENERATE
CFLAGS_PGO := -fprofile-generate
LIBS_PGO := -fprofile-generate
else ifdef PGO_USE
CFLAGS_PGO := -fprofile-use -fprofile-correction
LIBS_PGO := -fprofile-use
else
CFLAGS_PGO :=
LIBS_PGO :=
endif

ifeq ($(COMPILER_TYPE),gcc)
CFLAGS_LTO := -flto=auto -fno-stack-protector
LIBS_LTO := -flto=auto -Wl,--gc-sections
else ifeq ($(COMPILER_TYPE),clang)
# Disable LTO on macOS due to compatibility issues
ifeq ($(UNAME_S),Darwin)
CFLAGS_LTO := -fno-stack-protector
LIBS_LTO := -Wl,-dead_strip
else
CFLAGS_LTO := -flto -fno-stack-protector
LIBS_LTO := -flto -Wl,-dead_strip
endif
else
CFLAGS_LTO :=
LIBS_LTO :=
endif

BUILD_TIER_NAME := Maximum
endif

# ============================================================================
# Platform-Specific Adjustments
# ============================================================================

ifeq ($(UNAME_S),Linux)
CFLAGS_PLATFORM := -D_GNU_SOURCE
LIBS_PLATFORM := -lm
else ifeq ($(UNAME_S),Darwin)
CFLAGS_PLATFORM := -D_DARWIN_C_SOURCE
LIBS_PLATFORM :=
else ifeq ($(UNAME_S),FreeBSD)
CFLAGS_PLATFORM := -D_BSD_SOURCE
LIBS_PLATFORM := -lm
else
CFLAGS_PLATFORM :=
LIBS_PLATFORM :=
endif

# ============================================================================
# Final Flag Assembly
# ============================================================================

CFLAGS := $(CFLAGS_BASE) $(CFLAGS_OPT) $(CFLAGS_ARCH) $(CFLAGS_LTO) $(CFLAGS_PGO) $(CFLAGS_PLATFORM) $(CFLAGS_EXTRA)
LIBS := $(LIBS_BASE) $(LIBS_LTO) $(LIBS_PGO) $(LIBS_PLATFORM) $(LIBS_EXTRA)

# ============================================================================
# Build Information Display
# ============================================================================

.PHONY: show-build-config
show-build-config:
	@echo "============================================================================"
	@echo "cJSON-Tools Build Configuration"
	@echo "============================================================================"
	@echo "Build Environment: $(BUILD_ENV)"
	@echo "Build Tier:        $(BUILD_TIER) ($(BUILD_TIER_NAME))"
	@echo "Build Type:        $(BUILD_TYPE)"
	@echo "Platform:          $(UNAME_S) $(UNAME_M)"
	@echo "Architecture:      $(ARCH)"
	@echo "Compiler:          $(CC) ($(COMPILER_TYPE))"
	@echo "Threading:         $(if $(THREADING_DISABLED),Disabled,Enabled)"
	@echo ""
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LIBS:   $(LIBS)"
	@echo "============================================================================"
