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

#include <Mmdeviceapi.h>

#include "AudioDriver.hpp"

#include "../../core/NstLog.hpp"
#include "../../core/api/NstApiMachine.hpp"

#include <assert.h>
#include <string.h>

#include <wrl\implements.h>

#define LATENCY 83
#define DEFAULT_FRAME_RATE 60

#define DISABLE_RECORDER 0
#define SPECIAL_LOG 0

#ifdef max
#	undef max
#endif

#ifdef min
#	undef min
#endif

#ifndef __max__
#	define __max__(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef __min__
#	define __min__(a,b) ((a) < (b) ? (a) : (b))
#endif

#define MONO SPEAKER_FRONT_CENTER
#define STEREO (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT)
#define QUAD (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT)
#define X5DOT1 (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT)
#define X5DOT1SIDE (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT)
#define X6DOT1 (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_CENTER|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT)
#define X7DOT1 (SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY|SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT|SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT)


using namespace Microsoft::WRL;
using namespace Nes::WinRT::Utils;

namespace Nes {
	namespace Sound {
		namespace WinRT {
			struct soxr_io_spec_ex : public soxr_io_spec_t {
				double isample_rate;//input sample rate
				double osample_rate;//output sample rate

				uint num_channels;
			};

			//helpers
			static soxr_datatype_t get_soxr_datatype_integer(UINT bitsPerSample) {
				switch (bitsPerSample)
				{
				case 16:
					return SOXR_INT16_I;
				case 32:
					return SOXR_INT32_I;
				}

				//not supported
				return (soxr_datatype_t)-1;
			}

			static soxr_datatype_t get_soxr_datatype_float(UINT bitsPerSample) {
				switch (bitsPerSample)
				{
				case 32:
					return SOXR_FLOAT32_I;
				case 64:
					return SOXR_FLOAT64_I;
				}

				//not supported
				return (soxr_datatype_t)-1;
			}

