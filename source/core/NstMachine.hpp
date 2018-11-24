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

#ifndef NST_MACHINE_H
#define NST_MACHINE_H

#include <iosfwd>
#include "NstCpu.hpp"
#include "NstPpu.hpp"
#include "NstTracker.hpp"
#include "NstVideoRenderer.hpp"

#include <memory>
#include <string>
#include <mutex>

#ifdef NST_PRAGMA_ONCE
#pragma once
#endif

namespace HQRemote {
	class IData;
	class BaseEngine;
	class Engine;
	class Client;
	class IConnectionHandler;
	class IImgCompressor;
	struct Event;
}

namespace Nes
{
	namespace Core
	{
		namespace Video
		{
			class Output;
		}

		namespace Sound
		{
			class Output;
		}

		namespace Input
		{
			class Device;
			class Adapter;
			class Controllers;
		}

		class Image;
		class Cheats;
		class ImageDatabase;

		class FrameCompressorBase;//LHQ
		class FrameDecompressorBase;//LHQ

		class Machine
		{
		public:

			Machine();
			~Machine();

			void Execute
			(
				Video::Output*,
				Sound::Output*,
				Input::Controllers*,
				Sound::Input*
			);

			enum ColorMode
			{
				COLORMODE_YUV,
				COLORMODE_RGB,
				COLORMODE_CUSTOM
			};

			Result Load
			(
				std::istream&,
				FavoredSystem,
				bool,
				std::istream*,
				bool,
				Result*,
				uint
			);

			Result LoadRemote(std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* clientName = NULL);
			Result LoadRemote(const std::string& remoteIp, int remotePort, const char* clientName = NULL);

			Result EnableRemoteController(uint idx, std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* hostName);
			Result EnableRemoteController(uint idx, int networkListenPort, const char* hostName);

			//get connection handler which was passed to EnableRemoteController()
			std::shared_ptr<HQRemote::IConnectionHandler> GetRemoteControllerHostConnHandler();
			std::shared_ptr<HQRemote::IConnectionHandler> GetRemoteControllerClientConnHandler();

			void DisableRemoteController(uint idx);
			void DisableRemoteControllers();
			bool RemoteControllerConnected(uint idx) const;
			bool RemoteControllerEnabled(uint idx) const;
			void EnableLowResRemoteControl(bool e);

			//<id> is used for ACK message later to acknowledge that the message is received by remote side.
			//<message> must not have more than MAX_REMOTE_MESSAGE_SIZE bytes (excluding NULL character). Otherwise RESULT_ERR_BUFFER_TOO_BIG is retuned.
			//This function can be used to send message between client & server
			Result SendMessageToRemote(uint64_t id, const char* message);

			//<idx> is ignored if machine is in client mode
			const char* GetRemoteName(uint remoteCtlIdx) const;

			Result Unload();
			Result PowerOff(Result=RESULT_OK);
			void   Reset(bool);
			void   SwitchMode();
			bool   LoadState(State::Loader&,bool);
			void   SaveState(State::Saver&) const;
			void   InitializeInputDevices() const;
			Result UpdateColorMode();
			Result UpdateColorMode(ColorMode);

			//implement HQRemote::IFrameCapturer
			size_t GetFrameSize();
			unsigned int GetNumColorChannels();
			void CaptureFrame(unsigned char * prevFrameDataToCopy);
			//end IFrameCapturer implementation

			//implement HQRemote::IAudioCapturer
			uint32_t GetAudioSampleRate() const;
			uint32_t GetNumAudioChannels() const;
			std::shared_ptr<HQRemote::IData> CaptureAudioAsServer();
			std::shared_ptr<HQRemote::IData> CaptureAudioAsClient();
			//end HQRemote::IAudioCapturer implementation
		private:
			typedef void (Machine::*remote_audio_mix_handler)(unsigned char* output, const unsigned char* remoteAudioData, size_t size);

			void StopRemoteControl();
			void UseFrameCompressorType(int type);
			void UseFrameDecompressorType(int type);
			void HandleRemoteEventsAsClient(Video::Output* videoOutput, Sound::Output* soundOutput);
			bool HandleGenericRemoteEventAsClient();
			void HandleRemoteFrameEventAsClient(Video::Output* videoOutput);
			void HandleRemoteAudioEventAsClient(Sound::Output* soundOutput);

			void HandleRemoteEventsAsServer();
			bool HandleGenericRemoteEventAsServer();

			void HandleCommonEvent(const HQRemote::Event& event);

			void ConsumeRemoteAudioBuffer(size_t usedBytes);
			void ResetRemoteAudioBuffer();
			void MixAudioWithClientCapturedAudio(Sound::Output& soundOutput);//mix output sound with client's captured sound (e.g. microphone)

			void HandleRemoteAudio(Sound::Output& soundOutput, HQRemote::BaseEngine* remoteHandler, remote_audio_mix_handler mixer_func, uint *mixedLengths);
			void CopyAudio(unsigned char* output, const unsigned char* inputAudioData, size_t size);//copy <inputAudioData> to <output>
			void MixAudio(unsigned char* output, const unsigned char* inputAudioData, size_t size);//<inputAudioData> is input sound to be mixed with output sound
			void MixAudioSample(unsigned char* sample, int16_t inputAudioSample);

			void SendModeToClient();
			void EnsureCorrectRemoteSoundSettings();

			void UpdateModels();
			Result UpdateVideo(PpuModel,ColorMode);
			ColorMode GetColorMode() const;

			enum
			{
				OPEN_BUS = 0x40
			};

			NES_DECL_POKE( 4016 );
			NES_DECL_PEEK( 4016 );
			NES_DECL_POKE( 4017 );
			NES_DECL_PEEK( 4017 );

			uint state;
			dword frame;

			std::shared_ptr<HQRemote::Engine> hostEngine;
			std::shared_ptr<HQRemote::Client> clientEngine;
			std::shared_ptr<FrameCompressorBase> remoteFrameCompressor;
			std::shared_ptr<FrameDecompressorBase> remoteFrameDecompressor;
			int clientState;
			std::string hostName;//name of remote control's host
			std::string clientInfo;//name of remote control's client
			uint32_t numBandwidthDetectBytes;
			uint64_t bandwidthDetectStartTime;
			uint64_t lastRemoteDataRateUpdateTime;
			uint64_t lastSentInputId;
			uint64_t lastSentInputTime;
			uint lastSentInput;

			Sound::Input* currentInputAudio;

			std::vector<unsigned char> remoteAudioBuffers[2];
			int nextRemoteAudioBufferIdx;

			//LHQ: for profiling
			float avgExecuteTime;
			float executeWindowTime;
			float avgCpuExecuteTime;
			float cpuExecuteWindowTime;
			float avgPpuEndFrameTime;
			float ppuEndFrameWindowTime;
			float avgVideoBlitTime;
			float videoBlitWindowTime;
			float avgFrameCaptureTime;
			float frameCaptureWindowTime;
			float avgAudioCaptureTime;
			float audioCaptureWindowTime;
			std::mutex avgExecuteTimeLock;
		public:
			Cpu cpu;
			Input::Adapter* extPort;
			Input::Device* expPort;
			Image* image;
			Cheats* cheats;
			ImageDatabase* imageDatabase;
			Tracker tracker;
			Ppu ppu;
			Video::Renderer renderer;

			uint Is(uint a) const
			{
				return state & a;
			}

			bool Is(uint a,uint b) const
			{
				return (state & a) && (state & b);
			}
		};
	}
}

#endif
