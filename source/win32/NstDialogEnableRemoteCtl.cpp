////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2016-2018 Le Hoang Quyen
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

#include "NstWindowParam.hpp"
#include "NstCtrlCheckBox.hpp"
#include "NstWindowUser.hpp"
#include "NstDialogEnableRemoteCtl.hpp"

#include <RakPeerInterface.h>

#include <windows.h>

#include <sstream>
#include <mutex>

namespace Nestopia
{
	namespace Window
	{
		static const uint64_t g_generatedGUID = RakNet::RakPeerInterface::Get64BitUniqueRandomNumber();
		static String::Heap<char> g_historyPIN;
		static std::mutex g_globalLock;

		struct EnableRemoteControl::Handlers
		{
			static const MsgHandler::Entry<EnableRemoteControl> messages[];
			static const MsgHandler::Entry<EnableRemoteControl> commands[];
		};

		const MsgHandler::Entry<EnableRemoteControl> EnableRemoteControl::Handlers::messages[] =
		{
			{ WM_INITDIALOG, &EnableRemoteControl::OnInitDialog },
		};

		const MsgHandler::Entry<EnableRemoteControl> EnableRemoteControl::Handlers::commands[] =
		{
			{ IDOK, &EnableRemoteControl::OnCmdOk },
			{ IDC_LAN, &EnableRemoteControl::OnCmdLAN },
			{ IDC_INTERNET, &EnableRemoteControl::OnCmdInternet },
		};

		EnableRemoteControl::EnableRemoteControl()
			: dialog(IDD_ENABLE_REMOTE_CTL, this, Handlers::messages, Handlers::commands) {}

		EnableRemoteControl::~EnableRemoteControl()
		{
		}

		ibool EnableRemoteControl::OnInitDialog(Param&)
		{
			//enforce valid text format for PIN
			dialog.Edit(IDC_PIN).Limit(6);
			dialog.Edit(IDC_PIN).SetNumberOnly();

			//default mode is LAN
			dialog.RadioButton(IDC_LAN).Check();
			EnableInternetOptions(false);
			this->params.mode = REMOTE_CTL_MODE_LAN;

			//default values
			std::lock_guard<std::mutex> lg(g_globalLock);

			std::stringstream ss;
			ss << g_generatedGUID;

			dialog.Edit(IDC_GUID).Text() << ss.str().c_str();
			dialog.Edit(IDC_PIN).Text() << g_historyPIN.Ptr();

			return true;
		}

		void EnableRemoteControl::CloseOk()
		{
			/* --- store remote control parameters ----*/
			this->params.internetParams.guid = g_generatedGUID;
			this->params.internetParams.pin = 0;

			//get typed PIN
			{
				std::lock_guard<std::mutex> lg(g_globalLock);

				std::stringstream ss;
				dialog.Edit(IDC_PIN).Text() >> g_historyPIN;
				ss << g_historyPIN.Ptr();

				if (this->params.mode == REMOTE_CTL_MODE_INTERNET && g_historyPIN.Length() == 0)
				{
					//empty PIN is not allowed
					User::Fail(L"PIN must not be empty");

					return;
				}

				ss >> this->params.internetParams.pin;
			}

			dialog.Close();
		}

		void EnableRemoteControl::EnableInternetOptions(bool enable) {
			dialog.Edit(IDC_GUID).Enable(enable);
			dialog.Edit(IDC_PIN).Enable(enable);
			dialog.Control(IDC_INTERNET_PARAMS_GROUPBOX).Enable(enable);
			dialog.Control(IDC_GUID_LBL).Enable(enable);
			dialog.Control(IDC_PIN_LBL).Enable(enable);
		}

		ibool EnableRemoteControl::OnCmdOk(Param& param)
		{
			if (param.Button().Clicked())
			{
				CloseOk();
			}
			return true;
		}

		ibool EnableRemoteControl::OnCmdLAN(Param& param) {
			if (param.Button().Clicked())
			{
				//disable internet options
				EnableInternetOptions(false);

				//switch mode
				this->params.mode = REMOTE_CTL_MODE_LAN;
			}

			return true;
		}

		ibool EnableRemoteControl::OnCmdInternet(Param& param)
		{
			if (param.Button().Clicked())
			{
				//enable internet options
				EnableInternetOptions(true);

				//switch mode
				this->params.mode = REMOTE_CTL_MODE_INTERNET;
			}
			return true;
		}
	}
}