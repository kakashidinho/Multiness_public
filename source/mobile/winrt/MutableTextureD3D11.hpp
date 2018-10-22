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

#pragma once

#include "TextureD3D11.hpp"

#include <vector>

namespace Nes {
	namespace Video {
		namespace D3D11 {
			class MutableTexture : private BaseTexture, public ITexture {
			public:
				MutableTexture(D3DDeviceWrapper& d3dWrapper, unsigned int width, unsigned int height);
				~MutableTexture();

				virtual void BindTexture() override;//this must be called after Unlock()
				void* Lock(UINT &pitch);
				void Unlock();

				void Invalidate();//this should be call when D3D context lost
				bool IsInvalid() const;
			private:
				unsigned int m_width, m_height;

				std::vector<unsigned char> m_shadowBuffer;
				bool m_locked;
			};
		}
	}
}