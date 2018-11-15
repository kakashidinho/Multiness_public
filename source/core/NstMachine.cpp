////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2016-2018 Le Hoang Quyen
// Copyright (C) 2003-2008 Martin Freij
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
#include "NstCartridge.hpp"
#include "NstCheats.hpp"
#include "NstNsf.hpp"
#include "NstImageDatabase.hpp"
#include "NstRemoteEvent.hpp"
#include "input/NstInpDevice.hpp"
#include "input/NstInpAdapter.hpp"
#include "input/NstInpPad.hpp"
#include "api/NstApiMachine.hpp"
#include "api/NstApiSound.hpp"
#include "api/NstApiUser.hpp"

#include <map>
#include <unordered_map>
#include <list>
#include <sstream>
#include <algorithm>
#include <assert.h>

#ifdef WIN32
#	define thread_local __declspec(thread) 
#	define USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS 0
#else
#	define USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS 1
#	include <pthread.h>
#endif

#define DEFAULT_REMOTE_FPS 28
#define DEFAULT_REMOTE_FRAME_INTERVAL (1.0 / DEFAULT_REMOTE_FPS)

#define SLOW_NET_REMOTE_FPS 20
#define SLOW_NET_REMOTE_FRAME_INTERVAL (1.0 / SLOW_NET_REMOTE_FPS)

#define REMOTE_INPUT_SEND_INTERVAL 0.2

#define ENABLE_REMAP_REMOTE_COLOR 1
#define ENABLE_REMOTE_KEYFRAME 1
#define ENABLE_REMOTE_FRAME_COMPRESS 1

#define REMOTE_RCV_RATE_UPDATE_INTERVAL 2.0
#define REMOTE_SND_RATE_UPDATE_INTERVAL 2.0
#define REMOTE_DATA_RATE_INCREASE_INTERVAL 30.0
#define MAX_REMOTE_DATA_RATE_INCREASE_ATTEMPTS 5
#define MIN_REMOTE_DATA_RATE_REPORTS_TO_START_ADAPT_FPS 5
#define MIN_REMOTE_DATA_RATE_REPORTS_TO_START_ADAPT_RES 5
#define REMOTE_DATA_ADAPT_TO_LOWER_FPS_DECISION_FACTOR 1.0 / 5
#define REMOTE_DATA_ADAPT_TO_HIGHER_FPS_DECISION_FACTOR 9.0 / 10
#define REMOTE_DATA_ADAPT_TO_LOWER_RES_DECISION_FACTOR 1.0 / 7
#define REMOTE_DATA_ADAPT_TO_HIGHER_RES_DECISION_FACTOR 9.0 / 10

#if ENABLE_REMOTE_FRAME_COMPRESS
#	define ENABLE_REMOTE_FRAME_SIZE_LIMIT 1
#	define MIN_REMOTE_SEND_RATE_BUDGET (50 * 1024)
#endif//if ENABLE_REMOTE_FRAME_COMPRESS

#define REMOTE_KEYFRAME_INTERVAL 4//interval that one frame becomes keyframe

#define REMOTE_FRAME_BUNDLE 1

#define REMOTE_AUDIO_SAMPLE_RATE 48000
#define REMOTE_AUDIO_STEREO_CHANNELS false

#if defined DEBUG || defined _DEBUG
#	define PROFILE_REMOTE_FRAME_COMPRESSION 0
#	define PROFILE_EXECUTION_TIME 0
#else
#	define PROFILE_REMOTE_FRAME_COMPRESSION 0
#	define PROFILE_EXECUTION_TIME 0
#endif//#if defined DEBUG || defined _DEBUG

#define ADAPTIVE_DOWNSAMPLE_FLAG 0x80000000

#define CLIENT_CONNECTED_STATE 1
#define CLIENT_EXCHANGE_DATA_STATE 0x7fffffff

#ifndef __max__
#	define __max__(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef __min__
#	define __min__(a,b) ((a) < (b) ? (a) : (b))
#endif

namespace Nes
{
	namespace Core
	{
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

		/*--------------- NesFrameCapturer -------------------*/
		class NesFrameCapturer : public HQRemote::IFrameCapturer {
		public:
			NesFrameCapturer(Machine& _emu)
				: HQRemote::IFrameCapturer(0, Core::Video::Screen::WIDTH, Core::Video::Screen::HEIGHT),
				emulator(_emu)
			{
			}

			virtual size_t getFrameSize() override {
				return emulator.GetFrameSize();
			}
			virtual unsigned int getNumColorChannels() override {
				return emulator.GetNumColorChannels();
			}
			virtual void captureFrameImpl(unsigned char * prevFrameDataToCopy) override {
				emulator.CaptureFrame(prevFrameDataToCopy);
			}
		private:
			Machine& emulator;
		};

		class NesFrameCompressor : public HQRemote::ZlibImgComressor {
		public:
			NesFrameCompressor()
				: HQRemote::ZlibImgComressor(ENABLE_REMOTE_FRAME_COMPRESS ? 0 : -1),
				downSample(false), m_lastKeyframeId(0), m_avgKeyframeSize(0)
				, m_compressSizeBudget(0), m_dataRateBudget(0)
#if PROFILE_REMOTE_FRAME_COMPRESSION
				, m_avgCompressionTime(0), m_totalCompressionWindowTime(0)
#endif
			{
			}
			
			~NesFrameCompressor()
			{
			}

			std::atomic<uint32_t> downSample;

