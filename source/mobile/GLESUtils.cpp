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

#include "GLESUtils.hpp"

#include "../core/NstLog.hpp"

#ifdef __ANDROID__
#include <android/log.h>
#	define _LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "Nes", __VA_ARGS__)
#else
#	define _LOG(...) fprintf(stderr, __VA_ARGS__)
#endif

#include <string>
#include <stdlib.h>
#include <dlfcn.h>

GLMAPBUFFERRANGE glMapBufferRangeFuncPtr = NULL;
GLUNMAPBUFFER glUnmapBufferFuncPtr = NULL;
static void* GLES3_lib = NULL;

namespace Nes {
	namespace Video {
		namespace GL {
			void resetGL()
			{
				_LOG("entering GL::resetGL()\n");
				
#ifdef NO_GL_MAP_BUFFER_DEFINED
				if (GLES3_lib == NULL)
					GLES3_lib = dlopen("libGLESv3.so", RTLD_LAZY);
				if (GLES3_lib)
				{
					glMapBufferRangeFuncPtr = (GLMAPBUFFERRANGE)dlsym(GLES3_lib, "glMapBufferRange");
					glUnmapBufferFuncPtr = (GLUNMAPBUFFER)dlsym(GLES3_lib, "glUnmapBuffer");
				}
#else//NO_GL_MAP_BUFFER_DEFINED
				glMapBufferRangeFuncPtr = &glMapBufferRange;
				glUnmapBufferFuncPtr = &glUnmapBuffer;
#endif//NO_GL_MAP_BUFFER_DEFINED
				
				_LOG("leaving GL::resetGL()\n");
			}
			
			typedef void (*GLInfoFunction)(GLuint program, GLenum pname, GLint* params);
			typedef void (*GLLogFunction) (GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog);
			
			static std::string logForOpenGLObject(GLuint object, GLInfoFunction infoFunc, GLLogFunction logFunc)
			{
				std::string ret;
				GLint logLength = 0, charsWritten = 0;
				
				infoFunc(object, GL_INFO_LOG_LENGTH, &logLength);
				if (logLength < 1)
					return "";
				
				char *logBytes = (char*)malloc(logLength);
				logFunc(object, logLength, &charsWritten, logBytes);
				
				ret = logBytes;
				
				free(logBytes);
				return ret;
			}
			
			GLuint createShader(GLenum type, const char* source)
			{
				const GLchar *sources[] = {
					source
				};
				
				GLint status;
				
				auto shader = glCreateShader(type);
				glShaderSource(shader, sizeof(sources)/sizeof(*sources), sources, nullptr);
				glCompileShader(shader);
				
				glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
				
				if (! status)
				{
					GLsizei length;
					glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &length);
					char* src = (char *)malloc(sizeof(GLchar) * length);
					
					glGetShaderSource(shader, length, nullptr, src);
					Core::Log() << "ERROR: Failed to compile shader:\n" << src;
					
					Core::Log() << "LOG: "
								<< logForOpenGLObject(shader,
													  (GLInfoFunction)&glGetShaderiv,
													  (GLLogFunction)&glGetShaderInfoLog).c_str();
					
					free(src);
					
					glDeleteShader(shader);
					
					return 0;
				}
				
				return shader;
			}
			
			GLuint createProgram(const char* vss, const char* fss)
			{
				GLint status;
				auto program = glCreateProgram();
				
				auto vs = createShader(GL_VERTEX_SHADER, vss);
				auto fs = createShader(GL_FRAGMENT_SHADER, fss);
				
				glAttachShader(program, vs);
				glAttachShader(program, fs);
				
				glLinkProgram(program);
				
				glDeleteShader(vs);
				glDeleteShader(fs);
				
				glGetProgramiv(program, GL_LINK_STATUS, &status);
				
				if (status == GL_FALSE)
				{
					Core::Log() << "ERROR: Failed to link program: %s"
								<< logForOpenGLObject(program, (GLInfoFunction)&glGetProgramiv, (GLLogFunction)&glGetProgramInfoLog).c_str();
					glDeleteProgram(program);
					program = 0;
				}
				
				return program;
			}

			void setTextureFilterMode(GLint filterMode) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterMode);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterMode);
			}
		}
	}
}