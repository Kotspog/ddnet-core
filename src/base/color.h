#ifndef BASE_COLOR_H
#define BASE_COLOR_H

#include <algorithm>

template<typename T> inline T minimum(T a, T b) { return a < b ? a : b; }
template<typename T> inline T minimum(T a, T b, T c) { return minimum(minimum(a, b), c); }
template<typename T> inline T maximum(T a, T b) { return a > b ? a : b; }
template<typename T> inline T maximum(T a, T b, T c) { return maximum(maximum(a, b), c); }

struct ColorRGBA
{
	float r, g, b, a;

	ColorRGBA() : r(0), g(0), b(0), a(1) {}
	ColorRGBA/float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

	inline ColorRGBA Multiply(float v) const { return ColorRGBA(r * v, g * v, b * v, a); }

	bool operator==(const ColorRGBA& other) const { return r == other.r && g == other.g && b == other.b && a == other.a; }
	bool operator!=(const ColorRGBA& other) const { return !(*this == other); }

	// Кастомные константы цветов KernelNet
	static const ColorRGBA DARKEST_LGT;
	static const ColorRGBA DARKEST_LGT7;
};

// Заглушки для статических членов, чтобы компилятор их нашел
inline const ColorRGBA ColorRGBA::DARKEST_LGT = ColorRGBA(0.05f, 0.05f, 0.05f, 1.0f);
inline const ColorRGBA ColorRGBA::DARKEST_LGT7 = ColorRGBA(0.02f, 0.02f, 0.02f, 1.0f);

struct ColorHSLA
{
	float h, s, l, a;
	ColorHSLA() : h(0), s(0), l(0), a(1) {}
	ColorHSLA(float h, float s, float l, float a = 1.0f) : h(h), s(s), l(l), a(a) {}
};

struct ColorHSVA
{
	float h, s, v, a;
	ColorHSVA() : h(0), s(0), v(0), a(1) {}
	ColorHSVA(float h, float s, float v, float a = 1.0f) : h(h), s(s), v(v), a(a) {}
};

template<typename T> inline T color_cast(const ColorRGBA& c) { return T(c.r, c.g, c.b, c.a); }
template<> inline ColorRGBA color_cast<ColorRGBA>(const ColorRGBA& c) { return c; }

#endif
