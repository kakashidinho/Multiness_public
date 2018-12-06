////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2016-2018 Le Hoang Quyen
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

#include "NstMachine.hpp"
#include "NstCartridge.hpp"
#include "NstCheats.hpp"
#include "NstNsf.hpp"
#include "NstImageDatabase.hpp"
#include "NstRemoteEvent.hpp"
#include "NstFrameCompressorCommon.hpp"
#if REMOTE_USE_H264
#include "NstFrameCompressorH264.hpp"
#endif
#include "NstFrameCompressorZlib.hpp"
#include "input/NstInpDevice.hpp"
#include "input/NstInpAdapter.hpp"
#include "input/NstInpPad.hpp"
#include "api/NstApiMachine.hpp"
#include "api/NstApiSound.hpp"
#include "api/NstApiUser.hpp"

#include <map>
#include <unordered_map>
#include <list>
#include <sstream>
#include <algorithm>
#include <assert.h>

#define REMOTE_INPUT_SEND_INTERVAL 0.2

#define REMOTE_RCV_RATE_UPDATE_INTERVAL 2.0
#define REMOTE_SND_RATE_UPDATE_INTERVAL 2.0

#define REMOTE_FRAME_BUNDLE 1

#define SIMULATE_SLOW_DATA_RATE 0

#define REMOTE_AUDIO_SAMPLE_RATE 48000
#define REMOTE_AUDIO_STEREO_CHANNELS false

#define REMOTE_PREFERRED_DSCP_VALUE 0 // 34 // AF41

#if defined DEBUG || defined _DEBUG
#	define PROFILE_EXECUTION_TIME 0
#else
#	define PROFILE_EXECUTION_TIME 0
#endif//#if defined DEBUG || defined _DEBUG

#define CLIENT_CONNECTED_STATE 1
#define CLIENT_EXCHANGE_DATA_STATE 0x7fffffff

#ifndef __max__
#	define __max__(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef __min__
#	define __min__(a,b) ((a) < (b) ? (a) : (b))
#endif

namespace Nes
{
	namespace Core
	{
		

		/*--------------- NesFrameCapturer -------------------*/
		class NesFrameCapturer : public HQRemote::IFrameCapturer {
		public:
			NesFrameCapturer(Machine& _emu)
				: HQRemote::IFrameCapturer(0, Core::Video::Screen::WIDTH, Core::Video::Screen::HEIGHT),
				emulator(_emu)
			{
			}

			virtual size_t getFrameSize() override {
				return emulator.GetFrameSize();
			}
			virtual unsigned int getNumColorChannels() override {
				return emulator.GetNumColorChannels();
			}
			virtual void captureFrameImpl(unsigned char * prevFrameDataToCopy) override {
				emulator.CaptureFrame(prevFrameDataToCopy);
			}
		private:
			Machine& emulator;
		};

		class NesServerAudioCapturer : public HQRemote::IAudioCapturer {
		public:
			NesServerAudioCapturer(Machine& machine) : m_machine(machine) {
			}

			virtual uint32_t getAudioSampleRate() const override {
				return m_machine.GetAudioSampleRate();
			}

			virtual uint32_t getNumAudioChannels() const override {
				return m_machine.GetNumAudioChannels();
			}

			//capture current frame. TODO: only 16 bit PCM is supported for now
			virtual HQRemote::ConstDataRef beginCaptureAudio() override {
				return m_machine.CaptureAudioAsServer();
			}

		protected:
			Machine& m_machine;
		};

		class NesClientAudioCapturer : public NesServerAudioCapturer {
		public:
			NesClientAudioCapturer(Machine& machine) : NesServerAudioCapturer(machine) {
			}

			//capture current frame. TODO: only 16 bit PCM is supported for now
			virtual HQRemote::ConstDataRef beginCaptureAudio() override {
				return m_machine.CaptureAudioAsClient();
			}
		};

#ifdef NST_MSVC_OPTIMIZE
#pragma optimize("s", on)
#endif

		/*----------Machine ----------------*/
		Machine::Machine()
			:state(Api::Machine::NTSC),
			frame(0),
			extPort(new Input::AdapterTwo(*new Input::Pad(cpu, 0), *new Input::Pad(cpu, 1))),
			expPort(new Input::Device(cpu)),
			image(NULL),
			cheats(NULL),
			imageDatabase(NULL),
			ppu(cpu),
			lastSentInputId(0), lastSentInputTime(0), 
			avgExecuteTime(0), executeWindowTime(0),
			avgCpuExecuteTime(0), cpuExecuteWindowTime(0),
			avgPpuEndFrameTime(0), ppuEndFrameWindowTime(0),
			avgVideoBlitTime(0), videoBlitWindowTime(0),
			avgFrameCaptureTime(0), frameCaptureWindowTime(0),
			avgAudioCaptureTime(0), audioCaptureWindowTime(0),
			currentInputAudio(NULL)
		{
		}

		Machine::~Machine()
		{
			Unload();

			//frame compressor must be stopped before stopping host engine to prevent deadlock
			if (this->remoteFrameCompressor)
				this->remoteFrameCompressor->Stop();
			if (this->remoteFrameDecompressor)
				this->remoteFrameDecompressor->Stop();
			if (hostEngine)
				hostEngine->stop();
			if (clientEngine)
				clientEngine->stop();

			delete imageDatabase;
			delete cheats;
			delete expPort;

			for (uint ports=extPort->NumPorts(), i=0; i < ports; ++i)
				delete &extPort->GetDevice(i);

			delete extPort;
		}

		Result Machine::Load
		(
			std::istream& imageStream,
			FavoredSystem system,
			bool ask,
			std::istream* const patchStream,
			bool patchBypassChecksum,
			Result* patchResult,
			uint type
		)
		{
			Unload();

			Image::Context context
			(
				static_cast<Image::Type>(type),
				cpu,
				cpu.GetApu(),
				ppu,
				imageStream,
				patchStream,
				patchBypassChecksum,
				patchResult,
				system,
				ask,
				imageDatabase
			);

			image = Image::Load( context );

			switch (image->GetType())
			{
				case Image::CARTRIDGE:

					state |= Api::Machine::CARTRIDGE;

					switch (static_cast<const Cartridge*>(image)->GetProfile().system.type)
					{
						case Api::Cartridge::Profile::System::VS_UNISYSTEM:

							state |= Api::Machine::VS;
							break;

						case Api::Cartridge::Profile::System::PLAYCHOICE_10:

							state |= Api::Machine::PC10;
							break;
					}
					break;

				case Image::DISK:

					state |= Api::Machine::DISK;
					break;

				case Image::SOUND:

					state |= Api::Machine::SOUND;
					break;
			}

			UpdateModels();
			ppu.ResetColorUseCountTable();

			Api::Machine::eventCallback( Api::Machine::EVENT_LOAD, context.result );

			return context.result;
		}

			
		Result Machine::LoadRemote(const std::string& remoteIp, int remotePort, const char* clientInfo)
		{
			HQRemote::ConnectionEndpoint reliableHost(remoteIp.c_str(), remotePort);
			HQRemote::ConnectionEndpoint unreliableHost(remoteIp.c_str(), remotePort + 1);
			
			auto clientConnHandler = std::make_shared<HQRemote::SocketClientHandler>(remotePort + 2, remotePort + 3, reliableHost, unreliableHost);
			
			return LoadRemote(clientConnHandler, clientInfo);
		}
			
