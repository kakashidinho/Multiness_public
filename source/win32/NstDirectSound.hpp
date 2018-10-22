////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2003-2008 Martin Freij
//
// This file is part of Nestopia.
//
// Nestopia is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Nestopia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Nestopia; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef NST_DIRECTX_DIRECTSOUND_H
#define NST_DIRECTX_DIRECTSOUND_H

#pragma once

#include <vector>
#include "NstObjectPod.hpp"
#include "NstDirectX.hpp"

#if NST_MSVC >= 1200
#pragma warning( push )
#pragma warning( disable : 4201 )
#endif

#include <MMSystem.h>

#if NST_MSVC >= 1200
#pragma warning( pop )
#endif

#include <dsound.h>

namespace Nestopia
{
	namespace DirectX
	{
		class DirectSound
		{
		public:

			explicit DirectSound(HWND);
			~DirectSound();

			enum Channels
			{
				MONO = 1,
				STEREO
			};

			enum Pool
			{
				POOL_HARDWARE,
				POOL_SYSTEM
			};

			struct Adapter : BaseAdapter
			{
				Adapter();

				bool buggy;
			};

			typedef std::vector<Adapter> Adapters;

			cstring Update(uint,uint,uint,Channels,uint,Pool,bool);
			void Destroy();
			void StopStream();

		private:

			class SoundAdapters
			{
				static BOOL CALLBACK Enumerator(LPGUID,LPCTSTR,LPCTSTR,LPVOID);

			public:

				SoundAdapters();

				Adapters list;
			};

			struct Device : ComInterface<IDirectSound8>
			{
				explicit Device(HWND);

				HWND const hWnd;
				ushort id;
				bool priority;
				bool buggy;
			};

			template <class DeviceType, class BufferType>
			class BaseBuffer
			{
			public:

				BaseBuffer();
				~BaseBuffer();

				cstring Update(DeviceType&, bool, uint, uint, Channels, uint, Pool, bool);
				virtual void StartStream() = 0;
				void StopStream(DeviceType*, bool);
				void Release();

			protected:

				virtual cstring Create(DeviceType&, bool) = 0;

				enum
				{
					DC_OFFSET_8 = 0x80,
					DC_OFFSET_16 = 0x0000
				};

				struct Com : ComInterface<BufferType>
				{
					Com();

					uint writePos;
				};

				struct Settings
				{
					Settings();

					uint size;
					Pool pool;
					bool globalFocus;
				};

				Com com;
				Object::Pod<WAVEFORMATEX> waveFormat;
				Settings settings;

			public:

				const WAVEFORMATEX& GetWaveFormat() const
				{
					return waveFormat;
				}

				bool GlobalFocus() const
				{
					return settings.globalFocus;
				}

				NST_FORCE_INLINE bool LockStream(void** data, uint* size)
				{
					DWORD pos;
					HRESULT hr;
					if (SUCCEEDED(hr = com->GetCurrentPosition(&pos, NULL)))
					{
						pos = (pos > com.writePos ? pos - com.writePos : pos + settings.size - com.writePos);

						DWORD bytes[2];

						if (SUCCEEDED(hr = com->Lock(com.writePos, pos, data + 0, bytes + 0, data + 1, bytes + 1, 0)))
						{
							size[0] = bytes[0] / waveFormat.nBlockAlign;
							size[1] = bytes[1] / waveFormat.nBlockAlign;

							return true;
						}
					}

					com->Stop();

					NST_DEBUG_MSG("DirectSound::BaseBuffer::Lock() failed!");

					return false;
				}

				NST_FORCE_INLINE void UnlockStream(void** data, uint* size)
				{
					NST_ASSERT(data && com);
					auto size1InBytes = size[0] * waveFormat.nBlockAlign;
					auto size2InBytes = size[1] * waveFormat.nBlockAlign;
					com->Unlock(data[0], size1InBytes, data[1], size2InBytes);

					com.writePos = (com.writePos + size1InBytes + size2InBytes) % settings.size;
				}
			};

			class Buffer: public BaseBuffer<IDirectSound8, IDirectSoundBuffer8>
			{
			private:

