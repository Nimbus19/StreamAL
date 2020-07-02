//==============================================================================
// Windows WaveOut Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/WWaveOut
//==============================================================================
#pragma once

#include <stdint.h>

struct WWaveOut* WWaveOutCreate(int channel, int sampleRate);
void WWaveOutDestroy(struct WWaveOut* waveOut);
void WWaveOutQueue(struct WWaveOut* waveOut, uint64_t timestamp, const void* buffer, size_t bufferSize, bool sync = false);
