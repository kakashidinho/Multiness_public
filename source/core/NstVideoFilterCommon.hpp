////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Nestopia.
//
// Nestopia is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Nestopia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Nestopia; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef NST_VIDEO_FILTER_COMMON_H
#define NST_VIDEO_FILTER_COMMON_H

#include "NstVideoScreen.hpp"

#define NO_DOWNSAMPLE_INTERPOLATION 0

namespace Nes
{
	namespace Core
	{
		namespace Video {
			static inline dword GetPixelColor(const Screen& input, size_t idx) {
				Screen::Pixel cidx = input.pixels[idx];
				return input.palette[cidx];
			}

			static inline void GetColorRGBA(dword color, dword &R, dword &G, dword &B, dword &A) {
				R = color & 0xff;
				G = (color >> 8) & 0xff;
				B = (color >> 16) & 0xff;
				A = (color >> 24);
			}

			static inline dword MixColor(dword color1, dword color2) {
				dword r1, g1, b1, a1;
				dword r2, g2, b2, a2;

				GetColorRGBA(color1, r1, g1, b1, a1);
				GetColorRGBA(color2, r2, g2, b2, a2);

				auto r = ((r1 + r2) >> 1) & 0xff;
				auto g = ((g1 + g2) >> 1) & 0xff;
				auto b = ((b1 + b2) >> 1) & 0xff;
				auto a = ((a1 + a2) >> 1) & 0xff;

				return r | (g << 8) | (b << 16) | (a << 24);
			}

			static inline dword MixColor(dword color1, dword color2, dword color3, dword color4) {
				dword r1, g1, b1, a1;
				dword r2, g2, b2, a2;
				dword r3, g3, b3, a3;
				dword r4, g4, b4, a4;

				GetColorRGBA(color1, r1, g1, b1, a1);
				GetColorRGBA(color2, r2, g2, b2, a2);
				GetColorRGBA(color3, r3, g3, b3, a3);
				GetColorRGBA(color4, r4, g4, b4, a4);

				auto r = ((r1 + r2 + r3 + r4) >> 2) & 0xff;
				auto g = ((g1 + g2 + g3 + g4) >> 2) & 0xff;
				auto b = ((b1 + b2 + b3 + b4) >> 2) & 0xff;
				auto a = ((a1 + a2 + a3 + a4) >> 2) & 0xff;

				return r | (g << 8) | (b << 16) | (a << 24);
			}

			static inline dword GetDownsampledPixelColor(const Screen& input, size_t x, size_t y)
			{
				size_t i = y * Screen::WIDTH + x;

				auto xEven = x % 2 == 0;
				auto yEven = y % 2 == 0;

#if NO_DOWNSAMPLE_INTERPOLATION
				if (xEven && yEven) {
					return GetPixelColor(input, i);
				}
				else {
					size_t closest_x, closest_y;
					if (xEven)
						closest_x = x;
					else
						closest_x = x - 1;

					if (yEven)
						closest_y = y;
					else
						closest_y = y - 1;

					size_t closest_i = closest_y* Screen::WIDTH + closest_x;
					return GetPixelColor(input, closest_i);
				}
#else//if NO_DOWNSAMPLE_INTERPOLATION
				if (xEven)
				{
					if (yEven)
					{
						return GetPixelColor(input, i);
					}
					else
					{
						size_t top_i = (y - 1)* Screen::WIDTH + x;

						if (y == Screen::HEIGHT - 1)//edge pixel
						{
							//use closest pixel vertically
							return GetPixelColor(input, top_i);
						}
						else {
							//interpolate between top and bottom
							size_t bottom_i = (y + 1)* Screen::WIDTH + x;

							return MixColor(GetPixelColor(input, top_i), GetPixelColor(input, bottom_i));
						}
					}
				}//if (xEven)
				else if (yEven)
				{
					size_t left_i = y * Screen::WIDTH + x - 1;

					if (x == Screen::WIDTH - 1)//edge pixel
					{
						//use closest pixel horizontally
						return GetPixelColor(input, left_i);
					}
					else {
						//interpolate between left and right
						size_t right_i = y * Screen::WIDTH + x + 1;

						return MixColor(GetPixelColor(input, left_i), GetPixelColor(input, right_i));
					}
				}
				else {
					size_t top_left = (y - 1)* Screen::WIDTH + x - 1;

					auto tl = GetPixelColor(input, top_left);

					if (x == Screen::WIDTH - 1)//edge pixel
					{
						if (y == Screen::HEIGHT - 1)//corner pixel
						{
							return tl;
						}
						else {
							size_t bottom_left = (y + 1)* Screen::WIDTH + x - 1;
							auto bl = GetPixelColor(input, bottom_left);

							return MixColor(tl, bl);
						}
					}
					else {
						size_t top_right = (y - 1) * Screen::WIDTH + x + 1;
						auto tr = GetPixelColor(input, top_right);

						if (y == Screen::HEIGHT - 1) //edge pixel 
						{
							return MixColor(tl, tr);
						}
						else {
							size_t bottom_left = (y + 1)* Screen::WIDTH + x - 1;
							size_t bottom_right = (y + 1)* Screen::WIDTH + x + 1;

							auto bl = GetPixelColor(input, bottom_left);
							auto br = GetPixelColor(input, bottom_right);

							//interpolate between top left, to right, bottom left & bottom right
							return MixColor(tl, tr, bl, br);
						}
					}
				}
#endif//if NO_DOWNSAMPLE_INTERPOLATION
			}

			static inline dword GetDownsampledPixelColor2(const Screen& input, size_t x, size_t y)
			{
				size_t i = y * Screen::WIDTH + x;

				auto yEven = y % 2 == 0;
				if (yEven) {
					return GetPixelColor(input, i);
				}
				else {
#if NO_DOWNSAMPLE_INTERPOLATION
					size_t closest_y = y - 1;

					size_t closest_i = closest_y* Screen::WIDTH + x;
					return GetPixelColor(input, closest_i);
#else//if NO_DOWNSAMPLE_INTERPOLATION

					//interpolate between top and bottom
					size_t top = (y - 1) * Screen::WIDTH + x;
					if (y == Screen::HEIGHT - 1)//edge
						return GetPixelColor(input, top);
					
					size_t bottom = (y + 1) * Screen::WIDTH + x;
					return MixColor(GetPixelColor(input, top), GetPixelColor(input, bottom));
#endif//if NO_DOWNSAMPLE_INTERPOLATION
				}
			}
		}
	}
}

#endif
