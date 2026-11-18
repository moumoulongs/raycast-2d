#pragma once

#include <cmath>

struct vec2 {
    double x;
    double y;

    constexpr vec2(double xValue = 0.0, double yValue = 0.0) : x(xValue), y(yValue) {}

    constexpr vec2 operator+(const vec2& other) const {
        return vec2(x + other.x, y + other.y);
    }

    constexpr vec2 operator-(const vec2& other) const {
        return vec2(x - other.x, y - other.y);
    }

    constexpr double operator*(const vec2& other) const {
        return x * other.x + y * other.y;
    }

    constexpr vec2 operator*(double scalar) const {
        return vec2(x * scalar, y * scalar);
    }

    constexpr vec2 operator/(double scalar) const {
        return vec2(x / scalar, y / scalar);
    }

    vec2& operator+=(const vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    vec2& operator-=(const vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    vec2& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    vec2& operator/=(double scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    constexpr vec2 operator-() const {
        return vec2(-x, -y);
    }

    constexpr double magnitudeSquared() const {
        return x * x + y * y;
    }

    double magnitude() const {
        return std::sqrt(magnitudeSquared());
    }

    vec2& normalize() {
        const double length = magnitude();
        if (length != 0.0) {
            *this /= length;
        }
        return *this;
    }

    vec2 normalized() const {
        const double length = magnitude();
        if (length == 0.0) {
            return vec2();
        }
        return *this / length;
    }
};

inline constexpr vec2 operator*(double scalar, const vec2& value) {
    return value * scalar;
}
