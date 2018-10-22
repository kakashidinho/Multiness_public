////////////////////////////////////////////////////////////////////////////////////////
//
// Multiness - NES/Famicom emulator written in C++
// Based on Nestopia emulator
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Multiness.
//
// Multiness is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Multiness is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Multiness; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef COMMON_HPP
#define COMMON_HPP

#include <memory>
#include <functional>
#include <stdarg.h>
#include <math.h>
#include <assert.h>

#include <HQDataStream.h>

#include "../core/api/NstApiMachine.hpp"

namespace  Nes {
	namespace Callback {
		typedef std::function<void (const char* message)> ErrorCallback;
		typedef std::function<void (const char* format, va_list args)> VLogCallback;
		typedef std::function<void (Api::Machine::Event event, Result result)> MachineEventCallback;
		
		typedef std::function<HQDataReaderStream* (const char* file)> OpenFileCallback;
	}
	
	namespace Maths {
		static const float _PI = 3.14159265358979323846f;
		
		struct Vec2 {
			static const Vec2 XAXIS;
			static const Vec2 YAXIS;
			
			Vec2(float _x, float _y)
			:x(_x), y(_y)
			{}
			
			float x, y;
			
			//cross product's magnitude
			float crossLength(const Vec2 &rhs) const {
				return x * rhs.y - y * rhs.x;
			}
			
			//dot product
			float operator * (const Vec2& rhs) const {
				return x * rhs.x + y * rhs.y;
			}
			
			Vec2 operator * (float f) const {
				return Vec2(x * f, y * f);
			}
			
			Vec2 operator - (const Vec2& rhs) const {
				return Vec2(x - rhs.x, y - rhs.y);
			}
			
			Vec2& operator *= (float f) {
				x *= f;
				y *= f;
				return *this;
			}
			
			//squared length
			float lengthSq() const {
				return ((*this) * (*this));
			}
			
			float length() const {
				return sqrtf(lengthSq());
			}
			
			Vec2& normalize() {
				return (*this) *= (1.0f / length());
			}
			
			Vec2 normalizedVec() const {
				return (*this) * (1.0f / length());
			}
			
			//Note: this function only works on 2 normalized vectors.
			//the angle is in range [-pi, pi]
			float angle(const Vec2& rhs) const {
				assert(fabs(length() - 1.f) < 0.00001f);
				assert(fabs(rhs.length() - 1.f) < 0.00001f);
				
				auto _cos = (*this) * rhs;
				auto _sin = crossLength(rhs);
				
				auto angle = acosf(_cos);
				if (_sin < 0)//angle is in lower half of the circle
				{
					angle = -angle;
				}
				
				return angle;
			}
			
			float distanceSq(const Vec2 &point2) const {
				return ((*this) - point2).lengthSq();
			}
			
			float distance(const Vec2 &point2) const {
				return ((*this) - point2).length();
			}
		};
		
		struct Rect {
			float x, y;
			float width, height;
			
			bool contains(float x, float y) const {
				return x >= this->x && x <= this->x + this->width
					   && y >= this->y && y <= this->y + this->height;
			}
			
			bool contains(const Vec2& point) const {
				return contains(point.x, point.y);
			}
			
			float getMidX() const { return x + width * 0.5f; }
			float getMidY() const { return y + height * 0.5f; }
			
			float distanceFromCenter(const Vec2& point) const {
				return (Vec2(getMidX(), getMidY())).distance(point);
			}
			
			float distanceSqFromCenter(const Vec2& point) const {
				return (Vec2(getMidX(), getMidY())).distanceSq(point);
			}
			
			Vec2 distanceVecFromCenter(const Vec2& point) const {
				return point - (Vec2(getMidX(), getMidY()));
			}
		};
		
		struct Range {
			float min, max;
			
			bool contains(float f) const {
				return f >= min && f <= max;
			}
		};
	}
}

#endif