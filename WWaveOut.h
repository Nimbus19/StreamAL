//==============================================================================
// Windows WaveOut Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#pragma once

#include <stdint.h>

#ifndef STREAMAL_EXPORT
#define STREAMAL_EXPORT
#endif

STREAMAL_EXPORT struct WWaveOut* WWaveOutCreate(int channel, int sampleRate, int secondPerBuffer, bool record);
STREAMAL_EXPORT void WWaveOutDestroy(struct WWaveOut* waveOut);
STREAMAL_EXPORT uint64_t WWaveOutQueue(struct WWaveOut* waveOut, uint64_t now, uint64_t timestamp, int64_t adjust, const void* buffer, size_t bufferSize);
STREAMAL_EXPORT void WWaveOutVolume(struct WWaveOut* waveOut, float volume);
