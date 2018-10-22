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

#include "AudioDriver.hpp"

#include "../../core/NstLog.hpp"
#include "../../core/api/NstApiMachine.hpp"

#include <assert.h>
#include <string.h>

#include <math.h>

#include <time.h>

#define LATENCY 83
#define DEFAULT_FRAME_RATE 60

#define DISABLE_RECORDER 0
#define SPECIAL_LOG 0

#ifndef max
#	define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#	define min(a,b) ((a) < (b) ? (a) : (b))
#endif

			
#define SAFE_ASSIGN_JNI(re, call) \
	re = call;\
	if (jenv->ExceptionOccurred())\
	{\
		re = 0;\
		jenv->ExceptionDescribe();\
		jenv->ExceptionClear();\
	}

#define SAFE_SLES_STMT(code, errorHandler) \
	{ \
		SLresult result = (code); \
		if (SL_RESULT_SUCCESS != result) { \
			Core::Log() << __FILE__ << ":" << __LINE__ << ": sles error=" << result; \
			errorHandler; \
		} \
	} \

#define SAFE_SLES_STMT_RETURN(code) SAFE_SLES_STMT(code, return)

#define SAFE_SLES_STMT_IGNORE(code) SAFE_SLES_STMT(code, do {} while(0))

namespace Nes {
	namespace Sound {
		namespace Android {
			AudioDriver::AudioDriver(Api::Emulator& emulator) 
			:m_emulator(emulator), m_mode(Api::Machine::NTSC), 
			m_jAudioRecordCls(NULL),
			m_jAudioRecord(NULL),
			m_jrecByteBuffer(nullptr),
			m_playing(false),
			m_capturing(false),
			m_volume(1.f),
			m_recBufferThreadRunning(false),
			m_playQueuedSubBuffers(new HQPoolMemoryManager(sizeof(HQLinkedListNode<uint8_t *>), DEFAULT_FRAME_RATE)),
			m_playUnqueuedSubBuffers(m_playQueuedSubBuffers.GetMemManager())
			{
				m_sampleRate = m_recSampleRate = 48000;
				m_sampleBits = 16;
				m_channels = Api::Sound::SPEAKER_MONO;
				
				Api::Sound sound(m_emulator);
				sound.SetSampleRate(m_sampleRate);
				sound.SetSampleBits(m_sampleBits);
				sound.SetSpeaker(m_channels);
				sound.SetSpeed(Api::Sound::DEFAULT_SPEED);

				/*--------- init opensles ------------*/
				InitSLES();
				
				/*--------- init JNI stuffs ----------*/
				InitJni();
				
				//reset
				Reset();
				
				//register callbacks
				Api::Sound::Output::lockCallback.Set(Lock, this);
				Api::Sound::Output::unlockCallback.Set(Unlock, this);
				Api::Sound::Output::updateSettingsCallback.Set(UpdateSettings, this);
				
				Api::Sound::Input::readCallback.Set(ReadInput, this);
			}
			
			AudioDriver::~AudioDriver() {
				Release();

				// invalidate opensles
				if (m_outputMixObject != NULL) {
					(*m_outputMixObject)->Destroy(m_outputMixObject);
					m_outputMixObject = NULL;
				}

				if (m_engineObject != NULL) {
					(*m_engineObject)->Destroy(m_engineObject);
					m_engineObject = NULL;
					m_engineEngine = NULL;
				}

				// invalidate AudioRecord
				auto jenv = Jni::GetCurrenThreadJEnv();

				if (m_jAudioRecordCls)
					jenv->DeleteGlobalRef(m_jAudioRecordCls);

				// invalidate callbacks
				Api::Sound::Output::lockCallback.Set(NULL, NULL);
				Api::Sound::Output::unlockCallback.Set(NULL, NULL);
				Api::Sound::Output::updateSettingsCallback.Set(NULL, NULL);
				
				Api::Sound::Input::readCallback.Set(NULL, NULL);
			}
			
			void AudioDriver::Pause() {
				std::lock_guard<std::mutex> lg(m_threadSafetyLock);

				Core::Log() << ("SLES Player pausing\n");

				if (m_bqPlayerPlay)
				{
					SAFE_SLES_STMT_IGNORE((*m_bqPlayerPlay)->SetPlayState(m_bqPlayerPlay, SL_PLAYSTATE_PAUSED));
					
					Core::Log() << ("SLES Player paused\n");
				}//if (m_bqPlayerPlay)
				
				m_playing = false;
			}
			
