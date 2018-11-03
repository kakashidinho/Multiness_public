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

#include "NesSystemWrapper.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <vector>

#include "../core/NstLog.hpp"
#include "../core/api/NstApiUser.hpp"
#include "../core/api/NstApiFds.hpp"
#include "../core/api/NstApiDipSwitches.hpp"
#include "../core/api/NstApiRewinder.hpp"
#include "../core/api/NstApiCartridge.hpp"
#include "../core/api/NstApiMovie.hpp"
#include "../core/api/NstApiNsf.hpp"
#include "../core/api/NstApiMemoryStream.hpp"

#define MINIZ_NO_STDIO
#include "../../third-party/miniz/miniz.c"

#include "../../third-party/RemoteController/Common.h"


#define DEFAULT_REMOTE_PORT 23458
#define DEBUG_REMOTE_CONTROL 0

namespace  Nes {
	static bool ChecKRomExt(const char *filename, size_t len) {
		// Check if the file extension is valid
		if (len < 4)
			return false;
		
		if ((!strcasecmp(&filename[len-4], ".nes")) ||
			(!strcasecmp(&filename[len-4], ".fds")) ||
			(!strcasecmp(&filename[len-4], ".nsf")) ||
			(!strcasecmp(&filename[len-4], ".unf")) ||
			(!strcasecmp(&filename[len-5], ".unif"))||
			(!strcasecmp(&filename[len-4], ".xml"))) {
			return true;
		}
		return false;
	}
	
	static void LogWrapper(NesSystemWrapper::VLogCallback callback, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		
		if (callback != nullptr)
			callback(format, args);
		
		va_end(args);
	}

	/*------------- custom miniz handlers -------------*/
	static size_t mz_zip_istream_read_func(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n)
	{
		mz_zip_archive *pZip = (mz_zip_archive *)pOpaque;
		std::istream *piStream = (std::istream *)pZip->m_pState->m_pFile;

		mz_int64 cur_ofs = piStream->tellg();
		if ((mz_int64)file_ofs < 0)
			return 0;

		if (cur_ofs != (mz_int64)file_ofs)
		{
			piStream->seekg(file_ofs, std::ios_base::beg);
			if (piStream->bad() || piStream->fail())
				return 0;
		}

		piStream->read((char*)pBuf, n);
		if (piStream->bad() || piStream->fail())
			return 0;

		return n;
	}

	static mz_bool mz_zip_reader_init_istream(mz_zip_archive *pZip, std::istream& stream, mz_uint32 flags)
	{
		mz_uint64 file_size;
		if (!stream.good())
			return MZ_FALSE;

		stream.seekg(0, std::ios_base::end);
		if (stream.bad() || stream.fail())
			return MZ_FALSE;

		//get file size
		file_size = stream.tellg();
		stream.seekg(0, std::ios_base::beg);
		if (stream.bad() || stream.fail())
			return MZ_FALSE;

		if (!mz_zip_reader_init_internal(pZip, flags))
		{
			return MZ_FALSE;
		}

		pZip->m_pRead = mz_zip_istream_read_func;
		pZip->m_pIO_opaque = pZip;
		pZip->m_pState->m_pFile = (void**)&stream;
		pZip->m_archive_size = file_size;
		if (!mz_zip_reader_read_central_dir(pZip, flags))
		{
			mz_zip_reader_end(pZip);
			return MZ_FALSE;
		}
		return MZ_TRUE;
	}

	//NesSystemWrapper::StreamFinalizer
	NesSystemWrapper::StreamFinalizer::StreamFinalizer() 
		: m_deleterFunc(nullptr)
	{}
	NesSystemWrapper::StreamFinalizer::StreamFinalizer(delete_func_t deleteFunc)
		: m_deleterFunc(deleteFunc)
	{}

	void NesSystemWrapper::StreamFinalizer::operator() (std::istream* stream)
	{
		if (m_deleterFunc)
			m_deleterFunc(stream);
		else
			delete stream;
	}
	
