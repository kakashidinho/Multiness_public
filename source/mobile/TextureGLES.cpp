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

#include "TextureGLES.hpp"

#include "../core/NstLog.hpp"

#include <ImagesLoader/Bitmap.h>

#include <stdexcept>

namespace Nes {
	namespace Video {
		namespace GL {
			Texture::Texture()
			: m_name(0)
			{
			}
			
			Texture::~Texture() {
				Cleanup();
			}
			
			void Texture::Cleanup()
			{
				if (m_name)
				{
					glDeleteTextures(1, &m_name);
					m_name = 0;
					
					GL_ERR_CHECK
				}
			}

			bool Texture::IsInvalid() const {
				return m_name == 0;
			}
			
			void Texture::Invalidate()//should be called when GL/DX context lost
			{
				m_name = 0;
			}
			
			void Texture::Reset(const char* file, Callback::OpenFileCallback resourceLoader) 
			{
				Cleanup();
				
				Core::Log() << "Loading texture from " << file;
				
				auto stream = resourceLoader(file);
				
				Init(*stream);
				
				stream->Release();
			}
			
			void Texture::ResetIfInvalid(const char* file, Callback::OpenFileCallback resourceLoader)
			{
				if (IsInvalid())
				{
					Reset(file, resourceLoader);
				}
			}
			
			void Texture::Init(HQDataReaderStream& stream)
			{
				Bitmap loader;
				loader.SetLoadedOutputRGBLayout(LAYOUT_RGB);
				loader.SetLoadedOutputRGB16Layout(LAYOUT_RGB);
				
				if (loader.LoadFromStream(&stream) != IMG_OK)
					throw std::runtime_error("Cannot load texture");
				
				auto format = loader.GetSurfaceFormat();
				
				GLenum glformat;
				GLenum gltype;
				
				switch (format)
				{
				case FMT_A8B8G8R8: case FMT_A8R8G8B8:
					glformat = GL_RGBA;
					gltype = GL_UNSIGNED_BYTE;
					break;
				case FMT_B8G8R8: case FMT_R8G8B8:
					glformat = GL_RGB;
					gltype = GL_UNSIGNED_BYTE;
					break;
				case FMT_R5G6B5:
					glformat = GL_RGB;
					gltype = GL_UNSIGNED_SHORT_5_6_5;
					break;
				case FMT_A8L8:
					glformat = GL_LUMINANCE_ALPHA;
					gltype = GL_UNSIGNED_BYTE;
					break;
				default:
					throw std::runtime_error("Cannot load texture. Unsupported format.");
				}
				
				loader.SetPixelOrigin(ORIGIN_TOP_LEFT);
				
				//create texture
				glGenTextures(1, &m_name);
				glBindTexture(GL_TEXTURE_2D, m_name);
				
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				if (loader.GetWidth() == loader.GetHeight())//there are some bugs in Android GLES the prevent glGenerateMipmap() to work on non-square texture
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
				else
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				
				glTexImage2D(GL_TEXTURE_2D,
							 0,
							 glformat,
							 loader.GetWidth(),
							 loader.GetHeight(),
							 0,
							 glformat,
							 gltype,
							 loader.GetPixelData());
					
				if (loader.GetWidth() == loader.GetHeight())//only generate mipmap if this is square texture
					glGenerateMipmap(GL_TEXTURE_2D);
			}
				
			void Texture::BindTexture(){
				glBindTexture(GL_TEXTURE_2D, m_name);
			}
		}	
	}
}