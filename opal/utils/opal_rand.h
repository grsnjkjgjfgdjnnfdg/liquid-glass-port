#pragma once
#include <cstdint>

namespace opal::utils {

inline uint32_t Hash(uint32_t state) {
    state ^= 2747636419u;
    state *= 2654435769u;
    state ^= state >> 16;
    state *= 2654435769u;
    state ^= state >> 16;
    state *= 2654435769u;
    return state;
}

inline uint32_t Hash(uint32_t a, uint32_t b) {
    uint32_t h = (a * 0x9E3779B9u) ^ (b * 0x85EBCA77u);
    return Hash(h);
}

inline float HashFloat01(uint32_t seed) {
    return (float)(Hash(seed) & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
}

struct RNG {
    uint32_t state;
    RNG(uint32_t seed = 12345) : state(seed) {}
    uint32_t Next() { state = Hash(state); return state; }
    float    NextFloat() { return (float)(Next() & 0x7FFFFFFFu) / (float)0x7FFFFFFFu; }
    float    NextFloat(float lo, float hi) { return lo + NextFloat() * (hi - lo); }
};

} // namespace opal::utils
