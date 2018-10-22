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

#include "Direct3D11Common.hpp"

#include "../../core/NstLog.hpp"

namespace Nes {
	namespace Video {
		namespace D3D11 {

#define NULL_D3D_HANDLE(func_name) \
	if (!m_d3dDeviceContext) \
	{ \
		Core::Log() << "Called " #func_name "() with invalid d3d device context"; \
		return; \
	}\

			D3DDeviceWrapper::D3DDeviceWrapper(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext)
				: m_d3dDevice(d3dDevice), m_d3dDeviceContext(d3dDeviceContext), m_currentPrimitiveMode(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
			{}

			void D3DDeviceWrapper::InvalidateStateCache() {
				m_currentSamplerState.SafeRelease();
				m_currentVS.SafeRelease();
				m_currentPS.SafeRelease();

				m_currentVB.SafeRelease();
				m_currentVI.SafeRelease();

				for (auto& currentCB : m_currentCB)
					currentCB.SafeRelease();

				m_currentBlendState.SafeRelease();
				m_currentRasterState.SafeRelease();
				m_currentDepthState.SafeRelease();

				m_currentPrimitiveMode = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
			}

			void D3DDeviceWrapper::ResetD3D(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext) {
				m_d3dDevice = d3dDevice;
				m_d3dDeviceContext = d3dDeviceContext;

				InvalidateStateCache();
			}

			void D3DDeviceWrapper::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY mode) {
				NULL_D3D_HANDLE(IASetPrimitiveTopology);

				if (m_currentPrimitiveMode != mode) {
					m_currentPrimitiveMode = mode;

					m_d3dDeviceContext->IASetPrimitiveTopology(mode);
				}
			}

			void D3DDeviceWrapper::IASetInputLayout(ComWrapper<ID3D11InputLayout> layout) {
				NULL_D3D_HANDLE(IASetInputLayout);
				
				if (m_currentVI != layout) {
					m_currentVI = layout;

					if (layout)
						m_d3dDeviceContext->IASetInputLayout(layout);
				}
			}

			void D3DDeviceWrapper::IASetVertexBuffer(ComWrapper<ID3D11Buffer> VB, UINT stride)//set vertex buffer at slot 0
			{
				NULL_D3D_HANDLE(IASetVertexBuffer);

				if (m_currentVB != VB) {
					m_currentVB = VB;

					UINT offset = 0;

					if (VB)
						m_d3dDeviceContext->IASetVertexBuffers(0, 1, &m_currentVB, &stride, &offset);
				}
			}

			void D3DDeviceWrapper::VSSetConstantBuffer(UINT slot, ComWrapper<ID3D11Buffer> CB)
			{
				NULL_D3D_HANDLE(VSSetConstantBuffer);

				const auto numCachedCBs = sizeof(m_currentCB) / sizeof(m_currentCB[0]);
				if (slot < numCachedCBs)
				{
					if (m_currentCB[slot] != CB) {
						m_currentCB[slot] = CB;//avoid duplicated state changes

						m_d3dDeviceContext->VSSetConstantBuffers(slot, 1, &CB);
					}//if (m_currentCB[slot] != CB)
				}
				else {
					m_d3dDeviceContext->VSSetConstantBuffers(slot, 1, &CB);
				}
			}

			void D3DDeviceWrapper::VSSetShader(ComWrapper<ID3D11VertexShader> VS) {
				NULL_D3D_HANDLE(VSSetShader);

				if (m_currentVS != VS) {
					m_currentVS = VS;
					if (VS)
						m_d3dDeviceContext->VSSetShader(VS, NULL, 0);
				}
			}

			void D3DDeviceWrapper::PSSetShader(ComWrapper<ID3D11PixelShader> PS) {
				NULL_D3D_HANDLE(PSSetShader);

				if (m_currentPS != PS) {
					m_currentPS = PS;
					if (PS)
						m_d3dDeviceContext->PSSetShader(PS, NULL, 0);
				}
			}

			void D3DDeviceWrapper::PSSetSamplerState(ComWrapper<ID3D11SamplerState> samplerState) {
				NULL_D3D_HANDLE(PSSetSamplerState);

				if (m_currentSamplerState != samplerState)
				{
					m_currentSamplerState = samplerState;

					if (samplerState)
						m_d3dDeviceContext->PSSetSamplers(0, 1, &samplerState);
				}
			}

			void D3DDeviceWrapper::PSSetShaderTexture(ComWrapper<ID3D11ShaderResourceView> textureView) {
				NULL_D3D_HANDLE(PSSetShaderTexture);

				if (textureView)
					m_d3dDeviceContext->PSSetShaderResources(0, 1, &textureView);
			}

			void D3DDeviceWrapper::OMSetBlendState(ComWrapper<ID3D11BlendState> blendState) {
				NULL_D3D_HANDLE(OMSetBlendState);

				if (m_currentBlendState != blendState) {
					m_currentBlendState = blendState;

					m_d3dDeviceContext->OMSetBlendState(blendState, NULL, 0xffffffff);
				}
			}

			void D3DDeviceWrapper::RSSetState(ComWrapper<ID3D11RasterizerState> rasterState) {
				NULL_D3D_HANDLE(RSSetState);

				if (m_currentRasterState != rasterState) {
					m_currentRasterState = rasterState;

					m_d3dDeviceContext->RSSetState(rasterState);
				}
			}

			void D3DDeviceWrapper::OMSetDepthStencilState(ComWrapper<ID3D11DepthStencilState> depthState) {
				NULL_D3D_HANDLE(OMSetDepthStencilState);

				if (m_currentDepthState != depthState) {
					m_currentDepthState = depthState;

					m_d3dDeviceContext->OMSetDepthStencilState(depthState, 0);
				}
			}
		}
	}
}