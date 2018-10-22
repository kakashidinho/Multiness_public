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


#pragma once

#include "NstWindowDialog.hpp"

#include <string>

namespace Nestopia
{
	namespace Window
	{
		class EnableRemoteControl
		{
		public:
			enum Mode {
				REMOTE_CTL_MODE_LAN,
				REMOTE_CTL_MODE_INTERNET,
			};

			struct ModeParams {
				Mode mode;

				struct {
					uint64_t guid;
					uint64_t pin;
				} internetParams;
			};

			EnableRemoteControl();
			~EnableRemoteControl();

		private:

			struct Handlers;

			void CloseOk();
			void EnableInternetOptions(bool enable);

			ibool OnInitDialog(Param&);
			ibool OnCmdOk(Param&); 
			ibool OnCmdLAN(Param&);
			ibool OnCmdInternet(Param&);

			Dialog dialog;
			ModeParams params;
		public:

			const ModeParams Open()
			{
				dialog.Open();
				return params;
			}
		};
	}
}