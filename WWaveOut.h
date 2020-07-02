//==============================================================================
// Windows WaveOut Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#pragma once

#include <stdint.h>

struct WWaveOut* WWaveOutCreate(int channel, int sampleRate, int secondPerBuffer, bool record);
void WWaveOutDestroy(struct WWaveOut* waveOut);
uint64_t WWaveOutQueue(struct WWaveOut* waveOut, uint64_t timestamp, const void* buffer, size_t bufferSize, bool sync = false);
void WWaveOutVolume(struct WWaveOut* waveOut, float volume);