			virtual HQRemote::DataRef compress(HQRemote::ConstDataRef src, uint64_t id, uint32_t width, uint32_t height, unsigned int numChannels) override
			{
				assert(width == Core::Video::Screen::WIDTH);
				assert(height == Core::Video::Screen::HEIGHT);
				assert(numChannels == 1);

				uint32_t willDownSample = this->downSample.load(std::memory_order_relaxed);

#if PROFILE_REMOTE_FRAME_COMPRESSION
				HQRemote::ScopedTimeProfiler scopedProfiler("framecomp", m_avgCompressionTimeLock, m_avgCompressionTime, m_totalCompressionWindowTime);
#endif//#if PROFILE_REMOTE_FRAME_COMPRESSION

				//captured frame data = burst phase | screen | colors' usage count table
				auto screen = (const Video::Screen*)(src->data() + 1 *  sizeof(uint32_t));
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

			bool decompress(const void* compressed, size_t compressedSize, uint64_t id, Video::Screen& screen, uint& burstPhase, uint& permaDownsample)
			{
				uint32_t width, height, numChannels;
				auto packedFrame = ZlibImgComressor::decompress(compressed, compressedSize, width, height, numChannels);
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

			void start() {
				m_running = true;

				m_lastKeyframeId = 0;
				m_avgKeyframeSize = 0;
			}

			void stop() {
				std::lock_guard<std::mutex> lg(m_lock);
				m_running = false;

				//wake all waiting threads
				m_cv.notify_all();
			}

			// bytes per second
			void enableDataRateBudget(size_t rate, double frame_interval) {
				m_dataRateBudget = rate;
#if defined MIN_REMOTE_SEND_RATE_BUDGET
				if (m_dataRateBudget > 0 && m_dataRateBudget < MIN_REMOTE_SEND_RATE_BUDGET)
					m_dataRateBudget = MIN_REMOTE_SEND_RATE_BUDGET;
#endif
				HQRemote::Log("frame compressor's data rate budget changed to %u", (uint)m_dataRateBudget);
				updateFrameInterval(frame_interval);
			}

			void updateFrameInterval(double frame_interval) {
				if (m_dataRateBudget && frame_interval > 0.001) {
					m_compressSizeBudget = (size_t)(frame_interval * m_dataRateBudget);
				} else {
					m_compressSizeBudget = 0;
				}

				HQRemote::Log("frame compressor's size budget changed to %u", (uint)m_compressSizeBudget.load(std::memory_order_relaxed));
			}

		private:
			
#if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS
			static void makeThreadKey()
			{
				pthread_key_create(&s_threadKey, onThreadExit);
			}
				
			static void onThreadExit(void * arg)
			{
				//release thread key and buffer
				auto buffer = (unsigned char*)arg;
				
				free(buffer);
			}
				
			static unsigned char* getOrCreateThreadSpecificBuffer(size_t size)
			{
				pthread_once(&s_threadKeyCreateOnce, makeThreadKey);
				
				auto threadBuffer = (unsigned char*)pthread_getspecific(s_threadKey);
				if (threadBuffer == nullptr)
				{
					threadBuffer = (unsigned char*)malloc(size);
					if (threadBuffer)
						pthread_setspecific(s_threadKey, threadBuffer);
				}
				
				return threadBuffer;
			}
				
			static pthread_once_t s_threadKeyCreateOnce;
			static pthread_key_t s_threadKey;
#endif//#if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS

#if PROFILE_REMOTE_FRAME_COMPRESSION
			std::mutex m_avgCompressionTimeLock;
			float m_avgCompressionTime;
			float m_totalCompressionWindowTime;
#endif //if PROFILE_REMOTE_FRAME_COMPRESSION
				
			std::atomic<bool> m_running;
			std::mutex m_lock;
			std::condition_variable m_cv;
			Video::Screen m_lastKeyframe;
			uint64_t m_lastKeyframeId;
			double m_avgKeyframeSize;
			std::atomic<size_t> m_compressSizeBudget;
			size_t m_dataRateBudget;
		};
		
#if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS
		pthread_once_t NesFrameCompressor::s_threadKeyCreateOnce = PTHREAD_ONCE_INIT;
		pthread_key_t NesFrameCompressor::s_threadKey;
#endif // if USE_PTHREAD_KEY_FOR_REMOTE_FRAME_COMPRESS

		class NesServerAudioCapturer : public HQRemote::IAudioCapturer {
		public:
			NesServerAudioCapturer(Machine& machine) : m_machine(machine) {
			}

			virtual uint32_t getAudioSampleRate() const override {
				return m_machine.GetAudioSampleRate();
			}

			virtual uint32_t getNumAudioChannels() const override {
				return m_machine.GetNumAudioChannels();
			}

			//capture current frame. TODO: only 16 bit PCM is supported for now
			virtual HQRemote::ConstDataRef beginCaptureAudio() override {
				return m_machine.CaptureAudioAsServer();
			}

		protected:
			Machine& m_machine;
		};

		class NesClientAudioCapturer : public NesServerAudioCapturer {
		public:
			NesClientAudioCapturer(Machine& machine) : NesServerAudioCapturer(machine) {
			}

			//capture current frame. TODO: only 16 bit PCM is supported for now
			virtual HQRemote::ConstDataRef beginCaptureAudio() override {
				return m_machine.CaptureAudioAsClient();
			}
		};

#ifdef NST_MSVC_OPTIMIZE
#pragma optimize("s", on)
#endif

		/*----------Machine ----------------*/
		Machine::Machine()
			:state(Api::Machine::NTSC),
			frame(0),
			extPort(new Input::AdapterTwo(*new Input::Pad(cpu, 0), *new Input::Pad(cpu, 1))),
			expPort(new Input::Device(cpu)),
			image(NULL),
			cheats(NULL),
			imageDatabase(NULL),
			ppu(cpu),
			remoteFrameCompressor(std::make_shared<NesFrameCompressor>()),
			lastSentInputId(0), lastSentInputTime(0), 
			avgExecuteTime(0), executeWindowTime(0),
			avgCpuExecuteTime(0), cpuExecuteWindowTime(0),
			avgPpuEndFrameTime(0), ppuEndFrameWindowTime(0),
			avgVideoBlitTime(0), videoBlitWindowTime(0),
			avgFrameCaptureTime(0), frameCaptureWindowTime(0),
			avgAudioCaptureTime(0), audioCaptureWindowTime(0),
			currentInputAudio(NULL)
		{
		}

		Machine::~Machine()
		{
			Unload();

			//frame compressor must be stopped before stopping host engine to prevent deadlock
			this->remoteFrameCompressor->stop();
			if (hostEngine)
				hostEngine->stop();
			if (clientEngine)
				clientEngine->stop();

			delete imageDatabase;
			delete cheats;
			delete expPort;

			for (uint ports=extPort->NumPorts(), i=0; i < ports; ++i)
				delete &extPort->GetDevice(i);

			delete extPort;
		}

		Result Machine::Load
		(
			std::istream& imageStream,
			FavoredSystem system,
			bool ask,
			std::istream* const patchStream,
			bool patchBypassChecksum,
			Result* patchResult,
			uint type
		)
		{
			Unload();

			Image::Context context
			(
				static_cast<Image::Type>(type),
				cpu,
				cpu.GetApu(),
				ppu,
				imageStream,
				patchStream,
				patchBypassChecksum,
				patchResult,
				system,
				ask,
				imageDatabase
			);

			image = Image::Load( context );

			switch (image->GetType())
			{
				case Image::CARTRIDGE:

					state |= Api::Machine::CARTRIDGE;

					switch (static_cast<const Cartridge*>(image)->GetProfile().system.type)
					{
						case Api::Cartridge::Profile::System::VS_UNISYSTEM:

							state |= Api::Machine::VS;
							break;

						case Api::Cartridge::Profile::System::PLAYCHOICE_10:

							state |= Api::Machine::PC10;
							break;
					}
					break;

				case Image::DISK:

					state |= Api::Machine::DISK;
					break;

				case Image::SOUND:

					state |= Api::Machine::SOUND;
					break;
			}

			UpdateModels();
			ppu.ResetColorUseCountTable();

			Api::Machine::eventCallback( Api::Machine::EVENT_LOAD, context.result );

			return context.result;
		}

			
		Result Machine::LoadRemote(const std::string& remoteIp, int remotePort, const char* clientInfo)
		{
			HQRemote::ConnectionEndpoint reliableHost(remoteIp.c_str(), remotePort);
			HQRemote::ConnectionEndpoint unreliableHost(remoteIp.c_str(), remotePort + 1);
			
			auto clientConnHandler = std::make_shared<HQRemote::SocketClientHandler>(remotePort + 2, remotePort + 3, reliableHost, unreliableHost);
			
			return LoadRemote(clientConnHandler, clientInfo);
		}
			
		Result Machine::LoadRemote(std::shared_ptr<HQRemote::IConnectionHandler> clientConnHandler, const char* _clientInfo)
		{
			DisableRemoteControllers();
			Unload();

			Result re = RESULT_OK;

			auto audioCapturer = std::make_shared<NesClientAudioCapturer>(*this);

			this->clientEngine = std::make_shared<HQRemote::Client>(clientConnHandler, DEFAULT_REMOTE_FRAME_INTERVAL, audioCapturer);
			this->clientEngine->setDesc(_clientInfo);

			if (!this->clientEngine->start(true))
			{
				this->clientEngine = nullptr;
				re = RESULT_ERR_CONNECTION;
			}
			else {
				this->state |= Api::Machine::REMOTE;
				this->clientState = 0;

				this->clientInfo = _clientInfo != NULL ? _clientInfo : "";

				this->hostName.clear();//invalidate host name until receiving update from host

				this->lastReceivedRemoteFrameTime = 0;
				this->lastReceivedRemoteFrame = 0;
				this->lastSentInputId = 0;
				this->lastSentInput = 0;
				this->lastSentInputTime = 0;
				this->lastRemoteDataRateUpdateTime = 0;
				this->lastRemoteDataRateReducingTime = 0;
				this->numberOfRemoteDataRateIncreaseAttempts = 0;
				this->numRemoteDataRateReportsSlower = 0;
				this->numRemoteDataRateReports = 0;

				ResetRemoteAudioBuffer();
				EnsureCorrectRemoteSoundSettings();

				UpdateModels();

				//auto start
				Reset(true);
			}


			Api::Machine::eventCallback(Api::Machine::EVENT_LOAD_REMOTE, re);
			return re;
		}

		void Machine::StopRemoteControl()
		{
			if (clientEngine)
			{
				clientEngine->stop();
				clientEngine = nullptr;

				clientState = 0;

				renderer.ResetState();//TODO: error checking
			}


			state &= ~uint(Api::Machine::REMOTE);
		}

		Result Machine::EnableRemoteController(uint idx, int networkListenPort, const char* hostInfo) {

			std::shared_ptr<HQRemote::IConnectionHandler> connHandler = std::make_shared<HQRemote::SocketServerHandler>(networkListenPort, networkListenPort + 1);
		
			return EnableRemoteController(idx, connHandler, hostInfo);
		}
			
		Result Machine::EnableRemoteController(uint idx, std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* hostInfo) {
			StopRemoteControl();
			//TODO: support more than 1 remote controller
			DisableRemoteController(idx);
			
			auto frameCapturer = std::make_shared<NesFrameCapturer>(*this);
			auto audioCapturer = std::make_shared<NesServerAudioCapturer>(*this);
			this->hostEngine = std::make_shared<HQRemote::Engine>(connHandler, frameCapturer, audioCapturer, this->remoteFrameCompressor, REMOTE_FRAME_BUNDLE);
			this->hostEngine->setDesc(hostInfo);

			this->remoteFrameCompressor->start();
			if (!this->hostEngine->start()) {
				this->remoteFrameCompressor->stop();
				this->hostEngine = nullptr;

				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONTROLLER_ENABLED, (Result)RESULT_ERR_CONNECTION);

				return RESULT_ERR_CONNECTION;
			}

			// store host's name
			this->hostName = hostInfo != NULL ? hostInfo : "";

			this->clientInfo.clear();//invalidate client name until receiving update from client

			// reset timer
			this->lastRemoteDataRateUpdateTime = 0;
			this->lastRemoteDataRateReducingTime = 0;
			this->numberOfRemoteDataRateIncreaseAttempts = 0;
			this->numRemoteDataRateReportsSlower = 0;
			this->numRemoteDataRateReports = 0;

			//reset color use count table
			ppu.ResetColorUseCountTable();

			//reset remote audio buffer
			ResetRemoteAudioBuffer();

			EnsureCorrectRemoteSoundSettings();

			cpu.SetRemoteControllerIdx(idx);
			cpu.GetApu().EnableFrameSnapshot(true);
			cpu.GetApu().SetPostprocessCallback([this](Sound::Output& output) {
				MixAudioWithClientCapturedAudio(output);
			});
			
			Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONTROLLER_ENABLED, (Result)idx);