				virtual cstring Create(IDirectSound8&,bool) override;

			public:
				
				virtual void StartStream() override;

				bool Streaming() const
				{
					DWORD status;

					return
					(
						com && SUCCEEDED(com->GetStatus( &status )) &&
						(status & (DSBSTATUS_BUFFERLOST|DSBSTATUS_PLAYING|DSBSTATUS_LOOPING)) == (DSBSTATUS_PLAYING|DSBSTATUS_LOOPING)
					);
				}
			};

			class CaptureBuffer : public BaseBuffer<IDirectSoundCapture8, IDirectSoundCaptureBuffer8>
			{
			private:

				virtual cstring Create(IDirectSoundCapture8&, bool) override;

			public:

				virtual void StartStream() override;
			};

			Device device;
			Buffer buffer;
			const SoundAdapters adapters;

			//LHQ
			ComInterface<IDirectSoundCapture8> captureDevice;
			CaptureBuffer captureBuffer;
			//end LHQ
		public:

			bool Streaming() const
			{
				return buffer.Streaming();
			}

			void StartStream()
			{
				buffer.StartStream();
			}

			NST_FORCE_INLINE bool LockStream(void** data,uint* size)
			{
				return buffer.LockStream( data, size );
			}

			NST_FORCE_INLINE void UnlockStream(void** data,uint* size)
			{
				buffer.UnlockStream( data, size );
			}

			uint ReadInput(void* samples, uint maxSamples);

			const Adapters& GetAdapters() const
			{
				return adapters.list;
			}

			const WAVEFORMATEX& GetWaveFormat() const
			{
				return buffer.GetWaveFormat();
			}

			bool GlobalFocus() const
			{
				return buffer.GlobalFocus();
			}
		};


		//template implementation
		template<class DeviceType, class BufferType>
		DirectSound::BaseBuffer<DeviceType, BufferType>::Com::Com()
			: writePos(0) {}

		template<class DeviceType, class BufferType>
		DirectSound::BaseBuffer<DeviceType, BufferType>::Settings::Settings()
			: size(0), pool(POOL_HARDWARE), globalFocus(false) {}

		template<class DeviceType, class BufferType>
		DirectSound::BaseBuffer<DeviceType, BufferType>::BaseBuffer()
		{
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		}

		template<class DeviceType, class BufferType>
		DirectSound::BaseBuffer<DeviceType, BufferType>::~BaseBuffer()
		{
			Release();
		}

		template<class DeviceType, class BufferType>
		void DirectSound::BaseBuffer<DeviceType, BufferType>::Release()
		{
			if (com)
			{
				com->Stop();
				com.Release();
			}
		}

		template<class DeviceType, class BufferType>
		cstring DirectSound::BaseBuffer<DeviceType, BufferType>::Update
			(
				DeviceType& device,
				bool priority,
				uint rate,
				uint bits,
				Channels channels,
				uint latency,
				Pool pool,
				bool globalFocus
				)
		{
			const uint size = rate * latency / 1000 * (bits / 8 * channels);

			if
				(
					com &&
					waveFormat.nSamplesPerSec == rate &&
					waveFormat.wBitsPerSample == bits &&
					waveFormat.nChannels == channels &&
					settings.size == size &&
					settings.pool == pool &&
					settings.globalFocus == globalFocus
					)
				return NULL;

			waveFormat.nSamplesPerSec = rate;
			waveFormat.wBitsPerSample = bits;
			waveFormat.nChannels = channels;
			waveFormat.nBlockAlign = waveFormat.wBitsPerSample / 8 * waveFormat.nChannels;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

			settings.size = size;
			settings.pool = pool;
			settings.globalFocus = globalFocus;

			return Create(device, priority);
		}

		template<class DeviceType, class BufferType>
		void DirectSound::BaseBuffer<DeviceType, BufferType>::StopStream(DeviceType* device, bool priority)
		{
			if (com)
			{
				if (device)
					Create(*device, priority);
				else
					com->Stop();
			}
		}
	}
}

#endif