	//NesSystemWrapper
	NesSystemWrapper::NesSystemWrapper(std::istream* dbStream, VLogCallback logCallback, ErrorCallback errCallback)
		:	m_loaded(false),
			m_emulator(),
			m_errCallback(errCallback),
			m_vlogCallback(logCallback),
			m_machineEventCallback(nullptr)
	{
		if (dbStream)
		{
			Api::Cartridge::Database database(m_emulator);
			//load database
			if (NES_SUCCEEDED(database.Load(*dbStream)))
				database.Enable(true);
		}
		
		//init callbacks
		Api::User::logCallback.Set(LogCallback, this);
		Api::Machine::eventCallback.Set(MachineEvent, this);
		Api::User::questionCallback.Set(QuestionCallback, this);
	}
	
	NesSystemWrapper::~NesSystemWrapper() {
		Api::User::logCallback.Set(NULL, NULL);
		Api::Machine::eventCallback.Set(NULL, NULL);
		Api::User::questionCallback.Set(NULL, NULL);
	}
	
	
	void NesSystemWrapper::SetInput(Input::IInput*&& input)
	{
		m_input = std::unique_ptr<Input::IInput>(input);
		input = nullptr;
	}
	
	void NesSystemWrapper::LogCallback(void* userData, const char* string, ulong length)
	{
		auto instance = reinterpret_cast<NesSystemWrapper*>(userData);
		LogWrapper(instance->m_vlogCallback, "%.*s", (int)length, string);
	}
	
	
	void NesSystemWrapper::ResetView(unsigned int width, unsigned int height, bool contextRecreate, OpenFileCallback resourceLoader)
	{
		ResetView((float)width, (float)height, contextRecreate, resourceLoader);
	}

	void NesSystemWrapper::ResetView(float width, float height, bool contextRecreate, OpenFileCallback resourceLoader)
	{
		Core::Log() << "NesSystemWrapper::ResetView(" << (int)width << ", " << (int)height << ", " << (int)contextRecreate << ")";
		
		if (contextRecreate)
		{	
			m_renderer->Invalidate();
			if (m_input)
				m_input->Invalidate();
		}
		
		m_renderer->Reset(width, height);
		
		if (m_input)
			m_input->Reset(*m_renderer, resourceLoader);
	}
	
	void NesSystemWrapper::CleanupGraphicsResources() {
		m_renderer->Cleanup();
		if (m_input)
			m_input->CleanupGraphicsResources();
	}
	
	std::unique_ptr<std::istream, NesSystemWrapper::StreamFinalizer> NesSystemWrapper::OpenGameFileStream(std::istream& file)
	{
		std::istream *pis = NULL;

		StreamFinalizer streamDeleter;

		//first check if it is a zip file
		mz_zip_archive za = {0};
		if (mz_zip_reader_init_istream(&za, file, 0))
		{
			std::vector<char> pathBuf;
			pathBuf.insert(pathBuf.begin(), 1024, '\0');
			
			auto files = mz_zip_reader_get_num_files(&za);
			if (files != 0)
			{
				int fileIdx = -1;
				for (mz_uint i = 0; i < files; ++i)
				{
					if (mz_zip_reader_is_file_a_directory(&za, i) || mz_zip_reader_is_file_encrypted(&za, i))
						continue;
					if (fileIdx == -1)
					{
						auto pathLength = mz_zip_reader_get_filename(&za, i, 0, 0);
						if (pathLength > pathBuf.size())
							pathBuf.insert(pathBuf.end(), pathLength - pathBuf.size(), '\0');
						
						mz_zip_reader_get_filename(&za, i, pathBuf.data(), (mz_uint)pathBuf.size());
						
						auto path = pathBuf.data();
						
						//check if the file name contains valid extension
						if (ChecKRomExt(path, pathLength - 1))
						{
							fileIdx = i;//first file
							break;
						}
					}//if (fileIdx == -1)
				}//for (mz_uint i = 0; i < files; ++i)
				
				if (fileIdx != -1)
				{
					size_t fileSize = 0;
					auto fileData = (char*)mz_zip_reader_extract_to_heap(&za, fileIdx, &fileSize, 0);
					
					if (fileData)
					{
						//convert data to istream
						pis = new Api::MemInputStream(std::move(fileData), fileSize);
						
						free(fileData);
					}
				}
			}//if (files != 0)
			
			mz_zip_reader_end(&za);
		}//if (mz_zip_reader_init_file(&za, file.c_str, 0))
		else //normal file
		{
			//just copy the stream's reference directly (won't delete it after use)
			file.seekg(0, std::ios_base::beg);
			pis = &file;

			streamDeleter.m_deleterFunc = [](std::istream* pstream) {
				//DO NOTHING
			};
		}
		if (pis == NULL)
			return nullptr;
			
		return std::unique_ptr<std::istream, StreamFinalizer>(pis, streamDeleter);
	}
	

