#ifndef SOL_COLOR_H
#define SOL_COLOR_H

#include <algorithm>

#include <proto/utils.h>

namespace sol {

/// Color encoded using three floating-point values, using the sRGB color space.
struct RgbColor {
    float r, g, b;

    RgbColor() = default;
    explicit constexpr RgbColor(float rgb) : r(rgb), g(rgb), b(rgb) {}
    explicit constexpr RgbColor(float r, float g, float b) : r(r), g(g), b(b) {}

    RgbColor& operator += (const RgbColor& other) { return *this = *this + other; }
    RgbColor& operator -= (const RgbColor& other) { return *this = *this - other; }
    RgbColor& operator *= (const RgbColor& other) { return *this = *this * other; }
    RgbColor& operator /= (const RgbColor& other) { return *this = *this / other; }
    RgbColor& operator *= (float other) { return *this = *this * other; }
    RgbColor& operator /= (float other) { return *this = *this * other; }

    RgbColor operator + (const RgbColor& other) const {
        return RgbColor(r + other.r, g + other.g, b + other.b);
    }

    RgbColor operator - (const RgbColor& other) const {
        return RgbColor(r - other.r, g - other.g, b - other.b);
    }

    RgbColor operator * (const RgbColor& other) const {
        return RgbColor(r * other.r, g * other.g, b * other.b);
    }

    RgbColor operator / (const RgbColor& other) const {
        return RgbColor(r / other.r, g / other.g, b / other.b);
    }

    RgbColor operator * (float other) const { return RgbColor(r * other, g * other, b * other); }
    RgbColor operator / (float other) const { return *this * (1.0f / other); }

    friend RgbColor operator * (float other, const RgbColor& color) { return color * other; }
    friend RgbColor operator / (float other, const RgbColor& color) { return RgbColor(other / color.r, other / color.g, other / color.b); }

    bool operator == (const RgbColor& other) const {
        return r == other.r && g == other.g && b == other.b;
    }

    template <typename Hasher>
    Hasher& hash(Hasher& hasher) const {
        return hasher.combine(r).combine(g).combine(b);
    }

    static constexpr float default_gamma() { return 2.2f; }

    float luminance() const { return r * 0.2126f + g * 0.7152f + b * 0.0722f; }
    bool is_black() const { return r == 0.0f && g == 0.0f && b == 0.0f; }
    bool is_constant() const { return r == g && r == b; }

    static constexpr RgbColor black() { return constant(0); }
    static constexpr RgbColor constant(float c) { return RgbColor(c); }
};

inline RgbColor lerp(const RgbColor& a, const RgbColor& b, float t) {
    return RgbColor(
        proto::lerp(a.r, b.r, t),
        proto::lerp(a.g, b.g, t),
        proto::lerp(a.b, b.b, t));
}

inline RgbColor lerp(const RgbColor& a, const RgbColor& b, const RgbColor& c, float u, float v) {
    return RgbColor(
        proto::lerp(a.r, b.r, c.r, u, v),
        proto::lerp(a.g, b.g, c.g, u, v),
        proto::lerp(a.b, b.b, c.b, u, v));
}

using Color = RgbColor;

} // namespace sol

#endif
