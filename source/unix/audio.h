#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <SDL.h>
#include "core/api/NstApiEmulator.hpp"
#include "core/api/NstApiSound.hpp"

using namespace Nes::Api;

void audio_init();
void audio_deinit();
void audio_pause();
void audio_unpause();
void audio_set_params();
void audio_lock(Sound::Output *soundoutput);
void audio_unlock(Sound::Output *soundoutput);
uint audio_read_input(Sound::Input &input, void* samples, uint maxSamples);
void audio_adj_volume();

bool timing_frameskip();
void timing_set_default();
void timing_set_altspeed();

#endif
