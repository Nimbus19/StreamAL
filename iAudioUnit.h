//==============================================================================
// iOS AudioUnit Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/iAudioUnit
//==============================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>

extern bool iAudioUnitAvailable;
//==============================================================================
// AudioUnit Utility
//==============================================================================
struct iAudioUnit* iAudioUnitCreate(int channel, int sampleRate, int secondPerBuffer, bool record = false);
void iAudioUnitQueue(struct iAudioUnit* audioUnit, uint64_t timestamp, const void* buffer, size_t bufferSize, bool sync = false);
size_t iAudioUnitDequeue(struct iAudioUnit* audioUnit, void* buffer, size_t bufferSize, bool drop = false);
void iAudioUnitPlay(struct iAudioUnit* audioUnit);
void iAudioUnitStop(struct iAudioUnit* audioUnit);
void iAudioUnitPause(struct iAudioUnit* audioUnit);
void iAudioUnitVolume(struct iAudioUnit* audioUnit, float volume);
void iAudioUnitDestroy(struct iAudioUnit* audioUnit);
