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

#include "NstMachine.hpp"
#include "NstFrameCompressorZlib.hpp"
#include "NstRemoteEvent.hpp"
#include "api/NstApiMachine.hpp"

#include <assert.h>

#ifdef WIN32
#	define thread_local __declspec(thread) 
#	define USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS 0
#else
#	define USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS 1
#	include <pthread.h>
#endif

#define INVALID_FRAME_ID (uint64_t)((int64_t)-1)

#define ENABLE_REMAP_REMOTE_COLOR 1
#define ENABLE_REMOTE_KEYFRAME 1
#define ENABLE_REMOTE_FRAME_COMPRESS 1

#if ENABLE_REMOTE_FRAME_COMPRESS
#	define ENABLE_REMOTE_FRAME_SIZE_LIMIT 1
#	define MIN_REMOTE_SEND_RATE_BUDGET (50 * 1024)
#endif//if ENABLE_REMOTE_FRAME_COMPRESS

#define REMOTE_KEYFRAME_INTERVAL 4//interval that one frame becomes keyframe

#define ADAPTIVE_DOWNSAMPLE_FLAG 0x80000000

namespace Nes {
	namespace Core {
#if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS
		static pthread_once_t g_threadKeyCreateOnce = PTHREAD_ONCE_INIT;
		static pthread_key_t g_threadKey;

		static void onThreadExit(void * arg)
		{
			//release thread key and buffer
			auto buffer = (unsigned char*)arg;

			free(buffer);
		}

		static void makeThreadKey()
		{
			pthread_key_create(&g_threadKey, onThreadExit);
		}

		static unsigned char* getOrCreateThreadSpecificBuffer(size_t size)
		{
			pthread_once(&g_threadKeyCreateOnce, makeThreadKey);

			auto threadBuffer = (unsigned char*)pthread_getspecific(g_threadKey);
			if (threadBuffer == nullptr)
			{
				threadBuffer = (unsigned char*)malloc(size);
				if (threadBuffer)
					pthread_setspecific(g_threadKey, threadBuffer);
			}

			return threadBuffer;
		}
#endif//#if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS

		//encode a ULEB128Ex value.
		static inline void encodeULEB128Ex(uint32_t Value, unsigned char *p, unsigned int bitOffset, unsigned char** nextAddr, unsigned int *nextBitOffset) {
			assert(bitOffset < 8 && (bitOffset % 4) == 0);

			do {
				unsigned char HalfByte = Value & 0x7;
				Value >>= 3;
				if (Value != 0)
					HalfByte |= 0x8; // Mark this half byte to show that more half bytes will follow.
				*p &= ~(0xf << bitOffset);
				*p |= HalfByte << bitOffset;
				bitOffset += 4;

				if (bitOffset == 8)
				{
					p++;
					bitOffset = 0;
				}
			} while (Value != 0);

			*nextAddr = p;
			*nextBitOffset = bitOffset;
		}

		//decode a ULEB128 value.
		static inline uint32_t decodeULEB128Ex(const unsigned char* address, unsigned int bitOffset, const unsigned char **nextAddr, unsigned int* nextBitOffset) {
			assert(bitOffset < 8 && (bitOffset % 4) == 0);

			const unsigned char* p = address;
			uint32_t value = 0;
			unsigned int shift = 0;
			unsigned char halfByte;
			int steps = 0;//in case the data is corrupted
			do {
				halfByte = ((*p) >> bitOffset) & 0xf;
				value += uint32_t(halfByte & 0x7) << shift;
				shift += 3;

				bitOffset += 4;
				if (bitOffset == 8)
				{
					p++;
					bitOffset = 0;
				}
			} while (halfByte >= 0x8 && (++steps) < 4);

			*nextAddr = p;
			*nextBitOffset = bitOffset;

			return value;
		}