			static soxr_datatype_t get_soxr_datatype(const WAVEFORMATEX* pwfx) {
				if (!pwfx)
					return (soxr_datatype_t)-1;

				switch (pwfx->wFormatTag) {
				case WAVE_FORMAT_PCM:
					return get_soxr_datatype_integer(pwfx->wBitsPerSample);

				case WAVE_FORMAT_IEEE_FLOAT:
					return get_soxr_datatype_float(pwfx->wBitsPerSample);
				case WAVE_FORMAT_EXTENSIBLE:
				{
					const WAVEFORMATEXTENSIBLE *pWaveFormatExtensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(pwfx);
					if (pWaveFormatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
					{
						return get_soxr_datatype_integer(pWaveFormatExtensible->Samples.wValidBitsPerSample);
					}
					else if (pWaveFormatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
					{
						return get_soxr_datatype_float(pWaveFormatExtensible->Samples.wValidBitsPerSample);
					}
				}
					break;
				}

				//error
				return (soxr_datatype_t)-1;
			}

			static size_t get_soxr_datatype_max_value(soxr_datatype_t type) {
				size_t maxIValue = 1;

				switch (type) {
				case SOXR_INT16_I:
					maxIValue = INT16_MAX;
					break;
				case SOXR_INT32_I:
					maxIValue = INT32_MAX;
					break;
				}

				return maxIValue;
			}

			static size_t get_soxr_datatype_size(soxr_datatype_t type) {
				size_t size = 0;

				switch (type) {
				case SOXR_INT16_I:
					size = 2;
					break;
				case SOXR_INT32_I:
				case SOXR_FLOAT32_I:
					size = 4;
					break;
				case SOXR_FLOAT64_I:
					size = 8;
					break;
				}

				return size;
			}

			template <class I, class F>
			static void format_converter_i2f(const I* input, F* output, size_t numSamples) {
				for (size_t i = 0; i < numSamples; ++i)
				{
					output[i] = (F)input[i] / std::numeric_limits<I>::max();
				}
			}

			template <class F, class I>
			static void format_converter_f2i(const F* input, I* output, size_t numSamples) {
				for (size_t i = 0; i < numSamples; ++i)
				{
					output[i] = (I)(input[i] * std::numeric_limits<I>::max());
				}
			}

			template <class F1, class F2>
			static void format_converter_f2f(const F1* input, F2* output, size_t numSamples) {
				for (size_t i = 0; i < numSamples; ++i)
				{
					output[i] = (F2)input[i];
				}
			}

			template <class I1, class I2>
			static void format_converter_i2i(const I1* input, I2* output, size_t numSamples) {
				for (size_t i = 0; i < numSamples; ++i)
				{
					double f1 = (double)input[i] / std::numeric_limits<I1>::max();

					output[i] = (I2)(f1 * std::numeric_limits<I2>::max());
				}
			}

			template <class T1>
			static void format_converter_from_float(const T1* input, void* output, soxr_datatype_t otype, size_t numSamples)
			{
				switch (otype) {
				case SOXR_INT16_I:
					format_converter_f2i(input, (int16_t*)output, numSamples);
					break;
				case SOXR_INT32_I:
					format_converter_f2i(input, (int32_t*)output, numSamples);
					break;
				case SOXR_FLOAT32_I:
					format_converter_f2f(input, (float*)output, numSamples);
					break;
				case SOXR_FLOAT64_I:
					format_converter_f2f(input, (double*)output, numSamples);
					break;
				}
			}

			template <class T1>
			static void format_converter_from_integer(const T1* input, void* output, soxr_datatype_t otype, size_t numSamples)
			{
				switch (otype) {
				case SOXR_INT16_I:
					format_converter_i2i(input, (int16_t*)output, numSamples);
					break;
				case SOXR_INT32_I:
					format_converter_i2i(input, (int32_t*)output, numSamples);
					break;
				case SOXR_FLOAT32_I:
					format_converter_i2f(input, (float*)output, numSamples);
					break;
				case SOXR_FLOAT64_I:
					format_converter_i2f(input, (double*)output, numSamples);
					break;
				}
			}

			static void format_converter(const void* input, soxr_datatype_t itype, void* output, soxr_datatype_t otype, size_t numSamples) {
				if (itype == otype)
				{
					memcpy(output, input, get_soxr_datatype_size(itype) * numSamples);
					return;
				}

				switch (itype) {
				case SOXR_INT16_I:
					format_converter_from_integer(reinterpret_cast<const int16_t*>(input), output, otype, numSamples);
					break;
				case SOXR_INT32_I:
					format_converter_from_integer(reinterpret_cast<const int32_t*>(input), output, otype, numSamples);
					break;
				case SOXR_FLOAT32_I:
					format_converter_from_float(reinterpret_cast<const float*>(input), output, otype, numSamples);
					break;
				case SOXR_FLOAT64_I:
					format_converter_from_float(reinterpret_cast<const double*>(input), output, otype, numSamples);
					break;
				}
			}

			static inline soxr_error_t resample(
				soxr_t resampler,
				const soxr_io_spec_ex& spec,
				const void* input, size_t ilen, size_t *ilenDone, //len is in number of samples
				void* output, size_t olen, size_t *olenDone) //len is in number of samples
			{
				
				soxr_error_t err = NULL;

				if (spec.isample_rate == spec.osample_rate)
				{
					//fast conversion 
					auto len = __min__(ilen, olen);
					format_converter(input, spec.itype, output, spec.otype, len * spec.num_channels);

					if (ilenDone)
						*ilenDone = len;
					if (olenDone)
						*olenDone = len;
				}//if (spec.isample_rate == spec.osample_rate)
				else {
					err = soxr_process(resampler, input, ilen, ilenDone, output, olen, olenDone);
				}

				return err;
			}

			//callback handler to be called when audio interface has been activated
			template <class T>
			class ActivateAudioInterfaceCompletionHandler : public RuntimeClass< RuntimeClassFlags< ClassicCom >, FtmBase, IActivateAudioInterfaceCompletionHandler >
			{
			public:
				ActivateAudioInterfaceCompletionHandler(ComWrapper<T>& pinterface)
					: m_pInterface(pinterface)
				{}

			private:
				~ActivateAudioInterfaceCompletionHandler() {
					//wait until ActivateCompleted finished
					std::unique_lock<std::mutex> lk(m_lock);
				}

			public:

				HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation)
				{
					std::unique_lock<std::mutex> lk(m_lock);

					HRESULT hr = S_OK;
					HRESULT hrActivateResult = S_OK;
					IUnknown *punkAudioInterface = nullptr;

					auto ppInterface = &m_pInterface;

					// Check for a successful activation result 
					hr = operation->GetActivateResult(&hrActivateResult, &punkAudioInterface);
					if (SUCCEEDED(hr) && SUCCEEDED(hrActivateResult))
					{
						// Get the pointer for the Audio Client 
						punkAudioInterface->QueryInterface(ppInterface);
						if (nullptr == ppInterface)
						{
							hr = E_FAIL;
							goto exit;
						}

					}

				exit:
					if (punkAudioInterface)
					{
						punkAudioInterface->Release();
						punkAudioInterface = 0;
					}

					if (FAILED(hr))
					{
						if (*ppInterface)
						{
							static_cast<IUnknown*>(*ppInterface)->Release();
							*ppInterface = 0;
						}
					}

					m_result = hr;

					//notify the waiting thread that we has finished the operation
					m_cv.notify_all();

					return S_OK;
				}

				void ReleaseWaitingWithResult(HRESULT hr) {
					std::unique_lock<std::mutex> lk(m_lock);

					m_result = hr;

					m_cv.notify_all();
				}

				void Wait(HRESULT &hr) {
					std::unique_lock<std::mutex> lk(m_lock);

					Wait(hr, lk);
				}

				void Wait(HRESULT &hr, std::unique_lock<std::mutex>& lk) {
					m_cv.wait(lk);

					hr = m_result;
				}

			private:
				HRESULT m_result;
				std::mutex m_lock;
				std::condition_variable m_cv;

				ComWrapper<T>& m_pInterface;
			};

			/*---------- AudioDriver::AsyncTaskThread ------------*/
			class AudioDriver::AsyncTaskThread {
			public:
				typedef std::function<void(AudioDriver*)> task_t;

				AsyncTaskThread()
					: m_running(true)
				{
					m_thread = std::thread([this] { ThreadLoop(this); });
				}

				~AsyncTaskThread() {
					Invalidate();
					if (m_thread.joinable())
						m_thread.join();
				}

				void QueueTask(AudioDriver* ref, task_t task) {
					std::lock_guard<std::mutex> lg(m_lock);
					ref->AddRef();
					m_tasks.push_back(std::make_pair(ref, task));

					m_cv.notify_all();
				}

				void Invalidate() {
					std::lock_guard<std::mutex> lg(m_lock);
					m_running = false;

					for (auto &task : m_tasks)
					{
						task.first->Release();
					}
					m_tasks.clear();

					m_cv.notify_all();
				}
			private:
				void ThreadLoop() {
					std::unique_lock<std::mutex> lg(m_lock);
					while (m_running) {

						//wait until we have at least one available task
						m_cv.wait(lg, [this] { return !m_running || m_tasks.size() > 0; });

						if (!m_running)
							break;

						std::pair<AudioDriver*, task_t> task;

						//get oldest task
						if (m_tasks.size())
						{
							task = m_tasks.front();
							m_tasks.pop_front();
						}

						lg.unlock();

						//execute the task
						task.second(task.first);
						task.first->Release();

						//sleep for a while
						std::this_thread::sleep_for(std::chrono::milliseconds(2));

						lg.lock();
					}
				}

				static void ThreadLoop(AsyncTaskThread* thiz) {
					thiz->ThreadLoop();
				}

				std::thread m_thread;
				std::mutex m_lock;
				std::condition_variable m_cv;
				std::list<std::pair<AudioDriver*, task_t>> m_tasks;

				bool m_running;
			};

			/*--------- AudioDriverDeleter ----------*/
			class AudioDriverDeleter {
			public:
				void operator() (AudioDriver* driver) {
					driver->Release();
				}
			};

			/*------------ AudioDriver ----------*/
			std::shared_ptr<AudioDriver> AudioDriver::CreateInstanceRef(Api::Emulator& emulator, Windows::UI::Core::CoreDispatcher^ uiDispatcher)
			{
				return std::shared_ptr<AudioDriver>(new AudioDriver(emulator, uiDispatcher), AudioDriverDeleter());
			}

			AudioDriver::AudioDriver(Api::Emulator& emulator, Windows::UI::Core::CoreDispatcher^ uiDispatcher)
				:m_emulator(emulator), m_mode(Api::Machine::NTSC),
				m_playing(false),
				m_capturing(false),
				m_alive(true),
				m_audioFeedingThreadRunning(false),
				m_volume(1.f),
				m_uiDispatcher(uiDispatcher),
				m_asyncTaskThread(std::make_shared<AsyncTaskThread>())
			{
				m_sampleRate = 48000;
				m_sampleBits = 16;
				m_channels = Api::Sound::SPEAKER_MONO;

				Api::Sound sound(m_emulator);
				sound.SetSampleRate(m_sampleRate);
				sound.SetSampleBits(m_sampleBits);
				sound.SetSpeaker(m_channels);
				sound.SetSpeed(Api::Sound::DEFAULT_SPEED);

				//create events
				m_audioFeedingEvent = CreateEventEx(NULL, NULL, 0, DELETE | SYNCHRONIZE | EVENT_MODIFY_STATE);

				//reset
				Reset();

				//register callbacks
				Api::Sound::Output::lockCallback.Set(Lock, this);
				Api::Sound::Output::unlockCallback.Set(Unlock, this);
				Api::Sound::Output::updateSettingsCallback.Set(UpdateSettings, this);

				Api::Sound::Input::readCallback.Set(ReadInput, this);
			}

			AudioDriver::~AudioDriver() {
				m_alive = false;

				m_asyncTaskThread->Invalidate();
				ReleaseResources(true, true);

				if (m_audioFeedingEvent)
					CloseHandle(m_audioFeedingEvent);

				Api::Sound::Output::lockCallback.Set(NULL, NULL);
				Api::Sound::Output::unlockCallback.Set(NULL, NULL);
				Api::Sound::Output::updateSettingsCallback.Set(NULL, NULL);

				Api::Sound::Input::readCallback.Set(NULL, NULL);
			}

			void AudioDriver::SetUIDispatcher(Windows::UI::Core::CoreDispatcher^ uiDispatcher) {
				std::lock_guard<std::mutex> lg(m_dispatcherLock);

				m_uiDispatcher = uiDispatcher;
			}

			void AudioDriver::EnableOutput(bool enable)
			{
				std::lock_guard<std::mutex> lg(m_genericThreadSafetyLock);

				EnableOutputNoLock(enable);
			}

			void AudioDriver::EnableInput(bool enable)
			{
				std::lock_guard<std::mutex> lg(m_genericThreadSafetyLock);

				EnableInputNoLock(enable);
			}

			void AudioDriver::EnableOutputNoLock(bool enable)
			{
				if (m_playing == enable)
					return;

				m_playing = enable;

				if (m_audioClient)
				{
					if (enable)
						m_audioClient->Start();
					else
						m_audioClient->Stop();
				}
			}

			void AudioDriver::EnableInputNoLock(bool enable)
			{
				if (m_capturing != enable)
				{
					m_capturing = enable;
					
					//TODO
				}
			}

			void AudioDriver::SetOutputVolume(float gain) {
				std::lock_guard<std::mutex> lg(m_genericThreadSafetyLock);

				SetOutputVolumeNoLock(gain);
			}

			void AudioDriver::SetOutputVolumeNoLock(float gain) {
				m_volume = gain;

				if (m_audioVolumeControl)
					m_audioVolumeControl->SetMasterVolume(gain, NULL);
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

			void AudioDriver::ActivateAudioDevices(std::unique_lock<std::mutex>& lk) {
				HRESULT hr;
				//activate render audio interface on UI thread
				auto audioClientActivationHandler = Make<ActivateAudioInterfaceCompletionHandler<IAudioClient>>(m_audioClient);

				auto activateTask = [this, audioClientActivationHandler]()
				{
					//get default device id
					auto deviceIdString = Windows::Media::Devices::MediaDevice::GetDefaultAudioRenderId(
						Windows::Media::Devices::AudioDeviceRole::Default);

					IActivateAudioInterfaceAsyncOperation *asyncOp;

					auto hr = ActivateAudioInterfaceAsync(deviceIdString->Data(), __uuidof(IAudioClient2), nullptr, audioClientActivationHandler.Get(), &asyncOp);

					SAFE_RELEASE(asyncOp);

					if (FAILED(hr))
						audioClientActivationHandler->ReleaseWaitingWithResult(hr);//failed, unblock the waiting thread
				};

				{
					std::lock_guard<std::mutex> lg(m_dispatcherLock);

					if (m_uiDispatcher->HasThreadAccess)//this is ui thread
						throw std::runtime_error("AudioDriver's methods must not be called on UI thread");
					else//run it on ui thread
						m_uiDispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
							ref new Windows::UI::Core::DispatchedHandler(activateTask));
				}

				//wait for the activation to finish
				audioClientActivationHandler->Wait(hr, lk);

				if (FAILED(hr)) {
					Core::Log() << "Audio interface activation failed with result=" << hr;
				}
			}

			bool AudioDriver::MakeWaveFormatFromSettings(WAVEFORMATEX* pwfx) {
				uint channels = m_channels == Api::Sound::SPEAKER_MONO ? 1 : 2;
				uint channelMask = m_channels == Api::Sound::SPEAKER_MONO ? MONO : STEREO;

				WAVEFORMATEX fmt;
				fmt.wFormatTag = WAVE_FORMAT_PCM;
				fmt.wBitsPerSample = m_sampleBits;
				fmt.nBlockAlign = m_sampleBlockAlign;
				fmt.nChannels = channels;
				fmt.nSamplesPerSec = m_sampleRate;
				fmt.nAvgBytesPerSec = pwfx->nSamplesPerSec * pwfx->nBlockAlign;
				fmt.cbSize = 0;

				switch (pwfx->wFormatTag)
				{
				case WAVE_FORMAT_EXTENSIBLE:
				{
					WAVEFORMATEXTENSIBLE *pWaveFormatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(pwfx);

					pWaveFormatExtensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
					pWaveFormatExtensible->Format = fmt;
					pWaveFormatExtensible->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
					pWaveFormatExtensible->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(fmt);
					pWaveFormatExtensible->Samples.wValidBitsPerSample =
						pWaveFormatExtensible->Format.wBitsPerSample;

					pWaveFormatExtensible->dwChannelMask = channelMask;

					break;
				}

				default:
					*pwfx = fmt;
					//TODO
					return false;
				}

				return true;
			}

			void AudioDriver::CacheWaveFormatAsExtensible(WAVEFORMATEXTENSIBLE* out, const WAVEFORMATEX* in)
			{
				memset(out, 0, sizeof(*out));
				if (in->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
					*out = *(WAVEFORMATEXTENSIBLE*)in;
				else if (in->wFormatTag == WAVE_FORMAT_PCM)
				{
					out->Format = *in;
					out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
					out->Format.cbSize = sizeof(*out) - sizeof(*in);

					out->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

					out->Samples.wValidBitsPerSample = in->wBitsPerSample;
				}
				else if (in->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
				{
					out->Format = *in;
					out->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
					out->Format.cbSize = sizeof(*out) - sizeof(*in);

					out->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

					out->Samples.wValidBitsPerSample = in->wBitsPerSample;
				}
			}

			void AudioDriver::ReleaseResources(bool immediate, bool lock)
			{
				auto task = [lock] (AudioDriver* driver) {

					if (lock)
						driver->m_genericThreadSafetyLock.lock();

					driver->m_audioFeedingThreadRunning = false;
					SetEvent(driver->m_audioFeedingEvent);

					if (driver->m_audioFeedingThread && driver->m_audioFeedingThread->joinable())
						driver->m_audioFeedingThread->join();
					driver->m_audioFeedingThread = nullptr;

					driver->m_audioClient.SafeRelease();
					driver->m_audioVolumeControl.SafeRelease();
					driver->m_audioRenderClient.SafeRelease();

					if (lock)
						driver->m_genericThreadSafetyLock.unlock();
				};

				if (immediate)
					task(this);
				else
					m_asyncTaskThread->QueueTask(this, task);
			}

			void AudioDriver::Reset()
			{
				m_asyncTaskThread->QueueTask(this, [this](AudioDriver*) {
					std::unique_lock<std::mutex> lg(m_genericThreadSafetyLock);

					ReleaseResources(true, false);

					if (!m_alive || m_audioFeedingEvent == NULL)
						return;

					//activate autdio devices
					ActivateAudioDevices(lg);

					HRESULT hr;
					WAVEFORMATEX *pwfx = NULL;
					WAVEFORMATEX *pwfx2 = NULL;

					uint channels = m_channels == Api::Sound::SPEAKER_MONO ? 1 : 2;

					m_sampleBlockAlign = m_sampleBits / 8 * channels;
					m_mode = Api::Machine(m_emulator).GetMode();

					if (m_audioClient)
					{
						//describe format
						hr = m_audioClient->GetMixFormat(&pwfx);

						REFERENCE_TIME hnsRequestedDuration = LATENCY * 10000;
						//REFERENCE_TIME hnsRequestedDuration, hnsDefaultDevicePeriod;
						//m_audioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, &hnsRequestedDuration);


						if (SUCCEEDED(hr) && MakeWaveFormatFromSettings(pwfx))
						{
							hr = m_audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pwfx, &pwfx2);
							if (hr != S_OK)
							{
								//fallback
								CoTaskMemFree(pwfx);
								pwfx = NULL;

								if (hr == S_FALSE && pwfx2 != NULL)
								{
									pwfx = pwfx2;
									pwfx2 = NULL;

									hr = S_OK;
								}
								else
									hr = m_audioClient->GetMixFormat(&pwfx);
							}
						}

						if (SUCCEEDED(hr) &&
							SUCCEEDED(hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsRequestedDuration, 0, pwfx, NULL))) {

							Core::Log() << "AudioClient successfully initialized";

							//cache this format struct for later use
							CacheWaveFormatAsExtensible(&m_wfx, pwfx);

							//set event handle
							m_audioClient->SetEventHandle(m_audioFeedingEvent);

							//get buffer size (in samples)
							m_audioClient->GetBufferSize(&m_audioClientBufferSize);

							//create ring buffer
							m_audioClientBuffer = std::unique_ptr<ByteRingBuffer>(new ByteRingBuffer(m_audioClientBufferSize * m_sampleBlockAlign));

							//get services
							if (FAILED(hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_audioRenderClient)))
							{
								Core::Log() << "Failed to get Audio Render Client, error=" << hr;
							}

							if (FAILED(hr = m_audioClient->GetService(__uuidof(ISimpleAudioVolume), (void**)&m_audioVolumeControl)))
							{
								Core::Log() << "Failed to get Audio Volume Controller, error=" << hr;
							}

							//re-enable playback
							auto wasPlaying = m_playing;
							m_playing = false;
							EnableOutputNoLock(wasPlaying);

							//reset volume 
							SetOutputVolumeNoLock(m_volume);

							//start data feeding thread
							m_audioFeedingThreadRunning = true;
							m_audioFeedingThread = std::unique_ptr<std::thread>(new std::thread([this] {
								AudioFeedingProc();
							}));
						}//if (SUCCEEDED(hr)) 
						else {
							Core::Log() << "Failed to initalize AudioClient, result=" << hr;

							m_audioClient.SafeRelease();
						}

						if (pwfx)
							CoTaskMemFree(pwfx);
						if (pwfx2)
							CoTaskMemFree(pwfx2);

					}//if (m_audioClient)
				});
			}

