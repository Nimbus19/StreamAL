//==============================================================================
// Android OpenSL ES Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/AOpenSLES
//==============================================================================
#include <memory.h>
#include <dlfcn.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <vector>
#include <mutex>
#include "AOpenSLES.h"

//==============================================================================
// OpenSL ES supported since Android Ice Cream Sandwich (4.0)
//==============================================================================
static bool AOpenSLESInitialize()
{
    void* so = dlopen("libOpenSLES.so", RTLD_LAZY);
    if (so == nullptr)
    {
        __android_log_print(ANDROID_LOG_WARN, "AOpenSLES", "Failed to open libOpenSLES.so");
        return false;
    }

    (void*&)AOpenSLESCreateEngine = dlsym(so, "slCreateEngine");
    (void*&)AOpenSLESQueryNumSupportedEngineInterfaces = dlsym(so, "slQueryNumSupportedEngineInterfaces");
    (void*&)AOpenSLESQuerySupportedEngineInterfaces = dlsym(so, "slQuerySupportedEngineInterfaces");

    if (void* pointer = dlsym(so, "SL_IID_ENGINE"))
        memcpy(&AOpenSLES_SL_IID_ENGINE, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE"))
        memcpy(&AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_VOLUME"))
        memcpy(&AOpenSLES_SL_IID_VOLUME, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_PLAY"))
        memcpy(&AOpenSLES_SL_IID_PLAY, pointer, sizeof(SLInterfaceID));

    return  AOpenSLESCreateEngine != nullptr &&
            AOpenSLESQueryNumSupportedEngineInterfaces != nullptr &&
            AOpenSLESQuerySupportedEngineInterfaces != nullptr &&
            AOpenSLES_SL_IID_ENGINE != nullptr &&
            AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE != nullptr &&
            AOpenSLES_SL_IID_VOLUME != nullptr &&
            AOpenSLES_SL_IID_PLAY != nullptr;
}
//------------------------------------------------------------------------------
bool AOpenSLESAvailable = AOpenSLESInitialize();
uint32_t (*AOpenSLESCreateEngine)(SLObjectItf* pEngine, uint32_t numOptions, const SLEngineOption* pEngineOptions, uint32_t numInterfaces, const SLInterfaceID* pInterfaceIds, const uint32_t* pInterfaceRequired);
uint32_t (*AOpenSLESQueryNumSupportedEngineInterfaces)(uint32_t* pNumSupportedInterfaces);
uint32_t (*AOpenSLESQuerySupportedEngineInterfaces)(uint32_t index, SLInterfaceID* pInterfaceId);
SLInterfaceID AOpenSLES_SL_IID_ENGINE;
SLInterfaceID AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
SLInterfaceID AOpenSLES_SL_IID_VOLUME;
SLInterfaceID AOpenSLES_SL_IID_PLAY;
//==============================================================================
// OpenSL ES Utility
//==============================================================================
struct AOpenSLES
{
    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    SLObjectItf outputMixObject;

    SLObjectItf playerObject;
    SLPlayItf playerPlay;
    SLAndroidSimpleBufferQueueItf playerBufferQueue;
    SLVolumeItf playerVolume;

    char* ringBuffer;
    size_t ringBufferSize;
    uint64_t bufferQueueSend;
    uint64_t bufferQueuePick;

    uint32_t channel;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;

    bool cancel;
    bool ready;
    bool sync;
};
//------------------------------------------------------------------------------
static void playerCallback(SLAndroidSimpleBufferQueueItf, void* context)
{
    AOpenSLES& thiz = *(AOpenSLES*)context;

    if (thiz.cancel == false)
    {
        if (thiz.sync)
        {
            if (thiz.bufferQueuePick < thiz.bufferQueueSend - thiz.bytesPerSecond / 2)
                thiz.bufferQueuePick = thiz.bufferQueueSend - thiz.bytesPerSecond / 8;
            else if (thiz.bufferQueuePick > thiz.bufferQueueSend)
                thiz.bufferQueuePick = thiz.bufferQueueSend - thiz.bytesPerSecond / 8;
        }

        char* queue = &thiz.ringBuffer[thiz.bufferQueuePick % thiz.ringBufferSize];
        int queueSize = 4096;
        if (queueSize > thiz.ringBufferSize - (queue - thiz.ringBuffer))
            queueSize = thiz.ringBufferSize - (queue - thiz.ringBuffer);
        thiz.bufferQueuePick += queueSize;

        (*thiz.playerBufferQueue)->Enqueue(thiz.playerBufferQueue, queue, queueSize);

        return;
    }

    thiz.ready = false;
}
//------------------------------------------------------------------------------
struct AOpenSLES* AOpenSLESCreate(int channel, int sampleRate)
{
    AOpenSLES* openSLES = nullptr;

    switch (0) case 0: default:
    {
        if (AOpenSLESAvailable == false)
            break;

        if (channel == 0)
            break;
        if (sampleRate == 0)
            break;

        openSLES = new AOpenSLES{};
        if (openSLES == nullptr)
            break;
        AOpenSLES& thiz = (*openSLES);

        thiz.ringBufferSize = sampleRate * sizeof(int16_t) * channel * 8;
        thiz.ringBuffer = (char*)malloc(thiz.ringBufferSize);
        if (thiz.ringBuffer == nullptr)
            break;
        memset(thiz.ringBuffer, 0, thiz.ringBufferSize);

        if (AOpenSLESCreateEngine(&thiz.engineObject, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.engineObject)->Realize(thiz.engineObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.engineObject)->GetInterface(thiz.engineObject, AOpenSLES_SL_IID_ENGINE, &thiz.engineEngine) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.engineEngine)->CreateOutputMix(thiz.engineEngine, &thiz.outputMixObject, 0, nullptr, nullptr) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.outputMixObject)->Realize(thiz.outputMixObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
            break;

        SLDataLocator_AndroidSimpleBufferQueue locatorQueue = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
        SLDataFormat_PCM formatPCM = { SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN } ;
        SLDataSource audioSource = { &locatorQueue, &formatPCM };
        SLDataLocator_OutputMix locMix = { SL_DATALOCATOR_OUTPUTMIX, thiz.outputMixObject };
        SLDataSink audioSink = { &locMix, nullptr };
        SLInterfaceID ids[2] = { AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, AOpenSLES_SL_IID_VOLUME };
        SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

        switch (channel)
        {
        case 1:
            formatPCM.numChannels = 1;
            formatPCM.channelMask = SL_SPEAKER_FRONT_CENTER;
            break;
        case 2:
        default:
            formatPCM.numChannels = 2;
            formatPCM.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
            break;
        }
        formatPCM.samplesPerSec = sampleRate * 1000u;

        if ((*thiz.engineEngine)->CreateAudioPlayer(thiz.engineEngine, &thiz.playerObject, &audioSource, &audioSink, 2, ids, req) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.playerObject)->Realize(thiz.playerObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.playerObject)->GetInterface(thiz.playerObject, AOpenSLES_SL_IID_PLAY, &thiz.playerPlay) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.playerObject)->GetInterface(thiz.playerObject, AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &thiz.playerBufferQueue) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.playerBufferQueue)->RegisterCallback(thiz.playerBufferQueue, playerCallback, openSLES) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.playerObject)->GetInterface(thiz.playerObject, AOpenSLES_SL_IID_VOLUME, &thiz.playerVolume) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.playerPlay)->SetPlayState(thiz.playerPlay, SL_PLAYSTATE_PLAYING) != SL_RESULT_SUCCESS)
            break;

        thiz.channel = channel;
        thiz.sampleRate = sampleRate;
        thiz.bytesPerSecond = sampleRate * sizeof(int16_t) * channel;

        return openSLES;
    }
    AOpenSLESDestroy(openSLES);

    return nullptr;
}
//------------------------------------------------------------------------------
void AOpenSLESQueue(struct AOpenSLES* openSLES, uint64_t timestamp, const void* buffer, size_t bufferSize, bool sync)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    uint64_t queueOffset = timestamp * thiz.bytesPerSecond / 1000000;

    if (bufferSize)
    {
        queueOffset = queueOffset + bufferSize / 2 - 1;
        queueOffset = queueOffset - queueOffset % bufferSize;
    }