		//encode a SLEB128 value
		static inline void encodeSLEB128Ex(int32_t Value, unsigned char *p, unsigned int bitOffset, unsigned char** nextAddr, unsigned int *nextBitOffset) {
			assert(bitOffset < 8 && (bitOffset % 4) == 0);

			bool more;
			do {
				unsigned char HalfByte = Value & 0x7;
				Value >>= 3;

				more = !((((Value == 0) && ((HalfByte & 0x4) == 0)) ||
					((Value == -1) && ((HalfByte & 0x4) != 0))));
				if (more)
					HalfByte |= 0x8; // Mark this half byte to show that more half bytes will follow.

				*p &= ~(0xf << bitOffset);
				*p |= HalfByte << bitOffset;
				bitOffset += 4;

				if (bitOffset == 8)
				{
					p++;
					bitOffset = 0;
				}
			} while (more);

			*nextAddr = p;
			*nextBitOffset = bitOffset;
		}

		//decode a SLEB128 value.
		static inline int32_t decodeSLEB128Ex(const unsigned char* address, unsigned int bitOffset, const unsigned char **nextAddr, unsigned int* nextBitOffset) {
			assert(bitOffset < 8 && (bitOffset % 4) == 0);

			const unsigned char* p = address;
			int32_t value = 0;
			unsigned int shift = 0;
			unsigned char halfByte;
			int steps = 0;//in case the data is corrupted
			do {
				halfByte = ((*p) >> bitOffset) & 0xf;
				value |= (halfByte & 0x7) << shift;
				shift += 3;

				bitOffset += 4;
				if (bitOffset == 8)
				{
					p++;
					bitOffset = 0;
				}
			} while (halfByte >= 0x8 && (++steps) < 4);

			//sign extend negative numbers.
			if (halfByte & 0x4)
				value |= int32_t(-1) << shift;

			*nextAddr = p;
			*nextBitOffset = bitOffset;

			return value;
		}

		/*------------- ZlibFrameCompressorBase ---------------*/
		ZlibFrameCompressorBase::ZlibFrameCompressorBase()
			: m_lastKeyframeId(0)
		{
			Reset();
		}

		void ZlibFrameCompressorBase::Reset() {
			m_lastKeyframeId = 0;
		}

		/* ------------- FrameCompressorZlib ------------------*/
		ZlibFrameCompressor::ZlibFrameCompressor(const HQRemote::Engine& server)
			: FrameCompressorBase(FRAME_COMPRESSOR_TYPE_ZLIB, true),
			HQRemote::ZlibImgComressor(ENABLE_REMOTE_FRAME_COMPRESS ? 0 : -1),
			m_server(server),
			m_avgKeyframeSize(0)
			, m_compressSizeBudget(0), m_dataRateBudget(0)
#if PROFILE_REMOTE_FRAME_COMPRESSION
			, m_avgCompressionTime(0), m_totalCompressionWindowTime(0)
#endif
		{
		}

		ZlibFrameCompressor::~ZlibFrameCompressor()
		{
			Stop();
		}

