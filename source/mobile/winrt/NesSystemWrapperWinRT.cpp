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

#include "NesSystemWrapperWinRT.hpp"
#include "RendererD3D11.hpp"
#include "InputD3D11.hpp"

namespace  Nes {
	NesSystemWrapperWinRT::NesSystemWrapperWinRT(std::istream* dbStream)
	: NesSystemWrapper(dbStream)
	{
		auto pRenderer = new Video::D3D11::Renderer(m_emulator);

		m_renderer = std::unique_ptr<Video::IRenderer>(pRenderer);
		
		SetInput(new Input::D3D11::InputDPad(*pRenderer));
	}

	NesSystemWrapperWinRT::~NesSystemWrapperWinRT() {
		SetInput(nullptr);
	}

	void NesSystemWrapperWinRT::ResetD3D(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext, const DirectX::XMFLOAT4X4& orientationMatrix)
	{
		auto rendererD3D = static_cast<Video::D3D11::Renderer*>(m_renderer.get());
		rendererD3D->ResetD3D(d3dDevice, d3dDeviceContext, orientationMatrix);
	}

	void NesSystemWrapperWinRT::SetUIDispatcher(Windows::UI::Core::CoreDispatcher^ uiDispatcher) {
		if (m_soundDriver == nullptr)
			m_soundDriver = Sound::WinRT::AudioDriver::CreateInstanceRef(m_emulator, uiDispatcher);
		else
			m_soundDriver->SetUIDispatcher(uiDispatcher);
	}

	void NesSystemWrapperWinRT::SetAudioOutputVolume(float gain) {
		m_soundDriver->SetOutputVolume(gain);
	}

	void NesSystemWrapperWinRT::EnableAudioInput(bool enable) {
		m_soundDriver->EnableInput(enable);
	}

	void NesSystemWrapperWinRT::EnableAudioOutput(bool enable) {
		m_soundDriver->EnableOutput(enable);
	}
}