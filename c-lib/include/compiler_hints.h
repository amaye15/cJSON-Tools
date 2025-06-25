#ifndef COMPILER_HINTS_H
#define COMPILER_HINTS_H

// Advanced compiler hints for better optimization
#ifdef _MSC_VER
// MSVC-specific definitions
#include <intrin.h>
#include <emmintrin.h>

#define FLATTEN
#define NO_INLINE __declspec(noinline)
#define ALWAYS_INLINE __forceinline
#define PURE_FUNC
#define CONST_FUNC
#define HOT_PATH
#define COLD_PATH
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define ASSUME_ALIGNED(ptr, alignment) __assume((uintptr_t)(ptr) % (alignment) == 0)
#define PREFETCH_READ(ptr) _mm_prefetch((char*)(ptr), _MM_HINT_T0)
#define PREFETCH_WRITE(ptr) _mm_prefetch((char*)(ptr), _MM_HINT_T0)
#define RESTRICT __restrict
#define PACKED
#define ALIGNED(n) __declspec(align(n))

#elif defined(__GNUC__)
// GCC-specific definitions
#define FLATTEN __attribute__((flatten))
#define NO_INLINE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define PURE_FUNC __attribute__((pure))
#define CONST_FUNC __attribute__((const))
#define HOT_PATH __attribute__((hot))
#define COLD_PATH __attribute__((cold))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ASSUME_ALIGNED(ptr, alignment) __builtin_assume_aligned(ptr, alignment)
#define PREFETCH_READ(ptr) __builtin_prefetch(ptr, 0, 3)
#define PREFETCH_WRITE(ptr) __builtin_prefetch(ptr, 1, 3)
#define RESTRICT __restrict__
#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))

#else
// Fallback for other compilers
#define FLATTEN
#define NO_INLINE
#define ALWAYS_INLINE inline
#define PURE_FUNC
#define CONST_FUNC
#define HOT_PATH
#define COLD_PATH
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define ASSUME_ALIGNED(ptr, alignment) (ptr)
#define PREFETCH_READ(ptr)
#define PREFETCH_WRITE(ptr)
#define RESTRICT
#define PACKED
#define ALIGNED(n)
#endif

// Branch prediction hints
#define BRANCH_LIKELY(x) LIKELY(x)
#define BRANCH_UNLIKELY(x) UNLIKELY(x)

// Profile-guided optimization hints
#ifdef USE_PGO
#define PGO_HOT HOT_PATH
#define PGO_COLD COLD_PATH
#else
#define PGO_HOT
#define PGO_COLD
#endif

// Function multi-versioning for different CPU capabilities
#ifdef __GNUC__
#define CPU_GENERIC __attribute__((target("generic")))
#define CPU_SSE2 __attribute__((target("sse2")))
#define CPU_AVX2 __attribute__((target("avx2")))
#define CPU_AVX512 __attribute__((target("avx512f")))
#else
#define CPU_GENERIC
#define CPU_SSE2
#define CPU_AVX2
#define CPU_AVX512
#endif

// Platform-specific memory prefetching optimizations
#ifndef PREFETCH_L1
#if defined(__x86_64__) || defined(__i386__)
    #define PREFETCH_L1(ptr) __builtin_prefetch(ptr, 0, 3)
    #define PREFETCH_L2(ptr) __builtin_prefetch(ptr, 0, 2)
    #define PREFETCH_L3(ptr) __builtin_prefetch(ptr, 0, 1)
    #define PREFETCH_WRITE(ptr) __builtin_prefetch(ptr, 1, 3)
    #define PREFETCH_NTA(ptr) __builtin_prefetch(ptr, 0, 0)
#elif defined(__aarch64__)
    #define PREFETCH_L1(ptr) __asm__ ("prfm pldl1keep, %0" :: "Q"(*(ptr)))
    #define PREFETCH_L2(ptr) __asm__ ("prfm pldl2keep, %0" :: "Q"(*(ptr)))
    #define PREFETCH_L3(ptr) __asm__ ("prfm pldl3keep, %0" :: "Q"(*(ptr)))
    #define PREFETCH_WRITE(ptr) __asm__ ("prfm pstl1keep, %0" :: "Q"(*(ptr)))
    #define PREFETCH_NTA(ptr) __asm__ ("prfm pldl1strm, %0" :: "Q"(*(ptr)))