		HQRemote::DataRef ZlibFrameCompressor::compress(HQRemote::ConstDataRef src, uint64_t id, uint32_t width, uint32_t height, unsigned int numChannels)
		{
			assert(width == Core::Video::Screen::WIDTH);
			assert(height == Core::Video::Screen::HEIGHT);
			assert(numChannels == 1);

			uint32_t willDownSample = this->downSample.load(std::memory_order_relaxed);

#if PROFILE_REMOTE_FRAME_COMPRESSION
			HQRemote::ScopedTimeProfiler scopedProfiler("framecomp", m_avgCompressionTimeLock, m_avgCompressionTime, m_totalCompressionWindowTime);
#endif//#if PROFILE_REMOTE_FRAME_COMPRESSION

			//captured frame data = burst phase | screen | colors' usage count table
			auto screen = (const Video::Screen*)(src->data() + 1 * sizeof(uint32_t));
			auto colorUseCounts = (const Ppu::ColorUseCount*)(screen + 1);

#if ENABLE_REMAP_REMOTE_COLOR
			uint16_t remapColorTbl[Video::Screen::PALETTE];
			//make sure uint16_t can hold any color index
			static_assert(sizeof(remapColorTbl[0]) == sizeof(Video::Screen::Pixel), "Incompatible type");
			uint32_t numUsedColors = 0;
#else
			uint numUsedColors = Video::Screen::PALETTE;
#endif//if ENABLE_REMAP_REMOTE_COLOR

			//packed data structure = keyframe id | burst phase | downsample flag | num colors | pixels | palette table
			const auto bufferSize = sizeof(Video::Screen) + sizeof(uint32_t) * 3 + sizeof(uint64_t);
#if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS
			auto buffer = getOrCreateThreadSpecificBuffer(bufferSize);
			if (!buffer)
				return nullptr;
#else
			static thread_local unsigned char buffer[bufferSize];//TODO: local variable (may cause stack overflow) or static thread local buffer?
#endif//if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS

			uint64_t* const pKeyFrameId = (uint64_t*)buffer;
			uint32_t* const pBurstPhase = (uint32_t*)(pKeyFrameId + 1);
			uint32_t* const pDownSample = pBurstPhase + 1;
			uint32_t* const pNumUsedColors = pDownSample + 1;

			memcpy(pBurstPhase, src->data(), sizeof(uint32_t));
			size_t stepsX, stepsY;


			size_t startX, startY, endY;
			startX = startY = 0;
			stepsX = 1;
			endY = Video::Screen::HEIGHT;
			if (willDownSample)
			{
				stepsY = 2;
			}
			else {
				stepsY = 1;
			}

			try {

#if ENABLE_REMOTE_KEYFRAME
				bool isKeyFrame = false;
				std::unique_lock<std::mutex> lk(m_lock, std::defer_lock);
#endif//ENABLE_REMOTE_KEYFRAME

#if ENABLE_REMAP_REMOTE_COLOR

				//remap the color so that color with most number of using pixels will have the smallest index
				memset(remapColorTbl, 0, sizeof(remapColorTbl));
				for (uint32_t i = 0; i < Video::Screen::PALETTE && colorUseCounts[i].count != 0; ++i) {
					remapColorTbl[colorUseCounts[i].idx] = numUsedColors++;
				}
#endif//if ENABLE_REMAP_REMOTE_COLOR

				if (numUsedColors == 0)
					return nullptr;

			beginCompress:
				unsigned char* packedPixels = (unsigned char*)(pNumUsedColors + 1);
				unsigned int packedPixelsBitOffset = 0;

				memcpy(pDownSample, &willDownSample, sizeof willDownSample);
				memcpy(pNumUsedColors, &numUsedColors, sizeof numUsedColors);

#if ENABLE_REMOTE_KEYFRAME
				if (willDownSample || ((id - 1) % REMOTE_KEYFRAME_INTERVAL) == 0 || m_lastKeyframeId == 0)
				{
					//this is keyframe
					if (!willDownSample)
					{
						isKeyFrame = true;

						lk.lock();//lock other threads until we done constructing keyframe
					}
#else//ENABLE_REMOTE_KEYFRAME
					{
#endif//ENABLE_REMOTE_KEYFRAME

						memset(pKeyFrameId, 0, sizeof(m_lastKeyframeId));

						//update the pixels' color index
						for (size_t y = startY; y < endY; y += stepsY) {
							for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX) {
								size_t i = y * Video::Screen::WIDTH + x;
								uint32_t oldColorIdx = screen->pixels[i];
#if ENABLE_REMAP_REMOTE_COLOR
								auto newColorIdx = remapColorTbl[oldColorIdx];
#else
								auto newColorIdx = oldColorIdx;
#endif//if ENABLE_REMAP_REMOTE_COLOR

								const unsigned char* oldPtr = packedPixels;
								auto oldOffset = packedPixelsBitOffset;
								encodeULEB128Ex(newColorIdx, packedPixels, packedPixelsBitOffset, &packedPixels, &packedPixelsBitOffset);

#if defined DEBUG || defined _DEBUG
								if (newColorIdx > 7)
								{
									bool valid = (decodeULEB128Ex(oldPtr, oldOffset, &oldPtr, &oldOffset) == newColorIdx);
									if (!valid)
									{
										int a = 0;//debugging
									}
									assert(valid);
								}
#endif

#if ENABLE_REMOTE_KEYFRAME
								//cache keyframe's pixel
								if (!willDownSample)
									m_lastKeyframe.pixels[i] = newColorIdx;
#endif//if ENABLE_REMOTE_KEYFRAME

							}//for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX)
						}//for (size_t y = startY; y < endY; y += stepsY)
					}//if (((id - 1) % REMOTE_KEYFRAME_INTERVAL) == 0 || m_lastKeyframeId == 0)

#if ENABLE_REMOTE_KEYFRAME
				else {
					lk.lock();//wait for keyframe to be available
					auto timeout = !m_cv.wait_for(lk, std::chrono::milliseconds(5000), [this, id] { return !m_running || id - m_lastKeyframeId < REMOTE_KEYFRAME_INTERVAL; });
					lk.unlock();

					if (timeout || !m_running)
					{
						if (timeout)
							HQRemote::Log("compressor dropped a frame due to timeout");
						return nullptr;
					}
					memcpy(pKeyFrameId, &m_lastKeyframeId, sizeof(m_lastKeyframeId));

					//store the pixels' delta color index
					for (size_t y = startY; y < endY; y += stepsY) {
						for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX) {
							size_t i = y * Video::Screen::WIDTH + x;
							uint32_t oldColorIdx = screen->pixels[i];
							uint32_t keyColorIdx = m_lastKeyframe.pixels[i];
#if ENABLE_REMAP_REMOTE_COLOR
							auto newColorIdx = remapColorTbl[oldColorIdx];
#else
							auto newColorIdx = oldColorIdx;
#endif//if ENABLE_REMAP_REMOTE_COLOR
							int32_t deltaColor = newColorIdx - keyColorIdx;

							const unsigned char* oldPtr = packedPixels;
							auto oldOffset = packedPixelsBitOffset;
							encodeSLEB128Ex(deltaColor, packedPixels, packedPixelsBitOffset, &packedPixels, &packedPixelsBitOffset);

#if defined DEBUG || defined _DEBUG
							if (deltaColor > 7 || deltaColor < 0)
							{
								bool valid = (decodeSLEB128Ex(oldPtr, oldOffset, &oldPtr, &oldOffset) == deltaColor);
								if (!valid)
								{
									int a = 0;//debugging
								}
								assert(valid);
							}
#endif
						}//for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX)
					}//for (size_t y = startY; y < endY; y += stepsY)
				}//else of if (((id - 1) % REMOTE_KEYFRAME_INTERVAL) == 0 || m_lastKeyframeId == 0)

				 //done constructing keyframe, wake other compression threads
				if (isKeyFrame)
				{
					m_lastKeyframeId = id;

					m_cv.notify_all();
					lk.unlock();
				}//if (isKeyFrame)
#endif //if ENABLE_REMOTE_KEYFRAME

				 //write new color palette table
				if (packedPixelsBitOffset > 0)
					packedPixels++;
				uint32_t *newColorTbl = (uint32_t*)(packedPixels);

#if ENABLE_REMAP_REMOTE_COLOR
				//fill the table
				for (uint32_t i = 0; i < numUsedColors; ++i)
				{
					auto oldIdx = colorUseCounts[i].idx;
					auto newIndex = remapColorTbl[oldIdx];

					auto color = screen->palette[oldIdx];
					auto newColorEntry = newColorTbl + newIndex;

					//don't use assignment because of potential misaligned integer's address
					memcpy(newColorEntry, &color, sizeof(color));

#if ENABLE_REMOTE_KEYFRAME
					//cache keyframe's palette entry
					if (isKeyFrame)
						m_lastKeyframe.palette[newIndex] = color;
#endif//ENABLE_REMOTE_KEYFRAME
				}
#else// if ENABLE_REMAP_REMOTE_COLOR
				//clone the palette table
				memcpy(newColorTbl, screen->palette, sizeof(screen->palette));
				//cache keyframe's palette entry
				if (m_lastKeyframeId == id)
					memcpy(m_lastKeyframe.palette, screen->palette, sizeof(screen->palette));
#endif//if ENABLE_REMAP_REMOTE_COLOR

				size_t packedSize = (unsigned char*)(newColorTbl + *pNumUsedColors) - buffer;

				//compress frame
				auto dataToSend = ZlibImgComressor::compress(buffer, packedSize, width, height, numChannels);

				//mutex scope
				{
					std::lock_guard<std::mutex> lg(m_lock);

#if ENABLE_REMOTE_KEYFRAME
					if (!isKeyFrame)//cannot downsample keyframe
#endif//if ENABLE_REMOTE_KEYFRAME
					{
						//frame too large?
						auto compressSizeBudget = m_compressSizeBudget.load(std::memory_order_relaxed);
						if (!willDownSample &&
#if ENABLE_REMOTE_FRAME_SIZE_LIMIT
							compressSizeBudget > 0 && (HQRemote::_ssize_t)dataToSend->size() > compressSizeBudget
#else
							(HQRemote::_ssize_t)dataToSend->size() > m_avgKeyframeSize * 1.05
#endif//if ENABLE_REMOTE_FRAME_SIZE_LIMIT
							)
						{
							willDownSample = 1 | ADAPTIVE_DOWNSAMPLE_FLAG;//downsample
							stepsY = 2;
							//restart
							goto beginCompress;
						}
					}
				}//mutex scope

#if ENABLE_REMOTE_KEYFRAME
				if (isKeyFrame && dataToSend) {
					if (m_avgKeyframeSize == 0)
						m_avgKeyframeSize = dataToSend->size();
					else
						m_avgKeyframeSize = 0.8 * m_avgKeyframeSize + 0.2 * dataToSend->size();
				}
#endif//ENABLE_REMOTE_KEYFRAME

				return dataToSend;
				}
			catch (...) {
				return nullptr;
			}
		}

