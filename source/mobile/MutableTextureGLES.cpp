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

#include "MutableTextureGLES.hpp"

#include "../core/NstLog.hpp"

#include "../../third-party/RemoteController/Timer.h"

#include <vector>
#include <stdexcept>
#include <stdlib.h>

#define PROFILE_TEXTURE_UPDATE_TIME 0

namespace Nes
{
	namespace Video {
		namespace GL {
			//helpers
			static void defineTexture(GLuint texture, unsigned int width, unsigned int height, GLint filterMode)
			{
				glBindTexture(GL_TEXTURE_2D, texture);
				
				setTextureFilterMode(filterMode);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				
				glTexImage2D(GL_TEXTURE_2D,
							 0,
							 GL_RGBA,
							 width,
							 height,
							 0,
							 GL_RGBA,
							 GL_UNSIGNED_BYTE,
							 0);
			}

			static void defineTexture(GLuint texture, unsigned int width, unsigned int height)
			{
				defineTexture(texture, width, height, GL_NEAREST);
			}
			
			//Impl
			class MutableTexture::Impl {
			public:
				Impl(unsigned int width, unsigned int height)
				: m_width(width), m_height(height)
				{
				}
				
				virtual ~Impl() {
				}
				
				virtual void* Lock() = 0;
				virtual void  Unlock() = 0;
				virtual GLuint GetOutputTexture() = 0;

				virtual void SetFilterMode(bool linear) = 0;
				
				virtual void Invalidate() = 0;
				virtual bool IsInvalid() const = 0;
			protected:
				unsigned int m_width, m_height;
			};
			
			
			class SysmemImpl: public MutableTexture::Impl {
			public:
				SysmemImpl(unsigned int width, unsigned int height)
				: MutableTexture::Impl(width, height)
#if PROFILE_TEXTURE_UPDATE_TIME
				, m_avgTextureUploadTime(0), m_textureUploadWindowTime(0)
#endif
				{
					//create GL texture
					glGenTextures(1, &m_outputTexture);
					defineTexture(m_outputTexture, width, height);
					
					//create system memory
					m_sysMem.insert(m_sysMem.end(), width * height * 4, 0);
					
					Core::Log() << "Using system memory\n";
				}
				
				virtual ~SysmemImpl() {
					glDeleteTextures(1, &m_outputTexture);
					GL_ERR_CHECK
				}
				
				virtual void* Lock() override { return m_sysMem.data(); }
				virtual void  Unlock() override  {
#if PROFILE_TEXTURE_UPDATE_TIME
					HQRemote::ScopedTimeProfiler profiler("SysmemImpl's texture upload", m_avgTextureUploadTime, m_textureUploadWindowTime);
#endif//if PROFILE_TEXTURE_UPDATE_TIME
					
					//upload from system memory to GL texture
					glBindTexture(GL_TEXTURE_2D, m_outputTexture);
					
#if 0
					glTexImage2D(GL_TEXTURE_2D,
								 0,
								 GL_RGBA,
								 m_width,
								 m_height,
								 0,
								 GL_RGBA,
								 GL_UNSIGNED_BYTE,
								 m_sysMem.data());
#else
					glTexSubImage2D(GL_TEXTURE_2D,
									0,
									0,
									0,
									m_width,
									m_height,
									GL_RGBA,
									GL_UNSIGNED_BYTE,
									m_sysMem.data());
#endif

#if 0
					auto randIdx = rand() % m_sysMem.size();
					Core::Log() << "Random texture data: (" << randIdx << ") = " << m_sysMem[randIdx];
#endif
				}
				virtual GLuint GetOutputTexture() override  { return m_outputTexture; }
				
				virtual void Invalidate() override  { m_outputTexture = 0; }
				virtual bool IsInvalid() const override { return m_outputTexture == 0; }

				virtual void SetFilterMode(bool linear) override {
					glBindTexture(GL_TEXTURE_2D, m_outputTexture);

					if (linear)
						setTextureFilterMode(GL_LINEAR);
					else
						setTextureFilterMode(GL_NEAREST);
				}
			private:
				GLuint m_outputTexture;
				
				std::vector<unsigned char> m_sysMem;
				
#if PROFILE_TEXTURE_UPDATE_TIME
				float m_avgTextureUploadTime;
				float m_textureUploadWindowTime;
#endif
			};
			
			class PBOImpl: public MutableTexture::Impl
			{
			public:
				PBOImpl(unsigned int width, unsigned int height)
				: MutableTexture::Impl(width, height), m_activeTexture(1), m_nextActiveTexture(0), m_bufferSize(width * height * 4)
				{
					glGenTextures(2, m_textures);
					glGenBuffers(2, m_pbos);
					
					GLint oldBoundPBO;
					glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &oldBoundPBO);
					
					for (int i = 0; i < 2; ++i)
					{
						defineTexture(m_textures[i], width, height);
						
						glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbos[i]);
						glBufferData(GL_PIXEL_UNPACK_BUFFER, m_bufferSize, 0, GL_STREAM_DRAW);
					}
					
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, oldBoundPBO);
					
					Core::Log() << "Using PBO\n";
				}
				
				virtual ~PBOImpl() {
					glDeleteTextures(2, m_textures);
					GL_ERR_CHECK
					
					glDeleteBuffers(2, m_pbos);
					GL_ERR_CHECK
				}
				
