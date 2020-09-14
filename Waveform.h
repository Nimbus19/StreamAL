//==============================================================================
// Waveform
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef STREAMAL_EXPORT
#define STREAMAL_EXPORT
#endif

STREAMAL_EXPORT void scaleWaveform(int16_t* waveform, size_t count, float scale);