			void AudioDriver::Resume() {
				std::lock_guard<std::mutex> lg(m_threadSafetyLock);
				
				ResumeNoLock();
			}
			
			void AudioDriver::ResumeNoLock() {
				Core::Log() << ("SLES Player resuming\n");

				if (m_bqPlayerPlay)
				{
					SAFE_SLES_STMT_IGNORE((*m_bqPlayerPlay)->SetPlayState(m_bqPlayerPlay, SL_PLAYSTATE_PLAYING));

					Core::Log() << ("SLES Player resumed\n");
				}//if (m_bqPlayerPlay)
				
				m_playing = true;
			}
			
			inline uint AudioDriver::GetNumChannels() {
				static_assert(Api::Sound::SPEAKER_MONO == 0 && Api::Sound::SPEAKER_STEREO == 1, "Unaware enum values");
				uint numChannels = m_channels + 1;
				
				return numChannels;
			}
			
			uint AudioDriver::GetExpectedFrameRate() {
				auto frameRate = Api::Machine(m_emulator).GetMode() == Api::Machine::PAL ? (DEFAULT_FRAME_RATE * 5 / 6) : DEFAULT_FRAME_RATE;
				
				return frameRate;
			}
			
			inline uint AudioDriver::GetExpectedSamplesPerFrame(uint sampleRate) {
				auto frameRate = GetExpectedFrameRate();
				auto frameSizeInSamples = sampleRate / frameRate;
				
				return frameSizeInSamples;
			}

			void AudioDriver::InitSLES() {
				// create engine
				SAFE_SLES_STMT_RETURN(slCreateEngine(&m_engineObject, 0, NULL, 0, NULL, NULL));

				// realize the engine
				SAFE_SLES_STMT_RETURN((*m_engineObject)->Realize(m_engineObject, SL_BOOLEAN_FALSE));

				// get the engine interface, which is needed in order to create other objects
				SAFE_SLES_STMT_RETURN((*m_engineObject)->GetInterface(m_engineObject, SL_IID_ENGINE, &m_engineEngine));

				// create output mix
				SAFE_SLES_STMT_RETURN((*m_engineEngine)->CreateOutputMix(m_engineEngine, &m_outputMixObject, 0, NULL, NULL));

				// realize the output mix
				SAFE_SLES_STMT_RETURN((*m_outputMixObject)->Realize(m_outputMixObject, SL_BOOLEAN_FALSE));
			}
			
