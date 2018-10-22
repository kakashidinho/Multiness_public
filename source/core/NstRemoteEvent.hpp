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

#ifndef NST_REMOTE_EVENT_H
#define NST_REMOTE_EVENT_H

#include "api/NstApiMachine.hpp"

#include <RemoteController/Server/Engine.h>
#include <RemoteController/Client/Client.h>

#include <string>

#ifdef NST_PRAGMA_ONCE
#pragma once
#endif

namespace Nes
{
	namespace Core
	{
		namespace Remote {
			/*----------custom remote event ------------*/
			enum RemoteEventType {
				REMOTE_INPUT = HQRemote::NO_EVENT + 1,
				RESET_REMOTE_INPUT,
				REMOTE_MODE,
				REMOTE_DOWNSAMPLE_FRAME,
				REMOTE_DATA_RATE, // either sending rate from host side or receiving rate on client side
				REMOTE_ADAPTIVE_DATA_RATE, // enable adaptive frame compression size based on data rate
			};

			struct RemoteInput {
				uint64_t id;
				uint32_t buttons;
			};

			struct RemoteMode {
				uint32_t mode;
			};
		}
	}
}


#endif//NST_REMOTE_EVENT_H
