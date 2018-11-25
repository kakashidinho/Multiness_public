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

#include "NstBase.hpp"
#include "NstVideoScreen.hpp"

#include <RemoteController/Server/Engine.h>

#include <atomic>

#define DEFAULT_REMOTE_FPS 28
#define DEFAULT_REMOTE_FRAME_INTERVAL (1.0 / DEFAULT_REMOTE_FPS)
#define DEFAULT_REMOTE_FRAME_INTERVAL_MS (1000 / DEFAULT_REMOTE_FPS)

#define SLOW_NET_REMOTE_FPS 20
#define SLOW_NET_REMOTE_FRAME_INTERVAL (1.0 / SLOW_NET_REMOTE_FPS)

#define REMOTE_USE_H264 0
#define REMOTE_USE_VPX 0

#define NES_WIDTH Core::Video::Screen::WIDTH
#define NES_HEIGHT Core::Video::Screen::HEIGHT

namespace Nes
{
	namespace Core {
		namespace Video {
			class Output;
		}
		
		enum FrameCompressorType {
			FRAME_COMPRESSOR_TYPE_ZLIB,
			FRAME_COMPRESSOR_TYPE_H264,
		};

		class FrameCompressorOrDecompressorBase {
		public:
			virtual ~FrameCompressorOrDecompressorBase();

			// set this to true to downsample the frame before compression, maybe unused by some implementations
			// for decompressor. This value would be set to true during decompression to indicate the frame was downsampled by host side
			std::atomic<uint32_t> downSample;

			const FrameCompressorType type;

			virtual void Start() = 0;
			virtual void Stop() = 0;

			virtual void FrameStep(Video::Output* videoOutput) {}

			virtual bool OnRemoteEvent(const HQRemote::Event& event) { return false; }
		protected:
			FrameCompressorOrDecompressorBase(FrameCompressorType _type)
				: downSample(false), type(_type)
			{}
		};

		class FrameCompressorBase: public FrameCompressorOrDecompressorBase, public HQRemote::IImgCompressor {
		public:
			bool UseIndexedColor() const { return m_useIndexedColor; }

			virtual void Restart() {}

			virtual void AdaptToClientSlowRecvRate(float clientRcvRate, float ourSendingRate) {}
			virtual void AdaptToClientFastRecvRate(float clientRcvRate, float ourSendingRate) {}
		protected:
			FrameCompressorBase(FrameCompressorType _type, bool useIndexedColor)
				: FrameCompressorOrDecompressorBase(_type),
				m_useIndexedColor(useIndexedColor)
			{}

			bool m_useIndexedColor;
		};

		class VideoFrameCompressorBase : public FrameCompressorBase {
		public:
			virtual void OnDecoderCannotDecode() {}
			virtual void ForceSoftwareEncoder() {}

			virtual bool OnRemoteEvent(const HQRemote::Event& event) override;

			// IImgCompressor
			virtual bool canSupportMultiThreads() const override { return false; }
		protected:
			VideoFrameCompressorBase(FrameCompressorType _type)
				: FrameCompressorBase(_type, false)
			{}
		};

		class FrameDecompressorBase : public FrameCompressorOrDecompressorBase {
		protected:
			FrameDecompressorBase(FrameCompressorType type)
				: FrameCompressorOrDecompressorBase(type)
			{}
		};
	}
}
