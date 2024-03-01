#pragma once
#include <cmath>

struct vec2
{
    double x, y;
	vec2(double x = 0, double y = 0)
	{
		this->x = x;
		this->y = y;
	}
	
	vec2 operator + (const vec2& v) const
	{
		return vec2
		(
			x + v.x,
			y + v.y
		);
	}
	
	vec2 operator - (const vec2& v) const
	{
		return vec2
		(
			x - v.x,
			y - v.y
		);
	}
	
	double operator * (const vec2& v) const
	{
		return x*v.x + y*v.y;
	}
	
	vec2 operator * (double k) const
	{
		return vec2
		(
			k*x,
			k*y
		);
	}
	vec2 operator / (double k) const
	{
		return vec2
		(
			x/k,
			y/k
		);
	}
	
	vec2& operator += (const vec2& v)
	{
		x += v.x;
		y += v.y;
	}
	
	vec2& operator -= (const vec2& v)
	{
		x -= v.x;
		y -= v.y;
	}
	
	vec2& operator *= (double k)
	{
		x *= k;
		y *= k;
	}
	
	vec2& operator /= (double k)
	{
		x /= k;
		y /= k;
	}
	
	vec2 operator - ()
	{
		x=-x;
		y=-y;
	}
	
	double magnitute2() const
	{
		return x*x + y*y;
	}
	
	double magnitute() const
	{
		return sqrt(magnitute2());
	}
	
	vec2& normalize()
	{
		double mag=magnitute();
		(*this)/=mag;
	}

	vec2 normalized()
	{
		double mag = magnitute();
		return *this / mag;
	}
};

vec2 operator * (double k, const vec2& v)
{
	return v*k;
}