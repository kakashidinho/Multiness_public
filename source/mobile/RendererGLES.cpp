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

#include "RendererGLES.hpp"

#include "../core/NstLog.hpp"

#ifndef max
#	define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#	define min(a,b) ((a) < (b) ? (a) : (b))
#endif

namespace Nes
{
	namespace Video {
		namespace GL {
			// Shader sources
			static const GLchar vshader_src[] =
			"uniform highp vec4 transform;"
			"attribute vec2 position;"
			"attribute vec2 texcoord;"
			"varying mediump vec2 outcoord;"
			"void main() {"
			"	vec2 wposition = transform.xy + position * transform.zw;"
			"	outcoord = texcoord;"
			"	gl_Position = vec4(wposition, 0.0, 1.0);"
			"}";

			static const GLchar fshader_src[] =
			"varying mediump vec2 outcoord;"
			"uniform sampler2D nestex;"
			"void main() {"
			"	gl_FragColor = texture2D(nestex, outcoord).zyxw;"
			"}";

			static const GLchar vshader_color_src[] =
			"uniform highp vec4 transform;"
			"uniform lowp vec4 color;"
			"attribute vec2 position;"
			"varying lowp vec4 outcolor;"
			"void main() {"
			"	vec2 wposition = transform.xy + position * transform.zw;"
			"	outcolor = color;"
			"	gl_Position = vec4(wposition, 0.0, 1.0);"
			"}";

			static const GLchar fshader_color_src[] =
			"varying lowp vec4 outcolor;"
			"void main() {"
			"	gl_FragColor = outcolor;"
			"}";

			struct Vertex {
				float position[2];
			};

			struct VertexWithTexCoords {
				float position[2];
				float texcoord[2];
			};

			static const int POSITION_LOC = 0;
			static const int TEXCOORD_LOC = 1;

			/*-------  Renderer -----------*/
			Renderer::Renderer(Core::Machine& emulator)
			: m_videoTexture(nullptr),
			  m_renderProgram(0), m_renderOutlineProgram(0),
			  m_renderVBO(0), m_renderOutlineVBO(0),
			  m_esVersionMajor(2),
			  m_videoFullscreen(false)
			{
				//setup render state
				Api::Video::RenderState renderstate;

				renderstate.filter = Api::Video::RenderState::FILTER_NONE;
				renderstate.width = Core::Video::Output::WIDTH;
				renderstate.height = Core::Video::Output::HEIGHT;
				renderstate.bits.count = 32;

				renderstate.bits.mask.r = 0x00ff0000;
				renderstate.bits.mask.g = 0x0000ff00;
				renderstate.bits.mask.b = 0x000000ff;

				Api::Video(emulator).SetRenderState(renderstate);
			}

			Renderer::~Renderer() {
				Cleanup();
			}

			void Renderer::Cleanup()
			{

				if (m_renderProgram)
				{
					glDeleteProgram(m_renderProgram);
					m_renderProgram = 0;

					GL_ERR_CHECK
				}

				if (m_renderOutlineProgram) {
					glDeleteProgram(m_renderOutlineProgram);
					m_renderOutlineProgram = 0;

					GL_ERR_CHECK
				}

				if (m_renderVBO)
				{
					glDeleteBuffers(1, &m_renderVBO);
					m_renderVBO = 0;

					GL_ERR_CHECK
				}

				if (m_renderOutlineVBO)
				{
					glDeleteBuffers(1, &m_renderOutlineVBO);
					m_renderOutlineVBO = 0;

					GL_ERR_CHECK
				}

				m_videoTexture = nullptr;
			}

			void Renderer::Invalidate()
			{
				Core::Log() << "Renderer::Invalidate() called\n";

				if (m_videoTexture)
					m_videoTexture->Invalidate();

				m_renderProgram = 0;
				m_renderVBO = 0;

				m_renderOutlineProgram = 0;
				m_renderOutlineVBO = 0;
			}

