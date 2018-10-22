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

#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include "JniUtils.hpp"


#include "../../core/NstRingBuffer.hpp"
#include "../../core/api/NstApiMachine.hpp"
#include "../../core/api/NstApiEmulator.hpp"
#include "../../core/api/NstApiSound.hpp"

#include <HQLinkedList.h>
#include <HQMemoryManager.h>

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

namespace Nes {
	namespace Sound {
		namespace Android {
			class AudioDriver {
			public:
				AudioDriver(Api::Emulator& emulator);
				~AudioDriver();
				
				void Pause();
				void Resume();

				void SetOutputVolume(float gain);
				void EnableInput(bool enable);
				void InputPermissionChanged(bool hasPermission);
				
				uint GetExpectedFrameRate();
			private:
				void InitSLES();
				void InitJni();
				void Release(bool inputOnly = false);
				void Reset(bool inputOnly = false);
				void ResumeNoLock();
				void SetOutputVolumeNoLock(float gain);
				void EnableInputNoLock(bool enable);
				void ResetSLESPlayer(uint desiredBufferSize, uint frameSizeInBytes);
				void ResetAudioRecord(JNIEnv* jenv, uint desiredBufferSize);
				jobject CreateAudioRecord(JNIEnv* jenv, jint source, jint jchannels, jint jencoding, jint& bufferSize, bool safeMode = false);
			
				uint GetNumChannels();
				uint GetExpectedSamplesPerFrame(uint sampleRate);
			
				bool Lock(Api::Sound::Output& output);
				void Unlock(Api::Sound::Output& output);
				void UpdateSettings();
				void StartInputBufferFillingThread();
				void StopInputBufferFillingThread();
				void InputBufferFillingProc();
				uint ReadInput(Api::Sound::Input& input, void* samples, uint maxSamples);

				void SLESBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq);
			
				static bool NST_CALLBACK Lock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK Unlock(void* data, Api::Sound::Output& output);
				static void NST_CALLBACK UpdateSettings(void* data);
				
				static uint NST_CALLBACK ReadInput(void* data, Api::Sound::Input& input, void* samples, uint maxSamples);

				static void SLESBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
			
				typedef Core::RingBuffer<unsigned char> ByteRingBuffer;

				// OpenSL ES
				// engine interfaces
				SLObjectItf m_engineObject = NULL;
				SLEngineItf m_engineEngine = NULL;

				// output mix interfaces
				SLObjectItf m_outputMixObject = NULL;

				// buffer queue player interfaces
				SLObjectItf m_bqPlayerObject = NULL;
				SLPlayItf m_bqPlayerPlay = NULL;
				SLAndroidSimpleBufferQueueItf m_bqPlayerBufferQueue = NULL;
				SLVolumeItf m_bqPlayerVolume = NULL;

				// SLES buffer
				std::mutex m_playBufferLock;
				std::vector<uint8_t> m_playBuffer;
				std::vector<uint8_t> m_playTempBuffer;
				HQLinkedList<uint8_t*, HQPoolMemoryManager> m_playQueuedSubBuffers;
				HQLinkedList<uint8_t*, HQPoolMemoryManager> m_playUnqueuedSubBuffers;
				size_t m_playSubBufferSize;

				// java Audio related stuffs
				jint ENCODING_PCM_16BIT;
				jint ENCODING_PCM_8BIT;

				//AudioRecord
				jclass m_jAudioRecordCls;
				jobject m_jAudioRecord;
				std::unique_ptr<Jni::Buffer> m_jrecByteBuffer;
				Jni::MethodID m_recConstructorMethod;
				Jni::MethodID m_recGetStateMethod;
				Jni::MethodID m_recStartMethod;
				Jni::MethodID m_recStopMethod;
				Jni::MethodID m_recReadMethod;
				Jni::MethodID m_recReleaseMethod;
				Jni::MethodID m_recGetMinBufferSizeMethod;
				
				jint CHANNEL_IN_MONO;
				jint CHANNEL_IN_STEREO;
				jint REC_STATE_INITIALIZED;
				jint DEFAULT_RECORD_SOURCE;
				jint VOICE_COMMUNICATION;
				jint MIC_RECORD_SOURCE;
				
				//recorder's ring buffer
				std::mutex m_recBufferLock;
				std::condition_variable m_recBufferCv;
				std::unique_ptr<std::thread> m_recBufferThread;
				std::atomic<bool> m_recBufferThreadRunning;
				std::unique_ptr<ByteRingBuffer> m_recBuffer;

				bool m_hasRecordPermission = false;

				// common
				uint m_sampleRate;
				uint m_recSampleRate;
				uint m_sampleBits;
				Api::Sound::Speaker m_channels;
				size_t m_sampleBlockAlign;
				
				std::mutex m_threadSafetyLock;
				
				bool m_playing;
				bool m_capturing;
				float m_volume;
				
				Api::Machine::Mode m_mode;
				Api::Emulator& m_emulator;
			};
		}
	}
}

#endif