			void AudioDriver::InitJni()
			{
				auto jenv = Jni::GetCurrenThreadJEnv();
				
				/*------ common stuffs ---*/
				jclass jAudioFormatCls;
				SAFE_ASSIGN_JNI(jAudioFormatCls, jenv->FindClass("android/media/AudioFormat"))

				if (jAudioFormatCls != NULL)
				{
					jfieldID ENCODING_PCM_16BIT_ID, ENCODING_PCM_8BIT_ID;

					SAFE_ASSIGN_JNI(ENCODING_PCM_16BIT_ID, jenv->GetStaticFieldID(jAudioFormatCls, "ENCODING_PCM_16BIT", "I"))
					SAFE_ASSIGN_JNI(ENCODING_PCM_8BIT_ID, jenv->GetStaticFieldID(jAudioFormatCls, "ENCODING_PCM_8BIT", "I"))

					SAFE_ASSIGN_JNI(ENCODING_PCM_16BIT, jenv->GetStaticIntField(jAudioFormatCls, ENCODING_PCM_16BIT_ID))
					SAFE_ASSIGN_JNI(ENCODING_PCM_8BIT, jenv->GetStaticIntField(jAudioFormatCls, ENCODING_PCM_8BIT_ID))
				}
			
				/*------ AudioRecord related stuffs ---*/
				jclass jAudioRecordCls,	jAudioSourceCls; 
				SAFE_ASSIGN_JNI(jAudioRecordCls, jenv->FindClass("android/media/AudioRecord"))
				SAFE_ASSIGN_JNI(jAudioSourceCls, jenv->FindClass("android/media/MediaRecorder$AudioSource"))
				
#if !DISABLE_RECORDER
				if (jAudioRecordCls != NULL && jAudioFormatCls != NULL && jAudioSourceCls != NULL)
				{
					m_jAudioRecordCls = (jclass)jenv->NewGlobalRef(jAudioRecordCls);
					
					SAFE_ASSIGN_JNI(m_recConstructorMethod, jenv->GetMethodID(m_jAudioRecordCls, "<init>", "(IIIII)V"))
					SAFE_ASSIGN_JNI(m_recGetStateMethod, jenv->GetMethodID(m_jAudioRecordCls, "getState", "()I"))
					SAFE_ASSIGN_JNI(m_recStartMethod, jenv->GetMethodID(m_jAudioRecordCls, "startRecording", "()V"))
					SAFE_ASSIGN_JNI(m_recStopMethod, jenv->GetMethodID(m_jAudioRecordCls, "stop", "()V"))
					SAFE_ASSIGN_JNI(m_recReleaseMethod, jenv->GetMethodID(m_jAudioRecordCls, "release", "()V"))
					SAFE_ASSIGN_JNI(m_recGetMinBufferSizeMethod, jenv->GetStaticMethodID(m_jAudioRecordCls, "getMinBufferSize", "(III)I"))
				
					jfieldID CHANNEL_IN_MONO_ID, CHANNEL_IN_STEREO_ID, 
							 REC_STATE_INITIALIZED_ID,
							 DEFAULT_RECORD_SOURCE_ID, VOICE_COMMUNICATION_ID, MIC_ID;
					
					SAFE_ASSIGN_JNI(REC_STATE_INITIALIZED_ID, jenv->GetStaticFieldID(m_jAudioRecordCls, "STATE_INITIALIZED", "I"))
					
					SAFE_ASSIGN_JNI(CHANNEL_IN_MONO_ID, jenv->GetStaticFieldID(jAudioFormatCls, "CHANNEL_IN_MONO", "I"))
					SAFE_ASSIGN_JNI(CHANNEL_IN_STEREO_ID, jenv->GetStaticFieldID(jAudioFormatCls, "CHANNEL_IN_STEREO", "I"))
					
					SAFE_ASSIGN_JNI(DEFAULT_RECORD_SOURCE_ID, jenv->GetStaticFieldID(jAudioSourceCls, "DEFAULT", "I"))
					SAFE_ASSIGN_JNI(VOICE_COMMUNICATION_ID, jenv->GetStaticFieldID(jAudioSourceCls, "VOICE_COMMUNICATION", "I"))
					SAFE_ASSIGN_JNI(MIC_ID, jenv->GetStaticFieldID(jAudioSourceCls, "MIC", "I"))
					
					
					SAFE_ASSIGN_JNI(REC_STATE_INITIALIZED, jenv->GetStaticIntField(m_jAudioRecordCls, REC_STATE_INITIALIZED_ID))
					
					SAFE_ASSIGN_JNI(CHANNEL_IN_MONO, jenv->GetStaticIntField(jAudioFormatCls, CHANNEL_IN_MONO_ID))
					SAFE_ASSIGN_JNI(CHANNEL_IN_STEREO, jenv->GetStaticIntField(jAudioFormatCls, CHANNEL_IN_STEREO_ID))
					
					SAFE_ASSIGN_JNI(DEFAULT_RECORD_SOURCE, jenv->GetStaticIntField(jAudioSourceCls, DEFAULT_RECORD_SOURCE_ID))
					SAFE_ASSIGN_JNI(VOICE_COMMUNICATION, jenv->GetStaticIntField(jAudioSourceCls, VOICE_COMMUNICATION_ID))
					SAFE_ASSIGN_JNI(MIC_RECORD_SOURCE, jenv->GetStaticIntField(jAudioSourceCls, MIC_ID))
				}//if (jAudioRecordCls != NULL && jAudioFormatCls != NULL && jAudioSourceCls != NULL)
#endif//if !DISABLE_RECORDER	
			
				// cleanup
				if (jAudioFormatCls)
					jenv->DeleteLocalRef(jAudioFormatCls);
				if (jAudioRecordCls)
					jenv->DeleteLocalRef(jAudioRecordCls);
				if (jAudioSourceCls)
					jenv->DeleteLocalRef(jAudioSourceCls);
			}
			
