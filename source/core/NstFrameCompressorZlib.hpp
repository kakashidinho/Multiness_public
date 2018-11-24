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

#include "NstFrameCompressorCommon.hpp"

#include <RemoteController/Server/Engine.h>
#include <RemoteController/Client/Client.h>

#if defined DEBUG || defined _DEBUG
#	define PROFILE_REMOTE_FRAME_COMPRESSION 0
#else
#	define PROFILE_REMOTE_FRAME_COMPRESSION 0
#endif//#if defined DEBUG || defined _DEBUG

namespace Nes {
	namespace Core {
		class ZlibFrameCompressorBase {
		protected:
			ZlibFrameCompressorBase();

			void Reset();

			uint64_t m_lastKeyframeId;
			Video::Screen m_lastKeyframe;
		};

		class ZlibFrameCompressor : public FrameCompressorBase, ZlibFrameCompressorBase, HQRemote::ZlibImgComressor {
		public:
			ZlibFrameCompressor(const HQRemote::Engine& server);
			~ZlibFrameCompressor();

			// implements IImgCompressor
			virtual HQRemote::DataRef compress(HQRemote::ConstDataRef src, uint64_t id, uint32_t width, uint32_t height, unsigned int numChannels) override;

			// implements FrameCompressorBase
			virtual void Restart() override;

			virtual void Start() override;
			virtual void Stop() override;

			virtual bool OnRemoteEvent(const HQRemote::Event& event) override;

			virtual void AdaptToClientSlowRecvRate(float clientRcvRate, float ourSendingRate) override;
			virtual void AdaptToClientFastRecvRate(float clientRcvRate, float ourSendingRate) override;
		private:
			// bytes per second
			void EnableDataRateBudget(size_t rate, double frame_interval);
			void UpdateFrameInterval(double frame_interval);

#if PROFILE_REMOTE_FRAME_COMPRESSION
			std::mutex m_avgCompressionTimeLock;
			float m_avgCompressionTime;
			float m_totalCompressionWindowTime;
#endif //if PROFILE_REMOTE_FRAME_COMPRESSION

			const HQRemote::Engine& m_server;

			std::atomic<bool> m_running;
			std::mutex m_lock;
			std::condition_variable m_cv;
			double m_avgKeyframeSize;
			std::atomic<size_t> m_compressSizeBudget;
			size_t m_dataRateBudget;
		};

		class ZlibFrameDecompressor : public FrameDecompressorBase, ZlibFrameCompressorBase {
		public:
			ZlibFrameDecompressor(Machine& machine, HQRemote::Client& client);

			virtual void Start() override;
			virtual void Stop() override;

			virtual void FrameStep(Video::Output* videoOutput) override;
			virtual bool OnRemoteEvent(const HQRemote::Event& event) override;
		private:
			bool HandleRemoteFrame(uint& burstPhase);
			bool Decompress(const void* compressed, size_t compressedSize, uint64_t id, uint& burstPhase, uint& permaDownsample);

			Machine& m_machine;
			HQRemote::Client& m_client;

			uint64_t m_lastDecompressId;
		};
	}
}
