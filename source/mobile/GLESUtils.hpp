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

#ifndef GLESUtils_hpp
#define GLESUtils_hpp

#include "../core/NstLog.hpp"

#ifdef __APPLE__
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#ifdef DEBUG
#	define GL_ERR_CHECK \
{\
	auto err = glGetError();\
	if (err != GL_NO_ERROR)\
	{\
		Core::Log() << "gl error " << err << " at file '" << __FILE__ << "': "<< __LINE__ << "\n";\
	}\
}
#else
#	define GL_ERR_CHECK
#endif


#ifndef GL_PIXEL_PACK_BUFFER
#	define GL_PIXEL_PACK_BUFFER                             0x88EB
#	define GL_PIXEL_UNPACK_BUFFER                           0x88EC
#	define GL_PIXEL_PACK_BUFFER_BINDING                     0x88ED
#	define GL_PIXEL_UNPACK_BUFFER_BINDING                   0x88EF
#endif//ifndef GL_PIXEL_PACK_BUFFER

#ifndef GL_MAP_INVALIDATE_BUFFER_BIT
#	define GL_MAP_READ_BIT                                  0x0001
#	define GL_MAP_WRITE_BIT                                 0x0002
#	define GL_MAP_INVALIDATE_RANGE_BIT                      0x0004
#	define GL_MAP_INVALIDATE_BUFFER_BIT                     0x0008
#	define GL_MAP_FLUSH_EXPLICIT_BIT                        0x0010
#	define GL_MAP_UNSYNCHRONIZED_BIT                        0x0020
#	define NO_GL_MAP_BUFFER_DEFINED
#endif//ifndef GL_MAP_INVALIDATE_BUFFER_BIT

typedef GLboolean (GL_APIENTRY *GLUNMAPBUFFER) (GLenum target);
typedef GLvoid*   (GL_APIENTRY *GLMAPBUFFERRANGE) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
extern GLMAPBUFFERRANGE glMapBufferRangeFuncPtr;
extern GLUNMAPBUFFER glUnmapBufferFuncPtr;

namespace Nes {
	namespace Video {
		namespace GL {
			void resetGL();
			
			GLuint createShader(GLenum type, const char* source);
			GLuint createProgram(const char* vs, const char* fs);

			void setTextureFilterMode(GLint mode);
		}
	}
}

#endif /* GLESUtils_hpp */