		void ZlibFrameCompressor::Restart() {
			// enable 50KB budget for 20 fps by default to be compatible with older client
			EnableDataRateBudget(MIN_REMOTE_SEND_RATE_BUDGET, SLOW_NET_REMOTE_FRAME_INTERVAL);
		}

		void ZlibFrameCompressor::Start() {
			m_running = true;

			m_avgKeyframeSize = 0;
			Reset();
			Restart();
		}

		void ZlibFrameCompressor::Stop() {
			std::lock_guard<std::mutex> lg(m_lock);
			m_running = false;

			//wake all waiting threads
			m_cv.notify_all();
		}

		bool ZlibFrameCompressor::OnRemoteEvent(const HQRemote::Event& event) {
			switch (event.type) {
			case HQRemote::FRAME_INTERVAL: {
				// we have new frame interval settings, update the data rate budget
				UpdateFrameInterval(event.frameInterval);
			}
				return true;
			case Remote::REMOTE_ENABLE_ADAPTIVE_DATA_RATE:

				// reset budget
				EnableDataRateBudget(0, DEFAULT_REMOTE_FRAME_INTERVAL);

				return true;
			}

			return false;
		}

		void ZlibFrameCompressor::AdaptToClientSlowRecvRate(float clientRcvRate, float ourSendingRate) {
			EnableDataRateBudget((size_t)clientRcvRate, m_server.getFrameInterval());
		}

