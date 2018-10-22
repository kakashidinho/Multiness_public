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

#include "WinRTUtils.hpp"

#include <d3d11_2.h>
#include <DirectXMath.h>

#include <HQSharedPointer.h>

namespace Nes {
	namespace Video {
		namespace D3D11 {
			template <class T>
			using ComWrapper = WinRT::Utils::ComWrapper<T>;

			/*------- D3DDeviceWrapper ---------*/
			class D3DDeviceWrapper {
			public:
				D3DDeviceWrapper(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext);

				void InvalidateStateCache();
				void ResetD3D(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext);

				ID3D11Device2* GetD3DDevice() { return m_d3dDevice; }
				ID3D11DeviceContext2* GetD3DDeviceContext() { return m_d3dDeviceContext; }

				void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY mode);
				void IASetInputLayout(ComWrapper<ID3D11InputLayout> layout);
				void IASetVertexBuffer(ComWrapper<ID3D11Buffer> VB, UINT stride);//set vertex buffer at slot 0

				void VSSetConstantBuffer(UINT slot, ComWrapper<ID3D11Buffer> CB);//set constant buffer for vertex shader

				void VSSetShader(ComWrapper<ID3D11VertexShader> VS);
				void PSSetShader(ComWrapper<ID3D11PixelShader> PS);
				void PSSetShaderTexture(ComWrapper<ID3D11ShaderResourceView> textureView);//set shader's texture resource view at slot 0
				void PSSetSamplerState(ComWrapper<ID3D11SamplerState> samplerState);//set sampler state at slot 0

				void RSSetState(ComWrapper<ID3D11RasterizerState> rasterState);
				void OMSetBlendState(ComWrapper<ID3D11BlendState> blendState);
				void OMSetDepthStencilState(ComWrapper<ID3D11DepthStencilState> depthState);
			protected:
				ComWrapper<ID3D11SamplerState> m_currentSamplerState;
				ComWrapper<ID3D11VertexShader> m_currentVS;
				ComWrapper<ID3D11PixelShader> m_currentPS;
				ComWrapper<ID3D11Buffer> m_currentVB;//current vertex buffer at slot 0
				ComWrapper<ID3D11Buffer> m_currentCB[2];//current constant buffer at slot 0
				ComWrapper<ID3D11InputLayout> m_currentVI;//current vertex input layout

				ComWrapper<ID3D11BlendState> m_currentBlendState;
				ComWrapper<ID3D11RasterizerState> m_currentRasterState;
				ComWrapper<ID3D11DepthStencilState> m_currentDepthState;

				D3D11_PRIMITIVE_TOPOLOGY m_currentPrimitiveMode;

				ID3D11Device2* m_d3dDevice;
				ID3D11DeviceContext2* m_d3dDeviceContext;
			};

			/*--------- D3DObject --------------*/
			template <class T>
			class D3DObject {
			public:
				D3DObject(D3DDeviceWrapper& d3dWrapper)
					: m_d3dWrapper(d3dWrapper)
				{}

				virtual ~D3DObject() {
					SafeReleaseWrappedObject();
				}

				void SafeReleaseWrappedObject() {
					m_d3dWrappedObject.SafeRelease();
				}
			protected:
				D3DDeviceWrapper& m_d3dWrapper;
				ComWrapper<T> m_d3dWrappedObject;
			};
		}
	}
}
