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

#include "TextureD3D11.hpp"

#include "../../core/NstLog.hpp"

#include <ImagesLoader/Bitmap.h>

#include <stdexcept>

namespace Nes {
	namespace Video {
		namespace D3D11 {
			/*------- BaseTexture ------------*/
			BaseTexture::BaseTexture(D3DDeviceWrapper& d3dWrapper)
				: D3DObject(d3dWrapper)
			{
			}

			BaseTexture::~BaseTexture() {
				ReleaseTextureResources();
			}

			void BaseTexture::BindTextureToPipeline() {
				m_d3dWrapper.PSSetShaderTexture(m_textureView);
				m_d3dWrapper.PSSetSamplerState(m_samplerState);
			}

			void BaseTexture::ReleaseTextureResources() {
				SafeReleaseWrappedObject();
				m_textureView.SafeRelease();
				m_samplerState.SafeRelease();
			}

			void BaseTexture::CreateTextureResources(const D3D11_TEXTURE2D_DESC& t2dDesc, const D3D11_SUBRESOURCE_DATA* initData, const D3D11_SAMPLER_DESC& t2dSamplerDesc) {
				ReleaseTextureResources();

				HRESULT hr;

				//create texture
				if (!SUCCEEDED(hr = m_d3dWrapper.GetD3DDevice()->CreateTexture2D(&t2dDesc, initData, &m_d3dWrappedObject)))
				{
					Core::Log() << "BaseTexture: Failed to create shader resource view";
					return;
				}

				//create shader resource view
				D3D11_SHADER_RESOURCE_VIEW_DESC t2dViewDesc;
				t2dViewDesc.Format = t2dDesc.Format;
				t2dViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				t2dViewDesc.Texture2D.MipLevels = t2dDesc.MipLevels;
				t2dViewDesc.Texture2D.MostDetailedMip = 0;

				if (!SUCCEEDED(hr = m_d3dWrapper.GetD3DDevice()->CreateShaderResourceView(m_d3dWrappedObject.Get(), &t2dViewDesc, &m_textureView)))
				{
					Core::Log() << "BaseTexture: Failed to create shader resource view";
					return;
				}

				//create sampler state
				if (!SUCCEEDED(hr = m_d3dWrapper.GetD3DDevice()->CreateSamplerState(&t2dSamplerDesc, &m_samplerState)))
				{
					Core::Log() << "BaseTexture: Failed to create sampler state";
					return;
				}
			}

			/*---------- StaticTexture --------------*/
			StaticTexture::StaticTexture(D3DDeviceWrapper& d3dWrapper)
				: BaseTexture(d3dWrapper)
			{
			}

			StaticTexture::~StaticTexture() {
				Cleanup();
			}

			void StaticTexture::Cleanup()
			{
				ReleaseTextureResources();
			}

			bool StaticTexture::IsInvalid() const {
				return !m_d3dWrappedObject;
			}

			void StaticTexture::Invalidate()//should be called when GL/DX context lost
			{
				Cleanup();
			}

			void StaticTexture::Reset(const char* file, Callback::OpenFileCallback resourceLoader)
			{
				Cleanup();

				if (resourceLoader == nullptr)
					return;

				Core::Log() << "Loading texture from " << file;
				
				auto stream = resourceLoader(file);

				Init(*stream);

				stream->Release();
			}

			void StaticTexture::ResetIfInvalid(const char* file, Callback::OpenFileCallback resourceLoader)
			{
				if (IsInvalid())
				{
					Reset(file, resourceLoader);
				}
			}

			void StaticTexture::Init(HQDataReaderStream& stream)
			{
				Bitmap loader;
				loader.SetLoadedOutputRGBLayout(LAYOUT_RGB);
				loader.SetLoadedOutputRGB16Layout(LAYOUT_RGB);

				if (loader.LoadFromStream(&stream) != IMG_OK)
					throw std::runtime_error("Cannot load texture");

				loader.SetPixelOrigin(ORIGIN_TOP_LEFT);
				auto format = loader.GetSurfaceFormat();

				//texture desc
				D3D11_TEXTURE2D_DESC t2dDesc;
				
				switch (format)
				{
				case FMT_A8B8G8R8: case FMT_A8R8G8B8:
					t2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					break;
				case FMT_B8G8R8: case FMT_R8G8B8:
					loader.RGB24ToRGBA();
					t2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					break;
				case FMT_R5G6B5:
					t2dDesc.Format = DXGI_FORMAT_B5G6R5_UNORM;
					break;
				case FMT_A8L8:
					loader.AL16ToRGBA();
					t2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					break;
				default:
					throw std::runtime_error("Cannot load texture. Unsupported format.");
				}

				t2dDesc.Usage = D3D11_USAGE_DEFAULT;
				t2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				t2dDesc.Width = loader.GetWidth();
				t2dDesc.Height = loader.GetHeight();
				t2dDesc.MiscFlags = 0;
				t2dDesc.ArraySize = 1;
				t2dDesc.CPUAccessFlags = 0;
				t2dDesc.SampleDesc.Count = 1;
				t2dDesc.SampleDesc.Quality = 0;

				//get number of mipmaps
				loader.GenerateMipmaps();

				auto imageExtInfo = loader.GetSurfaceComplex();
				if (imageExtInfo.dwComplexFlags & SURFACE_COMPLEX_MIPMAP)
					t2dDesc.MipLevels = imageExtInfo.nMipMap;
				else
					t2dDesc.MipLevels = 1;

				//sampler state
				D3D11_SAMPLER_DESC t2dSamplerDesc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());

				t2dSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
				t2dSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
				t2dSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

				t2dSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

				//create texture resources
				CreateTextureResources(t2dDesc, NULL, t2dSamplerDesc);

				//copy data from bitmap to texture
				auto w = loader.GetWidth();
				auto h = loader.GetHeight();
				auto ptr = loader.GetPixelData();
				for (UINT level = 0; level < t2dDesc.MipLevels; ++level) {
					auto rowSize = loader.CalculateRowSize(w);
					auto lvlSize = loader.CalculateSize(w, h);

					m_d3dWrapper.GetD3DDeviceContext()->UpdateSubresource(m_d3dWrappedObject, level, NULL, ptr, rowSize, 0);

					if (w > 1)
						w >>= 1; //w/=2
					if (h > 1)
						h >>= 1; //h/=2

					ptr += lvlSize;
				}
			}

			void StaticTexture::BindTexture() {
				BindTextureToPipeline();
			}
		}
	}
}