			void AudioDriver::Release(bool inputOnly)
			{
				auto jenv = Jni::GetCurrenThreadJEnv();

				if (!inputOnly) {
					// invalidate opensles player
					if (m_bqPlayerObject != NULL) {
						if (m_bqPlayerPlay)
							SAFE_SLES_STMT_IGNORE((*m_bqPlayerPlay)->SetPlayState(m_bqPlayerPlay, SL_PLAYSTATE_STOPPED));
						if (m_bqPlayerBufferQueue) {
							SAFE_SLES_STMT_IGNORE(
									(*m_bqPlayerBufferQueue)->Clear(m_bqPlayerBufferQueue));
						}

						(*m_bqPlayerObject)->Destroy(m_bqPlayerObject);

						m_bqPlayerObject = NULL;
						m_bqPlayerPlay = NULL;
						m_bqPlayerBufferQueue = NULL;
						m_bqPlayerVolume = NULL;
					}
				} // if (!inputOnly)

				// stop recording
				StopInputBufferFillingThread();
				if (m_jAudioRecord)
				{
					jenv->CallVoidMethod(m_jAudioRecord, m_recStopMethod);
					jenv->CallVoidMethod(m_jAudioRecord, m_recReleaseMethod);
					jenv->DeleteGlobalRef(m_jAudioRecord);
					
					m_jAudioRecord = NULL;
				}
				
				m_jrecByteBuffer = nullptr;
				
				m_recBuffer = nullptr;
			}
			
			void AudioDriver::Reset(bool inputOnly)
			{
				Release(inputOnly);
				
				auto jenv = Jni::GetCurrenThreadJEnv();
				
				assert(m_sampleBits == 8 || m_sampleBits == 16);
				
				m_mode = Api::Machine(m_emulator).GetMode();
				
				auto numChannels = GetNumChannels();
				auto frameLength = 1000 / GetExpectedFrameRate();//ms
				auto samplesPerFrame = GetExpectedSamplesPerFrame(m_sampleRate);
				
				m_sampleBlockAlign = m_sampleBits / 8 * numChannels;
				jint bufferSizeInBytes = LATENCY / frameLength * samplesPerFrame * m_sampleBlockAlign;

				// opensles player
				if (!inputOnly)
					ResetSLESPlayer(bufferSizeInBytes, samplesPerFrame * m_sampleBlockAlign);

				// AudioRecord
				ResetAudioRecord(jenv, bufferSizeInBytes);
				
				SetOutputVolumeNoLock(m_volume);
			}

			void AudioDriver::ResetSLESPlayer(uint desiredBufferSize, uint frameSizeInBytes)
			{
				if (!m_engineEngine)
					return;

				Core::Log() << "AudioDriver::ResetSLESPlayer()";

				// Android buffer queue's samplerate is actually samples per millisecond
				auto bqPlayerSampleRate = m_sampleRate * 1000;

				// create buffer
				try {
					m_playBuffer.resize(desiredBufferSize);
					m_playTempBuffer.resize(desiredBufferSize);
				} catch (...) {
					m_playBuffer.resize(0);
					return;
				}

				// divide buffer into several sub buffers to queue to android's sles buffer queue
				m_playSubBufferSize = frameSizeInBytes;
				auto numSubBuffers = m_playBuffer.size() / m_playSubBufferSize;

				{
					std::unique_lock<std::mutex> lg2(m_playBufferLock);

					m_playQueuedSubBuffers.RemoveAll();
					m_playUnqueuedSubBuffers.RemoveAll();

					for (size_t i = 0; i < numSubBuffers; ++i) {
						m_playUnqueuedSubBuffers.PushBack(m_playBuffer.data() + m_playSubBufferSize * i);
					}
				}

				// configure audio source
				SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
						SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
						numSubBuffers};
				SLDataFormat_PCM format;
				format.formatType = SL_DATAFORMAT_PCM;
				format.numChannels = m_channels == Api::Sound::SPEAKER_MONO? 1: 2;
				format.samplesPerSec = bqPlayerSampleRate; //mHz
				format.bitsPerSample = m_sampleBits;
				format.containerSize = m_sampleBits;
				format.channelMask = SL_SPEAKER_FRONT_CENTER;
				format.endianness = SL_BYTEORDER_LITTLEENDIAN;

