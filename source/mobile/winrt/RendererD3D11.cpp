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

#include "RendererD3D11.hpp"
#include "WinRTUtils.hpp"

#include "../../core/NstLog.hpp"

#include <directxmath.h>

#ifndef max
#	define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#	define min(a,b) ((a) < (b) ? (a) : (b))
#endif

using namespace DirectX;

namespace Nes
{
	namespace Video {
		namespace D3D11 {
			typedef DirectX::XMFLOAT4 Vertex;

			typedef DirectX::XMFLOAT4 TransformConstantBuffer;
			typedef DirectX::XMFLOAT4X4 OrientationConstantBuffer;

			/*-------  Renderer -----------*/
			Renderer::Renderer(Core::Machine& emulator)
				: D3DDeviceWrapper(nullptr, nullptr), m_videoTexture(nullptr)
			{
				//setup render state
				Api::Video::RenderState renderstate;

				renderstate.filter = Api::Video::RenderState::FILTER_NONE;
				renderstate.width = Core::Video::Output::WIDTH;
				renderstate.height = Core::Video::Output::HEIGHT;
				renderstate.bits.count = 32;

				renderstate.bits.mask.r = 0x00ff0000;
				renderstate.bits.mask.g = 0x0000ff00;
				renderstate.bits.mask.b = 0x000000ff;

				Api::Video(emulator).SetRenderState(renderstate);
			}

			Renderer::~Renderer() {
				Cleanup();
			}

			void Renderer::ResetD3D(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext, const DirectX::XMFLOAT4X4& orientationMatrix) {
				bool oldResourcesInvalid = d3dDevice != GetD3DDevice() || d3dDeviceContext != GetD3DDeviceContext();

				D3DDeviceWrapper::ResetD3D(d3dDevice, d3dDeviceContext);

				//need to transpose before copying to constant buffer
				auto simdOrientation = XMLoadFloat4x4(&orientationMatrix);
				simdOrientation = XMMatrixTranspose(simdOrientation);

				XMStoreFloat4x4(&m_orientationMatrix, simdOrientation);

				if (oldResourcesInvalid)
					Cleanup();
				else {
					if (m_renderOrientationUniformBuf)
						GetD3DDeviceContext()->UpdateSubresource(m_renderOrientationUniformBuf, 0, NULL, &m_orientationMatrix, 0, 0);
				}
			}

			void Renderer::Cleanup()
			{
				m_renderTransformUniformBuf.SafeRelease();
				m_renderOrientationUniformBuf.SafeRelease();
				m_quadRenderVS.SafeRelease();
				m_quadRenderPS.SafeRelease();
				m_quadRenderVB.SafeRelease();
				m_quadRenderVI.SafeRelease();

				m_rasterizerState.SafeRelease();
				m_blendState.SafeRelease();
				m_depthState.SafeRelease();

				m_videoTexture = nullptr;

				InvalidateStateCache();
			}

			void Renderer::Invalidate()
			{
				Core::Log() << "Renderer::Invalidate() called\n";

				Cleanup();
			}