			void AudioDriver::FeedAudio(const void* rawResampledData, BYTE* &audioClientDstBuffer, size_t numSamples) {
				HRESULT hr;

				const auto sampleSizePerChannel = m_wfx.Samples.wSamplesPerBlock / 8;
				const auto dst_stride = m_wfx.Format.wBitsPerSample / 8;

				const auto half_channels = m_wfx.Format.nChannels / 2;

				auto dstptr = audioClientDstBuffer;

				const unsigned char* srcptr;
				size_t src_stride;

				if (rawResampledData == NULL)
				{
					//no available data, write last submitted sample
					srcptr = m_lastRenderedSample;
					src_stride = 0;

				}//if (rawResampledData == NULL)
				else {
					srcptr = reinterpret_cast<const unsigned char*>(rawResampledData);
					src_stride = sampleSizePerChannel;
						
				}//if (rawResampledData == NULL)

					//duplicate the raw data's channel for all AudioClient's channels
				switch (m_channels) {
				case Api::Sound::SPEAKER_MONO:
				{
					for (size_t i = 0; i < numSamples; ++i, srcptr += src_stride) {
						//TODO: assume little endian
						for (UINT c = 0; c < m_wfx.Format.nChannels; ++c, dstptr += dst_stride) {
							memcpy(dstptr, srcptr, sampleSizePerChannel);
						}
					}
				}
				break;
				case Api::Sound::SPEAKER_STEREO:
				{
					for (size_t i = 0; i < numSamples; ++i, srcptr += src_stride * 2) {
						//TODO: assume little endian
						UINT c;

						//copy first channel to first half of destination channels
						for (c = 0; c < half_channels; ++c, dstptr += dst_stride) {
							memcpy(dstptr, srcptr, sampleSizePerChannel);
						}

						//copy second channel to second half of destination channels
						for (; c < m_wfx.Format.nChannels; ++c, dstptr += dst_stride) {
							memcpy(dstptr, srcptr + sampleSizePerChannel, sampleSizePerChannel);
						}
					}
				}
				break;
				default:
					//TODO
					break;
				}//switch (m_channels)

				//update destination buffer pointer
				audioClientDstBuffer = dstptr;
			}