	Result NesSystemWrapper::VerifyGame(const std::string& file) {
		std::ifstream fs(file);

		return VerifyGame(fs);
	}

	Result NesSystemWrapper::VerifyGame(std::istream& file)
	{
		auto pis = OpenGameFileStream(file);
		Result result = Nes::RESULT_ERR_INVALID_FILE;
		if (pis)
		{
			//create verifier emulator
			if (m_pVerifierEmulator == nullptr)
				m_pVerifierEmulator = std::unique_ptr<Api::Emulator>(new Api::Emulator());
			
			//disable machine event callback
			Api::Machine::EventCallback oldFunc;
			Api::Base::UserData oldUserData;
			Api::Machine::eventCallback.Get(oldFunc, oldUserData);
			Api::Machine::eventCallback.Unset();
			
			//try to load rom file
			Api::Machine verifierMachine(*m_pVerifierEmulator);
			result = verifierMachine.Load(*pis, Api::Machine::FAVORED_NES_NTSC);
			verifierMachine.Unload();
			
			//restore machine event callback
			Api::Machine::eventCallback.Set(oldFunc, oldUserData);
		}
		
		return result;
	}
	
	Result NesSystemWrapper::LoadGame(const std::string& file)
	{
		std::ifstream fs(file);
		return LoadGame(file, fs);
	}

	Result NesSystemWrapper::LoadGame(const std::string& fileName, std::istream& fileStream)
	{
		if (m_loadedFile.compare(fileName) == 0)
			return Nes::RESULT_OK;//already loaded
		
		
		Result result = Nes::RESULT_ERR_INVALID_FILE;
		std::stringstream errorSS;
		
		auto pis = OpenGameFileStream(fileStream);
		
		
		if (pis)
		{
			result = Api::Machine(m_emulator).Load(*pis, Api::Machine::FAVORED_NES_NTSC);
#if DEBUG_REMOTE_CONTROL
			Api::Machine(m_emulator).EnableRemoteController(0, DEFAULT_REMOTE_PORT);
#endif
		}
		
		//check for error
		if (NES_FAILED(result)) {
			switch (result) {
				case Nes::RESULT_ERR_INVALID_FILE:
					errorSS << "Error: Invalid file " << fileName;
					break;
					
				case Nes::RESULT_ERR_OUT_OF_MEMORY:
					errorSS << "Error: Out of Memory";
					break;
					
				case Nes::RESULT_ERR_CORRUPT_FILE:
					errorSS << "Error: Corrupt or Missing File " << fileName;
					break;
					
				case Nes::RESULT_ERR_UNSUPPORTED_MAPPER:
					errorSS << "Error: Unsupported Mapper";
					break;
					
				case Nes::RESULT_ERR_MISSING_BIOS:
					errorSS << "Error: Missing Fds BIOS";
					break;
					
				default:
					errorSS << "Error: " << result;
					break;
			}
			
			m_loaded = false;
			
			auto errorMsg = errorSS.str();
			if (m_errCallback)
			{	
				m_errCallback(errorMsg.c_str());
			}
		}
		else
		{
			Api::Machine machine(m_emulator);
			// Set the region
			if (Api::Cartridge::Database(m_emulator).IsLoaded())
				machine.SetMode(machine.GetDesiredMode());
			
			if (machine.GetMode() == Api::Machine::PAL) {
				LogWrapper(m_vlogCallback, "Region: PAL\n");
			}
			else {
				LogWrapper(m_vlogCallback, "Region: NTSC\n");
			}
			
			machine.Power(true);
			
			m_loaded = true;
			
			m_loadedFile = fileName;
		}
		
		return result;
	}
	
	bool NesSystemWrapper::LoadRemote(const std::string&ip, int port, const char* clientInfo)
	{
		m_loadedFile.clear();
		m_loaded = false;
		
		auto result = Api::Machine(m_emulator).LoadRemote(ip, port, clientInfo);
		
		if (NES_FAILED(result))
		{
			if (m_errCallback)
				m_errCallback("Error: Failed to establish remote connection");
			return false;
		}
		
		m_loaded = true;
		
		return true;
	}
	
