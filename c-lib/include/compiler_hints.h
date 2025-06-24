#ifndef COMPILER_HINTS_H
#define COMPILER_HINTS_H

// Advanced compiler hints for better optimization
#ifdef __GNUC__
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

// CPU relaxation for spin loops
ALWAYS_INLINE static void cpu_relax(void) {
#ifdef __x86_64__
    __asm__ volatile("pause" ::: "memory");
#elif defined(__aarch64__)
    __asm__ volatile("yield" ::: "memory");
#elif defined(__powerpc__)
    __asm__ volatile("or 27,27,27" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

// Memory barriers
#define MEMORY_BARRIER() __asm__ volatile("" ::: "memory")
#define READ_BARRIER() __asm__ volatile("" ::: "memory")
#define WRITE_BARRIER() __asm__ volatile("" ::: "memory")

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
