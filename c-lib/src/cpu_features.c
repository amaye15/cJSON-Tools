#include "../include/cpu_features.h"

// Platform-specific includes
#ifdef __WINDOWS__
#include <windows.h>
#include <intrin.h>
#elif defined(__linux__)
#include <unistd.h>
// Try to include auxv and hwcap headers if available
#ifdef __has_include
#if __has_include(<sys/auxv.h>)
#include <sys/auxv.h>
#define HAS_AUXV 1
#endif
#if __has_include(<asm/hwcap.h>)
#include <asm/hwcap.h>
#define HAS_HWCAP 1
#endif
#if __has_include(<linux/auxvec.h>)
#include <linux/auxvec.h>
#define HAS_AUXVEC 1
#endif
#else
// Fallback for older compilers
#include <sys/auxv.h>
#define HAS_AUXV 1
// Don't include hwcap.h as it may not be available
#endif
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

// Global CPU features structure
static cpu_features_t g_cpu_features = {0};
static ATOMIC_INT g_features_initialized = 0;

// ============================================================================
// Platform-Specific Detection Functions
// ============================================================================

#if defined(ARCH_X86_64) || defined(ARCH_X86_32)

/**
 * Execute CPUID instruction (x86/x86_64)
 */
static void cpuid(unsigned int leaf, unsigned int subleaf, 
                  unsigned int* eax, unsigned int* ebx, 
                  unsigned int* ecx, unsigned int* edx) {
#ifdef COMPILER_MSVC
    int regs[4];
    __cpuidex(regs, leaf, subleaf);
    *eax = regs[0];
    *ebx = regs[1];
    *ecx = regs[2];
    *edx = regs[3];
#elif defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
#else
    // Fallback: assume no features
    *eax = *ebx = *ecx = *edx = 0;
#endif
}

/**
 * Detect x86/x86_64 CPU features
 */
static void detect_x86_features(cpu_features_t* features) {
    unsigned int eax, ebx, ecx, edx;
    
    // Check if CPUID is supported
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax == 0) return;
    
    // Get vendor string
    memcpy(features->vendor_string, &ebx, 4);
    memcpy(features->vendor_string + 4, &edx, 4);
    memcpy(features->vendor_string + 8, &ecx, 4);
    features->vendor_string[12] = '\0';
    
    // Feature detection from CPUID leaf 1
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    // EDX features
    features->has_sse2 = (edx >> 26) & 1;
    
    // ECX features
    features->has_sse4_1 = (ecx >> 19) & 1;
    features->has_sse4_2 = (ecx >> 20) & 1;
    features->has_avx = (ecx >> 28) & 1;
    features->has_popcnt = (ecx >> 23) & 1;
    features->has_fma = (ecx >> 12) & 1;
    
    // Extended features (leaf 7)
    cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    features->has_avx2 = (ebx >> 5) & 1;
    features->has_bmi1 = (ebx >> 3) & 1;
    features->has_bmi2 = (ebx >> 8) & 1;
    features->has_avx512f = (ebx >> 16) & 1;
    
    // Cache information
    features->cache_line_size = 64; // Default for x86
    
    // Get brand string
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        unsigned int* brand = (unsigned int*)features->brand_string;
        cpuid(0x80000002, 0, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, 0, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, 0, &brand[8], &brand[9], &brand[10], &brand[11]);
        features->brand_string[48] = '\0';
    }
}

#elif defined(ARCH_ARM64) || defined(ARCH_ARM32)

/**
 * Detect ARM CPU features
 */
static void detect_arm_features(cpu_features_t* features) {
    strcpy(features->vendor_string, "ARM");

#ifdef __linux__
#ifdef HAS_AUXV
    // Use getauxval to detect ARM features if available
    #ifndef AT_HWCAP
    #define AT_HWCAP 16
    #endif
    unsigned long hwcap = getauxval(AT_HWCAP);

    // Define HWCAP constants if not available
    #ifndef HWCAP_NEON
    #define HWCAP_NEON (1 << 12)
    #endif
    #ifndef HWCAP_CRC32
    #define HWCAP_CRC32 (1 << 7)
    #endif
    #ifndef HWCAP_AES
    #define HWCAP_AES (1 << 3)
    #endif
    #ifndef HWCAP_SHA1
    #define HWCAP_SHA1 (1 << 5)
    #endif
    #ifndef HWCAP_SHA2
    #define HWCAP_SHA2 (1 << 6)
    #endif

    features->has_neon = (hwcap & HWCAP_NEON) != 0;
    features->has_crc32 = (hwcap & HWCAP_CRC32) != 0;
    features->has_aes = (hwcap & HWCAP_AES) != 0;
    features->has_sha1 = (hwcap & HWCAP_SHA1) != 0;
    features->has_sha2 = (hwcap & HWCAP_SHA2) != 0;
#else
    // Fallback: assume basic ARM features are available
    features->has_neon = 1;  // Most modern ARM processors have NEON
    features->has_crc32 = 0;
    features->has_aes = 0;
    features->has_sha1 = 0;
    features->has_sha2 = 0;
#endif

#elif defined(__APPLE__)
    // macOS ARM detection
    size_t size = sizeof(int);
    int has_neon = 0;
    
    if (sysctlbyname("hw.optional.neon", &has_neon, &size, NULL, 0) == 0) {
        features->has_neon = has_neon;
    }
    
    // Apple Silicon typically has these features
    features->has_crc32 = 1;
    features->has_aes = 1;
    features->has_sha1 = 1;
    features->has_sha2 = 1;
#endif

    // Default cache line size for ARM
    features->cache_line_size = 64;
}

