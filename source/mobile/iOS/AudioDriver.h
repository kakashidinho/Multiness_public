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

#ifndef AudioDriver_h
#define AudioDriver_h

#include "../../core/NstRingBuffer.hpp"
#include "../../core/api/NstApiEmulator.hpp"
#include "../../core/api/NstApiSound.hpp"
#include "../../core/api/NstApiMachine.hpp"

#include <AudioUnit/AudioUnit.h>

#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace Nes {
	namespace Sound {
		namespace IOS {
			class AudioDriver {
			public:
				AudioDriver(Api::Emulator& emulator);
				~AudioDriver();
				
				void EnableInput(bool enable);
				uint GetDesiredFPS() const;
				uint GetDesiredSamplesPerFrame() const;
			private:
				void Reset();
				void Release();
				
				bool Lock(Api::Sound::Output& output);
				void Unlock(Api::Sound::Output& output);
				void UpdateSettings();
				uint ReadInput(Api::Sound::Input& input, void* samples, uint maxSamples);
				
				static bool NST_CALLBACK Lock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK Unlock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK UpdateSettings(void* data);
				
				static uint NST_CALLBACK ReadInput(void* data, Api::Sound::Input& input, void* samples, uint maxSamples);
				
				void EnableOutput(bool enable);
				
				//AudioUnit callbacks
				OSStatus PlaybackCallback(AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
										  UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
				
				OSStatus RecordingCallback(AudioUnitRenderActionFlags *ioActionFlags,
										   const AudioTimeStamp *inTimeStamp,
										   UInt32 inBusNumber,
										   UInt32 inNumberFrames,
										   AudioBufferList *ioData);
				
				static OSStatus PlaybackCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
												 UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
				
				static OSStatus RecordingCallback(void *inRefCon,
												  AudioUnitRenderActionFlags *ioActionFlags,
												  const AudioTimeStamp *inTimeStamp,
												  UInt32 inBusNumber,
												  UInt32 inNumberFrames,
												  AudioBufferList *ioData);
				
				
				AudioComponentInstance m_audioUnit;
				
				uint m_sampleRate;
				uint m_sampleBits;
				Api::Sound::Speaker m_channels;
				
				uint m_sampleBlockAlign;
				
				typedef Core::RingBuffer<unsigned char> ByteBuffer;
				
				std::mutex m_outputLock;
				std::mutex m_inputLock;
				
				std::condition_variable m_outputCv;
				std::condition_variable m_inputCv;
				
				unsigned char* m_intermediateOutputBuffer;
				unsigned char* m_intermediateInputBuffer;
				
				std::unique_ptr<ByteBuffer> m_outputRingBuffer;
				std::unique_ptr<ByteBuffer> m_inputRingBuffer;
				
				bool m_capturing;
				bool m_playing;
				std::atomic<bool> m_running;
				
				Api::Machine::Mode m_mode;
				Api::Emulator& m_emulator;
			};
		}
	}
}


#endif /* AudioDriver_h */
