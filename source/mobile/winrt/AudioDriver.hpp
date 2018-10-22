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

#include "../../core/NstRingBuffer.hpp"
#include "../../core/api/NstApiEmulator.hpp"
#include "../../core/api/NstApiSound.hpp"
#include "../../core/api/NstApiMachine.hpp"

#include <soxr.h>

#include <HQReferenceCountObj.h>

#include <audioclient.h>

#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <list>
#include <functional>
#include <thread>

namespace Nes {
	namespace Sound {
		namespace WinRT {
			class AudioDriver: public HQReferenceCountObjTemplate<std::atomic<uint32_t>> {
			public:
				static std::shared_ptr<AudioDriver> CreateInstanceRef(Api::Emulator& emulator, Windows::UI::Core::CoreDispatcher^ uiDispatcher);
				
				AudioDriver(Api::Emulator& emulator, Windows::UI::Core::CoreDispatcher^ uiDispatcher);

				void SetUIDispatcher(Windows::UI::Core::CoreDispatcher^ uiDispatcher);

				//thread safe
				void SetOutputVolume(float gain);
				//thread safe
				void EnableOutput(bool enable);
				//thread safe
				void EnableInput(bool enable);
				uint GetDesiredFPS() const;
				uint GetDesiredSamplesPerFrame() const;
			private:
				~AudioDriver();

				void SetOutputVolumeNoLock(float gain);
				void EnableOutputNoLock(bool enable);
				void EnableInputNoLock(bool enable);

				void ActivateAudioDevices(std::unique_lock<std::mutex>& lk);
				bool MakeWaveFormatFromSettings(WAVEFORMATEX* pwfx);
				void CacheWaveFormatAsExtensible(WAVEFORMATEXTENSIBLE* out, const WAVEFORMATEX* pwfx);
				void Reset();
				void ReleaseResources(bool immediate = false, bool lock = false);

				void AudioFeedingProc();
				void FeedAudio(const void* rawResampledData, BYTE* &audioClientDstBuffer, size_t numSamples);

				bool Lock(Api::Sound::Output& output);
				bool DoLock(Api::Sound::Output& output);
				void Unlock(Api::Sound::Output& output);
				void DoUnlock(Api::Sound::Output& output);
				void UpdateSettings();
				uint ReadInput(Api::Sound::Input& input, void* samples, uint maxSamples);

				static bool NST_CALLBACK Lock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK Unlock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK UpdateSettings(void* data);

				static uint NST_CALLBACK ReadInput(void* data, Api::Sound::Input& input, void* samples, uint maxSamples);

				class AsyncTaskThread;
				typedef Core::RingBuffer<unsigned char> ByteRingBuffer;

				uint m_sampleRate;
				uint m_sampleBits;
				Api::Sound::Speaker m_channels;

				uint m_sampleBlockAlign;

				WAVEFORMATEXTENSIBLE m_wfx;

				std::shared_ptr<AsyncTaskThread> m_asyncTaskThread;
				std::unique_ptr<std::thread> m_audioFeedingThread;
				HANDLE m_audioFeedingEvent;

				Windows::UI::Core::CoreDispatcher^ m_uiDispatcher;
				Nes::WinRT::Utils::ComWrapper<IAudioClient> m_audioClient;
				Nes::WinRT::Utils::ComWrapper<IAudioRenderClient> m_audioRenderClient;
				Nes::WinRT::Utils::ComWrapper<ISimpleAudioVolume> m_audioVolumeControl;

				std::mutex m_audioClientBufferLock;
				std::unique_ptr<ByteRingBuffer> m_audioClientBuffer;
				UINT32 m_audioClientBufferSize;//in samples
				unsigned char m_lastRenderedSample[128];

				bool m_capturing;
				bool m_playing;
				std::atomic<bool> m_audioFeedingThreadRunning;
				std::atomic<bool> m_alive;
				float m_volume;

				std::mutex m_genericThreadSafetyLock;
				std::mutex m_dispatcherLock;

				Api::Machine::Mode m_mode;
				Api::Emulator& m_emulator;
			};
		}
	}
}