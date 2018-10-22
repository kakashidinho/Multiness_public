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
#include "NstDialogRemote.hpp"

#include <windows.h>

namespace Nestopia
{
	namespace Window
	{
		struct RemoteControl::Handlers
		{
			static const MsgHandler::Entry<RemoteControl> messages[];
			static const MsgHandler::Entry<RemoteControl> commands[];
		};

		const MsgHandler::Entry<RemoteControl> RemoteControl::Handlers::messages[] =
		{
			{ WM_INITDIALOG, &RemoteControl::OnInitDialog }
		};

		const MsgHandler::Entry<RemoteControl> RemoteControl::Handlers::commands[] =
		{
			{ IDOK, &RemoteControl::OnCmdOk },
			{ IDCANCEL, &RemoteControl::OnCmdCancel },
		};

		RemoteControl::RemoteControl()
			: dialog(IDD_LOAD_REMOTE, this, Handlers::messages, Handlers::commands),
			cancel(false)
		{}

		RemoteControl::~RemoteControl()
		{
		}

		ibool RemoteControl::OnInitDialog(Param&)
		{
			this->remoteAddress = "";

			auto ipField = dialog.Edit(IDC_IPADDRESS1);
			ipField.Text() << "127.0.0.1";

			return true;
		}

		void RemoteControl::CloseOk()
		{
			auto ipField = dialog.Edit(IDC_IPADDRESS1);
			ipField.Text() >> this->remoteAddress;

			dialog.Close();
		}

		ibool RemoteControl::OnCmdOk(Param& param)
		{
			if (param.Button().Clicked())
				CloseOk();

			return true;
		}

		ibool RemoteControl::OnCmdCancel(Param& param) {
			if (param.Button().Clicked())
			{
				cancel = true;
				dialog.Close();
			}
			return true;
		}
	}
}