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

#ifndef RendererGLES_hpp
#define RendererGLES_hpp

#include "../core/api/NstApiEmulator.hpp"
#include "../core/api/NstApiVideo.hpp"

#include "Renderer.hpp"
#include "TextureGLES.hpp"
#include "MutableTextureGLES.hpp"

namespace Nes {
	namespace Video {
		namespace GL {
			class Renderer: public IRenderer
			{
			public:
				Renderer(Core::Machine& emulator);
				~Renderer();
				
				void SetESVersion(int versionMajor) { m_esVersionMajor = versionMajor; }
				virtual void Invalidate() override;//this should be called when GL context lost
				virtual void Cleanup() override;//cleanup graphics resources

				virtual void EnableFullScreenVideo(bool e) override ;

				unsigned int GetVideoMinX() const { return m_videoX; }
				unsigned int GetVideoMinY() const { return m_videoY; }
				
				virtual void DrawRect(ITexture& texture, float x, float y, float width, float height) override;
				virtual void DrawOutlineRect(const Color& color, float size, float x, float y, float width, float height) override;

				virtual void PresentVideo() override;
			private:
				virtual void ResetImpl() override;
				
				void DrawRect(float x, float y, float width, float height);
				void ApplyRectTransform(GLint uniformLoc, float x, float y, float width, float height);

				virtual bool VideoLock(Core::Video::Output& video) override;
				virtual void VideoUnlock(Core::Video::Output& video) override;

				bool m_videoFullscreen;
				float m_videoX, m_videoY, m_videoWidth, m_videoHeight;
				
				std::shared_ptr<MutableTexture> m_videoTexture;
				
				GLint m_renderTransformUniformLoc;
				GLuint m_renderProgram;
				GLuint m_renderVBO;

				GLint m_renderOutlineTransformUniformLoc, m_renderOutlineColorUniformLoc;
				GLuint m_renderOutlineProgram;
				GLuint m_renderOutlineVBO;
				
				int m_esVersionMajor;

			};
		}
	}
}

#endif /* RendererGLES_hpp */
