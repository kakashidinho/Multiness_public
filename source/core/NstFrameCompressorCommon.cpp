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

#include "NstFrameCompressorCommon.hpp"
#include "NstRemoteEvent.hpp"

namespace Nes {
	namespace Core {

		//----------------- FrameCompressorOrDecompressorBase -------
		FrameCompressorOrDecompressorBase::~FrameCompressorOrDecompressorBase() {
		}

		// ---------------- VideoFrameCompressorBase -----------------
		bool VideoFrameCompressorBase::OnRemoteEvent(const HQRemote::Event& event) {
			switch (event.type) {
			case Remote::REMOTE_FORCE_RESTART_COMPRESSION_STREAM:
				Restart();
				return true;
			case Remote::REMOTE_BROKEN_COMPRESSION_STREAM:
				OnDecoderCannotDecode();
				return true;
			}

			return false;
		}
	}
}