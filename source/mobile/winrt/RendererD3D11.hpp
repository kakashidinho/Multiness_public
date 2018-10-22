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
#include "../../core/api/NstApiEmulator.hpp"
#include "../../core/api/NstApiVideo.hpp"

#include "../Renderer.hpp"
#include "TextureD3D11.hpp"
#include "MutableTextureD3D11.hpp"

namespace Nes {
	namespace Video {
		namespace D3D11 {
			class Renderer : public IRenderer, public D3DDeviceWrapper
			{
			public:
				Renderer(Core::Machine& emulator);
				~Renderer();

				//this should be called after D3D is restored
				void ResetD3D(ID3D11Device2* d3dDevice, ID3D11DeviceContext2* d3dDeviceContext, const DirectX::XMFLOAT4X4& orientationMatrix);

				virtual void Invalidate() override;//this should be called when D3D context lost
				virtual void Cleanup() override;//cleanup graphics resources

				unsigned int GetVideoMinX() const { return (unsigned int)m_videoX; }
				unsigned int GetVideoMinY() const { return (unsigned int)m_videoY; }

				virtual void DrawRect(ITexture& texture, float x, float y, float width, float height) override;
				
				virtual void PresentVideo() override;
			private:
				virtual void ResetImpl() override;

				void DrawRect(float x, float y, float width, float height);

				virtual bool VideoLock(Core::Video::Output& video) override;
				virtual void VideoUnlock(Core::Video::Output& video) override;

				DirectX::XMFLOAT4X4 m_orientationMatrix;

				float m_videoX, m_videoY, m_videoWidth, m_videoHeight;

				std::shared_ptr<MutableTexture> m_videoTexture;

				ComWrapper<ID3D11Buffer> m_renderTransformUniformBuf;
				ComWrapper<ID3D11Buffer> m_renderOrientationUniformBuf;
				ComWrapper<ID3D11VertexShader> m_quadRenderVS;
				ComWrapper<ID3D11PixelShader> m_quadRenderPS;
				ComWrapper<ID3D11Buffer> m_quadRenderVB;
				ComWrapper<ID3D11InputLayout> m_quadRenderVI;

				ComWrapper<ID3D11RasterizerState> m_rasterizerState;
				ComWrapper<ID3D11BlendState> m_blendState;
				ComWrapper<ID3D11DepthStencilState> m_depthState;
			};
		}
	}
}