		Result Machine::LoadRemote(std::shared_ptr<HQRemote::IConnectionHandler> clientConnHandler, const char* _clientInfo)
		{
			DisableRemoteControllers();
			Unload();

			Result re = RESULT_OK;

			auto audioCapturer = std::make_shared<NesClientAudioCapturer>(*this);

			this->clientEngine = std::make_shared<HQRemote::Client>(clientConnHandler, DEFAULT_REMOTE_FRAME_INTERVAL, audioCapturer);
			this->clientEngine->setDesc(_clientInfo);

			if (!this->clientEngine->start(true))
			{
				this->clientEngine = nullptr;
				re = RESULT_ERR_CONNECTION;
			}
			else {
				this->state |= Api::Machine::REMOTE;
				this->clientState = 0;

				this->clientInfo = _clientInfo != NULL ? _clientInfo : "";

				this->hostName.clear();//invalidate host name until receiving update from host

				// by default use zlib frame decompressor
				UseFrameDecompressorType(FRAME_COMPRESSOR_TYPE_ZLIB);

				this->lastSentInputId = 0;
				this->lastSentInput = 0;
				this->lastSentInputTime = 0;
				this->lastRemoteDataRateUpdateTime = 0;
				this->numBandwidthDetectBytes = 0;

				ResetRemoteAudioBuffer();
				EnsureCorrectRemoteSoundSettings();

				UpdateModels();

				//auto start
				Reset(true);
			}


			Api::Machine::eventCallback(Api::Machine::EVENT_LOAD_REMOTE, re);
			return re;
		}

		void Machine::StopRemoteControl()
		{
			if (remoteFrameDecompressor) {
				remoteFrameDecompressor->Stop();
				remoteFrameDecompressor = nullptr;
			}
			if (clientEngine)
			{
				clientEngine->stop();
				clientEngine = nullptr;

				clientState = 0;

				renderer.ResetState();//TODO: error checking
			}


			state &= ~uint(Api::Machine::REMOTE);
		}

		void Machine::UseFrameCompressorType(int type) {
			if (!this->hostEngine)
				return;
			if (!this->remoteFrameCompressor || this->remoteFrameCompressor->type != type) {
				switch (type) {
#if REMOTE_USE_H264
				case FRAME_COMPRESSOR_TYPE_H264:
					this->remoteFrameCompressor = std::make_shared<H264FrameCompressor>(*this);
					ppu.EnableColorUseCount(false);
					renderer.EnableRenderedFrameCaching(true);
					break;
#endif
				default:
					this->remoteFrameCompressor = std::make_shared<ZlibFrameCompressor>(*this->hostEngine);
					ppu.EnableColorUseCount(true);
					renderer.EnableRenderedFrameCaching(false);
				}

				this->remoteFrameCompressor->Start();
				this->hostEngine->setImageCompressor(this->remoteFrameCompressor);
			}
			else {
				this->remoteFrameCompressor->Restart();
			}
		}

		void Machine::UseFrameDecompressorType(int type) {
			if (!this->clientEngine)
				return;
			if (!this->remoteFrameDecompressor || this->remoteFrameDecompressor->type != type) {
				switch (type) {
#if REMOTE_USE_H264
				case FRAME_COMPRESSOR_TYPE_H264:
					this->remoteFrameDecompressor = std::make_shared<H264FrameDecompressor>(*this, *this->clientEngine);
					this->clientEngine->setMaxPendingFrames(10);
					break;
#endif
				default:
					this->remoteFrameDecompressor = std::make_shared<ZlibFrameDecompressor>(*this, *this->clientEngine);
					this->clientEngine->setMaxPendingFrames(4);
				}
			}

			this->remoteFrameDecompressor->Start();
		}

		Result Machine::EnableRemoteController(uint idx, int networkListenPort, const char* hostInfo) {

			std::shared_ptr<HQRemote::IConnectionHandler> connHandler = std::make_shared<HQRemote::SocketServerHandler>(networkListenPort, networkListenPort + 1);
		
			return EnableRemoteController(idx, connHandler, hostInfo);
		}
			
		Result Machine::EnableRemoteController(uint idx, std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* hostInfo) {
			StopRemoteControl();
			//TODO: support more than 1 remote controller
			DisableRemoteController(idx);
			
			auto frameCapturer = std::make_shared<NesFrameCapturer>(*this);
			auto audioCapturer = std::make_shared<NesServerAudioCapturer>(*this);

			this->hostEngine = std::make_shared<HQRemote::Engine>(connHandler, frameCapturer, audioCapturer, nullptr, REMOTE_FRAME_BUNDLE);
			this->hostEngine->setDesc(hostInfo);

			// by default use zlib frame compressor
			UseFrameCompressorType(FRAME_COMPRESSOR_TYPE_ZLIB);

			// start server engine
			if (!this->hostEngine->start()) {
				this->remoteFrameCompressor->Stop();
				this->remoteFrameCompressor = nullptr;
				this->hostEngine = nullptr;

				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONTROLLER_ENABLED, (Result)RESULT_ERR_CONNECTION);

				return RESULT_ERR_CONNECTION;
			}

			// store host's name
			this->hostName = hostInfo != NULL ? hostInfo : "";

			this->clientInfo.clear();//invalidate client name until receiving update from client

			// reset timer
			this->lastRemoteDataRateUpdateTime = 0;

			//reset color use count table
			ppu.ResetColorUseCountTable();

			//reset remote audio buffer
			ResetRemoteAudioBuffer();

			EnsureCorrectRemoteSoundSettings();

			cpu.SetRemoteControllerIdx(idx);
			cpu.GetApu().EnableFrameSnapshot(true);
			cpu.GetApu().SetPostprocessCallback([this](Sound::Output& output) {
				MixAudioWithClientCapturedAudio(output);
			});
			
			Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONTROLLER_ENABLED, (Result)idx);

