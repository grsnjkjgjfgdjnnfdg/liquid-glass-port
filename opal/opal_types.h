#pragma once
#include <cstdint>

namespace opal {

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s) const       { return { x * s, y * s }; }
    Vec2 operator/(float s) const       { return { x / s, y / s }; }
    Vec2& operator+=(const Vec2& o)     { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o)     { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s)           { x *= s; y *= s; return *this; }
    bool operator==(const Vec2& o) const{ return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const{ return !(*this == o); }
};

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    Rect() = default;
    Rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
    Rect(const Vec2& pos, const Vec2& size) : x(pos.x), y(pos.y), w(size.x), h(size.y) {}

    float  Right()   const { return x + w; }
    float  Bottom()  const { return y + h; }
    Vec2   Center()  const { return { x + w * 0.5f, y + h * 0.5f }; }
    Vec2   TopLeft() const { return { x, y }; }
    Vec2   Size()    const { return { w, h }; }

    bool   Contains(const Vec2& p) const { return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h; }
    bool   IsEmpty() const { return w <= 0.0f || h <= 0.0f; }
};

struct Color {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    Color() = default;
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    static Color FromBytes(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
        return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    }

    unsigned int ToU32() const {
        auto c = [](float v) -> unsigned char { return (unsigned char)(v * 255.0f + 0.5f); };
        return (c(a) << 24) | (c(b) << 16) | (c(g) << 8) | c(r);
    }

    Color operator*(float s) const { return { r * s, g * s, b * s, a * s }; }
    Color WithAlpha(float na) const { return { r, g, b, na }; }
};

struct EdgeInsets {
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
    EdgeInsets() = default;
    EdgeInsets(float all) : left(all), top(all), right(all), bottom(all) {}
    EdgeInsets(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
    float Horizontal() const { return left + right; }
    float Vertical()   const { return top + bottom; }
};

template<typename T>
T Clamp(T v, T lo, T hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

template<typename T>
T Lerp(T a, T b, float t) {
    return a + (b - a) * t;
}

inline float Smoothstep(float edge0, float edge1, float x) {
    float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace opal