	bool NesSystemWrapper::LoadRemote(std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* clientInfo)
	{
		m_loadedFile.clear();
		m_loaded = false;
		
		auto result = Api::Machine(m_emulator).LoadRemote(connHandler, clientInfo);
		
		if (NES_FAILED(result))
		{
			if (m_errCallback)
				m_errCallback("Error: Failed to establish remote connection");			
			return false;
		}
		
		m_loaded = true;
		
		return true;
	}
	
	bool NesSystemWrapper::EnableRemoteController(uint32_t idx, int port, const char* hostName)
	{
		if (NES_SUCCEEDED(Api::Machine(m_emulator).EnableRemoteController(idx, port, hostName)))
			return true;
		
		if (m_errCallback)
			m_errCallback("Error: Failed to enable remote controller");
		
		return false;
	}
	
	bool NesSystemWrapper::EnableRemoteController(uint32_t idx, std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* hostName)
	{
		if (NES_SUCCEEDED(Api::Machine(m_emulator).EnableRemoteController(idx, connHandler, hostName)))
			return true;
		
		if (m_errCallback)
			m_errCallback("Error: Failed to enable remote controller");
		
		return false;
	}
	
	std::shared_ptr<HQRemote::IConnectionHandler> NesSystemWrapper::GetRemoteControllerHostConnHandler() {
		return Api::Machine(m_emulator).GetRemoteControllerHostConnHandler();
	}
	
	void NesSystemWrapper::DisableRemoteController(uint32_t idx)
	{
		Api::Machine(m_emulator).DisableRemoteController(idx);
	}
	
	bool NesSystemWrapper::RemoteControllerConnected(uint32_t idx)
	{
		return Api::Machine(m_emulator).RemoteControllerConnected(idx);
	}
	
	bool NesSystemWrapper::RemoteControllerEnabled(uint idx)
	{
		return Api::Machine(m_emulator).RemoteControllerEnabled(idx);
	}
	
	bool NesSystemWrapper::RemoteControlling()
	{
		return m_loaded && Api::Machine(m_emulator).Is(Api::Machine::REMOTE);
	}
	
	Result NesSystemWrapper::SendMessageToRemote(uint64_t id, const char* message)
	{
		return Api::Machine(m_emulator).SendMessageToRemote(id, message);
	}
	
	const char* NesSystemWrapper::GetRemoteName(uint32_t idx)
	{
		return Api::Machine(m_emulator).GetRemoteName(idx);
	}
	
	Result NesSystemWrapper::SaveState(const std::string& file) {
		if (m_loaded && !Api::Machine(m_emulator).Is(Api::Machine::REMOTE))
		{
			std::ofstream statefile(file, std::ifstream::out|std::ifstream::binary);
			return Api::Machine(m_emulator).SaveState(statefile);
		}
		return RESULT_NOP;
	}
	
	Result NesSystemWrapper::SaveState(std::ostream& file) {
		if (m_loaded && !Api::Machine(m_emulator).Is(Api::Machine::REMOTE))
		{
			return Api::Machine(m_emulator).SaveState(file);
		}
		return RESULT_NOP;
	}
	
	Result NesSystemWrapper::LoadState(const std::string& file) {
		if (m_loaded && !Api::Machine(m_emulator).Is(Api::Machine::REMOTE))
		{
			std::ifstream statefile(file, std::ifstream::in|std::ifstream::binary);
			return Api::Machine(m_emulator).LoadState(statefile);
		}
		return RESULT_NOP;
	}
	
	Result NesSystemWrapper::LoadState(std::istream& file) {
		if (m_loaded && !Api::Machine(m_emulator).Is(Api::Machine::REMOTE))
		{
			return Api::Machine(m_emulator).LoadState(file);
		}
		return RESULT_NOP;
	}
	
	//get name of loaded rom file
	std::string NesSystemWrapper::LoadedFileName() const {
		auto slashPos = m_loadedFile.find_last_of('/');
		if (slashPos == std::string::npos)
			slashPos = m_loadedFile.find_last_of('\\');
		
		if (slashPos == std::string::npos)
			return m_loadedFile;
		
		return m_loadedFile.substr(slashPos + 1);
	}
	