		void ZlibFrameCompressor::AdaptToClientFastRecvRate(float clientRcvRate, float ourSendingRate) {
			EnableDataRateBudget(0, m_server.getFrameInterval());
		}

		// bytes per second
		void ZlibFrameCompressor::EnableDataRateBudget(size_t rate, double frame_interval) {
			m_dataRateBudget = rate;
#if defined MIN_REMOTE_SEND_RATE_BUDGET
			if (m_dataRateBudget > 0 && m_dataRateBudget < MIN_REMOTE_SEND_RATE_BUDGET)
				m_dataRateBudget = MIN_REMOTE_SEND_RATE_BUDGET;
#endif
			HQRemote::Log("frame compressor's data rate budget changed to %u", (uint)m_dataRateBudget);
			UpdateFrameInterval(frame_interval);
		}

		void ZlibFrameCompressor::UpdateFrameInterval(double frame_interval) {
			if (m_dataRateBudget && frame_interval > 0.001) {
				m_compressSizeBudget = (size_t)(frame_interval * m_dataRateBudget);
			}
			else {
				m_compressSizeBudget = 0;
			}

			HQRemote::Log("frame compressor's size budget changed to %u", (uint)m_compressSizeBudget.load(std::memory_order_relaxed));
		}

		/*--------------- ZlibFrameDecompressor --------------------*/
		ZlibFrameDecompressor::ZlibFrameDecompressor(Machine& machine, HQRemote::Client& client)
			: FrameDecompressorBase(FRAME_COMPRESSOR_TYPE_ZLIB),
			m_machine(machine), m_client(client),
			m_lastDecompressId(INVALID_FRAME_ID)
		{}

