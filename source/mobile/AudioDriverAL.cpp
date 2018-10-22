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

#include "AudioDriverAL.hpp"

#include "../core/NstLog.hpp"

#include <thread>
#include <assert.h>
#include <string.h>

#define LATENCY 80
#define DEFAULT_FRAME_RATE 60

#ifndef max
#	define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#	define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifdef DEBUG
#	define AL_ERR_CHECK \
	{\
		auto err = alGetError();\
		if (err != AL_NO_ERROR)\
		{\
			Core::Log() << "al error " << err << " at line " << __LINE__ << "\n";\
		}\
	}
#else
#	define AL_ERR_CHECK
#endif

namespace Nes {
	namespace Sound {
		namespace AL {
			AudioDriver::AudioDriver(Api::Emulator& emulator)
			:m_emulator(emulator), m_mode(Api::Machine::NTSC),
			m_buffers(NULL), m_numBuffers(0),
			m_unusedBuffers(NULL), m_numUnusedBuffers(0),
			m_intermediateBuffer(NULL),
			m_source(0),
			m_device(NULL), m_context(NULL), m_captureDevice(NULL), m_capturing(false)
			{
				m_sampleRate = 48000;
				m_sampleBits = 16;
				m_channels = Api::Sound::SPEAKER_MONO;
				
				Api::Sound sound(m_emulator);
				sound.SetSampleRate(m_sampleRate);
				sound.SetSampleBits(m_sampleBits);
				sound.SetSpeaker(m_channels);
				sound.SetSpeed(Api::Sound::DEFAULT_SPEED);
				
				//create openAL device & context
				m_device = alcOpenDevice(NULL);
				if (m_device == NULL)
				{
					Core::Log() << ("Error : alcOpenDevice(NULL) failed!");
				}
				else
				{
					m_context = alcCreateContext(m_device, NULL);
					if (m_context == NULL)
					{
						Core::Log() << ("Error : alcCreateContext(m_device, NULL) failed!");
						alcCloseDevice(m_device);
						m_device = NULL;
					}
					if (alcMakeContextCurrent(m_context) == AL_FALSE)
					{
						Core::Log() << ("Error : alcMakeContextCurrent(m_context) failed!");
						alcDestroyContext(m_context);
						alcCloseDevice(m_device);
						
						m_context = NULL;
						m_device = NULL;
					}
					
					//set speed of sound
					alSpeedOfSound(0.00001f);
					
					//generate source
					alGenSources(1, &m_source);
					//alSourcei(m_source, AL_LOOPING, AL_FALSE);
					
					alGetError();//reset error state
				}
				
				Reset();
				
				//register callbacks
				Api::Sound::Output::lockCallback.Set(Lock, this);
				Api::Sound::Output::unlockCallback.Set(Unlock, this);
				Api::Sound::Output::updateSettingsCallback.Set(UpdateSettings, this);
				
				//register callbacks
				Api::Sound::Input::readCallback.Set(ReadInput, this);
			}
			
			AudioDriver::~AudioDriver() {
				Release();
				
				if (m_source)
					alDeleteSources(1, &m_source);
				
				alcMakeContextCurrent(NULL);
				if (m_context)
					alcDestroyContext(m_context);
				
				if (m_device)
					alcCloseDevice(m_device);
			}
			
			void AudioDriver::EnableInput(bool enable)
			{
				if (m_capturing != enable)
				{
					m_capturing = enable;
					if (m_captureDevice)
					{
						if (m_capturing)
							alcCaptureStart(m_captureDevice);
						else
							alcCaptureStop(m_captureDevice);
					}
				}
			}
			
			uint AudioDriver::GetDesiredFPS() const
			{
				auto frameRate = (Api::Machine(m_emulator).GetMode()) == Api::Machine::PAL ? (DEFAULT_FRAME_RATE * 5 / 6) : DEFAULT_FRAME_RATE;
				
				return frameRate;
			}
			