			return RESULT_OK;
		}

		std::shared_ptr<HQRemote::IConnectionHandler> Machine::GetRemoteControllerHostConnHandler() {
			if (hostEngine)
				return hostEngine->getConnHandler();
			return nullptr;
		}

		void Machine::DisableRemoteController(uint idx)
		{
			//TODO: support more than 1 remote controller
			if (hostEngine) {
				//frame compressor must be stopped before stopping host engine to prevent deadlock
				this->remoteFrameCompressor->stop();

				hostEngine->stop();
				hostEngine = nullptr;

				cpu.SetRemoteControllerIdx(0xffffffff);
			}

			cpu.GetApu().EnableFrameSnapshot(false);
			cpu.GetApu().SetPostprocessCallback(nullptr);
			
			Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONTROLLER_DISABLED, (Result)idx);
		}
			
		void Machine::DisableRemoteControllers()
		{
			//TODO: support more than 1 remote controller
			DisableRemoteController(0);
		}

		bool Machine::RemoteControllerConnected(uint idx) const
		{
			//TODO: support more than 1 remote controller
			if (cpu.GetRemoteControllerIdx() != idx)
				return false;
			return this->hostEngine != nullptr && this->hostEngine->connected();
		}

		bool Machine::RemoteControllerEnabled(uint idx) const
		{
			return cpu.GetRemoteControllerIdx() == idx;
		}

		void Machine::EnableLowResRemoteControl(bool e) {
			uint32_t downSample = this->remoteFrameCompressor->downSample = e ? 1 : 0;

			if (this->clientEngine && this->clientEngine->connected())
			{
				//tell host to send downsampled frame or not from now on
				HQRemote::PlainEvent event(Remote::REMOTE_DOWNSAMPLE_FRAME);
				memcpy(event.event.customData, &downSample, sizeof(downSample));
				this->clientEngine->sendEvent(event);
			}
		}

		//<id> is used for ACK message later to acknowledge that the message is received by remote side.
		//<message> must not have more than MAX_REMOTE_MESSAGE_SIZE bytes (excluding NULL character). Otherwise RESULT_ERR_BUFFER_TOO_BIG is retuned.
		//This function can be used to send message between client & server
		Result Machine::SendMessageToRemote(uint64_t id, const char* message) {
			//TODO: support more than 1 remote controller
			size_t msgLen = strlen(message);
			if (msgLen > Api::Machine::MAX_REMOTE_MESSAGE_SIZE)
				return RESULT_ERR_BUFFER_TOO_BIG;

			try {
				HQRemote::FrameEvent messageEvent(message, msgLen, id, HQRemote::MESSAGE);

				if (hostEngine && hostEngine->connected())
				{
					hostEngine->sendEvent(messageEvent);
				}
				else if (clientEngine && clientEngine->connected())
				{
					clientEngine->sendEvent(messageEvent);
				}
				else
				{
					return RESULT_ERR_NOT_READY;
				}
			}
			catch (...) {
				return RESULT_ERR_OUT_OF_MEMORY;
			}

			return RESULT_OK;
		}

		//<idx> is ignored if machine is in client mode
		const char* Machine::GetRemoteName(uint remoteCtlIdx) const {
			//TODO: support more than 1 remote controller
			if (hostEngine && remoteCtlIdx == cpu.GetRemoteControllerIdx())
			{
				return clientInfo.c_str();
			}
			else if (clientEngine)
			{
				return hostName.c_str();
			}
			else
			{
				return NULL;
			}
		}