			void AudioDriver::AudioFeedingProc() {
				auto inputType = get_soxr_datatype_integer(m_sampleBits);
				auto outputType = get_soxr_datatype(reinterpret_cast<WAVEFORMATEX*>(&m_wfx));

				if (inputType == (soxr_datatype_t)-1)
				{
					Core::Log() << "Unsupported audio format, stopping feeding thread ...";
					return;
				}

				if (outputType == (soxr_datatype_t)-1)
				{
					Core::Log() << "Unsupported Audio Client's format, stopping feeding thread ...";
					return;
				}

				soxr_io_spec_ex spec; 
				memcpy(&spec, &soxr_io_spec(inputType, outputType), sizeof(soxr_io_spec_t));

				spec.isample_rate = m_sampleRate;
				spec.osample_rate = m_wfx.Format.nSamplesPerSec;

				spec.num_channels = m_channels == Api::Sound::SPEAKER_MONO ? 1 : 2;

				//create intermediate buffer
				auto obuffer_sample_block_align = m_wfx.Samples.wValidBitsPerSample / 8 * spec.num_channels;
				auto obuffer_len_in_samples = (size_t)(m_audioClientBuffer->Capacity() / m_sampleBlockAlign * (spec.osample_rate / spec.isample_rate));
				auto obuffer_len = obuffer_len_in_samples * obuffer_sample_block_align;

				if (obuffer_sample_block_align > sizeof(m_lastRenderedSample))
				{
					Core::Log() << "resampled buffer's block align is too big: " << obuffer_sample_block_align;
					return;
				}

				unsigned char* obuffer = new (std::nothrow) unsigned char[obuffer_len];
				if (!obuffer)
					return;

				memset(m_lastRenderedSample, 0, sizeof(m_lastRenderedSample));

				//get device's default perdiod length
				REFERENCE_TIME defaultPer;
				if (FAILED(m_audioClient->GetDevicePeriod(&defaultPer, NULL)))
					return;

				auto periodBufferLenInSamples = __max__(1, defaultPer * m_wfx.Format.nSamplesPerSec / 10000000);

				//create resampler stream
				soxr_error_t soxr_err = 0;
 
				//use low quality resampler
				auto quality_spec = soxr_quality_spec(SOXR_QQ, 0);

				soxr_t soxr = NULL;

				soxr = soxr_create(
						spec.isample_rate, spec.osample_rate, spec.num_channels,             /* Input rate, output rate, # of channels. */
						&soxr_err,                         /* To report any error during creation. */
						&spec, &quality_spec, NULL);

				while (m_audioFeedingThreadRunning) {
					if (WaitForSingleObjectEx(m_audioFeedingEvent, 2000, FALSE) == WAIT_OBJECT_0) {
						//get number of samples writable to audio client
						UINT32 paddingSamples;
						if (FAILED(m_audioClient->GetCurrentPadding(&paddingSamples)))
							continue;

						//get available data from ring buffer
						std::unique_lock<std::mutex> lk(m_audioClientBufferLock);
						const unsigned char* ptr1, *ptr2;
						size_t size1, size2;

						const auto writableSamples = __min__(periodBufferLenInSamples, m_audioClientBufferSize - paddingSamples);

						BYTE* audioClientRenderBuffer;

						if (writableSamples == 0 || FAILED(m_audioRenderClient->GetBuffer(writableSamples, &audioClientRenderBuffer)))
							continue;

						auto remainWritableSamples = writableSamples;

						if (m_audioClientBuffer->BeginRead(ptr1, size1, ptr2, size2, writableSamples * m_sampleBlockAlign))
						{
							lk.unlock();

							auto size1InSamples = size1 / m_sampleBlockAlign;
							auto size2InSamples = size2 / m_sampleBlockAlign;

							//sample rate converting
							if (soxr) {
								//first region
								auto optr = obuffer;
								size_t olenInSamples = obuffer_len_in_samples;

								soxr_err = resample(soxr, spec, ptr1, size1InSamples, NULL, optr, olenInSamples, &olenInSamples);

								if (!soxr_err)
								{
									//submit to audio client
									FeedAudio(optr, audioClientRenderBuffer, olenInSamples);

									remainWritableSamples -= olenInSamples;

									//cache the last submitted sample
									if (olenInSamples > 1)
										memcpy(m_lastRenderedSample, optr + (olenInSamples - 1) * obuffer_sample_block_align, obuffer_sample_block_align);

								}//if (!soxr_err)

								//convert second region
								if (!soxr_err && ptr2)
								{
									optr += olenInSamples * m_sampleBlockAlign;
									olenInSamples = obuffer_len_in_samples - olenInSamples;

									soxr_err = resample(soxr, spec, ptr2, size2InSamples, NULL, optr, olenInSamples, &olenInSamples);

									if (!soxr_err)
									{
										//submit to audio client
										FeedAudio(optr, audioClientRenderBuffer, olenInSamples);

										remainWritableSamples -= olenInSamples;

										//cache the last submitted sample
										if (olenInSamples > 1)
											memcpy(m_lastRenderedSample, optr + (olenInSamples - 1) * obuffer_sample_block_align, obuffer_sample_block_align);
									}//if (!soxr_err)
								}
							}//if (soxr)

							lk.lock();
							m_audioClientBuffer->EndRead(ptr1, size1, ptr2, size2);
							lk.unlock();
						}//if (m_audioClientBuffer->BeginRead(...))

						if (remainWritableSamples > 0)
						{
							//write last submitted samples to the remaining unwritten region
							FeedAudio(NULL, audioClientRenderBuffer, remainWritableSamples);
						}

						m_audioRenderClient->ReleaseBuffer(writableSamples, 0);
					}//WaitForSingleObjectEx
				}//while (m_audioFeedingThreadRunning)

				//cleanup
				soxr_delete(soxr);

				delete[] obuffer;
			}