			void Renderer::ResetImpl()
			{
				resetGL();//GLESUtils.cpp

				//create video texture
				if (m_videoTexture == nullptr || m_videoTexture->IsInvalid())
				{
					m_videoTexture = std::make_shared<MutableTexture>(m_esVersionMajor, Core::Video::Output::WIDTH, Core::Video::Output::HEIGHT);
					Core::Log() << "Renderer created video texture\n";
				}

				//create shader program
				if (m_renderProgram == 0)
				{
					m_renderProgram = createProgram(vshader_src, fshader_src);

					glBindAttribLocation(m_renderProgram, POSITION_LOC, "position");
					glBindAttribLocation(m_renderProgram, TEXCOORD_LOC, "texcoord");

					glLinkProgram(m_renderProgram);

					glUseProgram(m_renderProgram);

					auto texLoc = glGetUniformLocation(m_renderProgram, "nestex");
					glUniform1i(texLoc, 0);

					m_renderTransformUniformLoc = glGetUniformLocation(m_renderProgram, "transform");

					Core::Log() << "Renderer created shader program\n";
				}//if (m_renderProgram == 0)

				if (m_renderOutlineProgram == 0)
				{
					m_renderOutlineProgram = createProgram(vshader_color_src, fshader_color_src);

					glBindAttribLocation(m_renderOutlineProgram, POSITION_LOC, "position");

					glLinkProgram(m_renderOutlineProgram);

					glUseProgram(m_renderOutlineProgram);

					m_renderOutlineTransformUniformLoc = glGetUniformLocation(m_renderOutlineProgram, "transform");
					m_renderOutlineColorUniformLoc = glGetUniformLocation(m_renderOutlineProgram, "color");

					Core::Log() << "Renderer created outline shader program\n";
				}//if (m_renderOutlineProgram == 0)

				//create general purpose quad buffer
				if (m_renderVBO == 0)
				{
					VertexWithTexCoords vertices[] = {
						{0, 0, 0, 1},
						{1, 0, 1, 1},
						{0, 1, 0, 0},
						{1, 1, 1, 0},
					};

					glGenBuffers(1, &m_renderVBO);
					glBindBuffer(GL_ARRAY_BUFFER, m_renderVBO);

					glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

					Core::Log() << "Renderer created quad buffer\n";
				}//if (m_renderVBO == 0)

				// create outline rect buffer
				if (m_renderOutlineVBO == 0)
				{
					Vertex vertices[] = {
							{0, 0},
							{1, 0},
							{1, 1},
							{0, 1},
					};

					glGenBuffers(1, &m_renderOutlineVBO);
					glBindBuffer(GL_ARRAY_BUFFER, m_renderOutlineVBO);

					glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

					Core::Log() << "Renderer created outline quad buffer\n";
				}//if (m_renderOutlineVBO == 0)

				//calculate video's screen region
				auto width = Core::Video::Output::WIDTH;
				auto height = Core::Video::Output::HEIGHT;

				auto widthRatio = (float)m_screenWidth / width;
				auto heightRatio = (float)m_screenHeight / height;

				auto ratio = min(widthRatio, heightRatio);

				m_videoWidth = width * ratio;
				m_videoHeight = height * ratio;
				m_videoX = 0.5f * (m_screenWidth - m_videoWidth);
				m_videoY = m_screenHeight - m_videoHeight;

				//set blending function
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			void Renderer::EnableFullScreenVideo(bool e) {
				m_videoFullscreen = e;
			}

			void Renderer::ApplyRectTransform(GLint uniformLoc, float x, float y, float width, float height) {
				auto wScale = 2.f / m_screenWidth;
				auto hScale = 2.f / m_screenHeight;
				auto wScaled = wScale * width;
				auto hScaled = hScale * height;
				auto xNormalized = wScale * x - 1;
				auto yNormalized = hScale * y - 1;

				glUniform4f(uniformLoc, xNormalized, yNormalized, wScaled, hScaled);
			}

			void Renderer::DrawRect(ITexture& texture, float x, float y, float width, float height)
			{
				//bind texture
				glActiveTexture(GL_TEXTURE0);
				texture.BindTexture();

				//draw rect
				DrawRect(x, y, width, height);
			}

			void Renderer::DrawRect(float x, float y, float width, float height)
			{
				if (m_renderProgram == 0)
					return;

				//bind VBO
				glBindBuffer(GL_ARRAY_BUFFER, m_renderVBO);

				glEnableVertexAttribArray(POSITION_LOC);
				glEnableVertexAttribArray(TEXCOORD_LOC);

				glVertexAttribPointer(POSITION_LOC,
									  2,
									  GL_FLOAT,
									  GL_FALSE,
									  sizeof(VertexWithTexCoords),
									  0);

				glVertexAttribPointer(TEXCOORD_LOC,
									  2,
									  GL_FLOAT,
									  GL_FALSE,
									  sizeof(VertexWithTexCoords),
									  (void*)(2 * sizeof(float)));

				//use program
				glUseProgram(m_renderProgram);

				ApplyRectTransform(m_renderTransformUniformLoc, x, y, width, height);

				//draw
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			}

			void Renderer::DrawOutlineRect(const Color& color, float size, float x, float y, float width, float height) {
				if (m_renderOutlineProgram == 0)
					return;

				//bind VBO
				glBindBuffer(GL_ARRAY_BUFFER, m_renderOutlineVBO);

				glEnableVertexAttribArray(POSITION_LOC);

				glVertexAttribPointer(POSITION_LOC,
									  2,
									  GL_FLOAT,
									  GL_FALSE,
									  sizeof(Vertex),
									  0);

				//use program
				glUseProgram(m_renderOutlineProgram);

				ApplyRectTransform(m_renderOutlineTransformUniformLoc, x, y, width, height);

				glUniform4f(m_renderOutlineColorUniformLoc, color.r, color.g, color.b, color.a);

				//draw
				glLineWidth(size);
				glDrawArrays(GL_LINE_LOOP, 0, 4);
			}

			void Renderer::PresentVideo() {
				/*------- render the video texture  ----*/
				//glClearColor(1, 1, 1, 1);
				
				//bind texture
				glActiveTexture(GL_TEXTURE0);
				m_videoTexture->BindTexture();
				
				
				glDisable(GL_BLEND);

				if (m_videoFullscreen)
					DrawRect(0, 0, m_screenWidth, m_screenHeight);
				else
					DrawRect(m_videoX, m_videoY, m_videoWidth, m_videoHeight);
				
				glEnable(GL_BLEND);
			}
			
			bool Renderer::VideoLock(Core::Video::Output& video)
			{
				if (!m_videoTexture)
					return false;
				
				auto pixels = m_videoTexture->Lock();
				
				if (!pixels)
					return false;
				
				video.pixels = pixels;
				video.pitch = Core::Video::Output::WIDTH * 4;//32 bit
				
				return true;
			}
			
			void Renderer::VideoUnlock(Core::Video::Output& video)
			{
				if (m_videoTexture == nullptr)
					return;
				
				m_videoTexture->Unlock();
			}
		}
	}
}