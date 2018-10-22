/*
 * Nestopia UE
 * 
 * Copyright (C) 2012-2016 R. Danbrook
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#	include <OpenAL/al.h>
#	include <OpenAL/alc.h>
#else
#	include <AL/al.h>
#	include <AL/alc.h>
#endif

#include "config.h"
#include "audio.h"

#if !defined __APPLE__ && !defined _MINGW
#	define HAVE_AO
#endif

#ifdef HAVE_AO
#include <ao/ao.h>

ao_device *aodevice;
ao_sample_format format;
#endif

extern settings_t conf;
extern Emulator emulator;
extern bool nst_pal;

SDL_AudioSpec spec, obtained;
SDL_AudioDeviceID dev;

static ALCdevice* g_captureDevice;

int16_t audiobuf[96000];

int framerate, channels, bufsize;

bool altspeed = false;
bool paused = false;

#ifndef min
#	define min(a, b) ((a) > (b) ? (b) : (a))
#endif


void audio_lock(Sound::Output *soundoutput) {
	soundoutput->samples[0] = audiobuf;
	soundoutput->length[0] = conf.audio_sample_rate / framerate;
	soundoutput->samples[1] = NULL;
	soundoutput->length[1] = 0;
}

void audio_unlock(Sound::Output *soundoutput) {
	
	if (paused) { return; }
	
	bufsize = 2 * channels * soundoutput->length[0];
	
	if (conf.audio_api == 0) { // SDL
		#if SDL_VERSION_ATLEAST(2,0,4)
		SDL_QueueAudio(dev, (const void*)audiobuf, bufsize);
		// Clear the audio queue arbitrarily to avoid it backing up too far
		if (SDL_GetQueuedAudioSize(dev) > (Uint32)(bufsize * 3)) { SDL_ClearQueuedAudio(dev); }
		#endif
	}
#ifdef HAVE_AO
	else if (conf.audio_api == 1) { // libao
		ao_play(aodevice, (char*)audiobuf, bufsize);
	}
#endif
}

uint audio_read_input(Sound::Input &input, void* samples, uint maxSamples)
{
	if (!g_captureDevice)
		return 0;
	
	ALCint availSamples = 0;
	alcGetIntegerv(g_captureDevice, ALC_CAPTURE_SAMPLES, 1, &availSamples);
	if (samples == NULL)//return number of available samples without reading data
		return availSamples;
	
	uint numSamples = min(availSamples, maxSamples);
	
	if (numSamples)
	{
		alcCaptureSamples(g_captureDevice, samples, numSamples);
	}
	
	return numSamples;
}

void audio_init() {
	// Initialize audio device
	
	// Set the framerate based on the region. For PAL: (60 / 6) * 5 = 50
	framerate = nst_pal ? (conf.timing_speed / 6) * 5 : conf.timing_speed;
	
	channels = conf.audio_stereo ? 2 : 1;
	
	memset(audiobuf, 0, sizeof(audiobuf));
	
	#ifdef _MINGW
	conf.audio_api = 0; // Set SDL audio for MinGW
	#endif
	
	#if SDL_VERSION_ATLEAST(2,0,4)
	#else // Force libao if SDL lib is not modern enough
	if (conf.audio_api == 0) {
		conf.audio_api = 1;
		fprintf(stderr, "Audio: Forcing libao\n");
	}
	#endif
	
	if (conf.audio_api == 0) { // SDL
		spec.freq = conf.audio_sample_rate;
		spec.format = AUDIO_S16SYS;
		spec.channels = channels;
		spec.silence = 0;
		spec.samples = 512;
		spec.userdata = 0;
		spec.callback = NULL; // Use SDL_QueueAudio instead
		
		dev = SDL_OpenAudioDevice(NULL, 0, &spec, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
		if (!dev) {
			fprintf(stderr, "Error opening audio device.\n");
		}
		else {
			fprintf(stderr, "Audio: SDL - %dHz %d-bit, %d channel(s)\n", spec.freq, 16, spec.channels);
		}
		
		SDL_PauseAudioDevice(dev, 1);  // Setting to 0 unpauses
	}
#ifdef HAVE_AO
	else if (conf.audio_api == 1) { // libao
		ao_initialize();
		
		int default_driver = ao_default_driver_id();
		
		memset(&format, 0, sizeof(format));
		format.bits = 16;
		format.channels = channels;
		format.rate = conf.audio_sample_rate;
		format.byte_format = AO_FMT_NATIVE;
		
		aodevice = ao_open_live(default_driver, &format, NULL);
		if (aodevice == NULL) {
			fprintf(stderr, "Error opening audio device.\n");
			aodevice = ao_open_live(ao_driver_id("null"), &format, NULL);
		}
		else {
			fprintf(stderr, "Audio: libao - %dHz, %d-bit, %d channel(s)\n", format.rate, format.bits, format.channels);
		}
	}
#endif
	
	//create capture device
	if (conf.mic_enable)
	{
		ALCenum alFormat;
		switch (channels)
		{
			case 1:
				alFormat = AL_FORMAT_MONO16;
				break;
			default:
				alFormat = AL_FORMAT_STEREO16;
				break;
		}
		
		auto frameRate = 60;
		auto frameLength = 1000 / frameRate;//ms
		auto samplesPerFrame = conf.audio_sample_rate / frameRate;
		auto captureBufferSize = 100 / frameLength * samplesPerFrame;
		
		g_captureDevice = alcCaptureOpenDevice(NULL, conf.audio_sample_rate, alFormat, captureBufferSize);
		if (!g_captureDevice)
		{
			fprintf(stderr, "Error alcCaptureOpenDevice().\n");
		}
		else
			alcCaptureStart(g_captureDevice);
	}//if (conf.mic_enable)
	paused = false;
}

void audio_deinit() {
	// Deinitialize audio
	
	if (conf.audio_api == 0) { // SDL
		SDL_CloseAudioDevice(dev);
	}
#ifdef HAVE_AO
	else if (conf.audio_api == 1) { // libao
		ao_close(aodevice);
		ao_shutdown();
	}
#endif
	
	if (g_captureDevice)
	{
		alcCaptureStop(g_captureDevice);
		alcCaptureCloseDevice(g_captureDevice);
		g_captureDevice = NULL;
	}
}

void audio_pause() {
	// Pause the SDL audio device
	if (conf.audio_api == 0) { // SDL
		SDL_PauseAudioDevice(dev, 1);
	}
	
	//pause openAL capture device
	if (g_captureDevice)
		alcCaptureStop(g_captureDevice);
	paused = true;
}

void audio_unpause() {
	// Unpause the SDL audio device
	if (conf.audio_api == 0) { // SDL
		SDL_PauseAudioDevice(dev, 0);
	}
	//resume openAL capture device
	if (g_captureDevice)
		alcCaptureStart(g_captureDevice);
	
	paused = false;
}

void audio_set_params() {
	// Set audio parameters
	Sound sound(emulator);
	
	sound.SetSampleBits(16);
	sound.SetSampleRate(conf.audio_sample_rate);
	
	sound.SetSpeaker(conf.audio_stereo ? Sound::SPEAKER_STEREO : Sound::SPEAKER_MONO);
	sound.SetSpeed(Sound::DEFAULT_SPEED);
	
	audio_adj_volume();
}

void audio_adj_volume() {
	// Adjust the audio volume to the current settings
	Sound sound(emulator);
	sound.SetVolume(Sound::ALL_CHANNELS, conf.audio_volume);
	sound.SetVolume(Sound::CHANNEL_SQUARE1, conf.audio_vol_sq1);
	sound.SetVolume(Sound::CHANNEL_SQUARE2, conf.audio_vol_sq2);
	sound.SetVolume(Sound::CHANNEL_TRIANGLE, conf.audio_vol_tri);
	sound.SetVolume(Sound::CHANNEL_NOISE, conf.audio_vol_noise);
	sound.SetVolume(Sound::CHANNEL_DPCM, conf.audio_vol_dpcm);
	sound.SetVolume(Sound::CHANNEL_FDS, conf.audio_vol_fds);
	sound.SetVolume(Sound::CHANNEL_MMC5, conf.audio_vol_mmc5);
	sound.SetVolume(Sound::CHANNEL_VRC6, conf.audio_vol_vrc6);
	sound.SetVolume(Sound::CHANNEL_VRC7, conf.audio_vol_vrc7);
	sound.SetVolume(Sound::CHANNEL_N163, conf.audio_vol_n163);
	sound.SetVolume(Sound::CHANNEL_S5B, conf.audio_vol_s5b);
	
	if (conf.audio_volume == 0) { memset(audiobuf, 0, sizeof(audiobuf)); }
}

// Timing Functions

bool timing_frameskip() {
	// Calculate whether to skip a frame or not
	
	if (conf.audio_api == 0) { // SDL
		// Wait until the audio is drained
		#if SDL_VERSION_ATLEAST(2,0,4)
		while (SDL_GetQueuedAudioSize(dev) > (Uint32)bufsize) {
			if (conf.timing_limiter) { SDL_Delay(1); }
		}
		#endif
	}
	
	static int flipper = 1;
	
	if (altspeed) {
		if (flipper > 2) { flipper = 0; return false; }
		else { flipper++; return true; }
	}
	
	return false;
}

void timing_set_default() {
	// Set the framerate to the default
	altspeed = false;
	framerate = nst_pal ? (conf.timing_speed / 6) * 5 : conf.timing_speed;
	#if SDL_VERSION_ATLEAST(2,0,4)
	if (conf.audio_api == 0) { SDL_ClearQueuedAudio(dev); }
	#endif
}

void timing_set_altspeed() {
	// Set the framerate to the alternate speed
	altspeed = true;
	framerate = conf.timing_altspeed;
}
