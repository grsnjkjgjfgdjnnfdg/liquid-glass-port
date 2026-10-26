#pragma once

// Lightweight header-only ring-buffer debug log for the UI test.
// Push events from anywhere in the framework (header-only inline code) and
// read them back from the test harness for display in the ImGui debug menu.
//
// Usage:
//   opal::debug::SetFrame(frameIndex, time);
//   opal::debug::Log("SB-Click", "Open() branch hit  hit=%d rawJP=%d", hit, rawJP);
//   for (auto& e : opal::debug::Recent()) ImGui::Text("[%u] %s %s", e.frame, e.tag, e.msg);

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace opal::debug {

inline constexpr int kMaxEntries = 256;
inline constexpr int kMaxMsgLen  = 200;
inline constexpr int kMaxTagLen  = 24;

struct Entry {
    uint32_t frame = 0;
    float    time  = 0.0f;
    char     tag[kMaxTagLen] = {};
    char     msg[kMaxMsgLen] = {};
};

inline Entry       g_entries[kMaxEntries];
inline int          g_count      = 0;       // total entries ever pushed
inline int          g_writeIdx   = 0;       // ring write cursor
inline uint32_t     g_frame      = 0;
inline float        g_time       = 0.0f;
inline bool         g_enabled    = true;

inline void SetFrame(uint32_t frame, float time) { g_frame = frame; g_time = time; }
inline void SetEnabled(bool e) { g_enabled = e; }
inline bool IsEnabled() { return g_enabled; }

inline void Clear() { g_count = 0; g_writeIdx = 0; }

inline void Log(const char* tag, const char* fmt, ...) {
    if (!g_enabled) return;
    Entry& e = g_entries[g_writeIdx];
    e.frame = g_frame;
    e.time  = g_time;
    if (tag) {
        size_t i = 0;
        for (; i < kMaxTagLen - 1 && tag[i]; ++i) e.tag[i] = tag[i];
        e.tag[i] = 0;
    } else {
        e.tag[0] = 0;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(e.msg, kMaxMsgLen, fmt, args);
    va_end(args);
    g_writeIdx = (g_writeIdx + 1) % kMaxEntries;
    ++g_count;
}

// Most-recent-first view of the ring buffer. Returns how many entries
// are valid (capped at kMaxEntries). `out` must hold kMaxEntries.
inline int Recent(Entry* out) {
    int total = g_count < kMaxEntries ? g_count : kMaxEntries;
    for (int i = 0; i < total; ++i) {
        int idx = (g_writeIdx - 1 - i + kMaxEntries) % kMaxEntries;
        out[i] = g_entries[idx];
    }
    return total;
}

inline int Count() { return g_count < kMaxEntries ? g_count : kMaxEntries; }

} // namespace opal::debug