#endif

/**
 * Detect number of CPU cores
 */
static void detect_cpu_cores(cpu_features_t* features) {
#ifdef __WINDOWS__
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    features->num_logical_cores = sysinfo.dwNumberOfProcessors;
    features->num_cores = sysinfo.dwNumberOfProcessors; // Approximation
    
#elif defined(__linux__) || defined(__APPLE__)
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    features->num_logical_cores = (nprocs > 0) ? (unsigned int)nprocs : 1;
    
#ifdef __linux__
    // Try to get physical core count from /proc/cpuinfo
    FILE* fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        int physical_cores = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "cpu cores", 9) == 0) {
                sscanf(line, "cpu cores\t: %d", &physical_cores);
                break;
            }
        }
        fclose(fp);
        features->num_cores = physical_cores > 0 ? (unsigned int)physical_cores : features->num_logical_cores;
    } else {
        features->num_cores = features->num_logical_cores;
    }
    
#elif defined(__APPLE__)
    size_t size = sizeof(unsigned int);
    sysctlbyname("hw.physicalcpu", &features->num_cores, &size, NULL, 0);
    sysctlbyname("hw.logicalcpu", &features->num_logical_cores, &size, NULL, 0);
#endif

#else
    // Fallback
    features->num_cores = 1;
    features->num_logical_cores = 1;
#endif
}

// ============================================================================
// Public API Implementation
// ============================================================================

/**
 * Initialize CPU feature detection
 */
int cpu_features_init(void) {
    // Check if already initialized
    if (ATOMIC_LOAD(&g_features_initialized)) {
        return SUCCESS;
    }
    
    // Clear the structure
    memset(&g_cpu_features, 0, sizeof(cpu_features_t));
    
    // Detect architecture-specific features
#if defined(ARCH_X86_64) || defined(ARCH_X86_32)
    detect_x86_features(&g_cpu_features);
    g_cpu_features.has_64bit = sizeof(void*) == 8;
#elif defined(ARCH_ARM64) || defined(ARCH_ARM32)
    detect_arm_features(&g_cpu_features);
    g_cpu_features.has_64bit = sizeof(void*) == 8;
#else
    // Unknown architecture
    strcpy(g_cpu_features.vendor_string, "Unknown");
    g_cpu_features.cache_line_size = 64; // Safe default
#endif
    
    // Detect CPU core information
    detect_cpu_cores(&g_cpu_features);
    
    // Mark as initialized
    ATOMIC_STORE(&g_features_initialized, 1);
    
    DEBUG_PRINT("CPU features initialized successfully");
    return SUCCESS;
}

/**
 * Get CPU features structure
 */
const cpu_features_t* cpu_features_get(void) {
    if (!ATOMIC_LOAD(&g_features_initialized)) {
        cpu_features_init();
    }
    return &g_cpu_features;
}

/**
 * Check if a specific feature is available
 */
int cpu_has_feature(const char* feature_name) {
    const cpu_features_t* features = cpu_features_get();
    if (!features || !feature_name) return 0;
    
    // String comparison for feature names
    if (strcmp(feature_name, "sse2") == 0) return features->has_sse2;
    if (strcmp(feature_name, "sse4.1") == 0) return features->has_sse4_1;
    if (strcmp(feature_name, "sse4.2") == 0) return features->has_sse4_2;
    if (strcmp(feature_name, "avx") == 0) return features->has_avx;
    if (strcmp(feature_name, "avx2") == 0) return features->has_avx2;
    if (strcmp(feature_name, "avx512f") == 0) return features->has_avx512f;
    if (strcmp(feature_name, "neon") == 0) return features->has_neon;
    if (strcmp(feature_name, "crc32") == 0) return features->has_crc32;
    if (strcmp(feature_name, "aes") == 0) return features->has_aes;
    if (strcmp(feature_name, "popcnt") == 0) return features->has_popcnt;
    if (strcmp(feature_name, "fma") == 0) return features->has_fma;
    
    return 0; // Unknown feature
}

