#include "../include/common.h"
#include "../include/string_view.h"
#include "../include/simd_utils.h"
#include "../include/cpu_features.h"

// Disable SIMD on Windows builds for initial PyPI release
#ifdef THREADING_DISABLED
#undef __SSE2__
#undef __AVX2__
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __aarch64__
#include <arm_neon.h>
#endif

// Runtime CPU feature detection
static int has_avx2 = -1;
static int has_sse2 = -1;
static int has_neon __attribute__((unused)) = -1;

static void __attribute__((unused)) detect_cpu_features(void) {
    if (has_avx2 == -1) {
#ifdef __x86_64__
        #ifdef __GNUC__
        // Use GCC builtin CPU support detection (more portable)
        has_avx2 = __builtin_cpu_supports("avx2") ? 1 : 0;
        has_sse2 = __builtin_cpu_supports("sse2") ? 1 : 0;
        #else
        // Fallback for non-GCC compilers
        has_avx2 = 0;
        has_sse2 = 0;
        #endif
#elif defined(__aarch64__)
        // ARM NEON is standard on AArch64
        has_neon = 1;
        has_avx2 = 0;
        has_sse2 = 0;
#else
        has_avx2 = 0;
        has_sse2 = 0;
        has_neon = 0;
#endif
    }
}

// Vectorized whitespace skipping
HOT_PATH static const char* skip_whitespace_simd(const char* str, size_t len) {
#ifdef __AVX2__
    const __m256i spaces = _mm256_set1_epi8(' ');
    const __m256i tabs = _mm256_set1_epi8('\t');
    const __m256i newlines = _mm256_set1_epi8('\n');
    const __m256i returns = _mm256_set1_epi8('\r');
    
    size_t i = 0;
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)(str + i));
        
        // Check for whitespace characters
        __m256i is_space = _mm256_cmpeq_epi8(chunk, spaces);
        __m256i is_tab = _mm256_cmpeq_epi8(chunk, tabs);
        __m256i is_newline = _mm256_cmpeq_epi8(chunk, newlines);
        __m256i is_return = _mm256_cmpeq_epi8(chunk, returns);
        
        __m256i is_whitespace = _mm256_or_si256(
            _mm256_or_si256(is_space, is_tab),
            _mm256_or_si256(is_newline, is_return)
        );
        
        // Find first non-whitespace character
        uint32_t mask = ~_mm256_movemask_epi8(is_whitespace);
        if (mask != 0) {
            return str + i + __builtin_ctz(mask);
        }
    }
    
    // Handle remaining characters
    for (; i < len; i++) {
        char c = str[i];
        if (LIKELY(c != ' ' && c != '\t' && c != '\n' && c != '\r')) {
            return str + i;
        }
    }
#elif defined(__SSE2__)
    const __m128i spaces = _mm_set1_epi8(' ');
    const __m128i tabs = _mm_set1_epi8('\t');
    const __m128i newlines = _mm_set1_epi8('\n');
    const __m128i returns = _mm_set1_epi8('\r');
    
    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
        __m128i chunk = _mm_loadu_si128((__m128i*)(str + i));
        
        __m128i is_space = _mm_cmpeq_epi8(chunk, spaces);
        __m128i is_tab = _mm_cmpeq_epi8(chunk, tabs);
        __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
        __m128i is_return = _mm_cmpeq_epi8(chunk, returns);
        
        __m128i is_whitespace = _mm_or_si128(
            _mm_or_si128(is_space, is_tab),
            _mm_or_si128(is_newline, is_return)
        );
        
        uint16_t mask = ~_mm_movemask_epi8(is_whitespace);
        if (mask != 0) {
            return str + i + __builtin_ctz(mask);
        }
    }
    
    // Handle remaining characters
    for (; i < len; i++) {
        char c = str[i];
        if (LIKELY(c != ' ' && c != '\t' && c != '\n' && c != '\r')) {
            return str + i;
        }
    }
#else
    // Fallback scalar implementation
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (LIKELY(c != ' ' && c != '\t' && c != '\n' && c != '\r')) {
            return str + i;
        }
    }
#endif
    return str + len;
}