				virtual void* Lock() override {
					m_activeTexture = m_nextActiveTexture;
					
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbos[m_nextActiveTexture]);
					return glMapBufferRangeFuncPtr(GL_PIXEL_UNPACK_BUFFER, 0, m_bufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
				}
				
				virtual void  Unlock() override {
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbos[m_nextActiveTexture]);
					glUnmapBufferFuncPtr(GL_PIXEL_UNPACK_BUFFER);
					
					m_nextActiveTexture = (m_nextActiveTexture + 1) % 2;
					//upload to texture
					glBindTexture(GL_TEXTURE_2D, m_textures[m_nextActiveTexture]);
					glTexSubImage2D(GL_TEXTURE_2D,
									0,
									0,
									0,
									m_width,
									m_height,
									GL_RGBA,
									GL_UNSIGNED_BYTE,
									0);
					
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				}
				
				virtual GLuint GetOutputTexture() override {
					return m_textures[m_activeTexture];
				}
				
				virtual void Invalidate() override {
					memset(m_textures, 0, sizeof(m_textures));
					memset(m_pbos, 0, sizeof(m_pbos));
					
					m_activeTexture = 1;
					m_nextActiveTexture = 0;
				}
				
				virtual bool IsInvalid() const override {
					for (int i = 0; i < sizeof(m_textures) / sizeof(m_textures[0]); ++i) {
						if (m_textures[i] != 0)
							return false;
					}
					for (int i = 0; i < sizeof(m_pbos) / sizeof(m_pbos[0]); ++i) {
						if (m_pbos[i] != 0)
							return false;
					}
					
					return true;
				}

				virtual void SetFilterMode(bool linear) override {
					for (auto texture: m_textures) {
						glBindTexture(GL_TEXTURE_2D, texture);

						if (linear)
							setTextureFilterMode(GL_LINEAR);
						else
							setTextureFilterMode(GL_NEAREST);
					}
				}
			private:
				GLuint m_textures[2];
				GLuint m_pbos[2];
				
				size_t m_bufferSize;
				
				int m_activeTexture, m_nextActiveTexture;
			};

			
			/*--------  MutableTexture ------*/
			MutableTexture::MutableTexture(int esVersionMajor, unsigned int width, unsigned int height)
			:m_locked(false)
			{
				if (esVersionMajor >= 3 && glMapBufferRangeFuncPtr != NULL && glUnmapBufferFuncPtr != NULL)
					m_impl = new PBOImpl(width, height);
				else
					m_impl = new SysmemImpl(width, height);

				// initialize texture with zeros data
				auto data = m_impl->Lock();
				if (data) {
					memset(data, 0, width * height * 4);
					m_impl->Unlock();
				}
			}

			MutableTexture::~MutableTexture() {
				delete m_impl;
			}

			void MutableTexture::BindTexture()
			{
				if (m_locked)
					throw std::runtime_error("Cannot bind texture when it is locked");
				
				glBindTexture(GL_TEXTURE_2D, m_impl->GetOutputTexture());
			}

			void MutableTexture::SetFilterMode(bool linear) {
				m_impl->SetFilterMode(linear);
			}

			void* MutableTexture::Lock() {
				if (m_locked)
					return NULL;
				m_locked = true;
				
				return m_impl->Lock();
			}

			void MutableTexture::Unlock() {
				if (!m_locked)
					return;
				
				m_impl->Unlock();
				
				m_locked = false;
			}
			
			bool MutableTexture::IsInvalid() const {
				return m_impl->IsInvalid();
			}
			
			void MutableTexture::Invalidate()
			{
				m_impl->Invalidate();
			}

			/*--------- RenderTargetTexture ---------------*/
			RenderTargetTexture::RenderTargetTexture(unsigned int width, unsigned int height)
					: m_width(width), m_height(height), m_texture(0), m_fbo(0),
					  m_fboComplete(false)
			{
				Reset();
			}

			RenderTargetTexture::~RenderTargetTexture()
			{
				Cleanup();
			}

			void RenderTargetTexture::Cleanup() {
				if (m_texture) {
					glDeleteTextures(1, &m_texture);
					m_texture = 0;
				}
				if (m_fbo) {
					glDeleteFramebuffers(1, &m_fbo);
					m_fbo = 0;
				}
				GL_ERR_CHECK
			}

			void RenderTargetTexture::BindTexture()//this must be called after Unlock()
			{
				glBindTexture(GL_TEXTURE_2D, m_texture);
			}

			void RenderTargetTexture::SetActive(bool active) {
				if (IsInvalid())
					return;

				glBindFramebuffer(GL_FRAMEBUFFER, active ? m_fbo : 0);
			}

			void RenderTargetTexture::Invalidate()//this should be call when opengl context lost
			{
				m_texture = 0; m_fbo = 0;
			}
			bool RenderTargetTexture::IsInvalid() const {
				return m_texture == 0 || m_fbo == 0 || !m_fboComplete;
			}

			void RenderTargetTexture::Reset() {
				Cleanup();

				//create GL texture
				glGenTextures(1, &m_texture);
				defineTexture(m_texture, m_width, m_height, GL_LINEAR);

				// create FBO
				glGenFramebuffers(1, &m_fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

				// attach texture to FBO
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
					m_fboComplete = true;

					Core::Log() << "FBO complete\n";
				}
				else {
					m_fboComplete = false;
					Core::Log() << "Error: FBO incomplete\n";
				}

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}

			void RenderTargetTexture::ResetIfInvalid() {
				if (IsInvalid())
					Reset();
			}
		}
	}
}