			void AudioDriver::Reset()
			{
				Release();
				
				m_mode = Api::Machine(m_emulator).GetMode();
				
				/*-- init output buffers ---*/
				uint channels;
				auto frameRate = GetDesiredFPS();
				
				auto frameLength = 1000 / frameRate;//ms
				
				switch (m_channels)
				{
					case Api::Sound::SPEAKER_MONO:
						channels = 1;
						if (m_sampleBits == 16)
							m_format = AL_FORMAT_MONO16;
						else
							m_format = AL_FORMAT_MONO8;
						break;
					default:
						channels = 2;
						if (m_sampleBits == 16)
							m_format = AL_FORMAT_STEREO16;
						else
							m_format = AL_FORMAT_STEREO8;
						break;
				}
				
				m_sampleBlockAlign = m_sampleBits / 8 * channels;
				m_bufferSize = m_sampleRate / frameRate * m_sampleBlockAlign / 2;//buffer size = half frame
				m_numBuffers = m_numUnusedBuffers = 2 * LATENCY / frameLength;
				
				m_buffers = new ALuint[m_numBuffers];
				m_unusedBuffers = new ALuint[m_numUnusedBuffers];
				
				m_intermediateBuffer = new unsigned char[m_bufferSize * m_numBuffers];
				
				memset(m_buffers, 0, m_numBuffers * sizeof(ALuint));
				alGenBuffers(m_numBuffers, m_buffers);
				
				memcpy(m_unusedBuffers, m_buffers, m_numBuffers * sizeof(ALuint));
				
				m_playing = false;
				
				/*---- init capture device ---*/
				m_captureDevice = alcCaptureOpenDevice(NULL, m_sampleRate, m_format, m_bufferSize * m_numBuffers / m_sampleBlockAlign);
				if (!m_captureDevice)
				{
					Core::Log() << ("Error : alcCaptureOpenDevice(NULL) failed!");
				}
				else
				{
					if (m_capturing)
						alcCaptureStart(m_captureDevice);
				}
			}
			
			void AudioDriver::Release()
			{
				//close capture device
				if (m_captureDevice) {
					if (m_capturing)
						alcCaptureStop(m_captureDevice);
					alcCaptureCloseDevice(m_captureDevice);
				}
				
				//empty the source's queue
				ALint numQueuedBuffers = 0;
				alGetSourcei(m_source, AL_BUFFERS_QUEUED, &numQueuedBuffers);
				if (numQueuedBuffers)
					alSourceUnqueueBuffers(m_source, numQueuedBuffers, m_unusedBuffers);
				
				if (m_buffers)
				{
					alDeleteBuffers(m_numBuffers, m_buffers);
					delete[] m_buffers;
					
					m_buffers = NULL;
				}
				
				if (m_unusedBuffers)
				{
					delete[] m_unusedBuffers;
					m_unusedBuffers = NULL;
				}
				
				if (m_intermediateBuffer)
				{
					delete[] m_intermediateBuffer;
					m_intermediateBuffer = NULL;
				}
				
				m_numBuffers = m_numUnusedBuffers = 0;
			}
			
			
			bool AudioDriver::Lock(Api::Sound::Output& output) {
				if (m_intermediateBuffer == NULL || Api::Machine(m_emulator).GetMode() != m_mode)
					Reset();
				
				if (m_intermediateBuffer == NULL)
					return false;
				
				do {
					ALint processedBuffers;
					alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processedBuffers);
					if (processedBuffers)
					{
						alSourceUnqueueBuffers(m_source, processedBuffers, m_unusedBuffers + m_numUnusedBuffers);
						m_numUnusedBuffers += processedBuffers;
					}
					
					std::this_thread::yield();
				} while (0);
				