		void Machine::HandleCommonEvent(const HQRemote::Event& event) {
			switch (event.type)
			{
			case HQRemote::MESSAGE:
			{
				Api::Machine::RemoteMsgEventData messageData;

				char message[Api::Machine::MAX_REMOTE_MESSAGE_SIZE + 1];
				auto messageLen = __min__(Api::Machine::MAX_REMOTE_MESSAGE_SIZE, event.renderedFrameData.frameSize);
				memcpy(message, event.renderedFrameData.frameData, messageLen);
				message[messageLen] = '\0';

				messageData.senderIdx = hostEngine ? cpu.GetRemoteControllerIdx() : -1;//host. TODO: support multiple clients?
				messageData.message = message;

				//invoke callback
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_MESSAGE, (Result)(intptr_t)(&messageData));
			}
			break;
			case HQRemote::MESSAGE_ACK:
			{
				auto id = event.renderedFrameData.frameId;
				//invoke callback
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_MESSAGE_ACK, (Result)(intptr_t)(&id));
			}
			break;
			}
		}

		void Machine::HandleRemoteEventsAsClient(uint &remoteBurstPhase, Sound::Output* soundOutput) {
			if (this->clientEngine->connected() == false) {
				auto errorMsg = this->clientEngine->getConnectionInternalError();
				if (errorMsg || this->clientEngine->timeSinceStart() > 30.0f || this->clientState != 0)
				{
#ifdef DEBUG
					HQRemote::Log("remote connection's error/timeout detected\n");
#endif
					
					Unload();
					
					if (errorMsg)
					{
#ifdef DEBUG
						HQRemote::Log("remote connection's invoking error callback\n");
#endif
						Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTION_INTERNAL_ERROR, (Result)(intptr_t)(errorMsg->c_str()));
						
#ifdef DEBUG
						HQRemote::Log("remote connection's invoked error callback\n");
#endif
					}
					else
						Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_DISCONNECTED);
				}
				
				//avoid using up too much cpu
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				
				return;
			}//if (this->clientEngine->connected() == false)

			if (this->clientState == 0) {
				this->clientState = CLIENT_CONNECTED_STATE;

				//send client info to host
				HQRemote::FrameEvent clientInfoEvent(this->clientInfo.size(), 0, HQRemote::ENDPOINT_NAME);
				memcpy(clientInfoEvent.event.renderedFrameData.frameData, this->clientInfo.c_str(), this->clientInfo.size());
				this->clientEngine->sendEvent(clientInfoEvent);

				HQRemote::PlainEvent event;

				// enable adaptive frame size compression based on data receiving speed from client
				// it is not enabled by default to allow compatibility with older client
				event.event.type = Remote::REMOTE_ADAPTIVE_DATA_RATE;
				this->clientEngine->sendEvent(event);

				//tell host to send frames in specified interval
				event.event.type = HQRemote::FRAME_INTERVAL;
				event.event.frameInterval = DEFAULT_REMOTE_FRAME_INTERVAL;
				this->clientEngine->sendEvent(event);

				//tell host to reset remote's buttons
				event.event.type = Remote::RESET_REMOTE_INPUT;
				this->clientEngine->sendEvent(event);

				//tell host to send its info
				event.event.type = (HQRemote::HOST_INFO);
				this->clientEngine->sendEvent(event);

				event.event.type = Remote::REMOTE_MODE;
				this->clientEngine->sendEvent(event);

				//notify interface system
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTED);
			}

			//update data rate
			{
				auto time = HQRemote::getTimeCheckPoint64();
				auto elapsed = HQRemote::getElapsedTime64(this->lastRemoteDataRateUpdateTime, time);
				if (this->lastRemoteDataRateUpdateTime == 0 || elapsed >= REMOTE_RCV_RATE_UPDATE_INTERVAL)
				{
					auto rate = this->clientEngine->getReceiveRate();

					// update ui about our data receiving rate
					Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_DATA_RATE, (Result)(intptr_t)(&rate));

					// update host about our data receiving rate
					HQRemote::PlainEvent event;
					event.event.type = Remote::REMOTE_DATA_RATE;
					event.event.floatValue = rate;
					this->clientEngine->sendEvent(event);

					this->lastRemoteDataRateUpdateTime = time;
				}
			}//update data rate

			HandleGenericRemoteEventAsClient();
			HandleRemoteFrameEventAsClient(remoteBurstPhase);
			HandleRemoteAudioEventAsClient(soundOutput);
		}

		void Machine::HandleGenericRemoteEventAsClient() {
			const int numHandledEventsToStartRcvFrames = 4;
			//handle generic event
			auto eventRef = this->clientEngine->getEvent();
			if (eventRef != nullptr)
			{
				auto & event = eventRef->event;

				switch (event.type) {
				case HQRemote::HOST_INFO:
				{
					assert(Core::Video::Screen::WIDTH == event.hostInfo.width);
					assert(Core::Video::Screen::HEIGHT == event.hostInfo.height);

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break;
				case HQRemote::AUDIO_STREAM_INFO:
				{
					//send client's captured audio info
					this->clientEngine->sendCapturedAudioInfo();

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break;
				case HQRemote::ENDPOINT_NAME://name of host
				{
					if (event.renderedFrameData.frameSize == 0)
						this->hostName = "A player";
					else
						this->hostName.assign((const char*)event.renderedFrameData.frameData, event.renderedFrameData.frameSize);
					
					//invoke callback
					Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTED, (Result)(intptr_t)(this->hostName.c_str()));

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break;
				case Remote::REMOTE_MODE:
				{
					Remote::RemoteMode mode;
					memcpy(&mode, event.customData, sizeof mode);

					if ((this->state & mode.mode) == 0)
						SwitchMode();

					if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
						this->clientState++;
				}
					break;
				case HQRemote::MESSAGE:
				case HQRemote::MESSAGE_ACK:
					HandleCommonEvent(event);
					break;
				case Remote::REMOTE_DATA_RATE: {
					if (this->clientState == CLIENT_EXCHANGE_DATA_STATE) {
						auto ourRate = this->clientEngine->getReceiveRate();
						auto hostSendRate = event.floatValue;

						if (ourRate < hostSendRate * 0.8f) {
							this->numRemoteDataRateReportsSlower ++;
						}
						this->numRemoteDataRateReports ++;

						if (this->numRemoteDataRateReports < MIN_REMOTE_DATA_RATE_REPORTS_TO_START_ADAPT_FPS)
							break;

						auto ratio = (double) this->numRemoteDataRateReportsSlower / this->numRemoteDataRateReports;

#if defined _DEBUG || defined DEBUG
						HQRemote::Log("client rcv slow reports: %zu, total reports: %zu. ratio=%.3f\n",
									  this->numRemoteDataRateReportsSlower,
									  this->numRemoteDataRateReports,
									  ratio);
#endif

						auto currentTime = HQRemote::getTimeCheckPoint64();

						// pending event to be sent to server
						HQRemote::PlainEvent event;
						event.event.type = HQRemote::FRAME_INTERVAL;

						if (ratio >= REMOTE_DATA_ADAPT_TO_LOWER_FPS_DECISION_FACTOR) {
							// our receiving rate is too low compare to host sending rate

							// tell host to send frames in slower rate
							if (this->lastRemoteDataRateReducingTime == 0) {
								// inform host
								event.event.frameInterval = SLOW_NET_REMOTE_FRAME_INTERVAL;
								this->clientEngine->sendEvent(event);
								this->clientEngine->setFrameInterval(
										SLOW_NET_REMOTE_FRAME_INTERVAL);

								this->lastRemoteDataRateReducingTime = currentTime;

								HQRemote::Log("force host to use slower sending rate");
							}
						} else if (ratio <= REMOTE_DATA_ADAPT_TO_HIGHER_FPS_DECISION_FACTOR) {
							// try to increase frame rate
							auto elapsedSinceLastReduce = HQRemote::getElapsedTime64(this->lastRemoteDataRateReducingTime, currentTime);

							if (this->lastRemoteDataRateReducingTime != 0
								&& this->numberOfRemoteDataRateIncreaseAttempts < MAX_REMOTE_DATA_RATE_INCREASE_ATTEMPTS
								&& elapsedSinceLastReduce >= REMOTE_DATA_RATE_INCREASE_INTERVAL) {
								// restart counters so that the rate reduction can start early
								this->lastRemoteDataRateReducingTime = 0;
								this->numRemoteDataRateReportsSlower = this->numRemoteDataRateReports = 0;

								this->numberOfRemoteDataRateIncreaseAttempts++;

								event.event.frameInterval = DEFAULT_REMOTE_FRAME_INTERVAL;
								this->clientEngine->sendEvent(event);
								this->clientEngine->setFrameInterval(DEFAULT_REMOTE_FRAME_INTERVAL);

								HQRemote::Log("attempt to tell host to use faster sending rate");
							}

						}
					}
				}
					break;
				default:
					break;
				}
			}//if (eventRef != nullptr)

			if (this->clientState < CLIENT_EXCHANGE_DATA_STATE && this->clientState >= CLIENT_CONNECTED_STATE + numHandledEventsToStartRcvFrames)
			{
				this->clientState = CLIENT_EXCHANGE_DATA_STATE;

				//we have all info needed from host. Now tell it to start sending frames
				HQRemote::PlainEvent eventToHost(HQRemote::START_SEND_FRAME);
				this->clientEngine->sendEvent(eventToHost);
			}
		}

		void Machine::HandleRemoteFrameEventAsClient(uint &remoteBurstPhase) {
			//handle frame event
			auto eventRef = this->clientEngine->getFrameEvent();
			if (eventRef != nullptr) {
				auto & event = eventRef->event;
				assert(event.type == HQRemote::RENDERED_FRAME);

				auto frameId = event.renderedFrameData.frameId;
				if (this->lastReceivedRemoteFrame < frameId)
				{
					uint remoteUsePermaLowres = 0;//host is using low resolution permanently or not?

					auto time = HQRemote::getTimeCheckPoint64();

					auto elapsedSinceLastFrame = this->lastReceivedRemoteFrameTime > 0 ? HQRemote::getElapsedTime64(this->lastReceivedRemoteFrameTime, time) : 0;

					auto intentedInterval = this->clientEngine->getFrameInterval() - 0.0001;
					//if (elapsedSinceLastFrame >= intentedInterval || elapsedSinceLastFrame == 0.0)
					{
						if (this->remoteFrameCompressor->decompress(event.renderedFrameData.frameData, event.renderedFrameData.frameSize, frameId, ppu.GetScreen(), remoteBurstPhase, remoteUsePermaLowres))
						{
							this->lastReceivedRemoteFrame = frameId;
							this->lastReceivedRemoteFrameTime = time;

							//check if host changed its frame resolution
							if (remoteUsePermaLowres != this->remoteFrameCompressor->downSample)
							{
								this->remoteFrameCompressor->downSample = remoteUsePermaLowres;

								Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_LOWRES, remoteUsePermaLowres ? RESULT_OK : RESULT_ERR_GENERIC);
							}
						}//if (event.renderedFrameData.frameSize == sizeof(screen))
					}//if (elapsedSinceLastFrame >= intentedInterval || elapsedSinceLastFrame == 0.0)
				}//if (this->lastRenderedRemoteFrame < event.renderedFrameData.frameId)
			}//if (eventRef != nullptr)
		}

		//TODO: move all audio processing functions to Apu class
		void Machine::HandleRemoteAudio(Sound::Output& soundOutput, HQRemote::BaseEngine* remoteHandler, remote_audio_mix_handler mixer_func, uint* mixedLengths)
		{
			auto numChannels = cpu.GetApu().InStereo() ? 2 : 1;
			auto sampleSize = numChannels * sizeof(uint16_t);
			HQRemote::ConstFrameEventRef eventRef;

			unsigned char* outputBuf[2];
			size_t outputBufSize[2];

			for (int i = 0; i < 2; ++i) {
				outputBuf[i] = (unsigned char*)soundOutput.samples[i];
				outputBufSize[i] = soundOutput.length[i] * sampleSize;
			}

			auto totalOutputSize = outputBufSize[0] + outputBufSize[1];
			while ((totalOutputSize = outputBufSize[0] + outputBufSize[1]) > 0
				&& (this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx].size() >= totalOutputSize || (eventRef = remoteHandler->getAudioEvent()))) {
				unsigned char dummy[sizeof(size_t)];
				unsigned char* packet = dummy;
				size_t packetSize = 0;

				if (eventRef) {
					auto & event = eventRef->event;
					assert(event.type == HQRemote::AUDIO_DECODED_PACKET);

					packet = (unsigned char*)event.renderedFrameData.frameData;
					packetSize = event.renderedFrameData.frameSize;
				}

				auto& buffer = this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx];

				try {
					//TODO: only support 16 bit PCM for now
					if (packetSize)
						buffer.insert(buffer.end(), packet, packet + packetSize);
					auto remainSize = buffer.size();
					//fill first output buffer
					size_t sizeToCopy = __min__(remainSize, outputBufSize[0]);

					(this->*mixer_func)(outputBuf[0], buffer.data(), sizeToCopy);

					remainSize -= sizeToCopy;
					outputBufSize[0] -= sizeToCopy;
					outputBuf[0] += sizeToCopy;

					if (remainSize && outputBuf[1] != NULL)//still has more data for second output buffer
					{
						//fill second output buffer
						auto offset = buffer.size() - remainSize;
						sizeToCopy = __min__(remainSize, outputBufSize[1]);

						(this->*mixer_func)(outputBuf[1], buffer.data() + offset, sizeToCopy);

						remainSize -= sizeToCopy;
						outputBufSize[1] -= sizeToCopy;
						outputBuf[1] += sizeToCopy;
					}

					auto bufferUsedBytes = buffer.size() - remainSize;
					ConsumeRemoteAudioBuffer(bufferUsedBytes);
				}
				catch (...) {
					//TODO
				}

#if 0
				char buf[1024];
				sprintf(buf, "submitted sound packet id=%llu, remain buffer=%llu\n",
					eventRef ? eventRef->event.renderedFrameData.frameId : -1,
					(uint64_t)this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx].size());
				OutputDebugStringA(buf);
