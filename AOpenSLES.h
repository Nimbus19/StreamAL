//==============================================================================
// Android OpenSL ES Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/AOpenSLES
//==============================================================================
#pragma once

#include <stdint.h>

typedef const struct SLObjectItf_ * const * SLObjectItf;
typedef struct SLEngineOption_ SLEngineOption;
typedef const struct SLInterfaceID_ * SLInterfaceID;

extern bool AOpenSLESAvailable;
extern uint32_t (*AOpenSLESCreateEngine)(SLObjectItf* pEngine, uint32_t numOptions, const SLEngineOption* pEngineOptions, uint32_t numInterfaces, const SLInterfaceID* pInterfaceIds, const uint32_t* pInterfaceRequired);
extern uint32_t (*AOpenSLESQueryNumSupportedEngineInterfaces)(uint32_t* pNumSupportedInterfaces);
extern uint32_t (*AOpenSLESQuerySupportedEngineInterfaces)(uint32_t index, SLInterfaceID* pInterfaceId);
extern SLInterfaceID AOpenSLES_SL_IID_ENGINE;
extern SLInterfaceID AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
extern SLInterfaceID AOpenSLES_SL_IID_VOLUME;
extern SLInterfaceID AOpenSLES_SL_IID_PLAY;
//==============================================================================
// OpenSL ES Utility
//==============================================================================
struct AOpenSLES* AOpenSLESCreate(int channel, int sampleRate);
void AOpenSLESQueue(struct AOpenSLES* openSLES, uint64_t timestamp, const void* buffer, size_t bufferSize, bool sync = false);
void AOpenSLESPlay(struct AOpenSLES* openSLES);
void AOpenSLESStop(struct AOpenSLES* openSLES);
void AOpenSLESPause(struct AOpenSLES* openSLES);
void AOpenSLESVolume(struct AOpenSLES* openSLES, float volume);
void AOpenSLESDestroy(struct AOpenSLES* openSLES);
