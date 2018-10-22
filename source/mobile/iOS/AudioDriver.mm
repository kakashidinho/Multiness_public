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

#import "AudioDriver.h"

#import <AVFoundation/AVFoundation.h>

#include "../../core/NstLog.hpp"

#include <thread>
#include <assert.h>
#include <string.h>

#define kOutputBus 0
#define kInputBus 1

#define LATENCY 80
#define DEFAULT_FRAME_RATE 60

#ifndef max
#	define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#	define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifdef DEBUG
#	define AU_ERR_CHECK(status) \
{\
	auto s = (status);\
	if (s != noErr)\
	{\
		Core::Log() << "AudioUnit error " << s << " at line " << __LINE__ << "\n";\
	}\
}
#else
#	define AU_ERR_CHECK(status) ((void)status)
#endif

namespace Nes {
	namespace Sound {
		namespace IOS {
			AudioDriver::AudioDriver(Api::Emulator& emulator)
			:m_emulator(emulator), m_mode(Api::Machine::NTSC), m_audioUnit(NULL),
			m_intermediateOutputBuffer(NULL), m_intermediateInputBuffer(NULL),
			m_capturing(false), m_playing(false), m_running(false)
			{
				m_sampleRate = 48000;
				m_sampleBits = 16;
				m_channels = Api::Sound::SPEAKER_MONO;
				
				Api::Sound sound(m_emulator);
				sound.SetSampleRate(m_sampleRate);
				sound.SetSampleBits(m_sampleBits);
				sound.SetSpeaker(m_channels);
				sound.SetSpeed(Api::Sound::DEFAULT_SPEED);
				
				//create audio unit
				// Describe audio component
				AudioComponentDescription desc;
				desc.componentType = kAudioUnitType_Output;
				desc.componentSubType = kAudioUnitSubType_RemoteIO;
				desc.componentFlags = 0;
				desc.componentFlagsMask = 0;
				desc.componentManufacturer = kAudioUnitManufacturer_Apple;
				
				// Get component
				AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
				
				// Get audio units
				auto status = AudioComponentInstanceNew(inputComponent, &m_audioUnit);
				if (status != noErr)
				{
					m_audioUnit = nullptr;
					Core::Log() << "AudioComponentInstanceNew() failed";
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
				
				if (m_audioUnit) {
					AudioComponentInstanceDispose(m_audioUnit);
				}
			}
			
			void AudioDriver::EnableInput(bool enable)
			{
				if (m_capturing != enable)
				{
					m_capturing = enable;
					if (m_audioUnit)
					{
						// Enable IO for recording
						UInt32 flag = m_capturing ? 1 : 0;
						
						AU_ERR_CHECK(AudioOutputUnitStop(m_audioUnit));
						AU_ERR_CHECK(AudioUnitUninitialize(m_audioUnit));
						
						auto status = AudioUnitSetProperty(m_audioUnit,
														   kAudioOutputUnitProperty_EnableIO,
														   kAudioUnitScope_Input,
														   kInputBus,
														   &flag,
														   sizeof(flag));
						
						AU_ERR_CHECK(status);
						
						AU_ERR_CHECK(AudioUnitInitialize(m_audioUnit));
						AU_ERR_CHECK(AudioOutputUnitStart(m_audioUnit));
					}
				}
			}
			
			uint AudioDriver::GetDesiredFPS() const
			{
				auto frameRate = (Api::Machine(m_emulator).GetMode()) == Api::Machine::PAL ? (DEFAULT_FRAME_RATE * 5 / 6) : DEFAULT_FRAME_RATE;
				
				return frameRate;
			}
			
			inline uint AudioDriver::GetDesiredSamplesPerFrame() const {
				auto frameRate = GetDesiredFPS();
				auto frameSizeInSamples = m_sampleRate / frameRate;
				
				return frameSizeInSamples;
			}
			
			void AudioDriver::Reset()
			{
				Release();
				
				OSStatus status;
				
				uint channels = m_channels == Api::Sound::SPEAKER_MONO ? 1 : 2;
				
				m_running = true;
				
				m_sampleBlockAlign = m_sampleBits / 8 * channels;
				m_mode = Api::Machine(m_emulator).GetMode();
				
				//describe format
				AudioStreamBasicDescription audioFormat;
				
				audioFormat.mSampleRate			= m_sampleRate;
				audioFormat.mFormatID			= kAudioFormatLinearPCM;
				audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
				audioFormat.mFramesPerPacket	= 1;
				audioFormat.mChannelsPerFrame	= channels;
				audioFormat.mBitsPerChannel		= m_sampleBits;
				audioFormat.mBytesPerPacket		= m_sampleBlockAlign;
				audioFormat.mBytesPerFrame		= m_sampleBlockAlign;
				
				//apply format
				status = AudioUnitSetProperty(m_audioUnit,
											  kAudioUnitProperty_StreamFormat,
											  kAudioUnitScope_Output,
											  kInputBus,
											  &audioFormat,
											  sizeof(audioFormat));
				AU_ERR_CHECK(status);
				status = AudioUnitSetProperty(m_audioUnit,
											  kAudioUnitProperty_StreamFormat,
											  kAudioUnitScope_Input,
											  kOutputBus,
											  &audioFormat,
											  sizeof(audioFormat));
				AU_ERR_CHECK(status);
				
				
				/*-- init buffers ---*/
				std::lock_guard<std::mutex> lg1(m_outputLock);
				std::lock_guard<std::mutex> lg2(m_inputLock);
				
				auto frameRate = GetDesiredFPS();
				auto frameLength = 1000 / frameRate;//ms
				
				auto frameSizeInBytes = GetDesiredSamplesPerFrame() * m_sampleBlockAlign;
				auto bufferSizeInBytes = LATENCY / frameLength * frameSizeInBytes;
				
				m_intermediateOutputBuffer = new unsigned char[bufferSizeInBytes];
				m_intermediateInputBuffer = new unsigned char[bufferSizeInBytes];
				m_outputRingBuffer = std::unique_ptr<ByteBuffer>(new ByteBuffer(bufferSizeInBytes));
				m_inputRingBuffer = std::unique_ptr<ByteBuffer>(new ByteBuffer(bufferSizeInBytes));
				
				
				//Disable buffer allocation for the recorder (optional - do this if we want to pass in our own)
				UInt32 flag = 0;
				status = AudioUnitSetProperty(m_audioUnit,
											  kAudioUnitProperty_ShouldAllocateBuffer,
											  kAudioUnitScope_Output,
											  kInputBus,
											  &flag,
											  sizeof(flag));
				AU_ERR_CHECK(status);
				
				//Set hardware io buffer duration
				NSTimeInterval ioBufferDuration = frameLength / 1000.0;
				NSError *error;
				[[AVAudioSession sharedInstance] setPreferredIOBufferDuration: ioBufferDuration error:&error];
				
				if (error)
					Core::Log() << [[error localizedDescription] UTF8String];
											  
				
				/*---- set callbacks ----*/
				AURenderCallbackStruct callback;
				callback.inputProcRefCon = this;
				
				//playback
				callback.inputProc = PlaybackCallback;
				status = AudioUnitSetProperty(m_audioUnit,
											  kAudioUnitProperty_SetRenderCallback,
											  kAudioUnitScope_Global,
											  kOutputBus,
											  &callback,
											  sizeof(callback));
				
				AU_ERR_CHECK(status);
				
				//record
				callback.inputProc = RecordingCallback;
				status = AudioUnitSetProperty(m_audioUnit,
											  kAudioOutputUnitProperty_SetInputCallback,
											  kAudioUnitScope_Global,
											  kInputBus,
											  &callback,
											  sizeof(callback));
				
				AU_ERR_CHECK(status);
			}
			
			void AudioDriver::Release()
			{
				//close capture device
				if (m_audioUnit) {
					m_running = false;
					{
						std::lock_guard<std::mutex> lg(m_outputLock);
						m_outputCv.notify_all();
					}
					{
						std::lock_guard<std::mutex> lg(m_inputLock);
						m_inputCv.notify_all();
					}
					EnableOutput(false);
					
					AU_ERR_CHECK(AudioOutputUnitStop(m_audioUnit));
					AU_ERR_CHECK(AudioUnitUninitialize(m_audioUnit));
					
					AURenderCallbackStruct nullCallback = {0};
					auto status = AudioUnitSetProperty(m_audioUnit,
												  kAudioOutputUnitProperty_SetInputCallback,
												  kAudioUnitScope_Global,
												  kInputBus,
												  &nullCallback,
												  sizeof(nullCallback));
					
					AU_ERR_CHECK(status);
					
					status = AudioUnitSetProperty(m_audioUnit,
												  kAudioUnitProperty_SetRenderCallback,
												  kAudioUnitScope_Global,
												  kOutputBus,
												  &nullCallback,
												  sizeof(nullCallback));
					
					AU_ERR_CHECK(status);
				}
				
				std::lock_guard<std::mutex> lg1(m_outputLock);
				std::lock_guard<std::mutex> lg2(m_inputLock);
				
				delete[] m_intermediateOutputBuffer;
				delete[] m_intermediateInputBuffer;
				m_intermediateOutputBuffer = NULL;
				m_intermediateInputBuffer = NULL;
				m_outputRingBuffer = nullptr;
				m_inputRingBuffer = nullptr;
			}
			
			
			bool AudioDriver::Lock(Api::Sound::Output& output) {
				if (Api::Machine(m_emulator).GetMode() != m_mode)
					Reset();
				
				if (!m_audioUnit || !m_outputRingBuffer)
					return false;
				
				auto frameSizeInBytes = GetDesiredSamplesPerFrame() * m_sampleBlockAlign;
				
				m_outputLock.lock();
				size_t available = m_outputRingBuffer->Capacity() - m_outputRingBuffer->Size();
				//available = MIN(available, frameSizeInBytes);
				m_outputLock.unlock();
				
				output.length[0] = (uint)available / m_sampleBlockAlign;
				output.samples[0] = m_intermediateOutputBuffer;
				
				output.length[1] = 0;
				output.samples[1] = nullptr;
				
				return true;
			}
			
			void AudioDriver::Unlock(Api::Sound::Output& output) {
				if (m_outputRingBuffer == nullptr)
					return;
				
				unsigned char* ptr;
				size_t size;
				
				ptr = (unsigned char*) output.samples[0];
				size = output.length[0] * m_sampleBlockAlign;
				
				m_outputLock.lock();
				
				auto pushedSize = m_outputRingBuffer->Push(ptr, size);
				assert(pushedSize == size);
				
				auto fillRatio = m_outputRingBuffer->FillRatio();
				
				m_outputCv.notify_all();
				
				m_outputLock.unlock();
				
				if (fillRatio > 0.5f)
					EnableOutput(true);
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
				std::unique_lock<std::mutex> lg(m_inputLock);
				
				if (samples == NULL)
					return (uint)m_inputRingBuffer->Size() / m_sampleBlockAlign;
				
				return (uint)m_inputRingBuffer->Pop((unsigned char*)samples, maxSamples * m_sampleBlockAlign) / m_sampleBlockAlign;
			}
			
			//Nestopia's callbacks
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
			
			//AudioUnit
			void AudioDriver::EnableOutput(bool enable)
			{
				if (m_playing == enable)
					return;
				
				m_playing = enable;
				
				UInt32 flag = enable ? 1 : 0;
				
				AU_ERR_CHECK(AudioOutputUnitStop(m_audioUnit));
				AU_ERR_CHECK(AudioUnitUninitialize(m_audioUnit));
				
				// Enable IO for playback
				auto status = AudioUnitSetProperty(m_audioUnit,
											  kAudioOutputUnitProperty_EnableIO,
											  kAudioUnitScope_Output,
											  kOutputBus,
											  &flag,
											  sizeof(flag));
				
				AU_ERR_CHECK(status);
				
				AU_ERR_CHECK(AudioUnitInitialize(m_audioUnit));
				AU_ERR_CHECK(AudioOutputUnitStart(m_audioUnit));
			}
			
			//AudioUnit callbacks
			OSStatus AudioDriver::PlaybackCallback(AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
												   UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
			{
				static uint16_t last_sample[2] = {0};
				size_t re = 0;
				std::unique_lock<std::mutex> lk(m_outputLock);
				if (m_outputRingBuffer->FillRatio() < 0.5f)
				{
					//fill buffer with last rendered sample
					for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
					{
						auto& buffer = ioData->mBuffers[i];
						auto eptr = (unsigned char*)buffer.mData + buffer.mDataByteSize;
						for (auto ptr = (unsigned char*)buffer.mData; ptr < eptr; ptr += m_sampleBlockAlign)
						{
							memcpy(ptr, last_sample, m_sampleBlockAlign);
						}
					}
					return noErr;
				}
				
				for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
				{
					auto& buffer = ioData->mBuffers[i];
					
					re = m_outputRingBuffer->Pop((unsigned char*)buffer.mData, buffer.mDataByteSize);
					
					//if we don't have enough samples, just repeat the last sample at the end
					if (re > m_sampleBlockAlign)
						memcpy(last_sample, (unsigned char*)buffer.mData + re - m_sampleBlockAlign, m_sampleBlockAlign);
					auto eptr = (unsigned char*)buffer.mData + buffer.mDataByteSize;
					for (auto ptr = (unsigned char*)buffer.mData + re; ptr < eptr; ptr += m_sampleBlockAlign)
					{
						memcpy(ptr, last_sample, m_sampleBlockAlign);
					}
				}
				//TODO
				return noErr;
			}
			
			OSStatus AudioDriver::RecordingCallback(AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
													UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
			{
				static_assert(Api::Sound::SPEAKER_MONO == 0 && Api::Sound::SPEAKER_STEREO == 1, "Unaware enum values");
				
				UInt32 bytes = (UInt32)MIN(m_inputRingBuffer->Capacity(), inNumberFrames * m_sampleBlockAlign);
				
				//write recorded data to intermediate buffer
				AudioBufferList bufferList;
				bufferList.mNumberBuffers = 1;
				bufferList.mBuffers[0].mData = m_intermediateInputBuffer;
				bufferList.mBuffers[0].mNumberChannels = m_channels + 1;
				bufferList.mBuffers[0].mDataByteSize = bytes;
				
				auto status = AudioUnitRender(m_audioUnit,
											  ioActionFlags,
											  inTimeStamp,
											  inBusNumber,
											  bytes / m_sampleBlockAlign,
											  &bufferList);
				
				AU_ERR_CHECK(status);
				
				//write to ring buffer
				std::unique_lock<std::mutex> lk(m_inputLock);
				
				m_inputRingBuffer->Push(m_intermediateInputBuffer, bytes);
				
				return noErr;
			}
			
			//static versions
			OSStatus AudioDriver::PlaybackCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
												   UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
			{
				auto driver = reinterpret_cast<AudioDriver*>(inRefCon);
				return driver->PlaybackCallback(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
			}
			
			OSStatus AudioDriver::RecordingCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
													UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
			{
				auto driver = reinterpret_cast<AudioDriver*>(inRefCon);
				return driver->RecordingCallback(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
			}
		}
	}
}