				SLDataSource audioSrc = {&loc_bufq, &format};

				// configure audio sink
				SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, m_outputMixObject};
				SLDataSink audioSnk = {&loc_outmix, NULL};

				/*
				 * create audio player
				 */
				const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
				const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

				SAFE_SLES_STMT_RETURN((*m_engineEngine)->CreateAudioPlayer(m_engineEngine, &m_bqPlayerObject, &audioSrc, &audioSnk,
															2, ids, req));

				// realize player
				SAFE_SLES_STMT_RETURN((*m_bqPlayerObject)->Realize(m_bqPlayerObject, SL_BOOLEAN_FALSE));

				// get the volume interface
				SAFE_SLES_STMT_IGNORE((*m_bqPlayerObject)->GetInterface(m_bqPlayerObject, SL_IID_VOLUME, &m_bqPlayerVolume));

				// get the play interface
				SAFE_SLES_STMT_RETURN((*m_bqPlayerObject)->GetInterface(m_bqPlayerObject, SL_IID_PLAY, &m_bqPlayerPlay));

				// get the buffer queue interface
				SAFE_SLES_STMT_RETURN((*m_bqPlayerObject)->GetInterface(m_bqPlayerObject, SL_IID_BUFFERQUEUE,
														 &m_bqPlayerBufferQueue));

				// register callback on the buffer queue
				SAFE_SLES_STMT_RETURN((*m_bqPlayerBufferQueue)->RegisterCallback(m_bqPlayerBufferQueue, SLESBufferQueueCallback, this));

				//start playing
				if (m_playing)
					ResumeNoLock();
			}
			
			void AudioDriver::ResetAudioRecord(JNIEnv* jenv, uint desiredBufferSize)
			{	
				if (!m_jAudioRecordCls || !m_hasRecordPermission)
					return;

				Core::Log() << "AudioDriver::ResetAudioRecord()";

				jint jencoding = m_sampleBits == 8 ? ENCODING_PCM_8BIT : ENCODING_PCM_16BIT;
				
				//create AudioRecord
				jint jchannels = m_channels == Api::Sound::SPEAKER_MONO? CHANNEL_IN_MONO : CHANNEL_IN_STEREO;
				jint bufferSize = desiredBufferSize;
				
				auto jAudioRecord = CreateAudioRecord(jenv, VOICE_COMMUNICATION, jchannels, jencoding, bufferSize);
				if (!jAudioRecord)
					jAudioRecord = CreateAudioRecord(jenv, MIC_RECORD_SOURCE, jchannels, jencoding, bufferSize);
				if (!jAudioRecord)
					jAudioRecord = CreateAudioRecord(jenv, DEFAULT_RECORD_SOURCE, jchannels, jencoding, bufferSize);
													  
				if (jAudioRecord)
				{	
					m_jAudioRecord = jenv->NewGlobalRef(jAudioRecord);
					jenv->DeleteLocalRef(jAudioRecord);
				}
				else
					return;
				
				//create byte buffer & choose appropriate read method
				switch (m_sampleBits)
				{
				case 8:
					m_jrecByteBuffer = std::unique_ptr<Jni::Buffer>(new Jni::ByteBuffer(jenv, bufferSize));
					
					SAFE_ASSIGN_JNI(m_recReadMethod, jenv->GetMethodID(m_jAudioRecordCls, "read", "([BII)I"))
					break;
				case 16:
					m_jrecByteBuffer = std::unique_ptr<Jni::Buffer>(new Jni::ShortBuffer(jenv, bufferSize / sizeof(jshort)));
					
					SAFE_ASSIGN_JNI(m_recReadMethod, jenv->GetMethodID(m_jAudioRecordCls, "read", "([SII)I"))
					break;
				}
				//verify
				if (m_jrecByteBuffer && m_jrecByteBuffer->GetJniHandle() == NULL)
					m_jrecByteBuffer = nullptr;
				
				//create ring buffer & polling thread
				if (m_jrecByteBuffer)
				{
					try {
						m_recBuffer = std::unique_ptr<ByteRingBuffer>(new ByteRingBuffer(m_jrecByteBuffer->GetSizeInBytes()));
					
						//update recoding state
						auto wasCapturing = m_capturing;
						m_capturing = false;
						EnableInputNoLock(wasCapturing);
					} catch (...) {
						m_jrecByteBuffer = nullptr;
					}
				}
			}
			
