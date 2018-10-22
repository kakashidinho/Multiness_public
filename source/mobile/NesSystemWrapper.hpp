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

#ifndef NesSystemWrapper_hpp
#define NesSystemWrapper_hpp

#include <string>
#include <stdio.h>
#include <istream>
#include <ostream>
#include <memory>

#include "Common.hpp"
#include "Renderer.hpp"
#include "Input.hpp"

#include "../core/api/NstApiEmulator.hpp"
#include "../core/api/NstApiUser.hpp"
#include "../core/api/NstApiVideo.hpp"
#include "../core/api/NstApiSound.hpp"
#include "../core/api/NstApiInput.hpp"
#include "../core/api/NstApiMachine.hpp"

namespace  Nes {
	class NesSystemWrapper {
	public:
		typedef Callback::ErrorCallback ErrorCallback;
		typedef Callback::VLogCallback VLogCallback;
		typedef Callback::OpenFileCallback OpenFileCallback;
		typedef Callback::MachineEventCallback MachineEventCallback;
		
		NesSystemWrapper(std::istream* dbStream, VLogCallback logCallback = nullptr, ErrorCallback errCallback = nullptr);
		
		virtual ~NesSystemWrapper();
		
		void SetErrorCallback(ErrorCallback callback) { m_errCallback = callback; }
		void SetLogCallback(VLogCallback callback) { m_vlogCallback = callback; }
		void SetMachineEventCallback(MachineEventCallback callback) { m_machineEventCallback = callback; }
		
		void SetInput(Input::IInput*&& input);
		
		void ResetView(unsigned int width, unsigned int height, bool contextRecreate, OpenFileCallback resourceLoader);
		void ResetView(float width, float height, bool contextRecreate, OpenFileCallback resourceLoader);
		void CleanupGraphicsResources();
		
		Result VerifyGame(const std::string& file);
		Result VerifyGame(std::istream& file);
		Result LoadGame(const std::string& file);
		Result LoadGame(const std::string& fileName, std::istream& fileStream);
		bool LoadRemote(const std::string&ip, int port, const char* clientInfo);
		bool LoadRemote(std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* clientInfo);
		bool EnableRemoteController(uint32_t idx, int port, const char* hostName);
		bool EnableRemoteController(uint32_t idx, std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* hostName);
		
		//get connection handler which was passed to EnableRemoteController()
		std::shared_ptr<HQRemote::IConnectionHandler> GetRemoteControllerHostConnHandler();
		
		void DisableRemoteController(uint32_t idx);
		bool RemoteControllerConnected(uint32_t idx);
		bool RemoteControllerEnabled(uint idx);
		bool RemoteControlling();
		
		Result SendMessageToRemote(uint64_t id, const char* message);
		const char* GetRemoteName(uint32_t idx);
		
		Result SaveState(const std::string& file);
		Result SaveState(std::ostream& file);
		Result LoadState(const std::string& file);
		Result LoadState(std::istream& file);
		
		//hardware input (thread safe)
		void OnJoystickMoved(float x, float y); // x, y must be in range [-1, 1]
		void OnUserHardwareButtonDown(int buttonCode);
		void OnUserHardwareButtonUp(int buttonCode);
		
		//touch management methods are thread safe
		bool OnTouchBegan(void* id, float x, float y);
		bool OnTouchMoved(void* id, float x, float y);
		bool OnTouchEnded(void* id, float x, float y);
		
		void Execute(bool skipRender=false);
		
		void Reset();
		void Shutdown();
		
		bool Loaded() const { return m_loaded; }
		std::string LoadedFile() const { return m_loadedFile; }
		std::string LoadedFileName() const;
		
		float GetScreenWidth() const { return m_renderer->GetScreenWidth(); }
		float GetScreenHeight() const { return m_renderer->GetScreenHeight(); }
		void EnableFullScreenVideo(bool e) { m_renderer->EnableFullScreenVideo(e); }
		
		Video::IRenderer& GetRenderer() { return *m_renderer; }
		
		void ScaleDPad(float scale, OpenFileCallback resourceLoader);
		void EnableUIButtons(bool e);
		// if enabled, normal A/B buttons will become auto A/B buttons
		void SwitchABTurboMode(bool aIsTurbo, bool bIsTurbo);

		Api::Machine::Mode GetMode();
		const Api::Emulator& GetEmulator() const { return m_emulator; }
	protected:
		struct StreamFinalizer {
			typedef std::function<void(std::istream*)> delete_func_t;

			StreamFinalizer();
			StreamFinalizer(delete_func_t deleteFunc);

			void operator() (std::istream* stream);

			delete_func_t m_deleterFunc;
		};

		static void NST_CALLBACK LogCallback(void* userData, const char* string, ulong length);
		static void NST_CALLBACK MachineEvent(void* userData, Api::Machine::Event event, Result result);
		static Api::User::Answer NST_CALLBACK QuestionCallback(void* userData, Api::User::Question question);
		
		void MachineEvent(Api::Machine::Event event, Result result);
		Api::User::Answer QuestionCallback(Api::User::Question question);
		
		std::unique_ptr<std::istream, StreamFinalizer> OpenGameFileStream(std::istream& file);

		std::unique_ptr<Api::Emulator> m_pVerifierEmulator;//the emulator that is used only for verifying images
		Api::Emulator m_emulator;
		
		bool m_loaded;
		
		Core::Video::Output m_video;
		Core::Sound::Output m_sound;
		Core::Sound::Input m_soundInput;
		
		std::unique_ptr<Video::IRenderer> m_renderer;
		std::unique_ptr<Input::IInput> m_input;
		
		ErrorCallback m_errCallback;
		VLogCallback m_vlogCallback;
		MachineEventCallback m_machineEventCallback;
		
		std::string m_loadedFile;
	};

}
#endif /* NesSystem_hpp */
