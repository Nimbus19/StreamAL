//==============================================================================
// iOS AudioUnit Wrapper
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

STREAMAL_EXPORT extern bool iAudioUnitAvailable;
//==============================================================================
// AudioUnit Utility
//==============================================================================
STREAMAL_EXPORT struct iAudioUnit* iAudioUnitCreate(int channel, int sampleRate, int secondPerBuffer, bool record = false);
STREAMAL_EXPORT uint64_t iAudioUnitQueue(struct iAudioUnit* audioUnit, uint64_t now, uint64_t timestamp, const void* buffer, size_t bufferSize);
STREAMAL_EXPORT size_t iAudioUnitDequeue(struct iAudioUnit* audioUnit, void* buffer, size_t bufferSize, bool drop = false);
STREAMAL_EXPORT void iAudioUnitPlay(struct iAudioUnit* audioUnit);
STREAMAL_EXPORT void iAudioUnitStop(struct iAudioUnit* audioUnit);
STREAMAL_EXPORT void iAudioUnitPause(struct iAudioUnit* audioUnit);
STREAMAL_EXPORT void iAudioUnitVolume(struct iAudioUnit* audioUnit, float volume);
STREAMAL_EXPORT void iAudioUnitDestroy(struct iAudioUnit* audioUnit);
