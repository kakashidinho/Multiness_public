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
#include "NstWindowUser.hpp"
#include "NstDialogInternetRemote.hpp"

#include <windows.h>

#include <mutex>
#include <sstream>

namespace Nestopia
{
	namespace Window
	{
		static String::Heap<char> g_historyGUID;
		static String::Heap<char> g_historyPIN;
		static std::mutex g_globalLock;

		struct InternetRemoteControl::Handlers
		{
			static const MsgHandler::Entry<InternetRemoteControl> messages[];
			static const MsgHandler::Entry<InternetRemoteControl> commands[];
		};

		const MsgHandler::Entry<InternetRemoteControl> InternetRemoteControl::Handlers::messages[] =
		{
			{ WM_INITDIALOG, &InternetRemoteControl::OnInitDialog }
		};

		const MsgHandler::Entry<InternetRemoteControl> InternetRemoteControl::Handlers::commands[] =
		{
			{ IDOK, &InternetRemoteControl::OnCmdOk },
			{ IDCANCEL, &InternetRemoteControl::OnCmdCancel },
		};

		InternetRemoteControl::InternetRemoteControl()
			: dialog(IDD_LOAD_REMOTE_INTERNET, this, Handlers::messages, Handlers::commands)
		{
			this->remoteParams.valid = true;
		}

		InternetRemoteControl::~InternetRemoteControl()
		{
		}

		ibool InternetRemoteControl::OnInitDialog(Param&)
		{
			//enforce valid text format for PIN & GUID
			dialog.Edit(IDC_GUID).Limit(20);
			dialog.Edit(IDC_GUID).SetNumberOnly();

			dialog.Edit(IDC_PIN).Limit(20);
			dialog.Edit(IDC_PIN).SetNumberOnly();

			//default value
			{
				std::lock_guard<std::mutex> lg(g_globalLock);

				dialog.Edit(IDC_GUID).Text() << g_historyGUID.Ptr();
				dialog.Edit(IDC_PIN).Text() << g_historyPIN.Ptr();
			}
			return true;
		}

		void InternetRemoteControl::CloseOk()
		{
			//read typed GUID and PIN
			{
				std::lock_guard<std::mutex> lg(g_globalLock);

				dialog.Edit(IDC_GUID).Text() >> g_historyGUID;
				dialog.Edit(IDC_PIN).Text() >> g_historyPIN;

				//parse GUID
				{
					std::stringstream ss;
					ss << g_historyGUID.Ptr();

					if (g_historyGUID.Length() == 0)
					{
						//empty PIN is not allowed
						User::Fail(L"GUID must not be empty");

						return;
					}

					ss >> this->remoteParams.remote_guid;
				}

				//parse PIN
				{
					std::stringstream ss;
					ss << g_historyPIN.Ptr();

					if (g_historyPIN.Length() == 0)
					{
						//empty PIN is not allowed
						User::Fail(L"PIN must not be empty");

						return;
					}

					ss >> this->remoteParams.remote_pin;
				}
				
			}

			dialog.Close();
		}

		ibool InternetRemoteControl::OnCmdOk(Param& param)
		{
			if (param.Button().Clicked())
				CloseOk();

			return true;
		}

		ibool InternetRemoteControl::OnCmdCancel(Param& param) {
			if (param.Button().Clicked())
			{
				this->remoteParams.valid = false;
				dialog.Close();
			}
			return true;
		}
	}
}