		void ZlibFrameDecompressor::Start() {
			m_lastDecompressId = INVALID_FRAME_ID; // invalid id
			Reset();
		}

		void ZlibFrameDecompressor::Stop() {
		}

		bool ZlibFrameDecompressor::OnRemoteEvent(const HQRemote::Event& event) {
			switch (event.type) {
			default:
				break;
			}

			return false;
		}

		void ZlibFrameDecompressor::FrameStep(Video::Output* videoOutput) {
			// try to receive frame frome remote side
			uint remoteBurstPhase;
			bool hasFrame = HandleRemoteFrame(remoteBurstPhase);

			// now render the frame
			auto& ppu = m_machine.ppu;
			auto& renderer = m_machine.renderer;

			if (videoOutput && (hasFrame || m_lastDecompressId == INVALID_FRAME_ID))
				renderer.Blit(*videoOutput, ppu.GetScreen(), remoteBurstPhase);
		}

		bool ZlibFrameDecompressor::HandleRemoteFrame(uint& remoteBurstPhase) {
			auto eventRef = m_client.getFrameEvent();
			if (!eventRef)
				return false;

			auto& event = eventRef->event;
			auto frameId = event.renderedFrameData.frameId;
			if (m_lastDecompressId != INVALID_FRAME_ID && m_lastDecompressId >= frameId)
				return false;

			uint remoteUsePermaLowres;

			if (!Decompress(event.renderedFrameData.frameData, event.renderedFrameData.frameSize, frameId, remoteBurstPhase, remoteUsePermaLowres))
				return false;

			//check if host changed its frame resolution
			if (remoteUsePermaLowres != this->downSample)
			{
				this->downSample = remoteUsePermaLowres;

				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_LOWRES, remoteUsePermaLowres ? RESULT_OK : RESULT_ERR_GENERIC);
			}

