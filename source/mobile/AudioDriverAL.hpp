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

#ifndef AudioDriverAL_hpp
#define AudioDriverAL_hpp

#include <stdio.h>

#ifdef __APPLE__
#	include <OpenAL/al.h>
#	include <OpenAL/alc.h>
#else
#	include <AL/al.h>
#	include <AL/alc.h>
#endif

#include "../core/api/NstApiEmulator.hpp"
#include "../core/api/NstApiSound.hpp"
#include "../core/api/NstApiMachine.hpp"

namespace Nes {
	namespace Sound {
		namespace AL {
			class AudioDriver {
			public:
				AudioDriver(Api::Emulator& emulator);
				virtual ~AudioDriver();
				
				virtual void EnableInput(bool enable);
				
				uint GetDesiredFPS() const;
			protected:
				virtual void Reset();
				virtual void Release();
				
				bool Lock(Api::Sound::Output& output);
				void Unlock(Api::Sound::Output& output);
				void UpdateSettings();
				virtual uint ReadInput(Api::Sound::Input& input, void* samples, uint maxSamples);
				
				static bool NST_CALLBACK Lock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK Unlock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK UpdateSettings(void* data);
				
				static uint NST_CALLBACK ReadInput(void* data, Api::Sound::Input& input, void* samples, uint maxSamples);
				
				ALsizei m_bufferSize;
				ALsizei m_numBuffers;
				ALsizei m_numUnusedBuffers;
				ALuint *m_buffers;
				ALuint *m_unusedBuffers;
				
				ALuint m_source;
				
				unsigned char* m_intermediateBuffer;
				
				ALCdevice * m_device;
				ALCcontext * m_context;
				
				ALCdevice * m_captureDevice;
				
				uint m_sampleRate;
				uint m_sampleBits;
				Api::Sound::Speaker m_channels;
				
				uint m_sampleBlockAlign;
				ALuint m_format;
				
				bool m_playing;
				bool m_capturing;
				
				Api::Machine::Mode m_mode;
				Api::Emulator& m_emulator;
			};
		}
	}
}

#endif /* AudioDriverAL_hpp */
