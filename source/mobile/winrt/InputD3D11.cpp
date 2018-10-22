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

#include "InputD3D11.hpp"
#include "TextureD3D11.hpp"

namespace Nes {
	namespace Input {
		namespace D3D11 {
			InputDPad::InputDPad(Video::D3D11::D3DDeviceWrapper& d3dWrapper)
				: Input::InputDPad(true)
			{
				for (int i = 0; i < sizeof(m_buttonRects) / sizeof(m_buttonRects[0]); ++i)
				{
					m_buttonTextures[i] = new Video::D3D11::StaticTexture(d3dWrapper);
					m_buttonHighlightTextures[i] = new Video::D3D11::StaticTexture(d3dWrapper);
				}

				m_dPadTexture = new Video::D3D11::StaticTexture(d3dWrapper);

				for (int i = 0; i < sizeof(m_dPadDirectionHighlightRects) / sizeof(m_dPadDirectionHighlightRects[0]); ++i)
				{
					m_dpadDirectionHighlightTextures[i] = new Video::D3D11::StaticTexture(d3dWrapper);
				}
			}
		}
	}
}