			return RESULT_OK;
		}

		std::shared_ptr<HQRemote::IConnectionHandler> Machine::GetRemoteControllerHostConnHandler() {
			if (hostEngine)
				return hostEngine->getConnHandler();
			return nullptr;
		}

		std::shared_ptr<HQRemote::IConnectionHandler> Machine::GetRemoteControllerClientConnHandler() {
			if (clientEngine)
				return clientEngine->getConnHandler();
			return nullptr;
		}

		void Machine::DisableRemoteController(uint idx)
		{
			//TODO: support more than 1 remote controller
			if (hostEngine) {
				//frame compressor must be stopped before stopping host engine to prevent deadlock
				if (this->remoteFrameCompressor) {
					this->remoteFrameCompressor->Stop();
					this->remoteFrameCompressor = nullptr;
				}

				hostEngine->stop();
				hostEngine = nullptr;

				cpu.SetRemoteControllerIdx(0xffffffff);
			}

			cpu.GetApu().EnableFrameSnapshot(false);
			cpu.GetApu().SetPostprocessCallback(nullptr);
			
			Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONTROLLER_DISABLED, (Result)idx);
		}
			
		void Machine::DisableRemoteControllers()
		{
			//TODO: support more than 1 remote controller
			DisableRemoteController(0);
		}

		bool Machine::RemoteControllerConnected(uint idx) const
		{
			//TODO: support more than 1 remote controller
			if (cpu.GetRemoteControllerIdx() != idx)
				return false;
			return this->hostEngine != nullptr && this->hostEngine->connected();
		}

		bool Machine::RemoteControllerEnabled(uint idx) const
		{
			return cpu.GetRemoteControllerIdx() == idx;
		}

		void Machine::EnableLowResRemoteControl(bool e) {
			uint32_t downSample = this->remoteFrameCompressor->downSample = e ? 1 : 0;

			if (this->clientEngine && this->clientEngine->connected())
			{
				//tell host to send downsampled frame or not from now on
				HQRemote::PlainEvent event(Remote::REMOTE_DOWNSAMPLE_FRAME);
				memcpy(event.event.customData, &downSample, sizeof(downSample));
				this->clientEngine->sendEvent(event);
			}
		}

		//<id> is used for ACK message later to acknowledge that the message is received by remote side.
		//<message> must not have more than MAX_REMOTE_MESSAGE_SIZE bytes (excluding NULL character). Otherwise RESULT_ERR_BUFFER_TOO_BIG is retuned.
		//This function can be used to send message between client & server
		Result Machine::SendMessageToRemote(uint64_t id, const char* message) {
			//TODO: support more than 1 remote controller
			size_t msgLen = strlen(message);
			if (msgLen > Api::Machine::MAX_REMOTE_MESSAGE_SIZE)
				return RESULT_ERR_BUFFER_TOO_BIG;

			try {
				HQRemote::FrameEvent messageEvent(message, msgLen, id, HQRemote::MESSAGE);

				if (hostEngine && hostEngine->connected())
				{
					hostEngine->sendEvent(messageEvent);
				}
				else if (clientEngine && clientEngine->connected())
				{
					clientEngine->sendEvent(messageEvent);
				}
				else
				{
					return RESULT_ERR_NOT_READY;
				}
			}
			catch (...) {
				return RESULT_ERR_OUT_OF_MEMORY;
			}

			return RESULT_OK;
		}

		//<idx> is ignored if machine is in client mode
		const char* Machine::GetRemoteName(uint remoteCtlIdx) const {
			//TODO: support more than 1 remote controller
			if (hostEngine && remoteCtlIdx == cpu.GetRemoteControllerIdx())
			{
				return clientInfo.c_str();
			}
			else if (clientEngine)
			{
				return hostName.c_str();
			}
			else
			{
				return NULL;
			}
		}

		void Machine::HandleCommonEvent(const HQRemote::Event& event) {
			switch (event.type)
			{
			case HQRemote::MESSAGE:
			{
				Api::Machine::RemoteMsgEventData messageData;

				char message[Api::Machine::MAX_REMOTE_MESSAGE_SIZE + 1];
				auto messageLen = __min__(Api::Machine::MAX_REMOTE_MESSAGE_SIZE, event.renderedFrameData.frameSize);
				memcpy(message, event.renderedFrameData.frameData, messageLen);
				message[messageLen] = '\0';

				messageData.senderIdx = hostEngine ? cpu.GetRemoteControllerIdx() : -1;//host. TODO: support multiple clients?
				messageData.message = message;

				//invoke callback
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_MESSAGE, (Result)(intptr_t)(&messageData));
			}
			break;
			case HQRemote::MESSAGE_ACK:
			{
				auto id = event.renderedFrameData.frameId;
				//invoke callback
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_MESSAGE_ACK, (Result)(intptr_t)(&id));
			}
			break;
			}
		}

		void Machine::HandleRemoteEventsAsClient(Video::Output* videoOutput, Sound::Output* soundOutput) {
			if (this->clientEngine->connected() == false) {
				auto errorMsg = this->clientEngine->getConnectionInternalError();
				if (errorMsg || this->clientEngine->timeSinceStart() > 60.0f || this->clientState != 0)
				{
#ifdef DEBUG
					HQRemote::Log("remote connection's error/timeout detected\n");
#endif
					
					Unload();
					
					if (errorMsg)
					{
#ifdef DEBUG
						HQRemote::Log("remote connection's invoking error callback\n");
#endif
						Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTION_INTERNAL_ERROR, (Result)(intptr_t)(errorMsg->c_str()));
						
#ifdef DEBUG
						HQRemote::Log("remote connection's invoked error callback\n");
#endif
					}
					else
						Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_DISCONNECTED);
				}
				
				//avoid using up too much cpu
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				
				return;
			}//if (this->clientEngine->connected() == false)

			if (this->clientState == 0) {
				this->clientState = CLIENT_CONNECTED_STATE;

				HQRemote::PlainEvent event;

				// request server to sending bandwidth measurement data
				event.event.type = Remote::REMOTE_BANDWITH_DETECT_START;
				this->clientEngine->sendEvent(event);

				//send client info to host
				HQRemote::FrameEvent clientInfoEvent(this->clientInfo.size(), 0, HQRemote::ENDPOINT_NAME);
				memcpy(clientInfoEvent.event.renderedFrameData.frameData, this->clientInfo.c_str(), this->clientInfo.size());
				this->clientEngine->sendEvent(clientInfoEvent);

				//tell host to reset remote's buttons
				event.event.type = Remote::RESET_REMOTE_INPUT;
				this->clientEngine->sendEvent(event);

				//tell host to send its info
				event.event.type = (HQRemote::HOST_INFO);
				this->clientEngine->sendEvent(event);

#if REMOTE_USE_H264
				// request host to use H264 frame comrepssion
				event.event.type = (Remote::REMOTE_USE_H264_COMPRESSOR);
				this->clientEngine->sendEvent(event);
#endif

				event.event.type = Remote::REMOTE_MODE;
				this->clientEngine->sendEvent(event);

				//notify interface system
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTED);
			}

			//update data rate
			{
				auto time = HQRemote::getTimeCheckPoint64();
				auto elapsed = HQRemote::getElapsedTime64(this->lastRemoteDataRateUpdateTime, time);
				if (this->lastRemoteDataRateUpdateTime == 0 || elapsed >= REMOTE_RCV_RATE_UPDATE_INTERVAL)
				{
					auto rate = this->clientEngine->getReceiveRate();

					// update ui about our data receiving rate
					Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_DATA_RATE, (Result)(intptr_t)(&rate));

					// update host about our data receiving rate
					HQRemote::PlainEvent event;
					event.event.type = Remote::REMOTE_DATA_RATE;
					event.event.floatValue = rate;
					this->clientEngine->sendEventUnreliable(event);

					this->lastRemoteDataRateUpdateTime = time;
				}
			}//update data rate

			// consume as many events as possible
			while (HandleGenericRemoteEventAsClient()) {
			}

			HandleRemoteFrameEventAsClient(videoOutput);
			HandleRemoteAudioEventAsClient(soundOutput);
		}

		bool Machine::HandleGenericRemoteEventAsClient() {
			const int numHandledEventsToStartRcvFrames = 4;

			//handle generic event
			auto eventRef = this->clientEngine->getEvent();
			if (eventRef != nullptr)
			{
				auto & event = eventRef->event;

				switch (event.type) {
				case Remote::REMOTE_BANDWITH_DETECT_START:
				{
					this->numBandwidthDetectBytes = 0;
					this->bandwidthDetectStartTime = HQRemote::getTimeCheckPoint64();

					HQRemote::Log("client received REMOTE_BANDWITH_DETECT_START\n");
				}
				break;
				case Remote::REMOTE_BANDWITH_DETECT_DATA:
				{
					this->numBandwidthDetectBytes += event.renderedFrameData.frameSize;
#if defined DEBUG || defined _DEBUG
					HQRemote::Log("received total=%u REMOTE_BANDWITH_DETECT_DATA packet size=%u, speed=%.3f KB/s\n", this->numBandwidthDetectBytes, event.renderedFrameData.frameSize, this->clientEngine->getReceiveRate() / 1024);
#endif
				}
				break;
				case Remote::REMOTE_BANDWITH_DETECT_END:
				{
					// received bandwith detection end mark from host
					auto curTime = HQRemote::getTimeCheckPoint64();
					auto elapsed = HQRemote::getElapsedTime64(this->bandwidthDetectStartTime, curTime);

#if SIMULATE_SLOW_DATA_RATE
					float rate = SIMULATE_SLOW_DATA_RATE;
#else
					float rate = (float)(this->numBandwidthDetectBytes / elapsed);
#endif

					if (rate <= 70 * 1024) // our receiving rate is less than 70 KB/s
					{
						this->clientEngine->setFrameInterval(SLOW_NET_REMOTE_FRAME_INTERVAL);
					}
					else {
						this->clientEngine->setFrameInterval(DEFAULT_REMOTE_FRAME_INTERVAL);
					}

					// send our receiving rate to host
					HQRemote::PlainEvent event;
					event.event.type = Remote::REMOTE_BANDWITH_DETECT_RESULT;
					event.event.floatValue = rate;
					this->clientEngine->sendEvent(event);

					HQRemote::Log("client detected bandwidth = %.3f KB/s by receiving %u bytes from server\n", rate / 1024, this->numBandwidthDetectBytes);
				}
					break;
				case HQRemote::HOST_INFO:
				{
					assert(Core::Video::Screen::WIDTH == event.hostInfo.width);
					assert(Core::Video::Screen::HEIGHT == event.hostInfo.height);

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break;
				case HQRemote::AUDIO_STREAM_INFO:
				{
					//send client's captured audio info
					this->clientEngine->sendCapturedAudioInfo();

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break;
				case HQRemote::ENDPOINT_NAME://name of host
				{
					if (event.renderedFrameData.frameSize == 0)
						this->hostName = "A player";
					else
						this->hostName.assign((const char*)event.renderedFrameData.frameData, event.renderedFrameData.frameSize);
					
					//invoke callback
					Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTED, (Result)(intptr_t)(this->hostName.c_str()));

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break;
				case Remote::REMOTE_MODE:
				{
					Remote::RemoteMode mode;
					memcpy(&mode, event.customData, sizeof mode);

					if ((this->state & mode.mode) == 0)
						SwitchMode();

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break; 
#if REMOTE_USE_H264
				case Remote::REMOTE_USE_H264_COMPRESSOR: {
					// server confirmed that H264 can be used
					UseFrameDecompressorType(FRAME_COMPRESSOR_TYPE_H264);
				}
					break;
#endif
				case HQRemote::MESSAGE:
				case HQRemote::MESSAGE_ACK:
					HandleCommonEvent(event);
					break;
				case Remote::REMOTE_DATA_RATE:
				{
					// ignore for now
				}
					break;
				default:
					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						break;
					this->remoteFrameDecompressor->OnRemoteEvent(event);
					break;
				}

			}//if (eventRef != nullptr)

			if (this->clientState < CLIENT_EXCHANGE_DATA_STATE && this->clientState >= CLIENT_CONNECTED_STATE + numHandledEventsToStartRcvFrames)
			{
				this->clientState = CLIENT_EXCHANGE_DATA_STATE;
				//we have all info needed from host. Now tell it to start sending frames
				this->remoteFrameDecompressor->Start();

				HQRemote::PlainEvent eventToHost;

				//tell host to send frames in specified interval
				eventToHost.event.type = HQRemote::FRAME_INTERVAL;
				eventToHost.event.frameInterval = this->clientEngine->getFrameInterval();
				this->clientEngine->sendEvent(eventToHost);

				// tell host to start send frames
				eventToHost.event.type = (HQRemote::START_SEND_FRAME);
				this->clientEngine->sendEvent(eventToHost);

#if REMOTE_PREFERRED_DSCP_VALUE
				// hint network driver to give higher priority for our packets
				this->clientEngine->getConnHandler()->setDscp(REMOTE_PREFERRED_DSCP_VALUE);
#endif

				HQRemote::Log("Client ready\n");
			}

			return eventRef != nullptr;
		}

		void Machine::HandleRemoteFrameEventAsClient(Video::Output* videoOutput) {
			this->remoteFrameDecompressor->FrameStep(videoOutput);
		}

		//TODO: move all audio processing functions to Apu class
		void Machine::HandleRemoteAudio(Sound::Output& soundOutput, HQRemote::BaseEngine* remoteHandler, remote_audio_mix_handler mixer_func, uint* mixedLengths)
		{
			auto numChannels = cpu.GetApu().InStereo() ? 2 : 1;
			auto sampleSize = numChannels * sizeof(uint16_t);
			HQRemote::ConstFrameEventRef eventRef;

			unsigned char* outputBuf[2];
			size_t outputBufSize[2];

			for (int i = 0; i < 2; ++i) {
				outputBuf[i] = (unsigned char*)soundOutput.samples[i];
				outputBufSize[i] = soundOutput.length[i] * sampleSize;
			}

			auto totalOutputSize = outputBufSize[0] + outputBufSize[1];
			while ((totalOutputSize = outputBufSize[0] + outputBufSize[1]) > 0
				&& (this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx].size() >= totalOutputSize || (eventRef = remoteHandler->getAudioEvent()))) {
				unsigned char dummy[sizeof(size_t)];
				unsigned char* packet = dummy;
				size_t packetSize = 0;

				if (eventRef) {
					auto & event = eventRef->event;
					assert(event.type == HQRemote::AUDIO_DECODED_PACKET);

					packet = (unsigned char*)event.renderedFrameData.frameData;
					packetSize = event.renderedFrameData.frameSize;
				}

				auto& buffer = this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx];

				try {
					//TODO: only support 16 bit PCM for now
					if (packetSize)
						buffer.insert(buffer.end(), packet, packet + packetSize);
					auto remainSize = buffer.size();
					//fill first output buffer
					size_t sizeToCopy = __min__(remainSize, outputBufSize[0]);

					(this->*mixer_func)(outputBuf[0], buffer.data(), sizeToCopy);

					remainSize -= sizeToCopy;
					outputBufSize[0] -= sizeToCopy;
					outputBuf[0] += sizeToCopy;

					if (remainSize && outputBuf[1] != NULL)//still has more data for second output buffer
					{
						//fill second output buffer
						auto offset = buffer.size() - remainSize;
						sizeToCopy = __min__(remainSize, outputBufSize[1]);

						(this->*mixer_func)(outputBuf[1], buffer.data() + offset, sizeToCopy);

						remainSize -= sizeToCopy;
						outputBufSize[1] -= sizeToCopy;
						outputBuf[1] += sizeToCopy;
					}

					auto bufferUsedBytes = buffer.size() - remainSize;
					ConsumeRemoteAudioBuffer(bufferUsedBytes);
				}
				catch (...) {
					//TODO
				}

#if 0
				char buf[1024];
				sprintf(buf, "submitted sound packet id=%llu, remain buffer=%llu\n",
					eventRef ? eventRef->event.renderedFrameData.frameId : -1,
					(uint64_t)this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx].size());
				OutputDebugStringA(buf);
#endif
			}//while(totalOutputSize > 0 && (eventRef = this->clientEngine->getAudioEvent()))

			if (mixedLengths != nullptr){
				mixedLengths[0] = soundOutput.length[0];
				mixedLengths[1] = soundOutput.length[1];

				//we haven't completely fill the entire buffers
				if (outputBufSize[0] + outputBufSize[1] > 0) {
					mixedLengths[0] -= outputBufSize[0] / sampleSize;
					mixedLengths[1] -= outputBufSize[1] / sampleSize;
				}
			}
		}

		void Machine::HandleRemoteAudioEventAsClient(Sound::Output* soundOutput) {
			//handle audio event
			bool locked = false;
			if (soundOutput != nullptr && (locked = Sound::Output::lockCallback(*soundOutput)) && (soundOutput->length[0] + soundOutput->length[1]) > 0) {
				uint filledLengths[2];

				//for client, simply copy the remote audio to the output buffer
				HandleRemoteAudio(*soundOutput, this->clientEngine.get(), &Machine::CopyAudio, filledLengths);

				soundOutput->length[0] = filledLengths[0];
				soundOutput->length[1] = filledLengths[1];
			}//if (soundOutput != nullptr)

			if (locked)
				Sound::Output::unlockCallback(*soundOutput);
		}

		void Machine::MixAudioWithClientCapturedAudio(Sound::Output& soundOutput) {
			if (!this->hostEngine)
				return;
			//mix local output sound with client's captured sound (e.g microphone)
			HandleRemoteAudio(soundOutput, this->hostEngine.get(), &Machine::MixAudio, nullptr);
		}

		void Machine::ConsumeRemoteAudioBuffer(size_t usedBytes)
		{
			auto& currentBuffer = this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx];

			//still has more data, store it for later use
			if (usedBytes < currentBuffer.size())
			{
				this->nextRemoteAudioBufferIdx = (this->nextRemoteAudioBufferIdx + 1) % 2;
				auto& nextBuffer = this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx];

				nextBuffer.insert(nextBuffer.end(), currentBuffer.begin() + usedBytes, currentBuffer.end());
			}//if (usedBytes < currentBuffer.size())

			currentBuffer.clear();
		}

		void Machine::ResetRemoteAudioBuffer() {
			this->remoteAudioBuffers[0].reserve(256 * 1024);
			this->remoteAudioBuffers[0].clear();
			this->remoteAudioBuffers[1].reserve(256 * 1024);
			this->remoteAudioBuffers[1].clear();
			this->nextRemoteAudioBufferIdx = 0;
		}

		void Machine::CopyAudio(unsigned char* output, const unsigned char* inputAudioData, size_t size)
		{
			memcpy(output, inputAudioData, size);
		}

		inline void Machine::MixAudioSample(unsigned char* outputPtr, int16_t inputSample)
		{
			const auto inputAudioThreshold = 0x7fff / 2;

			//TODO: we only support 16 bit pcm mono for now
			int16_t outputSample;
			memcpy(&outputSample, outputPtr, sizeof(outputSample));

			//mixing input audio & output audio
#if 1
			if (abs(inputSample) > inputAudioThreshold)
				outputSample = (int16_t)(outputSample * 0.2f + inputSample);
			else
#endif
				outputSample = (int16_t)(outputSample + inputSample);

			//update output buffer
			memcpy(outputPtr, &outputSample, sizeof(outputSample));
		}

		void Machine::MixAudio(unsigned char* output, const unsigned char* inputAudioData, size_t size)
		{
			if (size == 0)
				return;

			auto &apu = cpu.GetApu();

			//TODO: we only support 16 bit pcm mono for now
			auto assert_expr = apu.GetSampleBits() == 16 && !apu.InStereo();
			NST_ASSERT(assert_expr);
			if (!assert_expr)
				return;//ignore in release mode

			auto inputPtr = inputAudioData;

			//mix with input audio
			auto samplesToMix = size / apu.GetSampleBlockAlign();

			size_t offset = 0;
			auto outPutPtr = output;

			for (size_t j = 0; j < samplesToMix; ++j, outPutPtr += apu.GetSampleBlockAlign(), inputPtr += apu.GetSampleBlockAlign()) {
				int16_t inputSample;
				memcpy(&inputSample, inputPtr, sizeof(inputSample));

				//mixing input audio & output audio
				MixAudioSample(outPutPtr, inputSample);
			}//for (j ...)
		}

		void Machine::HandleRemoteEventsAsServer() {
			//check if connection handler has some internal errors
			auto errorMsg = this->hostEngine->getConnectionInternalError();
			if (errorMsg)
			{
				DisableRemoteControllers();
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTION_INTERNAL_ERROR, (Result)(intptr_t)(errorMsg->c_str()));
				return;
			}

			//check if client is connected
			if (this->clientState == 0)
			{
				if (this->hostEngine->connected())
				{
					this->clientState = CLIENT_CONNECTED_STATE;

					// reset timer
					this->lastRemoteDataRateUpdateTime = 0;

					// reset to default zlib compressor
					UseFrameCompressorType(FRAME_COMPRESSOR_TYPE_ZLIB);
				}
				else
					return;
			}//if (this->clientState == 0)

			//client disconnected
			if (this->hostEngine->connected() == false)
			{
				this->clientState = 0;
				if (clientInfo.size() != 0)
					Api::Machine::eventCallback(Api::Machine::EVENT_CLIENT_DISCONNECTED, (Result)(intptr_t)(this->clientInfo.c_str()));
				else
					Api::Machine::eventCallback(Api::Machine::EVENT_CLIENT_DISCONNECTED, (Result)(intptr_t)(NULL));

				return;
			}

			// update data sending rate
			auto time = HQRemote::getTimeCheckPoint64();
			auto elapsed = HQRemote::getElapsedTime64(this->lastRemoteDataRateUpdateTime, time);
			if (this->lastRemoteDataRateUpdateTime == 0 || elapsed >= REMOTE_SND_RATE_UPDATE_INTERVAL)
			{
				auto rate = this->hostEngine->getSendRate();

				// update UI about our data rate
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_DATA_RATE, (Result)(intptr_t)(&rate));

				// update client about our data sending rate
				HQRemote::PlainEvent event;
				event.event.type = Remote::REMOTE_DATA_RATE;
				event.event.floatValue = rate;
				this->hostEngine->sendEventUnreliable(event);

				this->lastRemoteDataRateUpdateTime = time;
			}

			// consume as many events as possible
			while (HandleGenericRemoteEventAsServer()) {
			}
		}

		bool Machine::HandleGenericRemoteEventAsServer() {
			int numHandledEventsBeforeAcceptAudio = 3;
			//retrieve event
			auto eventRef = this->hostEngine->getEvent();
			if (eventRef == nullptr)
				return false;

			auto & event = eventRef->event;

			switch (event.type) {
			case Remote::REMOTE_BANDWITH_DETECT_START:
			{
				try {
					this->hostEngine->sendEvent(HQRemote::PlainEvent(Remote::REMOTE_BANDWITH_DETECT_START));
					HQRemote::Log("server sent REMOTE_BANDWITH_DETECT_START\n");

					// send a data of 256 KB to client
					for (int i = 0; i < 256; ++i) 
					{
						HQRemote::FrameEvent bandwidthDetectEvent(1024, 0, Remote::REMOTE_BANDWITH_DETECT_DATA);

						this->hostEngine->sendEvent(bandwidthDetectEvent);
					}

					this->hostEngine->sendEvent(HQRemote::PlainEvent(Remote::REMOTE_BANDWITH_DETECT_END));
					HQRemote::Log("server sent REMOTE_BANDWITH_DETECT_END\n");
				}
				catch (const std::exception&  e) {
					HQRemote::LogErr("REMOTE_BANDWITH_DETECT_START failed with exception %s\n", e.what());
				}
			}
			break;
			case Remote::REMOTE_BANDWITH_DETECT_RESULT:
			{
				auto rate = event.floatValue;
				HQRemote::Log("server detected bandwidth = %.3f KB/s\n", rate / 1024);


				if (rate <= 200 * 1024) // client receiving rate is less than 200 KB/s
				{
					this->remoteFrameCompressor->AdaptToClientSlowRecvRate(rate, this->hostEngine->getSendRate());
				}
				else {
					this->remoteFrameCompressor->AdaptToClientFastRecvRate(rate, this->hostEngine->getSendRate());
				}
			}
			break;
			case HQRemote::ENDPOINT_NAME://name of client
			{
				if (event.renderedFrameData.frameSize == 0)
					this->clientInfo = "A player";
				else
					this->clientInfo.assign((const char*)event.renderedFrameData.frameData, event.renderedFrameData.frameSize);

				//send host's name to client
				HQRemote::FrameEvent hostNameEvent(this->hostName.size(), 0, HQRemote::ENDPOINT_NAME);
				memcpy(hostNameEvent.event.renderedFrameData.frameData, this->hostName.c_str(), this->hostName.size());
				this->hostEngine->sendEvent(hostNameEvent);
				
				//invoke callback
				Api::Machine::eventCallback(Api::Machine::EVENT_CLIENT_CONNECTED, (Result)(intptr_t)(this->clientInfo.c_str()));

				if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
					this->clientState++;
			}
				break;
			case Remote::REMOTE_MODE:
			{
				SendModeToClient();

				if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
					this->clientState++;
			}
				break;
			case HQRemote::AUDIO_STREAM_INFO:
			{
				if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
					this->clientState++;
			}
				break;
			case Remote::REMOTE_DOWNSAMPLE_FRAME:
			{
				//enable/disable downsampling frame before sending to client
				uint32_t downSample;
				memcpy(&downSample, event.customData, sizeof downSample);

				if (downSample != this->remoteFrameCompressor->downSample)
				{
					this->remoteFrameCompressor->downSample = downSample;

					Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_LOWRES, downSample ? RESULT_OK : RESULT_ERR_GENERIC);
				}
			}
				break;
#if REMOTE_USE_H264
			case Remote::REMOTE_USE_H264_COMPRESSOR: {
				// client requests us to use H264 compressor
				UseFrameCompressorType(FRAME_COMPRESSOR_TYPE_H264);

				// send confirm to client
				HQRemote::PlainEvent event(Remote::REMOTE_USE_H264_COMPRESSOR);
				this->hostEngine->sendEvent(event);
			}
				break;
#endif
			case HQRemote::MESSAGE:
			case HQRemote::MESSAGE_ACK:
				HandleCommonEvent(event);
				break;
			case Remote::REMOTE_DATA_RATE: {
				// ignore for now
			}
				break;
			case Remote::REMOTE_ENABLE_ADAPTIVE_DATA_RATE:
				// deprecated. ignore

				break;
			default:
				// cpu may receive reset input event, which can be sent before CLIENT_EXCHANGE_DATA_STATE state
				cpu.OnRemoteEvent(event);
				this->remoteFrameCompressor->OnRemoteEvent(event);
				break;
			}

			//we have all required info from client, tell it to start sending captured voice now
			if (this->clientState < CLIENT_EXCHANGE_DATA_STATE && this->clientState == CLIENT_CONNECTED_STATE + numHandledEventsBeforeAcceptAudio)
			{
				this->clientState = CLIENT_EXCHANGE_DATA_STATE;

				HQRemote::PlainEvent eventToClient(HQRemote::START_SEND_FRAME);
				this->hostEngine->sendEvent(eventToClient);

#if REMOTE_PREFERRED_DSCP_VALUE
				this->hostEngine->getConnHandler()->setDscp(REMOTE_PREFERRED_DSCP_VALUE);
#endif
			}

			return true;
		}

		std::shared_ptr<HQRemote::IData> Machine::CaptureAudioAsClient()
		{
			if (this->currentInputAudio == NULL)
				return nullptr;

			//only capture audio from input device (e.g. mic)
			auto& apu = cpu.GetApu();

			auto availInputAudioSamples = Sound::Input::readCallback(*this->currentInputAudio, NULL, 0);
			if (availInputAudioSamples == 0)
				return nullptr;

			try {
				auto totalSize = availInputAudioSamples * apu.GetSampleBlockAlign();
				auto pcmData = std::make_shared<HQRemote::CData>(totalSize);

				auto ptr = pcmData->data();
				Sound::Input::readCallback(*this->currentInputAudio, ptr, availInputAudioSamples);

				return pcmData;
			}
			catch (...)
			{
				return nullptr;
			}
		}

		std::shared_ptr<HQRemote::IData> Machine::CaptureAudioAsServer()
		{
			auto& apu = cpu.GetApu();

			//TODO: we only support 16 bit pcm mono for now
			if (apu.GetSampleBits() != 16 || apu.InStereo())
				return nullptr;

			auto& audioOutput = apu.GetFrameSnapshot();

			auto totalSize = audioOutput.samples.size();
			auto totalSamples = totalSize / audioOutput.blockAlign;
			if (totalSize == 0)
				return nullptr;

			int16_t inputAudioBuffer[1024];
			auto inputAudioBufferMaxSamples = sizeof(inputAudioBuffer) / audioOutput.blockAlign;

			try {
				auto pcmData = std::make_shared<HQRemote::CData>(totalSize);

				auto ptr = pcmData->data();

				if (totalSize)
				{
					//copy captured snapshot data
					memcpy(ptr, audioOutput.samples.data(), totalSize);

#if 1
					//mix with input audio
					auto remainSamples = totalSamples;
					size_t inputSamples = 0;
					int16_t lastSample = 0;

					while (this->currentInputAudio != NULL && (inputSamples = Sound::Input::readCallback(*this->currentInputAudio, inputAudioBuffer, __min__(remainSamples, inputAudioBufferMaxSamples))))
					{
						auto inputSize = inputSamples * audioOutput.blockAlign;

						MixAudio(ptr, (unsigned char*)inputAudioBuffer, inputSize);

						remainSamples -= inputSamples;
						ptr += inputSize;
						lastSample = inputAudioBuffer[inputSamples - 1];
					}//while ((inputSize = Sound::Input::readCallback(...)))

					//fill the rest with last captured sample
					for (size_t i = 0; i < remainSamples; ++i, ptr += audioOutput.blockAlign) {
						MixAudioSample(ptr, lastSample);
					}
#endif
				}//if (totalSize)

				return pcmData;
			}
			catch (...)
			{
				return nullptr;
			}
		}

		void Machine::SendModeToClient() {
			auto modeEvent = std::make_shared<HQRemote::PlainEvent >(Remote::REMOTE_MODE);
			Remote::RemoteMode mode;
			//send the current mode to client
			if (state & Api::Machine::NTSC)
				mode.mode = Api::Machine::NTSC;
			else
				mode.mode = Api::Machine::PAL;

			memcpy(modeEvent->event.customData, &mode, sizeof mode);
			this->hostEngine->sendEvent(modeEvent);
		}

		void Machine::EnsureCorrectRemoteSoundSettings() {
			auto& apu = cpu.GetApu();
			//TODO: only 16 bit PCM is supported for remote controlling for now
			if (apu.GetSampleBits() != 16 ||
				apu.GetSampleRate() != REMOTE_AUDIO_SAMPLE_RATE ||
				apu.InStereo() != REMOTE_AUDIO_STEREO_CHANNELS)
			{
				apu.SetSampleBits(16);
				apu.EnableStereo(REMOTE_AUDIO_STEREO_CHANNELS);
				apu.SetSampleRate(REMOTE_AUDIO_SAMPLE_RATE);

				//TODO: these settings may fail
				Sound::Output::updateSettingsCallback();
				Sound::Input::updateSettingsCallback();
			}
		}

		//implement HQRemote::IFrameCapturer
		size_t Machine::GetFrameSize() {
			if (!this->remoteFrameCompressor)
				return 0;
			if (this->remoteFrameCompressor->UseIndexedColor())
				return sizeof(Core::Video::Screen) + sizeof(uint) + sizeof(Ppu::ColorUseCountTable);

			return renderer.GetCachedRenderedFrameSize();
		}
		unsigned int Machine::GetNumColorChannels() {
			return 1;
		}
		void Machine::CaptureFrame(unsigned char * prevFrameDataToCopy) {
			if (!this->remoteFrameCompressor)
				return;
			if (prevFrameDataToCopy == NULL)
				return;

			if (this->remoteFrameCompressor->UseIndexedColor()) {// the legacy way of capture frame
				auto& screen = ppu.GetScreen();
				auto burstPhase = ppu.GetBurstPhase();
				auto& colorUseCountTable = ppu.GetColorUseCountTable();

				//layout: burst phase | screen | colors' usage count table
				memcpy(prevFrameDataToCopy, &burstPhase, sizeof(burstPhase));
				memcpy(prevFrameDataToCopy + sizeof burstPhase, &screen, sizeof(screen));
				auto& copiedTable = *(Ppu::ColorUseCountTable*)(prevFrameDataToCopy + sizeof burstPhase + sizeof screen);
				memcpy(&copiedTable, colorUseCountTable, sizeof(Ppu::ColorUseCountTable));

				//sort colors' usage count table 
				std::sort(&copiedTable[0], &copiedTable[Video::Screen::PALETTE], [](const Ppu::ColorUseCount& a, const Ppu::ColorUseCount& b) { return a.count > b.count; });

				ppu.MarkColorUseCountTableToReset();
			}
			else {
				// the new way of capturing frame, we capture the rendered frame by renderer instead
				memcpy(prevFrameDataToCopy, renderer.GetCachedRenderedFrameData(), renderer.GetCachedRenderedFrameSize());
			}
		}
		//end IFrameCapturer implementation

		//implement HQRemote::IAudioCapturer
		uint32_t Machine::GetAudioSampleRate() const {
			return cpu.GetApu().GetSampleRate();
		}
		uint32_t Machine::GetNumAudioChannels() const {
			return cpu.GetApu().InStereo() ? 2 : 1;
		}

		//end HQRemote::IAudioCapturer implementation

		Result Machine::Unload()
		{
			const Result result = PowerOff();

			tracker.Unload();

			if (image)
				Image::Unload( image );
			image = NULL;

			state &= (Api::Machine::NTSC|Api::Machine::PAL);

			Api::Machine::eventCallback( Api::Machine::EVENT_UNLOAD, result );

			return result;
		}

		void Machine::UpdateModels()
		{
			const Region region = (state & Api::Machine::NTSC) ? REGION_NTSC : REGION_PAL;

			CpuModel cpuModel;
			PpuModel ppuModel;

			if (image)
			{
				image->GetDesiredSystem( region, &cpuModel, &ppuModel );
			}
			else
			{
				cpuModel = (region == REGION_NTSC ? CPU_RP2A03 : CPU_RP2A07);
				ppuModel = (region == REGION_NTSC ? PPU_RP2C02 : PPU_RP2C07);
			}

			if ((state & Api::Machine::REMOTE) == 0)//LHQ: if are remotely control another machine, no need to update the current cpu
				cpu.SetModel( cpuModel );
			UpdateVideo( ppuModel, GetColorMode() );

			renderer.EnableForcedFieldMerging( ppuModel != PPU_RP2C02 );

			//tell client about our updated mode
			if (hostEngine && this->clientState == CLIENT_EXCHANGE_DATA_STATE)//TODO: race condition
				SendModeToClient();
		}

		Machine::ColorMode Machine::GetColorMode() const
		{
			return
			(
				renderer.GetPaletteType() == Video::Renderer::PALETTE_YUV    ? COLORMODE_YUV :
				renderer.GetPaletteType() == Video::Renderer::PALETTE_CUSTOM ? COLORMODE_CUSTOM :
																			   COLORMODE_RGB
			);
		}

		Result Machine::UpdateColorMode()
		{
			return UpdateColorMode( GetColorMode() );
		}

		Result Machine::UpdateColorMode(const ColorMode mode)
		{
			return UpdateVideo( ppu.GetModel(), mode );
		}

		Result Machine::UpdateVideo(const PpuModel ppuModel,const ColorMode mode)
		{
			ppu.SetModel( ppuModel, mode == COLORMODE_YUV );

			Video::Renderer::PaletteType palette;

			switch (mode)
			{
				case COLORMODE_RGB:

					switch (ppuModel)
					{
						case PPU_RP2C04_0001: palette = Video::Renderer::PALETTE_VS1;  break;
						case PPU_RP2C04_0002: palette = Video::Renderer::PALETTE_VS2;  break;
						case PPU_RP2C04_0003: palette = Video::Renderer::PALETTE_VS3;  break;
						case PPU_RP2C04_0004: palette = Video::Renderer::PALETTE_VS4;  break;
						default:              palette = Video::Renderer::PALETTE_PC10; break;
					}
					break;

				case COLORMODE_CUSTOM:

					palette = Video::Renderer::PALETTE_CUSTOM;
					break;

				default:

					palette = Video::Renderer::PALETTE_YUV;
					break;
			}

			return renderer.SetPaletteType( palette );
		}

		Result Machine::PowerOff(Result result)
		{
			if (state & Api::Machine::ON)
			{
				tracker.PowerOff();

				StopRemoteControl();

				if (image && !image->PowerOff() && NES_SUCCEEDED(result))
					result = RESULT_WARN_SAVEDATA_LOST;

				ppu.PowerOff();
				cpu.PowerOff();

				state &= ~uint(Api::Machine::ON);
				frame = 0;

				Api::Machine::eventCallback( Api::Machine::EVENT_POWER_OFF, result );
			}

			return result;
		}

		void Machine::Reset(bool hard)
		{
			if (state & Api::Machine::SOUND)
				hard = true;

			try
			{
				frame = 0;
				cpu.Reset( hard );

				if (!(state & Api::Machine::SOUND))
				{
					InitializeInputDevices();

					cpu.Map( 0x4016 ).Set( this, &Machine::Peek_4016, &Machine::Poke_4016 );
					cpu.Map( 0x4017 ).Set( this, &Machine::Peek_4017, &Machine::Poke_4017 );

					extPort->Reset();
					expPort->Reset();

					bool acknowledged = true;

					if (image)
					{
						switch (image->GetDesiredSystem((state & Api::Machine::NTSC) ? REGION_NTSC : REGION_PAL))
						{
							case SYSTEM_FAMICOM:
							case SYSTEM_DENDY:

								acknowledged = false;
								break;
						}
					}

					ppu.Reset( hard, acknowledged );

					if (image)
						image->Reset( hard );

					if (cheats)
						cheats->Reset();

					tracker.Reset();
				}
				else
				{
					image->Reset( true );
				}

				cpu.Boot( hard );

				if (state & Api::Machine::ON)
				{
					Api::Machine::eventCallback( hard ? Api::Machine::EVENT_RESET_HARD : Api::Machine::EVENT_RESET_SOFT );
				}
				else
				{
					state |= Api::Machine::ON;
					Api::Machine::eventCallback( Api::Machine::EVENT_POWER_ON );
				}
			}
			catch (...)
			{
				PowerOff();
				throw;
			}
		}

		void Machine::SwitchMode()
		{
			NST_ASSERT( !(state & Api::Machine::ON) || (state & Api::Machine::REMOTE) != 0 );//LHQ

			if (state & Api::Machine::NTSC)
				state = (state & ~uint(Api::Machine::NTSC)) | Api::Machine::PAL;
			else
				state = (state & ~uint(Api::Machine::PAL)) | Api::Machine::NTSC;

			UpdateModels();

			Api::Machine::eventCallback( (state & Api::Machine::NTSC) ? Api::Machine::EVENT_MODE_NTSC : Api::Machine::EVENT_MODE_PAL );
		}

		void Machine::InitializeInputDevices() const
		{
			if (state & Api::Machine::GAME)
			{
				const bool arcade = state & Api::Machine::VS;

				extPort->Initialize( arcade );
				expPort->Initialize( arcade );
			}
		}

		void Machine::SaveState(State::Saver& saver) const
		{
			if (!image)
				return;
			NST_ASSERT( (state & (Api::Machine::GAME|Api::Machine::ON)) > Api::Machine::ON );

			saver.Begin( AsciiId<'N','S','T'>::V | 0x1AUL << 24 );

			saver.Begin( AsciiId<'N','F','O'>::V ).Write32( image->GetPrgCrc() ).Write32( frame ).End();

			cpu.SaveState( saver, AsciiId<'C','P','U'>::V, AsciiId<'A','P','U'>::V );
			ppu.SaveState( saver, AsciiId<'P','P','U'>::V );
			image->SaveState( saver, AsciiId<'I','M','G'>::V );

			saver.Begin( AsciiId<'P','R','T'>::V );

			if (extPort->NumPorts() == 4)
			{
				static_cast<const Input::AdapterFour*>(extPort)->SaveState
				(
					saver, AsciiId<'4','S','C'>::V
				);
			}

			for (uint i=0; i < extPort->NumPorts(); ++i)
				extPort->GetDevice( i ).SaveState( saver, Ascii<'0'>::V + i );

			expPort->SaveState( saver, Ascii<'X'>::V );

			saver.End();

			saver.End();
		}

		bool Machine::LoadState(State::Loader& loader,const bool resetOnError)
		{
			NST_ASSERT( (state & (Api::Machine::GAME|Api::Machine::ON)) > Api::Machine::ON );

			try
			{
				if (loader.Begin() != (AsciiId<'N','S','T'>::V | 0x1AUL << 24))
					throw RESULT_ERR_INVALID_FILE;

				while (const dword chunk = loader.Begin())
				{
					switch (chunk)
					{
						case AsciiId<'N','F','O'>::V:
						{
							const dword crc = loader.Read32();

							if
							(
								loader.CheckCrc() && !(state & Api::Machine::DISK) &&
								crc && crc != image->GetPrgCrc() &&
								Api::User::questionCallback( Api::User::QUESTION_NST_PRG_CRC_FAIL_CONTINUE ) == Api::User::ANSWER_NO
							)
							{
								for (uint i=0; i < 2; ++i)
									loader.End();

								return false;
							}

							frame = loader.Read32();
							break;
						}

						case AsciiId<'C','P','U'>::V:
						case AsciiId<'A','P','U'>::V:

							cpu.LoadState( loader, AsciiId<'C','P','U'>::V, AsciiId<'A','P','U'>::V, chunk );
							break;

						case AsciiId<'P','P','U'>::V:

							ppu.LoadState( loader );
							break;

						case AsciiId<'I','M','G'>::V:

							image->LoadState( loader );
							break;

						case AsciiId<'P','R','T'>::V:

							extPort->Reset();
							expPort->Reset();

							while (const dword subId = loader.Begin())
							{
								if (subId == AsciiId<'4','S','C'>::V)
								{
									if (extPort->NumPorts() == 4)
										static_cast<Input::AdapterFour*>(extPort)->LoadState( loader );
								}
								else switch (const uint index = (subId >> 16 & 0xFF))
								{
									case Ascii<'2'>::V:
									case Ascii<'3'>::V:

										if (extPort->NumPorts() != 4)
											break;

									case Ascii<'0'>::V:
									case Ascii<'1'>::V:

										extPort->GetDevice( index - Ascii<'0'>::V ).LoadState( loader, subId & 0xFF00FFFF );
										break;

									case Ascii<'X'>::V:

										expPort->LoadState( loader, subId & 0xFF00FFFF );
										break;
								}

								loader.End();
							}
							break;
					}

					loader.End();
				}

				loader.End();
			}
			catch (...)
			{
				if (resetOnError)
					Reset( true );

				throw;
			}

			return true;
		}

		#ifdef NST_MSVC_OPTIMIZE
		#pragma optimize("", on)
		#endif

		void Machine::Execute
		(
			Video::Output* const video,
			Sound::Output* const sound,
			Input::Controllers* const input,
			Sound::Input* const inputSound
		)
		{
			NST_ASSERT( state & Api::Machine::ON );

#if PROFILE_EXECUTION_TIME
			HQRemote::ScopedTimeProfiler profiler("machine execution", avgExecuteTimeLock, avgExecuteTime, executeWindowTime);
#endif

			this->currentInputAudio = inputSound;//LHQ

			if ((state & Api::Machine::REMOTE) != 0 && this->clientEngine)
			{
				if (input && this->clientState == CLIENT_EXCHANGE_DATA_STATE)
				{
					//send input to remote host
					Input::Controllers::Pad& pad = input->pad[0];
					
					auto curTime = HQRemote::getTimeCheckPoint64();
					bool routineSend = false;
					if (this->lastSentInputTime == 0 || HQRemote::getElapsedTime64(this->lastSentInputTime, curTime) >= REMOTE_INPUT_SEND_INTERVAL)
					{
						routineSend = true;
						this->lastSentInputTime = curTime;
					}

					if (Input::Controllers::Pad::callback(pad, 0)
						&& (this->lastSentInput != pad.buttons || routineSend))
					{
						Remote::RemoteInput remoteInput;
						remoteInput.id = ++this->lastSentInputId;
						remoteInput.buttons = pad.buttons;

						HQRemote::PlainEvent inputEvent(Remote::REMOTE_INPUT);
						memcpy(inputEvent.event.customData, &remoteInput, sizeof(remoteInput));
						this->clientEngine->sendEventUnreliable(inputEvent);

#if (defined DEBUG || defined _DEBUG)
						std::stringstream ss;
						ss << "Sent input (" << remoteInput.id << "): " << remoteInput.buttons << std::endl;
#	ifdef WIN32
						OutputDebugStringA(ss.str().c_str());
#	endif
#endif
						this->lastSentInput = pad.buttons;
					}
				}//if (input)

				uint remoteBurstPhase = 0;

				//handle remote event
				EnsureCorrectRemoteSoundSettings();
				HandleRemoteEventsAsClient(video, sound);

				//capture input sound (e.g. mic) and send to host
				if (this->clientEngine)//need to check here as the pointer may be invalidated by HandleRemoteEventsAsClient()
					this->clientEngine->captureAndSendAudio();
			}
			else if (!(state & Api::Machine::SOUND))
			{
				if (state & Api::Machine::CARTRIDGE)
					static_cast<Cartridge*>(image)->BeginFrame( Api::Input(*this), input );

				extPort->BeginFrame( input );
				expPort->BeginFrame( input );

				ppu.BeginFrame( tracker.IsFrameLocked() );

				if (cheats)
					cheats->BeginFrame( tracker.IsFrameLocked() );

				//handle remote event
				if (this->hostEngine != nullptr)
				{
					HandleRemoteEventsAsServer();
				}

				//CPU
				{
#if PROFILE_EXECUTION_TIME
					HQRemote::ScopedTimeProfiler profiler("machine's cpu execution", avgCpuExecuteTime, cpuExecuteWindowTime);
#endif
					cpu.ExecuteFrame(sound);
				}

				//PPU
				{
#if PROFILE_EXECUTION_TIME
					HQRemote::ScopedTimeProfiler profiler("machine's ppu endframe", avgPpuEndFrameTime, ppuEndFrameWindowTime);
#endif
					ppu.EndFrame();
				}

				//render
				renderer.bgColor = ppu.output.bgColor;

				if (video)
				{
#if PROFILE_EXECUTION_TIME
					HQRemote::ScopedTimeProfiler profiler("machine's video blitting", avgVideoBlitTime, videoBlitWindowTime);
#endif
					renderer.Blit(*video, ppu.GetScreen(), ppu.GetBurstPhase());
				}

				cpu.EndFrame();

				//LHQ
				if (this->hostEngine)
				{
					// update frame compressor
					this->remoteFrameCompressor->FrameStep(video);

					//capture frame & audio
					EnsureCorrectRemoteSoundSettings();

					//capture video frame
					if (video)
					{
#if PROFILE_EXECUTION_TIME
						HQRemote::ScopedTimeProfiler profiler("captureAndSendFrame", avgFrameCaptureTime, frameCaptureWindowTime);
#endif
						this->hostEngine->captureAndSendFrame();
					}

					//capture audio
					if (sound)
					{
#if PROFILE_EXECUTION_TIME
						HQRemote::ScopedTimeProfiler profiler("captureAndSendAudio", avgAudioCaptureTime, audioCaptureWindowTime);
#endif
						this->hostEngine->captureAndSendAudio();
					}
				}
				//end LHQ

				if (image)
					image->VSync();

				extPort->EndFrame();
				expPort->EndFrame();

				frame++;
			}
			else
			{
				static_cast<Nsf*>(image)->BeginFrame();

				cpu.ExecuteFrame( sound );
				cpu.EndFrame();

				image->VSync();
			}

			this->currentInputAudio = nullptr;//LHQ
		}

		NES_POKE_D(Machine,4016)
		{
			extPort->Poke( data );
			expPort->Poke( data );
		}

		NES_PEEK_A(Machine,4016)
		{
			cpu.Update( address );
			return OPEN_BUS | extPort->Peek(0) | expPort->Peek(0);
		}

		NES_POKE_D(Machine,4017)
		{
			cpu.GetApu().WriteFrameCtrl( data );
		}

		NES_PEEK_A(Machine,4017)
		{
			cpu.Update( address );
			return OPEN_BUS | extPort->Peek(1) | expPort->Peek(1);
		}
	}
}
