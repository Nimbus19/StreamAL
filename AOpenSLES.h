//==============================================================================
// Android OpenSL ES Wrapper
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

typedef const struct SLObjectItf_ * const * SLObjectItf;
typedef struct SLEngineOption_ SLEngineOption;
typedef const struct SLInterfaceID_ * SLInterfaceID;

STREAMAL_EXPORT extern bool AOpenSLESAvailable;
STREAMAL_EXPORT extern uint32_t (*AOpenSLESCreateEngine)(SLObjectItf* pEngine, uint32_t numOptions, const SLEngineOption* pEngineOptions, uint32_t numInterfaces, const SLInterfaceID* pInterfaceIds, const uint32_t* pInterfaceRequired);
STREAMAL_EXPORT extern uint32_t (*AOpenSLESQueryNumSupportedEngineInterfaces)(uint32_t* pNumSupportedInterfaces);
STREAMAL_EXPORT extern uint32_t (*AOpenSLESQuerySupportedEngineInterfaces)(uint32_t index, SLInterfaceID* pInterfaceId);
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_ANDROIDACOUSTICECHOCANCELLATION;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_ANDROIDAUTOMATICGAINCONTROL;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_ANDROIDCONFIGURATION;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_ANDROIDNOISESUPPRESSION;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_ENGINE;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_PLAY;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_RECORD;
STREAMAL_EXPORT extern SLInterfaceID AOpenSLES_SL_IID_VOLUME;
//==============================================================================
// OpenSL ES Utility
//==============================================================================
STREAMAL_EXPORT struct AOpenSLES* AOpenSLESCreate(int channel, int sampleRate, int secondPerBuffer, bool record = false);
STREAMAL_EXPORT uint64_t AOpenSLESQueue(struct AOpenSLES* openSLES, uint64_t now, uint64_t timestamp, int64_t adjust, const void* buffer, size_t bufferSize, int gap);
STREAMAL_EXPORT size_t AOpenSLESDequeue(struct AOpenSLES* openSLES, void* buffer, size_t bufferSize, bool drop = false);
STREAMAL_EXPORT void AOpenSLESPlay(struct AOpenSLES* openSLES);
STREAMAL_EXPORT void AOpenSLESStop(struct AOpenSLES* openSLES);
STREAMAL_EXPORT void AOpenSLESPause(struct AOpenSLES* openSLES);
STREAMAL_EXPORT void AOpenSLESVolume(struct AOpenSLES* openSLES, float volume);
STREAMAL_EXPORT void AOpenSLESDestroy(struct AOpenSLES* openSLES);
