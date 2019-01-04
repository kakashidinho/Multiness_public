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

#ifndef Renderer_hpp
#define Renderer_hpp

#include "../core/api/NstApiEmulator.hpp"
#include "../core/api/NstApiVideo.hpp"

#include "Common.hpp"
#include "Texture.hpp"

namespace Nes {
	namespace Video {
		struct Color {
			float r, g, b, a;

			static const Color WHITE;
		};

		//base interface to draw a rect on the screen using specified texture
		class IRectRenderer {
		public:
			virtual ~IRectRenderer() {}

			inline void DrawRect(ITexture& texture, const Maths::Rect& rect) { DrawRect(texture, rect, Color::WHITE); }
			void DrawOutlineRect(const Color& color, float size, const Maths::Rect& rect)
			{ DrawOutlineRect(color, size, rect.x, rect.y , rect.width, rect.height); }

			inline void DrawRect(ITexture& texture, float x, float y, float width, float height) {
				Maths::Rect rect = {.x = x, .y = y, .width = width, .height = height};
				DrawRect(texture, rect, Color::WHITE);
			}
			virtual void DrawRect(ITexture& texture, const Maths::Rect& rect, const Color& color) = 0;
			virtual void DrawOutlineRect(const Color& color, float size, float x, float y, float width, float height) = 0;
		};

		//interface to handle all the rendering stuffs
		class IRenderer: public IRectRenderer
		{
		public:
			IRenderer();
			virtual ~IRenderer();

			virtual void Invalidate() = 0;//this should be called when GL/DX context lost
			virtual void Cleanup() = 0;//cleanup graphics resources
			void Reset(float screenWidth, float screenHeight);

			virtual bool SetFilterShader(const char* vshader, const char* fshader, float scaleX, float scaleY, bool videoLinearSampling) = 0;

			float GetScreenWidth() const { return m_screenWidth; }
			float GetScreenHeight() const { return m_screenHeight; }
			virtual void EnableFullScreenVideo(bool e) = 0;
			virtual unsigned int GetVideoMinX() const = 0;
			virtual unsigned int GetVideoMinY() const = 0;

			virtual void PresentVideo() = 0;//draw NES's video screen
		protected:
			virtual void ResetImpl() = 0;

			virtual bool VideoLock(Core::Video::Output& video) = 0;
			virtual void VideoUnlock(Core::Video::Output& video) = 0;


			static bool VideoLock(void* userData, Core::Video::Output& video);
			static void VideoUnlock(void* userData, Core::Video::Output& video);

			float m_screenWidth, m_screenHeight;
		};

		//NullRectRendere: does no rendering at all
		class NullRectRenderer: public IRectRenderer
		{
		public:
			virtual void DrawRect(ITexture& texture, const Maths::Rect& rect, const Color& color) override {}
			virtual void DrawOutlineRect(const Color& color, float size, float x, float y, float width, float height) override {}
		};
	}
}

#endif