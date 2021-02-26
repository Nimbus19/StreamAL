//==============================================================================
// Windows WaveIO Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#pragma once

#include <stdint.h>

#ifndef STREAMAL_EXPORT
#define STREAMAL_EXPORT
#endif

STREAMAL_EXPORT struct WWaveIO* WWaveIOCreate(int channel, int sampleRate, int secondPerBuffer, bool record);
STREAMAL_EXPORT void WWaveIODestroy(struct WWaveIO* waveOut);
STREAMAL_EXPORT uint64_t WWaveIOQueue(struct WWaveIO* waveOut, uint64_t now, uint64_t timestamp, int64_t adjust, const void* buffer, size_t bufferSize, int gap);
STREAMAL_EXPORT size_t WWaveIODequeue(struct WWaveIO* waveOut, void* buffer, size_t bufferSize, bool drop = false);
STREAMAL_EXPORT void WWaveIOReset(struct WWaveIO* waveOut);
STREAMAL_EXPORT void WWaveIOVolume(struct WWaveIO* waveOut, float volume);