			void Renderer::ResetImpl()
			{
				HRESULT hr;

				if (m_videoTexture == nullptr || m_videoTexture->IsInvalid())
				{
					m_videoTexture = std::make_shared<MutableTexture>(*this, Core::Video::Output::WIDTH, Core::Video::Output::HEIGHT);
					Core::Log() << "Renderer created video texture\n";
				}

				//create shader program
				if (!m_quadRenderVS)
				{
					std::vector<unsigned char> vsData;
					if (WinRT::Utils::ReadResourceFile("QuadVertexShader.cso", vsData)) {
						if (SUCCEEDED(hr = GetD3DDevice()->CreateVertexShader(vsData.data(), vsData.size(), NULL, &m_quadRenderVS)))
						{
							Core::Log() << "Renderer created vertex shader\n";
						}
						else
							Core::Log() << "CreateVertexShader() error=" << hr;

						//create vertex layout
						const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
						{
							{ "POSITION_TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
						};

						if (m_quadRenderVI)
							m_quadRenderVI.SafeRelease();

						if (SUCCEEDED(hr = GetD3DDevice()->CreateInputLayout(vertexDesc, 1, vsData.data(), vsData.size(), &m_quadRenderVI)))
						{
							Core::Log() << "Renderer created vertex input layout\n";
						}
						else
							Core::Log() << "CreateInputLayout() error=" << hr;
					}
				}//if (!m_quadRenderVS)

				if (!m_quadRenderPS)
				{
					std::vector<unsigned char> vsData;
					if (WinRT::Utils::ReadResourceFile("QuadPixelShader.cso", vsData)) {
						if (SUCCEEDED(hr = GetD3DDevice()->CreatePixelShader(vsData.data(), vsData.size(), NULL, &m_quadRenderPS)))
						{
							Core::Log() << "Renderer created pixel shader\n";
						}
						else
							Core::Log() << "CreatePixelShader() error=" << hr;
					}
				}//if (!m_quadRenderPS)

				 //create general purpose quad buffer
				if (!m_quadRenderVB)
				{
					Vertex vertices[] = {
						{ 0, 0, 0, 1 },
						{ 1, 0, 1, 1 },
						{ 0, 1, 0, 0 },
						{ 1, 1, 1, 0 },
					};

					D3D11_BUFFER_DESC vbd;
					vbd.Usage = D3D11_USAGE_DEFAULT;
					vbd.CPUAccessFlags = 0;
					vbd.ByteWidth = sizeof(vertices);
					vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
					vbd.MiscFlags = 0;
					vbd.StructureByteStride = 0;

					D3D11_SUBRESOURCE_DATA initData;
					initData.pSysMem = vertices;

					if (SUCCEEDED(hr = GetD3DDevice()->CreateBuffer(&vbd, &initData, &m_quadRenderVB)))
					{
						Core::Log() << "Renderer created quad buffer\n";
					}
					else 
						Core::Log() << "CreateBuffer(vertex buffer) error=" << hr;
				}//if (!m_quadRenderVB)

				//create constant buffers
				if (!m_renderTransformUniformBuf)
				{
					CD3D11_BUFFER_DESC constantBufferDesc(sizeof(TransformConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
					if (SUCCEEDED(hr = GetD3DDevice()->CreateBuffer(&constantBufferDesc, NULL, &m_renderTransformUniformBuf)))
					{
						Core::Log() << "Renderer created transform constant buffer\n";
					}
					else
						Core::Log() << "CreateBuffer(transform constant buffer) error=" << hr;
				}//if (!m_renderTransformUniformBuf)

				if (!m_renderOrientationUniformBuf)
				{
					CD3D11_BUFFER_DESC constantBufferDesc(sizeof(OrientationConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
					if (SUCCEEDED(hr = GetD3DDevice()->CreateBuffer(&constantBufferDesc, NULL, &m_renderOrientationUniformBuf)))
					{
						Core::Log() << "Renderer created orientation constant buffer\n";
					}
					else
						Core::Log() << "CreateBuffer(orientation constant buffer) error=" << hr;

					GetD3DDeviceContext()->UpdateSubresource(m_renderOrientationUniformBuf, 0, NULL, &m_orientationMatrix, 0, 0);//the value of this constant buffer will remain unchanged until next reset
				}//if (!m_renderOrientationUniformBuf)

				//create rasterizer state
				if (!m_rasterizerState) {
					D3D11_RASTERIZER_DESC rasterDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
					rasterDesc.CullMode = D3D11_CULL_NONE;

					if (SUCCEEDED(hr = GetD3DDevice()->CreateRasterizerState(&rasterDesc, &m_rasterizerState)))
					{
						Core::Log() << "Renderer created rasterizer state\n";
					}
					else
						Core::Log() << "CreateRasterizerState() error=" << hr;
				}//if (!m_rasterizerState) 

				 //create blend state
				if (!m_blendState) {
					D3D11_BLEND_DESC blendDesc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());
					blendDesc.RenderTarget[0].BlendEnable = TRUE;
					blendDesc.RenderTarget[0].SrcBlend = blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
					blendDesc.RenderTarget[0].DestBlend = blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;

					if (SUCCEEDED(hr = GetD3DDevice()->CreateBlendState(&blendDesc, &m_blendState)))
					{
						Core::Log() << "Renderer created blend state\n";
					}
					else
						Core::Log() << "CreateBlendState() error=" << hr;
				}//if (!m_blendState) 

				//create depth stencil state
				if (!m_depthState)
				{
					D3D11_DEPTH_STENCIL_DESC depthStateDesc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
					depthStateDesc.DepthEnable = FALSE;
					depthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

					if (SUCCEEDED(hr = GetD3DDevice()->CreateDepthStencilState(&depthStateDesc, &m_depthState)))
					{
						Core::Log() << "Renderer created depth stencil state\n";
					}
					else
						Core::Log() << "CreateDepthStencilState() error=" << hr;
				}//if (!m_depthState)

				 //calculate video's screen region
				auto width = Core::Video::Output::WIDTH;
				auto height = Core::Video::Output::HEIGHT;

				auto widthRatio = (float)m_screenWidth / width;
				auto heightRatio = (float)m_screenHeight / height;

				auto ratio = min(widthRatio, heightRatio);

				m_videoWidth = width * ratio;
				m_videoHeight = height * ratio;
				m_videoX = 0.5f * (m_screenWidth - m_videoWidth);
				m_videoY = m_screenHeight - m_videoHeight;

				//set blending state & rasterizer state
				OMSetBlendState(m_blendState);
				RSSetState(m_rasterizerState);
				OMSetDepthStencilState(m_depthState);
			}

			void Renderer::DrawRect(ITexture& texture, float x, float y, float width, float height)
			{
				if (!GetD3DDeviceContext())
					return;

				//bind texture
				texture.BindTexture();

				//draw rect
				DrawRect(x, y, width, height);
			}

			void Renderer::DrawRect(float x, float y, float width, float height)
			{
				HRESULT hr;

				auto wScale = 2.f / m_screenWidth;
				auto hScale = 2.f / m_screenHeight;
				auto wScaled = wScale * width;
				auto hScaled = hScale * height;
				auto xNormalized = wScale * x - 1;
				auto yNormalized = hScale * y - 1;

				//bind VBO
				IASetInputLayout(m_quadRenderVI);
				IASetVertexBuffer(m_quadRenderVB, sizeof(Vertex));

				//use shader program
				VSSetShader(m_quadRenderVS);
				PSSetShader(m_quadRenderPS);

				VSSetConstantBuffer(0, m_renderTransformUniformBuf);
				VSSetConstantBuffer(1, m_renderOrientationUniformBuf);

				//update constant buffers
				if (m_renderTransformUniformBuf) {
					TransformConstantBuffer transform = { xNormalized, yNormalized, wScaled, hScaled };
					GetD3DDeviceContext()->UpdateSubresource(m_renderTransformUniformBuf, 0, NULL, &transform, 0, 0);
				}

				//draw
				IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				GetD3DDeviceContext()->Draw(4, 0);
			}
			
			void Renderer::PresentVideo() {
				if (!m_videoTexture)
					return;

				/*------- render the video texture  ----*/
				//bind texture
				m_videoTexture->BindTexture();
				
				
				OMSetBlendState(nullptr);//disable blending
				
				DrawRect(m_videoX, m_videoY, m_videoWidth, m_videoHeight);
				
				OMSetBlendState(m_blendState);//re-enable blending
			}

			bool Renderer::VideoLock(Core::Video::Output& video)
			{
				if (!m_videoTexture)
					return false;

				UINT pitch;
				auto pixels = m_videoTexture->Lock(pitch);

				if (!pixels)
					return false;

				video.pixels = pixels;
				video.pitch = pitch;

				return true;
			}

			void Renderer::VideoUnlock(Core::Video::Output& video)
			{
				if (m_videoTexture == nullptr)
					return;

				m_videoTexture->Unlock();
			}
		}
	}
}