/**
 * Get optimal SIMD instruction set for current CPU
 */
const char* cpu_get_optimal_simd(void) {
    const cpu_features_t* features = cpu_features_get();
    if (!features) return "none";

#if defined(ARCH_X86_64) || defined(ARCH_X86_32)
    if (features->has_avx512f) return "avx512";
    if (features->has_avx2) return "avx2";
    if (features->has_avx) return "avx";
    if (features->has_sse4_2) return "sse4.2";
    if (features->has_sse4_1) return "sse4.1";
    if (features->has_sse2) return "sse2";
#elif defined(ARCH_ARM64) || defined(ARCH_ARM32)
    if (features->has_neon) return "neon";
#endif

    return "scalar";
}

/**
 * Get number of CPU cores (physical)
 */
unsigned int cpu_get_num_cores(void) {
    const cpu_features_t* features = cpu_features_get();
    return features ? features->num_cores : 1;
}

/**
 * Get number of logical CPU cores (including hyperthreading)
 */
unsigned int cpu_get_num_logical_cores(void) {
    const cpu_features_t* features = cpu_features_get();
    return features ? features->num_logical_cores : 1;
}

/**
 * Get cache line size
 */
unsigned int cpu_get_cache_line_size(void) {
    const cpu_features_t* features = cpu_features_get();
    return features ? features->cache_line_size : 64;
}

/**
 * Print CPU information to stdout
 */
void cpu_features_print_info(void) {
    const cpu_features_t* features = cpu_features_get();
    if (!features) {
        printf("CPU features not initialized\n");
        return;
    }

    printf("============================================================================\n");
    printf("CPU Information\n");
    printf("============================================================================\n");
    printf("Vendor:           %s\n", features->vendor_string);
    if (strlen(features->brand_string) > 0) {
        printf("Brand:            %s\n", features->brand_string);
    }
    printf("Architecture:     %s\n",
#if defined(ARCH_X86_64)
           "x86_64"
#elif defined(ARCH_X86_32)
           "x86_32"
#elif defined(ARCH_ARM64)
           "ARM64"
#elif defined(ARCH_ARM32)
           "ARM32"
#else
           "Unknown"
#endif
    );
    printf("64-bit:           %s\n", features->has_64bit ? "Yes" : "No");
    printf("Physical cores:   %u\n", features->num_cores);
    printf("Logical cores:    %u\n", features->num_logical_cores);
    printf("Cache line size:  %u bytes\n", features->cache_line_size);
    printf("Optimal SIMD:     %s\n", cpu_get_optimal_simd());

    printf("\nSIMD Features:\n");
#if defined(ARCH_X86_64) || defined(ARCH_X86_32)
    printf("  SSE2:           %s\n", features->has_sse2 ? "Yes" : "No");
    printf("  SSE4.1:         %s\n", features->has_sse4_1 ? "Yes" : "No");
    printf("  SSE4.2:         %s\n", features->has_sse4_2 ? "Yes" : "No");
    printf("  AVX:            %s\n", features->has_avx ? "Yes" : "No");
    printf("  AVX2:           %s\n", features->has_avx2 ? "Yes" : "No");
    printf("  AVX-512F:       %s\n", features->has_avx512f ? "Yes" : "No");
    printf("  POPCNT:         %s\n", features->has_popcnt ? "Yes" : "No");
    printf("  BMI1:           %s\n", features->has_bmi1 ? "Yes" : "No");
    printf("  BMI2:           %s\n", features->has_bmi2 ? "Yes" : "No");
    printf("  FMA:            %s\n", features->has_fma ? "Yes" : "No");
#elif defined(ARCH_ARM64) || defined(ARCH_ARM32)
    printf("  NEON:           %s\n", features->has_neon ? "Yes" : "No");
    printf("  CRC32:          %s\n", features->has_crc32 ? "Yes" : "No");
    printf("  AES:            %s\n", features->has_aes ? "Yes" : "No");
    printf("  SHA1:           %s\n", features->has_sha1 ? "Yes" : "No");
    printf("  SHA2:           %s\n", features->has_sha2 ? "Yes" : "No");
#endif

    printf("============================================================================\n");
}