			jobject AudioDriver::CreateAudioRecord(JNIEnv* jenv, jint source, jint jchannels, jint jencoding, jint &bufferSize, bool safeMode)
			{
				jobject jAudioRecord = NULL;
				const jint safe_sample_rate = 44100;
				jint jsampleRate = safeMode ? safe_sample_rate : m_sampleRate;
				
				jint desiredBufferSize = bufferSize;
				jint minBufferSize = jenv->CallStaticIntMethod(m_jAudioRecordCls, m_recGetMinBufferSizeMethod, jsampleRate, jchannels, jencoding);
				jint jbufferSizeInBytes = max(desiredBufferSize, minBufferSize);
				
				SAFE_ASSIGN_JNI(jAudioRecord, jenv->NewObject(m_jAudioRecordCls, m_recConstructorMethod, 
															  source,
															  jsampleRate,
															  jchannels,
															  jencoding,
															  jbufferSizeInBytes));
															 
				//verify
				if (jAudioRecord)
				{
					if (jenv->CallIntMethod(jAudioRecord, m_recGetStateMethod) != REC_STATE_INITIALIZED)
					{
						jenv->CallVoidMethod(jAudioRecord, m_recReleaseMethod);
						jenv->DeleteLocalRef(jAudioRecord);
						jAudioRecord = NULL;
					}
					else
					{	
						bufferSize = jbufferSizeInBytes;//return adjusted buffer size
						m_recSampleRate = jsampleRate;//save adjusted sample rate
					}
				}	
				
				if (jAudioRecord == NULL && !safeMode && m_sampleRate > safe_sample_rate)//try again with "safe" settings			
				{	
					Core::Log() << "Audio: CreateAudioRecord() retries with safe settings";
					jAudioRecord = CreateAudioRecord(jenv, source, jchannels, jencoding, bufferSize, true);
				}
				return jAudioRecord;
			}
			
			void AudioDriver::SetOutputVolume(float gain) {
				std::lock_guard<std::mutex> lg(m_threadSafetyLock);
				
				SetOutputVolumeNoLock(gain);
			}
			
			void AudioDriver::SetOutputVolumeNoLock(float gain) {
				m_volume = gain;
				
				if (!m_bqPlayerVolume)
					return;

				// convert to millibels
				auto mdb = gain < 0.01F ? -96.0F : 20 * log10(gain);
				mdb *= 100;

				(*m_bqPlayerVolume)->SetVolumeLevel(m_bqPlayerVolume, (SLmillibel)mdb);
			}
			
			void AudioDriver::EnableInput(bool enable)
			{
				Core::Log() << "AudioDriver: entering EnableInput()";

				std::lock_guard<std::mutex> lg(m_threadSafetyLock);

				Core::Log() << "AudioDriver: enabling input(" << (enable ? 1 : 0) << ")";

				EnableInputNoLock(enable);

				Core::Log() << "AudioDriver: finished enabling input(" << (enable ? 1 : 0) << ")";
			}

			void AudioDriver::InputPermissionChanged(bool hasPermission) {
				Core::Log() << "AudioDriver: entering InputPermissionChanged(" << hasPermission << ")";

				std::lock_guard<std::mutex> lg(m_threadSafetyLock);

				if (hasPermission != m_hasRecordPermission) {
					m_hasRecordPermission = hasPermission;

					Reset(true);
				}

				Core::Log() << "AudioDriver: finished InputPermissionChanged(" << hasPermission << ")";
			}
			
			void AudioDriver::EnableInputNoLock(bool enable) {
				if (m_capturing != enable)
				{
					m_capturing = enable;
					if (m_jAudioRecord)
					{
						auto jenv = Jni::GetCurrenThreadJEnv();
						
						if (m_capturing)
						{
							jenv->CallVoidMethod(m_jAudioRecord, m_recStartMethod);
							
							StartInputBufferFillingThread();
						}
						else//if (m_capturing)
						{
							jenv->CallVoidMethod(m_jAudioRecord, m_recStopMethod);

							StopInputBufferFillingThread();
						}//if (m_capturing)
						
						if (jenv->ExceptionOccurred())
						{
							jenv->ExceptionDescribe();
							jenv->ExceptionClear();
						}
					}//if (m_jAudioRecord)
				}//if (m_capturing != enable)
			}