#else
    #define PREFETCH_L1(ptr)
    #define PREFETCH_L2(ptr)
    #define PREFETCH_L3(ptr)
    #define PREFETCH_WRITE(ptr)
    #define PREFETCH_NTA(ptr)
#endif
#endif

// Cache line size detection
#include <stddef.h>  // For size_t
ALWAYS_INLINE static size_t get_cache_line_size_hint(void) {
#if defined(__x86_64__)
    return 64; // Most x86_64 systems
#elif defined(__aarch64__)
    return 64; // Most ARM64 systems
#else
    return 64; // Safe default
#endif
}

// Memory barriers for different architectures
#ifndef MEMORY_BARRIER
#if defined(_MSC_VER)
    // MSVC intrinsics (intrin.h already included above)
    #define MEMORY_BARRIER() _ReadWriteBarrier()
    #define READ_BARRIER() _ReadBarrier()
    #define WRITE_BARRIER() _WriteBarrier()
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang inline assembly
    #if defined(__x86_64__) || defined(__i386__)
        #define MEMORY_BARRIER() __asm__ volatile("mfence" ::: "memory")
        #define READ_BARRIER() __asm__ volatile("lfence" ::: "memory")
        #define WRITE_BARRIER() __asm__ volatile("sfence" ::: "memory")
    #elif defined(__aarch64__)
        #define MEMORY_BARRIER() __asm__ volatile("dmb sy" ::: "memory")
        #define READ_BARRIER() __asm__ volatile("dmb ld" ::: "memory")
        #define WRITE_BARRIER() __asm__ volatile("dmb st" ::: "memory")
    #else
        #define MEMORY_BARRIER() __sync_synchronize()
        #define READ_BARRIER() __sync_synchronize()
        #define WRITE_BARRIER() __sync_synchronize()
    #endif
#else
    // Fallback for other compilers
    #define MEMORY_BARRIER()
    #define READ_BARRIER()
    #define WRITE_BARRIER()
#endif
#endif

// CPU relaxation for spin loops
ALWAYS_INLINE static void cpu_relax(void) {
#if defined(_MSC_VER)
    // MSVC syntax
    #if defined(_M_X64) || defined(_M_IX86)
        _mm_pause();
    #else
        // No-op for other architectures on MSVC
        (void)0;
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang syntax
    #ifdef __x86_64__
        __asm__ volatile("pause" ::: "memory");
    #elif defined(__aarch64__)
        __asm__ volatile("yield" ::: "memory");
    #elif defined(__powerpc__)
        __asm__ volatile("or 27,27,27" ::: "memory");
    #else
        __asm__ volatile("" ::: "memory");
    #endif
#else
    // Fallback for other compilers
    (void)0;
#endif
}

// Memory barriers are defined above with platform-specific implementations

// Loop unrolling hints
#ifdef __GNUC__
#define UNROLL_LOOP(n) _Pragma("GCC unroll " #n)
#define UNROLL_LOOP_FULL _Pragma("GCC unroll 65534")
#else
#define UNROLL_LOOP(n)
#define UNROLL_LOOP_FULL
#endif

// Vectorization hints
#ifdef __GNUC__
#define VECTORIZE _Pragma("GCC ivdep")
#define NO_VECTORIZE _Pragma("GCC novector")
#else
#define VECTORIZE
#define NO_VECTORIZE
#endif

// Cache line size constants
#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED ALIGNED(CACHE_LINE_SIZE)

// Force inline for critical path functions
#define FORCE_INLINE ALWAYS_INLINE

// Mark functions as leaf (no calls to other functions)
#ifdef __GNUC__
#define LEAF_FUNC __attribute__((leaf))
#else
#define LEAF_FUNC
#endif

// Optimization for switch statements
#ifdef __GNUC__
#define SWITCH_LIKELY_CASE(case_val) case case_val: __attribute__((hot))
#define SWITCH_UNLIKELY_CASE(case_val) case case_val: __attribute__((cold))
#else
#define SWITCH_LIKELY_CASE(case_val) case case_val:
#define SWITCH_UNLIKELY_CASE(case_val) case case_val:
#endif

#endif /* COMPILER_HINTS_H */
