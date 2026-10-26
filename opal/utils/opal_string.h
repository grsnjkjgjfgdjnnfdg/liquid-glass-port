#pragma once

namespace opal::utils {

constexpr size_t ConstStrLen(const char* s) {
    size_t len = 0;
    while (s[len]) ++len;
    return len;
}

inline int CompareNoCase(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        ++a; ++b;
    }
    return *a - *b;
}

inline bool StartsWith(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return false;
        ++str; ++prefix;
    }
    return true;
}

} // namespace opal::utils
