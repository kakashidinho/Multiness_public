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

#include "NstIoLog.hpp"
#include "NstDirectSound.hpp"
#include <Shlwapi.h>

#include <algorithm>

#if NST_MSVC
#pragma comment(lib,"dsound")
#endif

namespace Nestopia
{
	namespace DirectX
	{
		DirectSound::Device::Device(HWND h)
		: hWnd(h), id(0), priority(false), buggy(false) {}

		DirectSound::Adapter::Adapter()
		: buggy(false) {}

		DirectSound::SoundAdapters::SoundAdapters()
		{
			Io::Log() << "DirectSound: initializing..\r\n";

			if (FAILED(::DirectSoundEnumerate( Enumerator, &list )) || list.empty())
				Enumerator( NULL, L"Primary Sound Driver", NULL, &list );

			bool report = true;

			for (Adapters::iterator it(list.begin()), end(list.end()); it != end; ++it)
			{
				if (::StrStrI( it->name.Ptr(), L"E-DSP Wave" ))
				{
					it->buggy = true;

					if (report)
					{
						report = false;
						Io::Log() << "DirectSound: warning, possibly buggy drivers!! activating stupid-mode..\r\n";
					}
				}
			}
		}

		DirectSound::DirectSound(HWND const hWnd)
		: device( hWnd ) {}

		DirectSound::~DirectSound()
		{
			Destroy();
		}

		void DirectSound::Destroy()
		{
			buffer.Release();
			device.Release();

			captureBuffer.Release();
			captureDevice.Release();
		}

		BOOL CALLBACK DirectSound::SoundAdapters::Enumerator(LPGUID const guid,LPCTSTR const desc,LPCTSTR,LPVOID const context)
		{
			Io::Log() << "DirectSound: enumerating device - name: "
                      << (desc && *desc ? desc : L"unknown")
                      << ", GUID: "
                      << (guid ? System::Guid( *guid ).GetString() : L"unspecified")
                      << "\r\n";

			ComInterface<IDirectSound8> device;

			if (SUCCEEDED(::DirectSoundCreate8( guid, &device, NULL )))
			{
				static_cast<Adapters*>(context)->resize( static_cast<Adapters*>(context)->size() + 1 );
				Adapter& adapter = static_cast<Adapters*>(context)->back();

				if (guid)
					adapter.guid = *guid;

				adapter.name = desc;
			}
			else
			{
				Io::Log() << "DirectSound: DirectSoundCreate8() failed on this device, continuing enumeration..\r\n";
			}

			return true;
		}

		cstring DirectSound::Update
		(
			const uint deviceId,
			const uint rate,
			const uint bits,
			const Channels channels,
			const uint latency,
			const Pool pool,
			const bool globalFocus
		)
		{
			NST_ASSERT( deviceId < adapters.list.size() );

			if (device.id != deviceId || !device || !captureDevice)
			{
				device.id = deviceId;

				Destroy();

				//create playback device
				if (FAILED(::DirectSoundCreate8( &adapters.list[device.id].guid, &device, NULL )))
					return "DirectSoundCreate8()";

				Io::Log() << "DirectSound: creating device #" << device.id << "\r\n";

				device.priority = SUCCEEDED(device->SetCooperativeLevel( device.hWnd, DSSCL_PRIORITY ));
				device.buggy = adapters.list[device.id].buggy;

				if (!device.priority)
				{
					Io::Log() << "DirectSound: warning, IDirectSound8::SetCooperativeLevel( DSSCL_PRIORITY ) failed! Retrying with DSSCL_NORMAL..\r\n";

					if (FAILED(device->SetCooperativeLevel( device.hWnd, DSSCL_NORMAL )))
					{
						device.Release();
						return "IDirectSound8::SetCooperativeLevel()";
					}
				}

				//create capture device
				if (FAILED(::DirectSoundCaptureCreate8(NULL, &captureDevice, NULL)))
					return "DirectSoundCaptureCreate8()";
			}

			if (cstring errMsg = buffer.Update( **device, device.priority, rate, bits, channels, latency, pool, globalFocus ))
			{
				Destroy();
				return errMsg;
			}

			//LHQ
			if (cstring errMsg = captureBuffer.Update(**captureDevice, device.priority, rate, bits, channels, latency, pool, globalFocus))
			{
				Destroy();
				return errMsg;
			}
			else
				captureBuffer.StartStream();
			//end LHQ

			return NULL;
		}

		void DirectSound::StopStream()
		{
			buffer.StopStream( device.buggy ? *device : NULL, device.priority );
		}

		//LHQ
		uint DirectSound::ReadInput(void* samples, uint maxSamples)
		{
			void* availData[2];
			uint availSizes[2];
			uint readSizes[2];
			uint maxSize = maxSamples * captureBuffer.GetWaveFormat().nBlockAlign;

			if (captureBuffer.LockStream(availData, availSizes))
			{
				if (samples == NULL)//return number of available samples without reading data
				{
					auto totalAvailSamples = availSizes[0] + availSizes[1];

					memset(availSizes, 0, sizeof(availSizes));
					captureBuffer.UnlockStream(availData, availSizes);

					return totalAvailSamples;
				}

				availSizes[0] *= captureBuffer.GetWaveFormat().nBlockAlign;
				availSizes[1] *= captureBuffer.GetWaveFormat().nBlockAlign;

				//copy region 1
				uint remainSize = maxSize;
				uint copySize = std::min(remainSize, availSizes[0]);
				memcpy(samples, availData[0], copySize);
				readSizes[0] = copySize;

				remainSize -= copySize;
				samples = (unsigned char*)samples + copySize;

				//copy region 2
				if (readSizes[0] == availSizes[0])
				{
					copySize = std::min(remainSize, availSizes[1]);
					memcpy(samples, availData[1], copySize);
					readSizes[1] = copySize;

					remainSize -= copySize;
				}//if (readSizes[0] == availSizes[0])
				else
					readSizes[1] = 0;

				readSizes[0] /= captureBuffer.GetWaveFormat().nBlockAlign;
				readSizes[1] /= captureBuffer.GetWaveFormat().nBlockAlign;

				captureBuffer.UnlockStream(availData, readSizes);

				return (maxSize - remainSize) / captureBuffer.GetWaveFormat().nBlockAlign;
			}
			return 0;
		}
		//end LHQ

