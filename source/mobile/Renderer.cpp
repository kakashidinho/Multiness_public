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

#include "Renderer.hpp"

namespace Nes
{
	namespace Video {
		const Color Color::WHITE = {.a = 1, .r = 1, .g = 1, .b = 1};

		/*-------  IRenderer -----------*/
		IRenderer::IRenderer()
		: m_screenWidth(0), m_screenHeight(0)
		{
			//register callbacks
			Core::Video::Output::lockCallback.Set(VideoLock, this);
			Core::Video::Output::unlockCallback.Set(VideoUnlock, this);
		}

		IRenderer::~IRenderer() {
			Core::Video::Output::lockCallback.Set(NULL, NULL);
			Core::Video::Output::unlockCallback.Set(NULL, NULL);
		}
		
		void IRenderer::Reset(float screenWidth, float screenHeight)
		{
			m_screenWidth = screenWidth;
			m_screenHeight = screenHeight;
			
			ResetImpl();
		}

		bool IRenderer::VideoLock(void* userData, Core::Video::Output& video) {
			auto renderer = reinterpret_cast<IRenderer*>(userData);
			
			return renderer->VideoLock(video);
		}

		void IRenderer::VideoUnlock(void* userData, Core::Video::Output& video)
		{
			auto renderer = reinterpret_cast<IRenderer*>(userData);
			
			renderer->VideoUnlock(video);
		}
	}

}