#endif
			}//while(totalOutputSize > 0 && (eventRef = this->clientEngine->getAudioEvent()))

			if (mixedLengths != nullptr){
				mixedLengths[0] = soundOutput.length[0];
				mixedLengths[1] = soundOutput.length[1];

				//we haven't completely fill the entire buffers
				if (outputBufSize[0] + outputBufSize[1] > 0) {
					mixedLengths[0] -= outputBufSize[0] / sampleSize;
					mixedLengths[1] -= outputBufSize[1] / sampleSize;
				}
			}
		}

		void Machine::HandleRemoteAudioEventAsClient(Sound::Output* soundOutput) {
			//handle audio event
			bool locked = false;
			if (soundOutput != nullptr && (locked = Sound::Output::lockCallback(*soundOutput)) && (soundOutput->length[0] + soundOutput->length[1]) > 0) {
				uint filledLengths[2];

				//for client, simply copy the remote audio to the output buffer
				HandleRemoteAudio(*soundOutput, this->clientEngine.get(), &Machine::CopyAudio, filledLengths);

				soundOutput->length[0] = filledLengths[0];
				soundOutput->length[1] = filledLengths[1];
			}//if (soundOutput != nullptr)

			if (locked)
				Sound::Output::unlockCallback(*soundOutput);
		}

		void Machine::MixAudioWithClientCapturedAudio(Sound::Output& soundOutput) {
			if (!this->hostEngine)
				return;
			//mix local output sound with client's captured sound (e.g microphone)
			HandleRemoteAudio(soundOutput, this->hostEngine.get(), &Machine::MixAudio, nullptr);
		}

		void Machine::ConsumeRemoteAudioBuffer(size_t usedBytes)
		{
			auto& currentBuffer = this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx];

			//still has more data, store it for later use
			if (usedBytes < currentBuffer.size())
			{
				this->nextRemoteAudioBufferIdx = (this->nextRemoteAudioBufferIdx + 1) % 2;
				auto& nextBuffer = this->remoteAudioBuffers[this->nextRemoteAudioBufferIdx];

				nextBuffer.insert(nextBuffer.end(), currentBuffer.begin() + usedBytes, currentBuffer.end());
			}//if (usedBytes < currentBuffer.size())

			currentBuffer.clear();
		}

		void Machine::ResetRemoteAudioBuffer() {
			this->remoteAudioBuffers[0].reserve(256 * 1024);
			this->remoteAudioBuffers[0].clear();
			this->remoteAudioBuffers[1].reserve(256 * 1024);
			this->remoteAudioBuffers[1].clear();
			this->nextRemoteAudioBufferIdx = 0;
		}

		void Machine::CopyAudio(unsigned char* output, const unsigned char* inputAudioData, size_t size)
		{
			memcpy(output, inputAudioData, size);
		}

		inline void Machine::MixAudioSample(unsigned char* outputPtr, int16_t inputSample)
		{
			const auto inputAudioThreshold = 0x7fff / 2;

			//TODO: we only support 16 bit pcm mono for now
			int16_t outputSample;
			memcpy(&outputSample, outputPtr, sizeof(outputSample));

			//mixing input audio & output audio
#if 1
			if (abs(inputSample) > inputAudioThreshold)
				outputSample = (int16_t)(outputSample * 0.2f + inputSample);
			else
#endif
				outputSample = (int16_t)(outputSample + inputSample);

			//update output buffer
			memcpy(outputPtr, &outputSample, sizeof(outputSample));
		}

		void Machine::MixAudio(unsigned char* output, const unsigned char* inputAudioData, size_t size)
		{
			if (size == 0)
				return;

			auto &apu = cpu.GetApu();

			//TODO: we only support 16 bit pcm mono for now
			auto assert_expr = apu.GetSampleBits() == 16 && !apu.InStereo();
			NST_ASSERT(assert_expr);
			if (!assert_expr)
				return;//ignore in release mode

			auto inputPtr = inputAudioData;

			//mix with input audio
			auto samplesToMix = size / apu.GetSampleBlockAlign();

			size_t offset = 0;
			auto outPutPtr = output;

			for (size_t j = 0; j < samplesToMix; ++j, outPutPtr += apu.GetSampleBlockAlign(), inputPtr += apu.GetSampleBlockAlign()) {
				int16_t inputSample;
				memcpy(&inputSample, inputPtr, sizeof(inputSample));

				//mixing input audio & output audio
				MixAudioSample(outPutPtr, inputSample);
			}//for (j ...)
		}

		void Machine::HandleRemoteEventsAsServer() {
			//check if connection handler has some internal errors
			auto errorMsg = this->hostEngine->getConnectionInternalError();
			if (errorMsg)
			{
				DisableRemoteControllers();
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_CONNECTION_INTERNAL_ERROR, (Result)(intptr_t)(errorMsg->c_str()));
				return;
			}

			//check if client is connected
			if (this->clientState == 0)
			{
				if (this->hostEngine->connected())
				{
					this->clientState = CLIENT_CONNECTED_STATE;

					// reset timer
					this->lastRemoteDataRateUpdateTime = 0;

					// enable 50KB budget for 20 fps by default to be compatible with older client
					this->remoteFrameCompressor->enableDataRateBudget(MIN_REMOTE_SEND_RATE_BUDGET, SLOW_NET_REMOTE_FRAME_INTERVAL);
				}
				else
					return;
			}//if (this->clientState == 0)

			//client disconnected
			if (this->hostEngine->connected() == false)
			{
				this->clientState = 0;
				if (clientInfo.size() != 0)
					Api::Machine::eventCallback(Api::Machine::EVENT_CLIENT_DISCONNECTED, (Result)(intptr_t)(this->clientInfo.c_str()));
				else
					Api::Machine::eventCallback(Api::Machine::EVENT_CLIENT_DISCONNECTED, (Result)(intptr_t)(NULL));

				return;
			}

			// update data sending rate
			auto time = HQRemote::getTimeCheckPoint64();
			auto elapsed = HQRemote::getElapsedTime64(this->lastRemoteDataRateUpdateTime, time);
			if (this->lastRemoteDataRateUpdateTime == 0 || elapsed >= REMOTE_SND_RATE_UPDATE_INTERVAL)
			{
				auto rate = this->hostEngine->getSendRate();

				// update UI about our data rate
				Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_DATA_RATE, (Result)(intptr_t)(&rate));

				// update client about our data sending rate
				HQRemote::PlainEvent event;
				event.event.type = Remote::REMOTE_DATA_RATE;
				event.event.floatValue = rate;
				this->hostEngine->sendEvent(event);

				this->lastRemoteDataRateUpdateTime = time;
			}

			HandleGenericRemoteEventAsServer();
		}

		void Machine::HandleGenericRemoteEventAsServer() {
			int numHandledEventsBeforeAcceptAudio = 3;
			//retrieve event
			auto eventRef = this->hostEngine->getEvent();
			if (eventRef == nullptr)
				return;

			auto & event = eventRef->event;

			switch (event.type) {
			case HQRemote::ENDPOINT_NAME://name of client
			{
				if (event.renderedFrameData.frameSize == 0)
					this->clientInfo = "A player";
				else
					this->clientInfo.assign((const char*)event.renderedFrameData.frameData, event.renderedFrameData.frameSize);

				//send host's name to client
				HQRemote::FrameEvent hostNameEvent(this->hostName.size(), 0, HQRemote::ENDPOINT_NAME);
				memcpy(hostNameEvent.event.renderedFrameData.frameData, this->hostName.c_str(), this->hostName.size());
				this->hostEngine->sendEvent(hostNameEvent);
				
				//invoke callback
				Api::Machine::eventCallback(Api::Machine::EVENT_CLIENT_CONNECTED, (Result)(intptr_t)(this->clientInfo.c_str()));

				if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
					this->clientState++;
			}
				break;
			case Remote::REMOTE_MODE:
			{
				SendModeToClient();

				if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
					this->clientState++;
			}
				break;
			case HQRemote::AUDIO_STREAM_INFO:
			{
				if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
					this->clientState++;
			}
				break;
			case Remote::REMOTE_DOWNSAMPLE_FRAME:
			{
				//enable/disable downsampling frame before sending to client
				uint32_t downSample;
				memcpy(&downSample, event.customData, sizeof downSample);

				if (downSample != this->remoteFrameCompressor->downSample)
				{
					this->remoteFrameCompressor->downSample = downSample;

					Api::Machine::eventCallback(Api::Machine::EVENT_REMOTE_LOWRES, downSample ? RESULT_OK : RESULT_ERR_GENERIC);
				}
			}
				break;
			case HQRemote::MESSAGE:
			case HQRemote::MESSAGE_ACK:
				HandleCommonEvent(event);
				break;
			case HQRemote::FRAME_INTERVAL: {
				this->remoteFrameCompressor->updateFrameInterval(event.frameInterval);
			}
				break;
			case Remote::REMOTE_DATA_RATE: {
				if (this->clientState < CLIENT_EXCHANGE_DATA_STATE)
					break;

				auto currentTime = HQRemote::getTimeCheckPoint64();
				auto clientRcvRate = event.floatValue;
				auto ourSendRate = this->hostEngine->getSendRate();

				if (clientRcvRate < ourSendRate * 0.8f) {
					this->numRemoteDataRateReportsSlower++;
				}

				this->numRemoteDataRateReports++;
				if (this->numRemoteDataRateReports < MIN_REMOTE_DATA_RATE_REPORTS_TO_START_ADAPT_RES)
					break;

				auto ratio = (double) this->numRemoteDataRateReportsSlower / this->numRemoteDataRateReports;

#if defined _DEBUG || defined DEBUG
				HQRemote::Log("client rcv slow reports: %u, total reports: %u. ratio=%.3f\n",
							  (uint)this->numRemoteDataRateReportsSlower,
							  (uint)this->numRemoteDataRateReports,
							  ratio);
#endif

				if (ratio >= REMOTE_DATA_ADAPT_TO_LOWER_RES_DECISION_FACTOR) {
					// try to reduce frame compression size budget since client's data receiving rate is too low
					this->remoteFrameCompressor->enableDataRateBudget((size_t)clientRcvRate, this->hostEngine->getFrameInterval());

					this->lastRemoteDataRateReducingTime = currentTime;
				} else if (ratio <= REMOTE_DATA_ADAPT_TO_HIGHER_RES_DECISION_FACTOR)
				{
					// attempt to increase frame compression size budget
					auto elapsedSinceLastReduce = HQRemote::getElapsedTime64(this->lastRemoteDataRateReducingTime, currentTime);
					if (this->lastRemoteDataRateReducingTime != 0
						&& this->numberOfRemoteDataRateIncreaseAttempts < MAX_REMOTE_DATA_RATE_INCREASE_ATTEMPTS
						&& elapsedSinceLastReduce >= REMOTE_DATA_RATE_INCREASE_INTERVAL) {
						// restart counters so that the rate reduction can start early
						this->lastRemoteDataRateReducingTime = 0;
						this->numRemoteDataRateReportsSlower = this->numRemoteDataRateReports = 0;

						this->numberOfRemoteDataRateIncreaseAttempts++;

						this->remoteFrameCompressor->enableDataRateBudget(0, this->hostEngine->getFrameInterval());
					}
				}
			}
				break;
			case Remote::REMOTE_ADAPTIVE_DATA_RATE:
				// reset timer
				this->lastRemoteDataRateReducingTime = 0;
				this->numberOfRemoteDataRateIncreaseAttempts = 0;
				this->numRemoteDataRateReportsSlower = 0;
				this->numRemoteDataRateReports = 0;

				// reset budget
				this->remoteFrameCompressor->enableDataRateBudget(0, DEFAULT_REMOTE_FRAME_INTERVAL);

				break;
			default:
				cpu.OnRemoteEvent(event);
				break;
			}

			//we have all required info from client, tell it to start sending captured voice now
			if (this->clientState < CLIENT_EXCHANGE_DATA_STATE && this->clientState == CLIENT_CONNECTED_STATE + numHandledEventsBeforeAcceptAudio)
			{
				this->clientState = CLIENT_EXCHANGE_DATA_STATE;

				HQRemote::PlainEvent eventToClient(HQRemote::START_SEND_FRAME);
				this->hostEngine->sendEvent(eventToClient);
			}
		}

		std::shared_ptr<HQRemote::IData> Machine::CaptureAudioAsClient()
		{
			if (this->currentInputAudio == NULL)
				return nullptr;

			//only capture audio from input device (e.g. mic)
			auto& apu = cpu.GetApu();

			auto availInputAudioSamples = Sound::Input::readCallback(*this->currentInputAudio, NULL, 0);
			if (availInputAudioSamples == 0)
				return nullptr;

			try {
				auto totalSize = availInputAudioSamples * apu.GetSampleBlockAlign();
				auto pcmData = std::make_shared<HQRemote::CData>(totalSize);

				auto ptr = pcmData->data();
				Sound::Input::readCallback(*this->currentInputAudio, ptr, availInputAudioSamples);

				return pcmData;
			}
			catch (...)
			{
				return nullptr;
			}
		}

		std::shared_ptr<HQRemote::IData> Machine::CaptureAudioAsServer()
		{
			auto& apu = cpu.GetApu();

			//TODO: we only support 16 bit pcm mono for now
			if (apu.GetSampleBits() != 16 || apu.InStereo())
				return nullptr;

			auto& audioOutput = apu.GetFrameSnapshot();

			auto totalSize = audioOutput.samples.size();
			auto totalSamples = totalSize / audioOutput.blockAlign;
			if (totalSize == 0)
				return nullptr;

			int16_t inputAudioBuffer[1024];
			auto inputAudioBufferMaxSamples = sizeof(inputAudioBuffer) / audioOutput.blockAlign;

			try {
				auto pcmData = std::make_shared<HQRemote::CData>(totalSize);

				auto ptr = pcmData->data();

				if (totalSize)
				{
					//copy captured snapshot data
					memcpy(ptr, audioOutput.samples.data(), totalSize);

#if 1
					//mix with input audio
					auto remainSamples = totalSamples;
					size_t inputSamples = 0;
					int16_t lastSample = 0;

					while (this->currentInputAudio != NULL && (inputSamples = Sound::Input::readCallback(*this->currentInputAudio, inputAudioBuffer, __min__(remainSamples, inputAudioBufferMaxSamples))))
					{
						auto inputSize = inputSamples * audioOutput.blockAlign;

						MixAudio(ptr, (unsigned char*)inputAudioBuffer, inputSize);

						remainSamples -= inputSamples;
						ptr += inputSize;
						lastSample = inputAudioBuffer[inputSamples - 1];
					}//while ((inputSize = Sound::Input::readCallback(...)))

					//fill the rest with last captured sample
					for (size_t i = 0; i < remainSamples; ++i, ptr += audioOutput.blockAlign) {
						MixAudioSample(ptr, lastSample);
					}
#endif
				}//if (totalSize)

				return pcmData;
			}
			catch (...)
			{
				return nullptr;
			}
		}

		void Machine::SendModeToClient() {
			auto modeEvent = std::make_shared<HQRemote::PlainEvent >(Remote::REMOTE_MODE);
			Remote::RemoteMode mode;
			//send the current mode to client
			if (state & Api::Machine::NTSC)
				mode.mode = Api::Machine::NTSC;
			else
				mode.mode = Api::Machine::PAL;

			memcpy(modeEvent->event.customData, &mode, sizeof mode);
			this->hostEngine->sendEvent(modeEvent);
		}

		void Machine::EnsureCorrectRemoteSoundSettings() {
			auto& apu = cpu.GetApu();
			//TODO: only 16 bit PCM is supported for remote controlling for now
			if (apu.GetSampleBits() != 16 ||
				apu.GetSampleRate() != REMOTE_AUDIO_SAMPLE_RATE ||
				apu.InStereo() != REMOTE_AUDIO_STEREO_CHANNELS)
			{
				apu.SetSampleBits(16);
				apu.EnableStereo(REMOTE_AUDIO_STEREO_CHANNELS);
				apu.SetSampleRate(REMOTE_AUDIO_SAMPLE_RATE);

				//TODO: these settings may fail
				Sound::Output::updateSettingsCallback();
				Sound::Input::updateSettingsCallback();
			}
		}

		//implement HQRemote::IFrameCapturer
		size_t Machine::GetFrameSize() {
			return sizeof(Core::Video::Screen) + sizeof(uint) + sizeof(Ppu::ColorUseCountTable);
		}
		unsigned int Machine::GetNumColorChannels() {
			return 1;
		}
		void Machine::CaptureFrame(unsigned char * prevFrameDataToCopy) {
			if (prevFrameDataToCopy == NULL)
				return;
			auto& screen = ppu.GetScreen();
			auto burstPhase = ppu.GetBurstPhase();
			auto& colorUseCountTable = ppu.GetColorUseCountTable();

			//layout: burst phase | screen | colors' usage count table
			memcpy(prevFrameDataToCopy, &burstPhase, sizeof(burstPhase));
			memcpy(prevFrameDataToCopy + sizeof burstPhase, &screen, sizeof(screen));
			auto& copiedTable = *(Ppu::ColorUseCountTable*)(prevFrameDataToCopy + sizeof burstPhase + sizeof screen);
			memcpy(&copiedTable, colorUseCountTable, sizeof(Ppu::ColorUseCountTable));

			//sort colors' usage count table 
			std::sort(&copiedTable[0], &copiedTable[Video::Screen::PALETTE], [](const Ppu::ColorUseCount& a, const Ppu::ColorUseCount& b) { return a.count > b.count; });

			ppu.MarkColorUseCountTableToReset();
		}
		//end IFrameCapturer implementation

		//implement HQRemote::IAudioCapturer
		uint32_t Machine::GetAudioSampleRate() const {
			return cpu.GetApu().GetSampleRate();
		}
		uint32_t Machine::GetNumAudioChannels() const {
			return cpu.GetApu().InStereo() ? 2 : 1;
		}

		//end HQRemote::IAudioCapturer implementation

		Result Machine::Unload()
		{
			const Result result = PowerOff();

			tracker.Unload();

			if (image)
				Image::Unload( image );
			image = NULL;

			state &= (Api::Machine::NTSC|Api::Machine::PAL);

			Api::Machine::eventCallback( Api::Machine::EVENT_UNLOAD, result );

			return result;
		}

		void Machine::UpdateModels()
		{
			const Region region = (state & Api::Machine::NTSC) ? REGION_NTSC : REGION_PAL;

			CpuModel cpuModel;
			PpuModel ppuModel;

			if (image)
			{
				image->GetDesiredSystem( region, &cpuModel, &ppuModel );
			}
			else
			{
				cpuModel = (region == REGION_NTSC ? CPU_RP2A03 : CPU_RP2A07);
				ppuModel = (region == REGION_NTSC ? PPU_RP2C02 : PPU_RP2C07);
			}

			if ((state & Api::Machine::REMOTE) == 0)//LHQ: if are remotely control another machine, no need to update the current cpu
				cpu.SetModel( cpuModel );
			UpdateVideo( ppuModel, GetColorMode() );

			renderer.EnableForcedFieldMerging( ppuModel != PPU_RP2C02 );

			//tell client about our updated mode
			if (hostEngine && this->clientState == CLIENT_EXCHANGE_DATA_STATE)//TODO: race condition
				SendModeToClient();
		}

		Machine::ColorMode Machine::GetColorMode() const
		{
			return
			(
				renderer.GetPaletteType() == Video::Renderer::PALETTE_YUV    ? COLORMODE_YUV :
				renderer.GetPaletteType() == Video::Renderer::PALETTE_CUSTOM ? COLORMODE_CUSTOM :
																			   COLORMODE_RGB
			);
		}

		Result Machine::UpdateColorMode()
		{
			return UpdateColorMode( GetColorMode() );
		}

		Result Machine::UpdateColorMode(const ColorMode mode)
		{
			return UpdateVideo( ppu.GetModel(), mode );
		}

		Result Machine::UpdateVideo(const PpuModel ppuModel,const ColorMode mode)
		{
			ppu.SetModel( ppuModel, mode == COLORMODE_YUV );

			Video::Renderer::PaletteType palette;

			switch (mode)
			{
				case COLORMODE_RGB:

					switch (ppuModel)
					{
						case PPU_RP2C04_0001: palette = Video::Renderer::PALETTE_VS1;  break;
						case PPU_RP2C04_0002: palette = Video::Renderer::PALETTE_VS2;  break;
						case PPU_RP2C04_0003: palette = Video::Renderer::PALETTE_VS3;  break;
						case PPU_RP2C04_0004: palette = Video::Renderer::PALETTE_VS4;  break;
						default:              palette = Video::Renderer::PALETTE_PC10; break;
					}
					break;

				case COLORMODE_CUSTOM:

					palette = Video::Renderer::PALETTE_CUSTOM;
					break;

				default:

					palette = Video::Renderer::PALETTE_YUV;
					break;
			}

			return renderer.SetPaletteType( palette );
		}

		Result Machine::PowerOff(Result result)
		{
			if (state & Api::Machine::ON)
			{
				tracker.PowerOff();

				StopRemoteControl();

				if (image && !image->PowerOff() && NES_SUCCEEDED(result))
					result = RESULT_WARN_SAVEDATA_LOST;

				ppu.PowerOff();
				cpu.PowerOff();

				state &= ~uint(Api::Machine::ON);
				frame = 0;

				Api::Machine::eventCallback( Api::Machine::EVENT_POWER_OFF, result );
			}

			return result;
		}

		void Machine::Reset(bool hard)
		{
			if (state & Api::Machine::SOUND)
				hard = true;

			try
			{
				frame = 0;
				cpu.Reset( hard );

				if (!(state & Api::Machine::SOUND))
				{
					InitializeInputDevices();

					cpu.Map( 0x4016 ).Set( this, &Machine::Peek_4016, &Machine::Poke_4016 );
					cpu.Map( 0x4017 ).Set( this, &Machine::Peek_4017, &Machine::Poke_4017 );

					extPort->Reset();
					expPort->Reset();

					bool acknowledged = true;

					if (image)
					{
						switch (image->GetDesiredSystem((state & Api::Machine::NTSC) ? REGION_NTSC : REGION_PAL))
						{
							case SYSTEM_FAMICOM:
							case SYSTEM_DENDY:

								acknowledged = false;
								break;
						}
					}

					ppu.Reset( hard, acknowledged );

					if (image)
						image->Reset( hard );

					if (cheats)
						cheats->Reset();

					tracker.Reset();
				}
				else
				{
					image->Reset( true );
				}

				cpu.Boot( hard );

				if (state & Api::Machine::ON)
				{
					Api::Machine::eventCallback( hard ? Api::Machine::EVENT_RESET_HARD : Api::Machine::EVENT_RESET_SOFT );
				}
				else
				{
					state |= Api::Machine::ON;
					Api::Machine::eventCallback( Api::Machine::EVENT_POWER_ON );
				}
			}
			catch (...)
			{
				PowerOff();
				throw;
			}
		}

		void Machine::SwitchMode()
		{
			NST_ASSERT( !(state & Api::Machine::ON) || (state & Api::Machine::REMOTE) != 0 );//LHQ

			if (state & Api::Machine::NTSC)
				state = (state & ~uint(Api::Machine::NTSC)) | Api::Machine::PAL;
			else
				state = (state & ~uint(Api::Machine::PAL)) | Api::Machine::NTSC;

			UpdateModels();

			Api::Machine::eventCallback( (state & Api::Machine::NTSC) ? Api::Machine::EVENT_MODE_NTSC : Api::Machine::EVENT_MODE_PAL );
		}

		void Machine::InitializeInputDevices() const
		{
			if (state & Api::Machine::GAME)
			{
				const bool arcade = state & Api::Machine::VS;

				extPort->Initialize( arcade );
				expPort->Initialize( arcade );
			}
		}

		void Machine::SaveState(State::Saver& saver) const
		{
			if (!image)
				return;
			NST_ASSERT( (state & (Api::Machine::GAME|Api::Machine::ON)) > Api::Machine::ON );

			saver.Begin( AsciiId<'N','S','T'>::V | 0x1AUL << 24 );

			saver.Begin( AsciiId<'N','F','O'>::V ).Write32( image->GetPrgCrc() ).Write32( frame ).End();

			cpu.SaveState( saver, AsciiId<'C','P','U'>::V, AsciiId<'A','P','U'>::V );
			ppu.SaveState( saver, AsciiId<'P','P','U'>::V );
			image->SaveState( saver, AsciiId<'I','M','G'>::V );

			saver.Begin( AsciiId<'P','R','T'>::V );

			if (extPort->NumPorts() == 4)
			{
				static_cast<const Input::AdapterFour*>(extPort)->SaveState
				(
					saver, AsciiId<'4','S','C'>::V
				);
			}

			for (uint i=0; i < extPort->NumPorts(); ++i)
				extPort->GetDevice( i ).SaveState( saver, Ascii<'0'>::V + i );

			expPort->SaveState( saver, Ascii<'X'>::V );

			saver.End();

			saver.End();
		}

		bool Machine::LoadState(State::Loader& loader,const bool resetOnError)
		{
			NST_ASSERT( (state & (Api::Machine::GAME|Api::Machine::ON)) > Api::Machine::ON );

			try
			{
				if (loader.Begin() != (AsciiId<'N','S','T'>::V | 0x1AUL << 24))
					throw RESULT_ERR_INVALID_FILE;

				while (const dword chunk = loader.Begin())
				{
					switch (chunk)
					{
						case AsciiId<'N','F','O'>::V:
						{
							const dword crc = loader.Read32();

							if
							(
								loader.CheckCrc() && !(state & Api::Machine::DISK) &&
								crc && crc != image->GetPrgCrc() &&
								Api::User::questionCallback( Api::User::QUESTION_NST_PRG_CRC_FAIL_CONTINUE ) == Api::User::ANSWER_NO
							)
							{
								for (uint i=0; i < 2; ++i)
									loader.End();

								return false;
							}

							frame = loader.Read32();
							break;
						}

						case AsciiId<'C','P','U'>::V:
						case AsciiId<'A','P','U'>::V:

							cpu.LoadState( loader, AsciiId<'C','P','U'>::V, AsciiId<'A','P','U'>::V, chunk );
							break;

						case AsciiId<'P','P','U'>::V:

							ppu.LoadState( loader );
							break;

						case AsciiId<'I','M','G'>::V:

							image->LoadState( loader );
							break;

						case AsciiId<'P','R','T'>::V:

							extPort->Reset();
							expPort->Reset();

							while (const dword subId = loader.Begin())
							{
								if (subId == AsciiId<'4','S','C'>::V)
								{
									if (extPort->NumPorts() == 4)
										static_cast<Input::AdapterFour*>(extPort)->LoadState( loader );
								}
								else switch (const uint index = (subId >> 16 & 0xFF))
								{
									case Ascii<'2'>::V:
									case Ascii<'3'>::V:

										if (extPort->NumPorts() != 4)
											break;

									case Ascii<'0'>::V:
									case Ascii<'1'>::V:

										extPort->GetDevice( index - Ascii<'0'>::V ).LoadState( loader, subId & 0xFF00FFFF );
										break;

									case Ascii<'X'>::V:

										expPort->LoadState( loader, subId & 0xFF00FFFF );
										break;
								}

								loader.End();
							}
							break;
					}

					loader.End();
				}

				loader.End();
			}
			catch (...)
			{
				if (resetOnError)
					Reset( true );

				throw;
			}

			return true;
		}

		#ifdef NST_MSVC_OPTIMIZE
		#pragma optimize("", on)
		#endif

		void Machine::Execute
		(
			Video::Output* const video,
			Sound::Output* const sound,
			Input::Controllers* const input,
			Sound::Input* const inputSound
		)
		{
			NST_ASSERT( state & Api::Machine::ON );

#if PROFILE_EXECUTION_TIME
			HQRemote::ScopedTimeProfiler profiler("machine execution", avgExecuteTimeLock, avgExecuteTime, executeWindowTime);
#endif

			this->currentInputAudio = inputSound;//LHQ

			if ((state & Api::Machine::REMOTE) != 0 && this->clientEngine)
			{
				if (input && this->clientState == CLIENT_EXCHANGE_DATA_STATE)
				{
					//send input to remote host
					Input::Controllers::Pad& pad = input->pad[0];
					
					auto curTime = HQRemote::getTimeCheckPoint64();
					bool routineSend = false;
					if (this->lastSentInputTime == 0 || HQRemote::getElapsedTime64(this->lastSentInputTime, curTime) >= REMOTE_INPUT_SEND_INTERVAL)
					{
						routineSend = true;
						this->lastSentInputTime = curTime;
					}

					if (Input::Controllers::Pad::callback(pad, 0)
						&& (this->lastSentInput != pad.buttons || routineSend))
					{
						Remote::RemoteInput remoteInput;
						remoteInput.id = ++this->lastSentInputId;
						remoteInput.buttons = pad.buttons;

						HQRemote::PlainEvent inputEvent(Remote::REMOTE_INPUT);
						memcpy(inputEvent.event.customData, &remoteInput, sizeof(remoteInput));
						this->clientEngine->sendEventUnreliable(inputEvent);

#if defined DEBUG || defined _DEBUG
						std::stringstream ss;
						ss << "Sent input (" << remoteInput.id << "): " << remoteInput.buttons << std::endl;
#	ifdef WIN32
						OutputDebugStringA(ss.str().c_str());
#	endif
#endif
						this->lastSentInput = pad.buttons;
					}
				}//if (input)

				uint remoteBurstPhase = 0;

				//handle remote event
				EnsureCorrectRemoteSoundSettings();
				HandleRemoteEventsAsClient(remoteBurstPhase, sound);

				//capture input sound (e.g. mic) and send to host
				if (this->clientEngine)//need to check here as the pointer may be invalidated by HandleRemoteEventsAsClient()
					this->clientEngine->captureAndSendAudio();

				//render remote frame
				if (video)
				{
					renderer.Blit(*video, ppu.GetScreen(), remoteBurstPhase);
				}
			}
			else if (!(state & Api::Machine::SOUND))
			{
				if (state & Api::Machine::CARTRIDGE)
					static_cast<Cartridge*>(image)->BeginFrame( Api::Input(*this), input );

				extPort->BeginFrame( input );
				expPort->BeginFrame( input );

				ppu.BeginFrame( tracker.IsFrameLocked() );

				if (cheats)
					cheats->BeginFrame( tracker.IsFrameLocked() );

				//handle remote event
				if (this->hostEngine != nullptr)
				{
					HandleRemoteEventsAsServer();
				}

				//CPU
				{
#if PROFILE_EXECUTION_TIME
					HQRemote::ScopedTimeProfiler profiler("machine's cpu execution", avgCpuExecuteTime, cpuExecuteWindowTime);
#endif
					cpu.ExecuteFrame(sound);
				}

				//PPU
				{
#if PROFILE_EXECUTION_TIME
					HQRemote::ScopedTimeProfiler profiler("machine's ppu endframe", avgPpuEndFrameTime, ppuEndFrameWindowTime);
#endif
					ppu.EndFrame();
				}

				//render
				renderer.bgColor = ppu.output.bgColor;

				if (video)
				{
#if PROFILE_EXECUTION_TIME
					HQRemote::ScopedTimeProfiler profiler("machine's video blitting", avgVideoBlitTime, videoBlitWindowTime);
#endif
					renderer.Blit(*video, ppu.GetScreen(), ppu.GetBurstPhase());
				}

				cpu.EndFrame();

				//LHQ
				if (this->hostEngine)
				{
					//capture frame & audio
					EnsureCorrectRemoteSoundSettings();

					//capture video frame
					{
#if PROFILE_EXECUTION_TIME
						HQRemote::ScopedTimeProfiler profiler("captureAndSendFrame", avgFrameCaptureTime, frameCaptureWindowTime);
#endif
						this->hostEngine->captureAndSendFrame();
					}

					//capture audio
					{
#if PROFILE_EXECUTION_TIME
						HQRemote::ScopedTimeProfiler profiler("captureAndSendAudio", avgAudioCaptureTime, audioCaptureWindowTime);
#endif
						this->hostEngine->captureAndSendAudio();
					}
				}
				//end LHQ

				if (image)
					image->VSync();

				extPort->EndFrame();
				expPort->EndFrame();

				frame++;
			}
			else
			{
				static_cast<Nsf*>(image)->BeginFrame();

				cpu.ExecuteFrame( sound );
				cpu.EndFrame();

				image->VSync();
			}

			this->currentInputAudio = nullptr;//LHQ
		}

		NES_POKE_D(Machine,4016)
		{
			extPort->Poke( data );
			expPort->Poke( data );
		}

		NES_PEEK_A(Machine,4016)
		{
			cpu.Update( address );
			return OPEN_BUS | extPort->Peek(0) | expPort->Peek(0);
		}

		NES_POKE_D(Machine,4017)
		{
			cpu.GetApu().WriteFrameCtrl( data );
		}

		NES_PEEK_A(Machine,4017)
		{
			cpu.Update( address );
			return OPEN_BUS | extPort->Peek(1) | expPort->Peek(1);
		}
	}
}