				if (m_numUnusedBuffers)
				{
					output.length[0] = m_bufferSize * m_numUnusedBuffers / m_sampleBlockAlign;
					output.samples[0] = m_intermediateBuffer;
					
					output.length[1] = 0;
					output.samples[1] = NULL;
				}
				else {
					return false;
				}
				
				return true;
			}
			
			void AudioDriver::Unlock(Api::Sound::Output& output) {
				if (m_intermediateBuffer == NULL)
					return;
				
				assert(m_numUnusedBuffers > 0);
				
				auto writtenSize = output.length[0] * m_sampleBlockAlign;
				auto buffersToWrite = writtenSize / m_bufferSize;
				auto startBufferIdx = m_numUnusedBuffers - buffersToWrite;
				auto endBufferIdx = m_numUnusedBuffers;
				
				for (ALsizei i = startBufferIdx, offset = 0; i < endBufferIdx; ++i, offset += m_bufferSize)
				{
					alBufferData(m_unusedBuffers[i],
								 m_format,
								 m_intermediateBuffer + offset,
								 m_bufferSize,
								 m_sampleRate);
				}
				
				if (buffersToWrite)
					alSourceQueueBuffers(m_source, buffersToWrite, m_unusedBuffers + startBufferIdx);
				m_numUnusedBuffers -= buffersToWrite;
				
				AL_ERR_CHECK
				
				ALint playing;
				
				
				float useRatio = 1.f - (float)m_numUnusedBuffers / m_numBuffers;
				if (!m_playing && useRatio >= 0.5f)
				{
					m_playing = true;
					
					alGetSourcei(m_source, AL_SOURCE_STATE, &playing);
					if (playing != AL_PLAYING)
					{
						alSourcePlay(m_source);
						
						AL_ERR_CHECK
					}
				}
		
				if (m_playing)
				{
					alGetSourcei(m_source, AL_SOURCE_STATE, &playing);
					if (playing != AL_PLAYING)
					{
						alSourcePlay(m_source);
						
						AL_ERR_CHECK
					}
				}
			}
			
			void AudioDriver::UpdateSettings() {
				Api::Sound sound(m_emulator);
				
				if (m_sampleRate != sound.GetSampleRate() ||
					m_sampleBits != sound.GetSampleBits() ||
					m_channels != sound.GetSpeaker())
				{
					m_sampleRate = sound.GetSampleRate();
					m_sampleBits = sound.GetSampleBits();
					m_channels = sound.GetSpeaker();
					
					Reset();
				}
			}
			
			uint AudioDriver::ReadInput(Api::Sound::Input& input, void* samples, uint maxSamples) {
				if (!m_captureDevice)
					return 0;
				
				ALint availSamples = 0;
				alcGetIntegerv(m_captureDevice, ALC_CAPTURE_SAMPLES, 1, &availSamples);
				if (samples == NULL)//return number of available samples without reading data
					return availSamples;
				
				uint numSamples = min(availSamples, maxSamples);
				
				if (numSamples)
				{
					alcCaptureSamples(m_captureDevice, samples, numSamples);
				}
				
				return numSamples;
			}
			
			bool NST_CALLBACK AudioDriver::Lock(void* data, Api::Sound::Output& output) {
				auto driver = reinterpret_cast<AudioDriver*>(data);
				return driver->Lock(output);
			}
			void NST_CALLBACK AudioDriver::Unlock(void* data, Api::Sound::Output& output) {
				auto driver = reinterpret_cast<AudioDriver*>(data);
				driver->Unlock(output);
			}
			void NST_CALLBACK AudioDriver::UpdateSettings(void* data) {
				auto driver = reinterpret_cast<AudioDriver*>(data);
				driver->UpdateSettings();
			}
			
			uint NST_CALLBACK AudioDriver::ReadInput(void* data, Api::Sound::Input& input, void* samples, uint maxSamples) {
				auto driver = reinterpret_cast<AudioDriver*>(data);
				
				return driver->ReadInput(input, samples, maxSamples);
			}
		}
	}
}