#if 0
    static uint64_t preOffset = offset;
    if (preOffset == offset)
    {
        __android_log_print(ANDROID_LOG_INFO, "AOpenSLES", "Jitter %d %d %zd %lu", thiz.sampleRate, thiz.channel, bufferSize, offset);
    }
    preOffset = offset;
#endif

    char* queue = &thiz.ringBuffer[queueOffset % thiz.ringBufferSize];
    size_t queueSize = bufferSize;
    if (queueSize > thiz.ringBufferSize - (queue - thiz.ringBuffer))
    {
        queueSize = thiz.ringBufferSize - (queue - thiz.ringBuffer);
        memcpy(queue, buffer, queueSize);

        buffer = (char*)buffer + queueSize;
        queue = thiz.ringBuffer;
        queueSize = bufferSize - queueSize;
    }
    memcpy(queue, buffer, queueSize);
    thiz.bufferQueueSend = queueOffset + bufferSize;

    if (thiz.ready == false)
    {
        thiz.ready = true;

        (*thiz.playerBufferQueue)->Enqueue(thiz.playerBufferQueue, "\0\0\0\0", 4);
    }

    thiz.sync = sync;
}
//------------------------------------------------------------------------------
void AOpenSLESPlay(struct AOpenSLES* openSLES)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    (*thiz.playerPlay)->SetPlayState(thiz.playerPlay, SL_PLAYSTATE_PLAYING);
}
//------------------------------------------------------------------------------
void AOpenSLESStop(struct AOpenSLES* openSLES)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    (*thiz.playerPlay)->SetPlayState(thiz.playerPlay, SL_PLAYSTATE_STOPPED);
}
//------------------------------------------------------------------------------
void AOpenSLESPause(struct AOpenSLES* openSLES)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    (*thiz.playerPlay)->SetPlayState(thiz.playerPlay, SL_PLAYSTATE_PAUSED);
}
//------------------------------------------------------------------------------
void AOpenSLESVolume(struct AOpenSLES* openSLES, float volume)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    if (volume <= 0.0f)
    {
        (*thiz.playerVolume)->SetVolumeLevel(thiz.playerVolume, SL_MILLIBEL_MIN);
        return;
    }

    float dbVolume = log10f(volume) / log10f(100.0f);
    SLmillibel slesVolume = (SLmillibel)(-10000 * (1.0f - dbVolume));
    (*thiz.playerVolume)->SetVolumeLevel(thiz.playerVolume, slesVolume);
}
//------------------------------------------------------------------------------
void AOpenSLESDestroy(struct AOpenSLES* openSLES)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    thiz.cancel = true;

    if (thiz.playerObject != nullptr)
    {
        (*thiz.playerObject)->Destroy(thiz.playerObject);
        thiz.playerObject = nullptr;
        thiz.playerPlay = nullptr;
        thiz.playerBufferQueue = nullptr;
        thiz.playerVolume = nullptr;
    }

    if (thiz.outputMixObject != nullptr)
    {
        (*thiz.outputMixObject)->Destroy(thiz.outputMixObject);
        thiz.outputMixObject = nullptr;
    }

    thiz.engineEngine = nullptr;

    if (thiz.engineObject != nullptr)
    {
        (*thiz.engineObject)->Destroy(thiz.engineObject);
        thiz.engineObject = nullptr;
    }

    free(thiz.ringBuffer);
    delete openSLES;
}
//------------------------------------------------------------------------------
