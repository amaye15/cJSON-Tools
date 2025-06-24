#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Zero-copy string view for avoiding allocations
typedef struct {
    const char* data;
    size_t length;
    uint32_t hash;  // Cached hash value
} StringView;

// Create string view from C string
static inline StringView make_string_view(const char* str, size_t len) {
    StringView sv = {str, len, 0};
    return sv;
}

// Create string view from null-terminated string
static inline StringView make_string_view_cstr(const char* str) {
    return make_string_view(str, strlen(str));
}

// Fast hash function using FNV-1a algorithm
static inline uint32_t string_view_hash(const StringView* sv) {
    if (sv->hash == 0 && sv->length > 0) {
        uint32_t hash = 2166136261u;  // FNV offset basis
        for (size_t i = 0; i < sv->length; i++) {
            hash ^= (uint8_t)sv->data[i];
            hash *= 16777619u;  // FNV prime
        }
        ((StringView*)sv)->hash = hash ? hash : 1;  // Avoid 0
    }
    return sv->hash;
}

// Fast equality comparison with hash optimization
static inline int string_view_equals(const StringView* a, const StringView* b) {
    if (a->length != b->length) return 0;
    if (a->data == b->data) return 1;  // Same pointer
    
    // Compare hashes first (fast rejection)
    if (string_view_hash(a) != string_view_hash(b)) return 0;
    
    return memcmp(a->data, b->data, a->length) == 0;
}

// Compare with C string
static inline int string_view_equals_cstr(const StringView* sv, const char* str) {
    size_t str_len = strlen(str);
    if (sv->length != str_len) return 0;
    return memcmp(sv->data, str, str_len) == 0;
}

// Check if string view starts with prefix
static inline int string_view_starts_with(const StringView* sv, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    if (sv->length < prefix_len) return 0;
    return memcmp(sv->data, prefix, prefix_len) == 0;
}

// Check if string view ends with suffix
static inline int string_view_ends_with(const StringView* sv, const char* suffix) {
    size_t suffix_len = strlen(suffix);
    if (sv->length < suffix_len) return 0;
    return memcmp(sv->data + sv->length - suffix_len, suffix, suffix_len) == 0;
}

// Find character in string view
static inline const char* string_view_find_char(const StringView* sv, char c) {
    return (const char*)memchr(sv->data, c, sv->length);
}

// Create substring view
static inline StringView string_view_substr(const StringView* sv, size_t start, size_t len) {
    if (start >= sv->length) {
        return make_string_view("", 0);
    }
    
    size_t actual_len = (start + len > sv->length) ? sv->length - start : len;
    return make_string_view(sv->data + start, actual_len);
}

// Convert to null-terminated string (allocates memory)
static inline char* string_view_to_cstr(const StringView* sv) {
    char* str = malloc(sv->length + 1);
    if (str) {
        memcpy(str, sv->data, sv->length);
        str[sv->length] = '\0';
    }
    return str;
}

#endif /* STRING_VIEW_H */
