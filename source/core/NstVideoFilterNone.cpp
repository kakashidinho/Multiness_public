////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2016-2018 Le Hoang Quyen
// Copyright (C) 2003-2008 Martin Freij
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

#include "NstCore.hpp"
#include "NstVideoRenderer.hpp"
#include "NstVideoFilterNone.hpp"
#include "NstVideoFilterCommon.hpp"

namespace Nes
{
	namespace Core
	{
		namespace Video
		{
			//LHQ TODO: only filternone works with downsampled input for now
			template<typename T>
			void Renderer::FilterNone::BlitAligned(const Input& input,const Output& output)
			{
				T* NST_RESTRICT dst = static_cast<T*>(output.pixels);

				//LHQ
				auto downsampled = input.pixels[Screen::PIXELS];

				if (downsampled == 1)
				{
					for (size_t y = 0; y < Video::Screen::HEIGHT; y ++) {
						for (size_t x = 0; x < Video::Screen::WIDTH; x ++) {
							*dst++ = GetDownsampledPixelColor(input, x, y);
						}
					}
				}//(downsampled)
				else if (downsampled == 2) {
					for (size_t y = 0; y < Video::Screen::HEIGHT; y++) {
						for (size_t x = 0; x < Video::Screen::WIDTH; x++) {
							*dst++ = GetDownsampledPixelColor2(input, x, y);
						}
					}
				}
				else if (downsampled == 4) {
					size_t startX = 0;
					size_t startY = 0;
					size_t endY = Video::Screen::HEIGHT;

					size_t stepsY = 2;

					for (size_t y = startY; y < endY; y += stepsY) {

						auto pdst = (T*)((byte*)dst + y * (sizeof(T) * Video::Screen::WIDTH));

						for (size_t x = startX; x < Video::Screen::WIDTH; x += 1) {
							size_t i = y * Video::Screen::WIDTH + x;
							*pdst++ = GetPixelColor(input, i);
						}
					}
				}
				else if (downsampled > 4) {
					size_t startX, startY, endY, steps;

					//frame was splitted into several parts, and only one part was sent. we need to figure out which part
					auto phase = downsampled - 5;

					startX = 0;
					startY = phase * (Video::Screen::HEIGHT / 4);
					endY = startY + (Video::Screen::HEIGHT / 4);

					steps = 1;

					dst = (T*)((byte*)dst + startY * (sizeof(T) * Video::Screen::WIDTH));

					for (size_t y = startY; y < endY; y += steps) {
						for (size_t x = startX; x < Video::Screen::WIDTH; x += steps) {
							size_t i = y * Video::Screen::WIDTH + x;
							*dst++ = GetPixelColor(input, i);
						}
					}
				}
				else {
					const Input::Pixel* NST_RESTRICT src = input.pixels;

					for (uint prefetched = *src++, i = PIXELS; i; --i)
					{
						const dword reg = input.palette[prefetched];
						prefetched = *src++;
						*dst++ = reg;
					}
				}//(downsampled)
			}

			template<typename T>
			void Renderer::FilterNone::BlitUnaligned(const Input& input,const Output& output)
			{
				T* NST_RESTRICT dst = static_cast<T*>(output.pixels);

				const long pad = output.pitch - WIDTH * sizeof(T);
				//LHQ
				auto downsampled = input.pixels[Screen::PIXELS];

				if (downsampled == 1)
				{
					for (size_t y = 0; y < Video::Screen::HEIGHT; y++) {
						for (size_t x = 0; x < Video::Screen::WIDTH; x++) {
							*dst++ = GetDownsampledPixelColor(input, x, y);
						}

						dst = reinterpret_cast<T*>(reinterpret_cast<byte*>(dst) + pad);
					}
				}//(downsampled)
				if (downsampled == 2)
				{
					for (size_t y = 0; y < Video::Screen::HEIGHT; y++) {
						for (size_t x = 0; x < Video::Screen::WIDTH; x++) {
							*dst++ = GetDownsampledPixelColor2(input, x, y);
						}

						dst = reinterpret_cast<T*>(reinterpret_cast<byte*>(dst) + pad);
					}
				}//(downsampled)
				else if (downsampled == 4) {
					size_t startX = 0;
					size_t startY = 0;
					size_t endY = Video::Screen::HEIGHT;
					size_t stepsY = 2;

					for (size_t y = startY; y < endY; y += stepsY) {

						auto pdst = (T*)((byte*)dst + y * (sizeof(T) * Video::Screen::WIDTH + pad));

						for (size_t x = startX; x < Video::Screen::WIDTH; x += 1) {
							size_t i = y * Video::Screen::WIDTH + x;
							*pdst++ = GetPixelColor(input, i);
						}
					}
				}
				else if (downsampled > 4) {
					size_t startX, startY, endY, steps;

					//frame was splitted into several parts, and only one part was sent. we need to figure out which part
					auto phase = downsampled - 5;

					startX = 0;
					startY = phase * (Video::Screen::HEIGHT / 4);
					endY = startY + (Video::Screen::HEIGHT / 4);

					steps = 1;

					dst = (T*)((byte*)dst + startY * (sizeof(T) * Video::Screen::WIDTH + pad));

					for (size_t y = startY; y < endY; y += steps) {
						for (size_t x = startX; x < Video::Screen::WIDTH; x += steps) {
							size_t i = y * Video::Screen::WIDTH + x;
							*dst++ = GetPixelColor(input, i);
						}

						dst = reinterpret_cast<T*>(reinterpret_cast<byte*>(dst) + pad);
					}
				}
				else {
					const Input::Pixel* NST_RESTRICT src = input.pixels;

					for (uint prefetched = *src++, y = HEIGHT; y; --y)
					{
						for (uint x = WIDTH; x; --x)
						{
							const dword reg = input.palette[prefetched];
							prefetched = *src++;
							*dst++ = reg;
						}

						dst = reinterpret_cast<T*>(reinterpret_cast<byte*>(dst) + pad);
					}
				}//(downsampled)
			}

			void Renderer::FilterNone::Blit(const Input& input,const Output& output,uint)
			{
				if (format.bpp == 32)
				{
					if (output.pitch == WIDTH * sizeof(dword))
						BlitAligned<dword>( input, output );
					else
						BlitUnaligned<dword>( input, output );
				}
				else
				{
					if (output.pitch == WIDTH * sizeof(word))
						BlitAligned<word>( input, output );
					else
						BlitUnaligned<word>( input, output );
				}
			}

			#ifdef NST_MSVC_OPTIMIZE
			#pragma optimize("s", on)
			#endif

			Renderer::FilterNone::FilterNone(const RenderState& state)
			: Filter(state)
			{
				NST_COMPILE_ASSERT( Video::Screen::PIXELS_PADDING >= 1 );
			}

			bool Renderer::FilterNone::Check(const RenderState& state)
			{
				return
				(
					(state.bits.count == 16 || state.bits.count == 32) &&
					(state.width == WIDTH && state.height == HEIGHT)
				);
			}

			#ifdef NST_MSVC_OPTIMIZE
			#pragma optimize("", on)
			#endif
		}
	}
}
