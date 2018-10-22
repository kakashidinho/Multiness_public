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

#include "Direct3D11Common.hpp"
#include "AudioDriver.hpp"

#include "../NesSystemWrapper.hpp"

namespace  Nes {
	class NesSystemWrapperWinRT: public NesSystemWrapper {
	public:
		NesSystemWrapperWinRT(std::istream* dbStream);
		~NesSystemWrapperWinRT();

		//thread safe
		void SetUIDispatcher(Windows::UI::Core::CoreDispatcher^ uiDispatcher);
		//thread safe
		void SetAudioOutputVolume(float gain);
		//thread safe
		void EnableAudioInput(bool enable);
		//thread safe
		void EnableAudioOutput(bool enable);

		void ResetD3D(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext, const DirectX::XMFLOAT4X4& orientationMatrix);
	private:
		std::shared_ptr<Sound::WinRT::AudioDriver> m_soundDriver;
	};
}