			return true;
		}

		bool ZlibFrameDecompressor::Decompress(const void* compressed, size_t compressedSize, uint64_t id, uint& burstPhase, uint& permaDownsample)
		{
			Video::Screen& screen = m_machine.ppu.GetScreen();

			uint32_t width, height, numChannels;
			auto packedFrame = HQRemote::ZlibImgComressor::decompress(compressed, compressedSize, width, height, numChannels);
			if (packedFrame != nullptr)
			{
				assert(width == Core::Video::Screen::WIDTH);
				assert(height == Core::Video::Screen::HEIGHT);
				assert(numChannels == 1);

				const unsigned char* pEnd = packedFrame->data() + packedFrame->size();
				auto fullPixels = sizeof(((Video::Screen*)0)->pixels) / sizeof(Video::Screen::Pixel);

				//packed data structure = keyframe Id | burst phase | downsample flag | num colors | pixels | palette table
				//parse burs phase & downsampling flag & number of used colors
				uint64_t keyFrameId;
				uint32_t wasDownsampled, numColors;
				size_t stepsX, stepsY;
				size_t startX, startY, endY;

				memcpy(&keyFrameId, packedFrame->data(), sizeof(keyFrameId));
				memcpy(&burstPhase, packedFrame->data() + sizeof(keyFrameId), sizeof(burstPhase));
				memcpy(&wasDownsampled, packedFrame->data() + sizeof(keyFrameId) + sizeof(burstPhase), sizeof(wasDownsampled));
				memcpy(&numColors, packedFrame->data() + sizeof(keyFrameId) + sizeof(burstPhase) + sizeof(wasDownsampled), sizeof(numColors));
				const unsigned char* packedPixels = packedFrame->data() + sizeof(keyFrameId) + sizeof(burstPhase) + sizeof(wasDownsampled) + sizeof(numColors);

				if (numColors > Video::Screen::PALETTE)//corruption
					return false;

				permaDownsample = 0;//indicates that host is using low resolution permanently or adaptively

				if (wasDownsampled != 0)
				{
					startX = 0;
					startY = 0;
					endY = Video::Screen::HEIGHT;

					stepsX = 1;
					stepsY = 2;

					assert(Video::Screen::PIXELS < fullPixels);
					screen.pixels[Video::Screen::PIXELS] = 2;//mark the element after last pixel to indicate that we use downsampled frame

					if (!(wasDownsampled & ADAPTIVE_DOWNSAMPLE_FLAG))//set <permaDownsample> to 1 if this is not adaptive downsample
						permaDownsample = 1;
				}
				else {
					startX = startY = 0;
					endY = Video::Screen::HEIGHT;
					stepsX = stepsY = 1;

					assert(Video::Screen::PIXELS < fullPixels);
					screen.pixels[Video::Screen::PIXELS] = 0;//mark the element after last pixel to indicate that we don't use downsampled frame
				}

				//unpacking pixels
				unsigned int packedPixelsBitOffset = 0;

				if (keyFrameId == 0)
				{
					//this is key frame or downsampled frame, it has full color info
					if (!wasDownsampled)
						m_lastKeyframeId = id;

					//decode pixels' color directly
					for (size_t y = startY; y < endY; y += stepsY) {
						for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX) {
							size_t i = y * Video::Screen::WIDTH + x;
							auto colorIdx = (Video::Screen::Pixel)decodeULEB128Ex(packedPixels, packedPixelsBitOffset, &packedPixels, &packedPixelsBitOffset);
							if (colorIdx < numColors)
								screen.pixels[i] = colorIdx;
							else
								screen.pixels[i] = 0;

							if (!wasDownsampled)
								m_lastKeyframe.pixels[i] = screen.pixels[i];
						}//for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX)
					}//for (size_t y = startY; y < endY; y += stepsY)
				}//if (keyFrameId == 0)
				else {
					//decode pixels' color base on difference from last key frame's pixels
					if (m_lastKeyframeId != keyFrameId)
						return false;

					for (size_t y = startY; y < endY; y += stepsY) {
						for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX) {
							size_t i = y * Video::Screen::WIDTH + x;
							auto delta = decodeSLEB128Ex(packedPixels, packedPixelsBitOffset, &packedPixels, &packedPixelsBitOffset);
							Video::Screen::Pixel colorIdx = delta + m_lastKeyframe.pixels[i];

							if (colorIdx < numColors)
								screen.pixels[i] = colorIdx;
							else
								screen.pixels[i] = 0;
						}//for (size_t x = startX; x < Video::Screen::WIDTH; x += stepsX)
					}//for (size_t y = startY; y < endY; y += stepsY)
				}//if (keyFrameId == 0)

				 //copy the color palette table
				if (packedPixelsBitOffset > 0)
					packedPixels++;
				const uint32_t *colorTbl = (const uint32_t*)(packedPixels);
				if ((unsigned char*)(colorTbl + numColors) > pEnd)//corruption
					return false;
				memcpy(screen.palette, colorTbl, numColors * sizeof(colorTbl[0]));

				if (keyFrameId == 0 && !wasDownsampled)
				{
					//cache key frame's palette
					memcpy(m_lastKeyframe.palette, screen.palette, numColors * sizeof(colorTbl[0]));
				}//if (keyFrameId == 0)

				return true;
			}//if (packedFrame != nullptr)

			return false;
		}
	}
}