// Vectorized string scanning for JSON delimiters
HOT_PATH static const char* find_json_delimiter_simd(const char* str, size_t len) {
#ifdef __AVX2__
    const __m256i quote = _mm256_set1_epi8('"');
    const __m256i comma = _mm256_set1_epi8(',');
    const __m256i colon = _mm256_set1_epi8(':');
    const __m256i brace_open = _mm256_set1_epi8('{');
    const __m256i brace_close = _mm256_set1_epi8('}');
    const __m256i bracket_open = _mm256_set1_epi8('[');
    const __m256i bracket_close = _mm256_set1_epi8(']');
    
    size_t i = 0;
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)(str + i));
        
        __m256i is_quote = _mm256_cmpeq_epi8(chunk, quote);
        __m256i is_comma = _mm256_cmpeq_epi8(chunk, comma);
        __m256i is_colon = _mm256_cmpeq_epi8(chunk, colon);
        __m256i is_brace_open = _mm256_cmpeq_epi8(chunk, brace_open);
        __m256i is_brace_close = _mm256_cmpeq_epi8(chunk, brace_close);
        __m256i is_bracket_open = _mm256_cmpeq_epi8(chunk, bracket_open);
        __m256i is_bracket_close = _mm256_cmpeq_epi8(chunk, bracket_close);
        
        __m256i is_delimiter = _mm256_or_si256(
            _mm256_or_si256(is_quote, is_comma),
            _mm256_or_si256(
                _mm256_or_si256(is_colon, is_brace_open),
                _mm256_or_si256(
                    _mm256_or_si256(is_brace_close, is_bracket_open),
                    is_bracket_close
                )
            )
        );
        
        uint32_t mask = _mm256_movemask_epi8(is_delimiter);
        if (mask != 0) {
            return str + i + __builtin_ctz(mask);
        }
    }
    
    // Handle remaining characters
    for (; i < len; i++) {
        char c = str[i];
        if (c == '"' || c == ',' || c == ':' || c == '{' || c == '}' || c == '[' || c == ']') {
            return str + i;
        }
    }
#else
    // Fallback scalar implementation
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '"' || c == ',' || c == ':' || c == '{' || c == '}' || c == '[' || c == ']') {
            return str + i;
        }
    }
#endif
    return str + len;
}

// Fast number validation using SIMD
HOT_PATH static int is_valid_number_simd(const char* str, size_t len) {
#ifdef __AVX2__
    const __m256i digit_0 = _mm256_set1_epi8('0');
    const __m256i digit_9 = _mm256_set1_epi8('9');
    const __m256i plus = _mm256_set1_epi8('+');
    const __m256i minus = _mm256_set1_epi8('-');
    const __m256i dot = _mm256_set1_epi8('.');
    const __m256i e_lower = _mm256_set1_epi8('e');
    const __m256i e_upper = _mm256_set1_epi8('E');
    
    size_t i = 0;
    for (; i + 32 <= len; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)(str + i));
        
        __m256i is_digit = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(digit_0, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(digit_9, _mm256_set1_epi8(1)), chunk)
        );
        
        __m256i is_plus = _mm256_cmpeq_epi8(chunk, plus);
        __m256i is_minus = _mm256_cmpeq_epi8(chunk, minus);
        __m256i is_dot = _mm256_cmpeq_epi8(chunk, dot);
        __m256i is_e_lower = _mm256_cmpeq_epi8(chunk, e_lower);
        __m256i is_e_upper = _mm256_cmpeq_epi8(chunk, e_upper);
        
        __m256i is_valid = _mm256_or_si256(
            is_digit,
            _mm256_or_si256(
                _mm256_or_si256(is_plus, is_minus),
                _mm256_or_si256(is_dot, _mm256_or_si256(is_e_lower, is_e_upper))
            )
        );
        
        uint32_t mask = _mm256_movemask_epi8(is_valid);
        if (mask != 0xFFFFFFFF) {
            return 0; // Invalid character found
        }
    }
    
    // Handle remaining characters
    for (; i < len; i++) {
        char c = str[i];
        if (!((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E')) {
            return 0;
        }
    }
#else
    // Fallback scalar implementation
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (!((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E')) {
            return 0;
        }
    }
#endif
    return 1;
}

// ARM NEON implementation
#ifdef __aarch64__
static size_t __attribute__((unused)) strlen_simd_neon(const char* str) {
    // SAFETY: For now, fall back to standard strlen to avoid buffer overflows
    // SIMD strlen is complex to implement safely without knowing buffer boundaries
    // TODO: Implement safe SIMD strlen with proper bounds checking
    return strlen(str);
}
#endif

// Optimized string length calculation with SIMD
HOT_PATH size_t strlen_simd(const char* str) {
    if (!str) return 0;

    // SAFETY: For now, fall back to standard strlen to avoid buffer overflows
    // SIMD strlen implementations are complex to make safe without knowing buffer boundaries
    // TODO: Implement safe SIMD strlen with proper bounds checking or use a different approach
    return strlen(str);
}

// Export optimized functions for use in other modules
const char* skip_whitespace_optimized(const char* str, size_t len) {
    return skip_whitespace_simd(str, len);
}

const char* find_delimiter_optimized(const char* str, size_t len) {
    return find_json_delimiter_simd(str, len);
}

int validate_number_optimized(const char* str, size_t len) {
    return is_valid_number_simd(str, len);
}