		/*----------Buffer --------------*/
		cstring DirectSound::Buffer::Create(IDirectSound8& device, const bool priority)
		{
			Release();

			Object::Pod<DSBUFFERDESC> desc;

			desc.dwSize = sizeof(desc);
			desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
			desc.lpwfxFormat = NULL;

			if (priority)
			{
				ComInterface<IDirectSoundBuffer> primary;

				if (FAILED(device.CreateSoundBuffer(&desc, &primary, NULL)) || FAILED(primary->SetFormat(&waveFormat)))
				{
					static bool logged = false;

					if (logged == false)
					{
						logged = true;
						Io::Log() << "DirectSound: warning, couldn't set the sample format for the primary sound buffer!\r\n";
					}
				}
			}

			NST_ASSERT(settings.size % waveFormat.nBlockAlign == 0);

			desc.Clear();
			desc.dwSize = sizeof(desc);
			desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;

			if (settings.globalFocus)
				desc.dwFlags |= DSBCAPS_GLOBALFOCUS;

			if (settings.pool == POOL_SYSTEM)
				desc.dwFlags |= DSBCAPS_LOCSOFTWARE;

			desc.dwBufferBytes = settings.size;
			desc.lpwfxFormat = &waveFormat;

			ComInterface<IDirectSoundBuffer> oldCom;

			if (FAILED(device.CreateSoundBuffer(&desc, &oldCom, NULL)))
			{
				if (!(desc.dwFlags & DSBCAPS_LOCSOFTWARE))
				{
					desc.dwFlags |= DSBCAPS_LOCSOFTWARE;

					static bool logged = false;

					if (logged == false)
					{
						logged = true;
						Io::Log() << "DirectSound: warning, couldn't create the sound buffer! Retrying with software buffers..\r\n";
					}

					if (FAILED(device.CreateSoundBuffer(&desc, &oldCom, NULL)))
						return "IDirectSound8::CreateSoundBuffer()";
				}
			}

			if (FAILED(oldCom->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void**>(&com))))
				return "IDirectSoundBuffer::QueryInterface()";

			return NULL;
		}


		void DirectSound::Buffer::StartStream()
		{
			if (com)
			{
				DWORD status;

				if (FAILED(com->GetStatus(&status)))
				{
					com.Release();
					return;
				}

				if ((status & (DSBSTATUS_BUFFERLOST | DSBSTATUS_LOOPING | DSBSTATUS_PLAYING)) == (DSBSTATUS_LOOPING | DSBSTATUS_PLAYING))
					return;

				if (status & DSBSTATUS_PLAYING)
					com->Stop();

				if (status & DSBSTATUS_BUFFERLOST)
				{
					const HRESULT hResult = com->Restore();

					if (FAILED(hResult) && hResult != DSERR_BUFFERLOST)
						com.Release();

					return;
				}

				void* data;
				DWORD size;

				if (SUCCEEDED(com->Lock(0, 0, &data, &size, NULL, NULL, DSBLOCK_ENTIREBUFFER)))
				{
					std::memset(data, waveFormat.wBitsPerSample == 16 ? DC_OFFSET_16 : DC_OFFSET_8, size);
					com->Unlock(data, size, NULL, 0);
				}

				com.writePos = 0;
				com->SetCurrentPosition(0);
				const HRESULT hResult = com->Play(0, 0, DSBPLAY_LOOPING);

				if (FAILED(hResult) && hResult != DSERR_BUFFERLOST)
					com.Release();
			}
		}

		/*----------CaptureBuffer --------------*/
		cstring DirectSound::CaptureBuffer::Create(IDirectSoundCapture8& device, const bool priority)
		{
			Release();

			Object::Pod<DSCBUFFERDESC> desc;

			desc.dwSize = sizeof(desc);
			desc.dwFlags = 0;

			NST_ASSERT(settings.size % waveFormat.nBlockAlign == 0);

			desc.dwSize = sizeof(desc);

			desc.dwBufferBytes = settings.size;
			desc.lpwfxFormat = &waveFormat;

			ComInterface<IDirectSoundCaptureBuffer> oldCom;

			if (FAILED(device.CreateCaptureBuffer(&desc, &oldCom, NULL)))
			{
				return "IDirectSoundCapture8::CreateCaptureBuffer()";
			}

			if (FAILED(oldCom->QueryInterface(IID_IDirectSoundCaptureBuffer8, reinterpret_cast<void**>(&com))))
				return "IDirectSoundCaptureBuffer::QueryInterface()";

			return NULL;
		}


		void DirectSound::CaptureBuffer::StartStream()
		{
			if (com)
			{
				DWORD status;

				if (FAILED(com->GetStatus(&status)))
				{
					com.Release();
					return;
				}

				if ((status & (DSCBSTATUS_CAPTURING | DSBSTATUS_LOOPING)) == (DSBSTATUS_LOOPING | DSCBSTATUS_CAPTURING))
					return;

				void* data;
				DWORD size;

				com.writePos = 0;
				const HRESULT hResult = com->Start(DSCBSTART_LOOPING);

				if (FAILED(hResult))
					com.Release();
			}
		}
	}
}
