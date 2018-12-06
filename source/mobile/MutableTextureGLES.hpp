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

#ifndef MutableTextureGLES_hpp
#define MutableTextureGLES_hpp

#include <stdio.h>

#include "GLESUtils.hpp"
#include "Texture.hpp"

namespace Nes {
	namespace Video {
		namespace GL {
			class MutableTexture: public ITexture {
			public:
				class Impl;
				
				MutableTexture(int esVersionMajor, unsigned int width, unsigned int height);
				~MutableTexture();
				
				virtual void BindTexture() override;//this must be called after Unlock()
				void SetFilterMode(bool linear);
				void* Lock();
				void Unlock();
				
				void Invalidate();//this should be call when opengl context lost
				bool IsInvalid() const;
			private:
				bool m_locked;
				
				Impl *m_impl;
			};

			class RenderTargetTexture: public ITexture {
			public:

				RenderTargetTexture(unsigned int width, unsigned int height);
				~RenderTargetTexture();

				virtual void BindTexture() override;//this must be called after Unlock()
				void SetActive(bool active); // set as active render target

				uint32_t GetWidth() const { return m_width; }
				uint32_t GetHeight() const { return m_height; }

				void Invalidate();//this should be call when opengl context lost
				bool IsInvalid() const;
				void Reset();
				void ResetIfInvalid();
			private:
				void Cleanup();

				uint32_t m_width, m_height;
				GLuint m_texture;
				GLuint m_fbo;
				bool m_fboComplete;
			};
		}
	}
}

#endif /* VideoTextureGLES_hpp */
