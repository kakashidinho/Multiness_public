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

#ifndef Texture_hpp
#define Texture_hpp

#include "Common.hpp"

namespace Nes {
	namespace Video {
		class ITexture {
		public:
			virtual ~ITexture() {}
			
			virtual void BindTexture() = 0;
		};
		
		class IStaticTexture: public ITexture {
		public:
			
			virtual bool IsInvalid() const = 0;
			virtual void Invalidate() = 0;//should be called when GL/DX context lost
			virtual void Reset(const char* file, Callback::OpenFileCallback resourceLoader) = 0;
			virtual void ResetIfInvalid(const char* file, Callback::OpenFileCallback resourceLoader) = 0;//perform Reset() only if texture is invalid (i.e. first created or after Invalidate())
			virtual void Cleanup() = 0;//release graphics resources
		};
	}	
}
	
#endif