			void AudioDriver::SLESBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq) {
				std::unique_lock<std::mutex> lg(m_playBufferLock);

				if (m_playQueuedSubBuffers.GetSize() == 0)
					return;

				// push the oldest sub buffer to unqueued list
				auto queuedBuffer = m_playQueuedSubBuffers.GetBack();

				m_playQueuedSubBuffers.PopBack();
				m_playUnqueuedSubBuffers.PushFront(queuedBuffer);
			}
				
			bool AudioDriver::Lock(Api::Sound::Output& output) {
				std::lock_guard<std::mutex> lg(m_threadSafetyLock);
				
				if (Api::Machine(m_emulator).GetMode() != m_mode)
					Reset();
				if (m_bqPlayerBufferQueue == NULL || !m_playing || m_playBuffer.size() == 0)
					return false;

				size_t numAvailableSubBuffers = 0;

				{
					// retrieve number of available sub regions
					std::unique_lock<std::mutex> lg2(m_playBufferLock);

					if ((numAvailableSubBuffers = m_playUnqueuedSubBuffers.GetSize()) == 0)
						return false;


				}

				// write to temp buffer 1st
				output.length[0] = numAvailableSubBuffers * m_playSubBufferSize / m_sampleBlockAlign;
				output.samples[0] = m_playTempBuffer.data();

				output.length[1] = 0;
				output.samples[1] = NULL;

				return true;
			} 
			
			void AudioDriver::Unlock(Api::Sound::Output& output) {
				std::lock_guard<std::mutex> lg(m_threadSafetyLock);
				
				if (m_bqPlayerBufferQueue == NULL || m_playBuffer.size() == 0)
					return;

				uint8_t * ptr1;
				size_t remainSize;

				ptr1 = (uint8_t*)output.samples[0];
				remainSize = output.length[0] * m_sampleBlockAlign;

				{
					std::unique_lock<std::mutex> lg2(m_playBufferLock);

					// transfer data from temp buffer to available sub regions and enqueue to opensles
					while (remainSize > 0 && ptr1 != nullptr) {
						auto availableSubBuffer = m_playUnqueuedSubBuffers.GetBack();

						// sanity check that the pointer is in valid buffer's region
						if (availableSubBuffer < m_playBuffer.data()
							|| availableSubBuffer + m_playSubBufferSize > m_playBuffer.data() + m_playBuffer.size())
						{
							Core::Log() << "AudioDriver::Lock() returns invalid pointer "
										<< availableSubBuffer << ", valid="
										<< m_playBuffer.data() << "-" << m_playBuffer.data() + m_playBuffer.size();

							break;
						}

						// write to the region
						auto sizeToWrite = min(remainSize, m_playSubBufferSize);
						memcpy(availableSubBuffer, ptr1, sizeToWrite);

						// submit to opensles
						auto slre = (*m_bqPlayerBufferQueue)->Enqueue(m_bqPlayerBufferQueue, availableSubBuffer, sizeToWrite);
						if (slre != SL_RESULT_SUCCESS)
							break;

						// pop this region from the available list
						m_playUnqueuedSubBuffers.PopBack();

						// mark this region as queued sub buffer
						m_playQueuedSubBuffers.PushFront(availableSubBuffer);

						// next region
						remainSize -= sizeToWrite;
						ptr1 += sizeToWrite;
					}
				}

#if SPECIAL_LOG
				Core::Log() << "Audio: " << (size1) << " bytes written";
#endif
			}
			
			void AudioDriver::UpdateSettings() {
				std::lock_guard<std::mutex> lg(m_threadSafetyLock);
				
				Api::Sound sound(m_emulator);
				
				if (m_sampleRate != sound.GetSampleRate() ||
					m_sampleBits != sound.GetSampleBits() ||
					m_channels != sound.GetSpeaker())
				{
					m_sampleRate = m_recSampleRate = sound.GetSampleRate();
					m_sampleBits = sound.GetSampleBits();
					m_channels = sound.GetSpeaker();
					
					Reset();
				}
			}
			
			void AudioDriver::StartInputBufferFillingThread()
			{
				m_recBufferThreadRunning = true;
				
				m_recBufferThread = std::unique_ptr<std::thread>(new std::thread([this]() {
					InputBufferFillingProc();
				}));
			}
			
			void AudioDriver::StopInputBufferFillingThread()
			{
				m_recBufferThreadRunning = false;
				
				{
					std::lock_guard<std::mutex> lg(m_recBufferLock);
					
					m_recBufferCv.notify_all();
				}
				
				if (m_recBufferThread && m_recBufferThread->joinable())
					m_recBufferThread->join();
				
				m_recBufferThread = nullptr;
			}
			
			void AudioDriver::InputBufferFillingProc() {
				auto jenv = Jni::GetCurrenThreadJEnv();
				
				uint numChannels = GetNumChannels();
				
				while (m_recBufferThreadRunning)
				{
					//read data from recorder
					auto frameSizeInElems = GetExpectedSamplesPerFrame(m_recSampleRate) * numChannels;
					auto readElems = jenv->CallIntMethod(m_jAudioRecord, m_recReadMethod, m_jrecByteBuffer->GetJniHandle(), 0, frameSizeInElems);
					auto remainBytes = readElems * m_jrecByteBuffer->GetElemSizeInBytes();
					size_t readOffset = 0;
					
					//write to input buffer
					while (remainBytes > 0)
					{
						std::unique_lock<std::mutex> lk(m_recBufferLock);
						
						//block until input buffer has room for additional data from recorder
						m_recBufferCv.wait(lk, [this] {return !(m_recBufferThreadRunning && m_recBuffer->Full()); });
						
						if (!m_recBufferThreadRunning)
							break;//stop
						
						unsigned char* ptr[2];
						size_t writeRegionSize[2];  
						
						if (!m_recBuffer->BeginWrite(ptr[0], writeRegionSize[0], ptr[1], writeRegionSize[1]))
							continue;
						lk.unlock();//we don't really need to lock the buffer while writing into it
						
						uint sizeToCopy = min(remainBytes, writeRegionSize[0]);
						
						m_jrecByteBuffer->CopyTo(jenv, ptr[0], readOffset, sizeToCopy);
						
						writeRegionSize[0] = sizeToCopy;
						
						remainBytes -= sizeToCopy;
						readOffset += sizeToCopy;
						
						if (remainBytes > 0 && writeRegionSize[1] > 0)
						{
							uint sizeToCopy = min(remainBytes, writeRegionSize[1]);
						
							m_jrecByteBuffer->CopyTo(jenv, ptr[1], readOffset, sizeToCopy);
							
							writeRegionSize[1] = sizeToCopy;
							
							remainBytes -= sizeToCopy;
							readOffset += sizeToCopy;
						}
						else
							writeRegionSize[1] = 0;
						
						lk.lock();//lock the buffer before updating its pointers
						m_recBuffer->EndWrite(ptr[0], writeRegionSize[0], ptr[1], writeRegionSize[1]);
						
#if SPECIAL_LOG
						Core::Log() << (writeRegionSize[0] + writeRegionSize[1]) << " read from recorder. Buffer filled = " << m_recBuffer->Size();
#endif
					}//while (remainBytes)
				}//while (m_recBufferThreadRunning)
			}
			
			uint AudioDriver::ReadInput(Api::Sound::Input& input, void* samples, uint maxSamples) {
				std::lock_guard<std::mutex> lg(m_threadSafetyLock);
				
				if (m_jAudioRecord == NULL || m_jrecByteBuffer == NULL || (maxSamples == 0 && samples != NULL))
					return 0;
				
				std::unique_lock<std::mutex> lk(m_recBufferLock);
				
				if (samples == NULL)//return number of available samples without reading data
				{
					return m_recBuffer->Size() / m_sampleBlockAlign;
				}
				
				auto re = m_recBuffer->Pop((unsigned char*)samples, maxSamples * m_sampleBlockAlign) / m_sampleBlockAlign;
				
				m_recBufferCv.notify_all();
				
				return re;
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

			void AudioDriver::SLESBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
				auto driver = reinterpret_cast<AudioDriver*>(context);

				return driver->SLESBufferQueueCallback(bq);
			}
		}
	}
}