			bool AudioDriver::Lock(Api::Sound::Output& output) {
				m_genericThreadSafetyLock.lock();
				auto re = DoLock(output);

				if (!re)
					m_genericThreadSafetyLock.unlock();

				return re;
			}

			void AudioDriver::Unlock(Api::Sound::Output& output)
			{
				DoUnlock(output);

				m_genericThreadSafetyLock.unlock();
			}

			bool AudioDriver::DoLock(Api::Sound::Output& output) {
				if (!m_audioClient || !m_audioRenderClient)
					return false;

				auto frameSizeInBytes = GetDesiredSamplesPerFrame() * m_sampleBlockAlign;

				std::unique_lock<std::mutex> lg(m_audioClientBufferLock);

				//size_t available = m_audioClientBuffer->Capacity() - m_audioClientBuffer->Size();
				//available = __min__(available, frameSizeInBytes);

				unsigned char* ptr1, *ptr2;
				size_t size1, size2;
				if (!m_audioClientBuffer->BeginWrite(ptr1, size1, ptr2, size2, frameSizeInBytes)) {
					return false;
				}

				output.length[0] = size1 / m_sampleBlockAlign;
				output.samples[0] = ptr1;

				output.length[1] = size2 / m_sampleBlockAlign;
				output.samples[1] = ptr2;

				return true;
			}

			void AudioDriver::DoUnlock(Api::Sound::Output& output) {
				if (!m_audioClient || !m_audioRenderClient)
					return;

				unsigned char* ptr1, *ptr2;
				size_t size1, size2;

				ptr1 = (unsigned char*)output.samples[0];
				size1 = output.length[0] * m_sampleBlockAlign;
				ptr2 = (unsigned char*)output.samples[1];
				size2 = output.length[1] * m_sampleBlockAlign;

				std::unique_lock<std::mutex> lg(m_audioClientBufferLock);
				m_audioClientBuffer->EndWrite(ptr1, size1, ptr2, size2);
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
				std::lock_guard<std::mutex> lg(m_genericThreadSafetyLock);

				//TODO
				return 0;
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
		}
	}
}