	//hardware input
	void NesSystemWrapper::OnJoystickMoved(float x, float y) // x, y must be in range [-1, 1]
	{
		if (m_input)
			m_input->OnJoystickMoved(x, y);
	}
	
	void NesSystemWrapper::OnUserHardwareButtonDown(int button) {
		if (m_input)
			m_input->OnUserHardwareButtonDown((Input::UserHardwareButton)button);
	}
	
	void NesSystemWrapper::OnUserHardwareButtonUp(int button) {
		if (m_input)
			m_input->OnUserHardwareButtonUp((Input::UserHardwareButton)button);
	}
	
	//touches
	bool NesSystemWrapper::OnTouchBegan(void* id, float x, float y)
	{
		if (m_input)
			return m_input->OnTouchBegan(id, x, y);
		return false;
	}
	bool NesSystemWrapper::OnTouchMoved(void* id, float x, float y)
	{
		if (m_input)
			return m_input->OnTouchMoved(id, x, y);
		
		return false;
	}
	bool NesSystemWrapper::OnTouchEnded(void* id, float x, float y)
	{
		if (m_input)
			return m_input->OnTouchEnded(id, x, y);	
		return false;
	}
	
	//execution loop
	void NesSystemWrapper::Execute(bool skipRender)
	{
		if (!m_loaded)
			return;
		
		if (m_input)
			m_input->BeginFrame();
		
		m_emulator.Execute(skipRender ? NULL : &m_video, &m_sound, m_input.get(), &m_soundInput);
		
		m_renderer->PresentVideo();
		
		if (m_input)
			m_input->EndFrame(*m_renderer);
	}
	
	//system controls
	void NesSystemWrapper::Reset()
	{
		Api::Machine(m_emulator).Reset(false);
	}
	
	void NesSystemWrapper::Shutdown()
	{
		if (RemoteControlling())
		{
			//only shutdown remote controlling session. Real game session will be retained for later resume
			Api::Machine(m_emulator).Power(false);
		
			m_loaded = false;
			m_loadedFile.clear();
		}
	}
	
	void NesSystemWrapper::EnableUIButtons(bool e) {
		if (m_input)
			m_input->EnableUI(e);
	}

	void NesSystemWrapper::SwitchABTurboMode(bool aIsTurbo, bool bIsTurbo) {
		if (m_input)
			m_input->SwitchABTurboMode(aIsTurbo, bIsTurbo);
	}

	Api::Machine::Mode NesSystemWrapper::GetMode() {
		return Api::Machine(m_emulator).GetMode();
	}
	
	//callbacks
	void NesSystemWrapper::MachineEvent(Api::Machine::Event event, Result value)
	{
		switch (event) {
		case Api::Machine::EVENT_REMOTE_CONNECTION_INTERNAL_ERROR://value is address of string data
		{
			auto message = (const char*)(intptr_t)value;
			
			if (strcmp(message, "remote_cancel") != 0)
			{
				LogWrapper(m_vlogCallback, "%s\n", message);
			}
		}
			break;
		case Api::Machine::EVENT_REMOTE_DISCONNECTED:
		{
			const char message[] = "Fatal: Remote connection timeout or disconnected";
			LogWrapper(m_vlogCallback, "%s\n", message);
		}
			break;
			
		case Api::Machine::EVENT_UNLOAD:
				
			m_loaded = false;
			m_loadedFile.clear();
			break;
		default:
			//TODO
			break;
		}
		
		if (m_machineEventCallback)
			m_machineEventCallback(event, value);
	}
	
	Api::User::Answer NesSystemWrapper::QuestionCallback(Api::User::Question question)
	{
		switch(question) {
		case Api::User::QUESTION_NST_PRG_CRC_FAIL_CONTINUE:
			return Api::User::ANSWER_NO;//TODO
		default:
			return Api::User::ANSWER_DEFAULT;
		}
	}
	
	
	void NST_CALLBACK NesSystemWrapper::MachineEvent(void* userData, Api::Machine::Event event, Result value)
	{
		auto system = reinterpret_cast<NesSystemWrapper*>(userData);
		system->MachineEvent(event, value);//notify nes system
	}
	
	Api::User::Answer NST_CALLBACK NesSystemWrapper::QuestionCallback(void* userData, Api::User::Question question)
	{
		auto system = reinterpret_cast<NesSystemWrapper*>(userData);
		return system->QuestionCallback(question);//notify nes system
	}
}