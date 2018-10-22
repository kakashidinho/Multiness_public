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

#include "MutableTextureD3D11.hpp"

#include "../../core/NstLog.hpp"

#define USE_SHADOW_BUFFER 0

namespace Nes {
	namespace Video {
		namespace D3D11 {
			static UINT CalculateDefaultPitch(UINT width) {
				return width * 4;//32 bit RGBA
			}

			MutableTexture::MutableTexture(D3DDeviceWrapper& d3dWrapper, unsigned int width, unsigned int height)
				:BaseTexture(d3dWrapper), m_width(width), m_height(height), m_locked(false)
			{
#if USE_SHADOW_BUFFER
				try {
					m_shadowBuffer.resize(width * height * 4);//RGBA data
				}
				catch (...) {
					Core::Log() << "MutableTexture: Failed to allocate shadow buffer";

					return;
				}
#endif

				//texture desc
				D3D11_TEXTURE2D_DESC t2dDesc;

				t2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				t2dDesc.Usage = D3D11_USAGE_DYNAMIC;
				t2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				t2dDesc.Width = width;
				t2dDesc.Height = height;
				t2dDesc.MiscFlags = 0;
				t2dDesc.ArraySize = 1;
				t2dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				t2dDesc.MipLevels = 1;
				t2dDesc.SampleDesc.Count = 1;
				t2dDesc.SampleDesc.Quality = 0;

				//sampler state
				D3D11_SAMPLER_DESC t2dSamplerDesc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());

				t2dSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
				t2dSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
				t2dSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

				t2dSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

				CreateTextureResources(t2dDesc, NULL, t2dSamplerDesc);
			}

			MutableTexture::~MutableTexture() {

			}

			void MutableTexture::BindTexture()//this must be called after Unlock()
			{
				if (m_locked)
					throw std::runtime_error("MutableTexture: Cannot bind texture when it is locked");
				BindTextureToPipeline();
			}

			void* MutableTexture::Lock(UINT &pitch) {
				//let application write to shadow buffer first
				if (!m_d3dWrappedObject)
					return NULL;

				m_locked = true;

#if USE_SHADOW_BUFFER
				pitch = CalculateDefaultPitch(m_width);

				return m_shadowBuffer.data();
#else//USE_SHADOW_BUFFER
				D3D11_MAPPED_SUBRESOURCE mappedData;

				auto re = m_d3dWrapper.GetD3DDeviceContext()->Map(m_d3dWrappedObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);

				if (!SUCCEEDED(re))
					return NULL;

				pitch = mappedData.RowPitch != 0 ? mappedData.RowPitch : CalculateDefaultPitch(m_width);

				return mappedData.pData;
#endif//USE_SHADOW_BUFFER
			}
			void MutableTexture::Unlock() {
				if (!m_locked)
					return;

				m_locked = false;

#if USE_SHADOW_BUFFER
				//copy from shadow buffer to d3d texture
				D3D11_MAPPED_SUBRESOURCE mappedData;

				auto re = m_d3dWrapper.GetD3DDeviceContext()->Map(m_d3dWrappedObject.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);

				if (!SUCCEEDED(re))
					return;

				auto src_row_size = CalculateDefaultPitch(m_width);

				auto dst_ptr = (unsigned char*)mappedData.pData;
				auto src_ptr = m_shadowBuffer.data();

				if (mappedData.RowPitch == 0)
					memcpy(mappedData.pData, src_ptr, m_shadowBuffer.size());
				else {
					//copy row by row
					for (unsigned int r = 0; r < m_height; ++r)
					{
						memcpy(dst_ptr, src_ptr, mappedData.RowPitch);
						dst_ptr += mappedData.RowPitch;
						src_ptr += src_row_size;
					}
				}
#endif//#if USE_SHADOW_BUFFER

				m_d3dWrapper.GetD3DDeviceContext()->Unmap(m_d3dWrappedObject, 0);
			}

			void MutableTexture::Invalidate()//this should be call when D3D context lost
			{
				ReleaseTextureResources();
			}

			bool MutableTexture::IsInvalid() const {
				return !m_d3dWrappedObject;
			}
		}
	}
}