//==============================================================================
// Android OpenSL ES Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#include <dlfcn.h>
#include <malloc.h>
#include <memory.h>
#include <math.h>
#include <new>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "RingBuffer.h"
#include "Waveform.h"
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

    if (void* pointer = dlsym(so, "SL_IID_ANDROIDACOUSTICECHOCANCELLATION"))
        memcpy(&AOpenSLES_SL_IID_ANDROIDACOUSTICECHOCANCELLATION, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_ANDROIDAUTOMATICGAINCONTROL"))
        memcpy(&AOpenSLES_SL_IID_ANDROIDAUTOMATICGAINCONTROL, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_ANDROIDCONFIGURATION"))
        memcpy(&AOpenSLES_SL_IID_ANDROIDCONFIGURATION, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_ANDROIDNOISESUPPRESSION"))
        memcpy(&AOpenSLES_SL_IID_ANDROIDNOISESUPPRESSION, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE"))
        memcpy(&AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_ENGINE"))
        memcpy(&AOpenSLES_SL_IID_ENGINE, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_PLAY"))
        memcpy(&AOpenSLES_SL_IID_PLAY, pointer, sizeof(SLInterfaceID));
    if (void* pointer = dlsym(so, "SL_IID_RECORD"))
        memcpy(&AOpenSLES_SL_IID_RECORD, pointer, sizeof(SLInterfaceID));

    return  AOpenSLESCreateEngine != nullptr &&
            AOpenSLESQueryNumSupportedEngineInterfaces != nullptr &&
            AOpenSLESQuerySupportedEngineInterfaces != nullptr &&
            AOpenSLES_SL_IID_ANDROIDCONFIGURATION != nullptr &&
            AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE != nullptr &&
            AOpenSLES_SL_IID_PLAY != nullptr &&
            AOpenSLES_SL_IID_ENGINE != nullptr &&
            AOpenSLES_SL_IID_RECORD != nullptr;
}
//------------------------------------------------------------------------------
bool AOpenSLESAvailable = AOpenSLESInitialize();
uint32_t (*AOpenSLESCreateEngine)(SLObjectItf* pEngine, uint32_t numOptions, const SLEngineOption* pEngineOptions, uint32_t numInterfaces, const SLInterfaceID* pInterfaceIds, const uint32_t* pInterfaceRequired);
uint32_t (*AOpenSLESQueryNumSupportedEngineInterfaces)(uint32_t* pNumSupportedInterfaces);
uint32_t (*AOpenSLESQuerySupportedEngineInterfaces)(uint32_t index, SLInterfaceID* pInterfaceId);
SLInterfaceID AOpenSLES_SL_IID_ANDROIDACOUSTICECHOCANCELLATION;
SLInterfaceID AOpenSLES_SL_IID_ANDROIDAUTOMATICGAINCONTROL;
SLInterfaceID AOpenSLES_SL_IID_ANDROIDCONFIGURATION;
SLInterfaceID AOpenSLES_SL_IID_ANDROIDNOISESUPPRESSION;
SLInterfaceID AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
SLInterfaceID AOpenSLES_SL_IID_PLAY;
SLInterfaceID AOpenSLES_SL_IID_ENGINE;
SLInterfaceID AOpenSLES_SL_IID_RECORD;
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

    SLObjectItf recorderObject;
    SLRecordItf recorderRecord;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;
    SLAndroidConfigurationItf recorderConfig;
    SLAndroidAcousticEchoCancellationItf recorderAEC;
    SLAndroidAutomaticGainControlItf recorderAGC;
    SLAndroidNoiseSuppressionItf recorderNS;

    RingBuffer bufferQueue;
    uint64_t bufferQueueSend;
    uint64_t bufferQueuePick;
    int64_t bufferQueueSendAdjust;
    int64_t bufferQueuePickAdjust;

    uint32_t channel;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;

    float volume;

    bool cancel;
    bool ready;
    bool go;
    bool record;

    int bufferSize;
    short temp[8192];
};
//------------------------------------------------------------------------------
static void playerCallback(SLAndroidSimpleBufferQueueItf, void* context)
{
    AOpenSLES& thiz = *(AOpenSLES*)context;

    if (thiz.cancel == false)
    {
        short* output = (short*)thiz.temp;
        uint64_t outputSize = thiz.bufferSize;
        if (thiz.go)
        {
            thiz.bufferQueuePick += thiz.bufferQueue.Gather(thiz.bufferQueuePick, output, outputSize, true);
            scaleWaveform(output, outputSize, thiz.volume);
        }
        else
        {
            memset(output, 0, outputSize);
        }

        (*thiz.playerBufferQueue)->Enqueue(thiz.playerBufferQueue, output, outputSize);

        return;
    }

    thiz.ready = false;
}
//------------------------------------------------------------------------------
static void recorderCallback(SLAndroidSimpleBufferQueueItf, void* context)
{
    AOpenSLES& thiz = *(AOpenSLES*)context;

    short* input = (short*)thiz.temp;
    uint64_t inputSize = 1024 * sizeof(short) * thiz.channel;
    scaleWaveform(input, inputSize, thiz.volume);
    thiz.bufferQueueSend += thiz.bufferQueue.Scatter(thiz.bufferQueueSend, input, inputSize);

    (*thiz.recorderBufferQueue)->Enqueue(thiz.recorderBufferQueue, input, inputSize);
}
//------------------------------------------------------------------------------
struct AOpenSLES* AOpenSLESCreate(int channel, int sampleRate, int secondPerBuffer, bool record)
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

        if (thiz.bufferQueue.Startup(sampleRate * sizeof(int16_t) * channel * secondPerBuffer) == false)
            break;

        if (AOpenSLESCreateEngine(&thiz.engineObject, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.engineObject)->Realize(thiz.engineObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
            break;
        if ((*thiz.engineObject)->GetInterface(thiz.engineObject, AOpenSLES_SL_IID_ENGINE, &thiz.engineEngine) != SL_RESULT_SUCCESS)
            break;

        SLDataLocator_AndroidSimpleBufferQueue locatorQueue = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
        SLDataFormat_PCM formatPCM = { SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN } ;

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

        if (record)
        {
            SLDataLocator_IODevice locIO = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr };
            SLDataSource audioSource = { &locIO, nullptr };
            SLDataSink audioSink = { &locatorQueue, &formatPCM };
            SLInterfaceID ids[] =
            {
                AOpenSLES_SL_IID_ANDROIDACOUSTICECHOCANCELLATION,
                AOpenSLES_SL_IID_ANDROIDAUTOMATICGAINCONTROL,
                AOpenSLES_SL_IID_ANDROIDCONFIGURATION,
                AOpenSLES_SL_IID_ANDROIDNOISESUPPRESSION,
                AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            };
            SLboolean req[] =
            {
                SL_BOOLEAN_FALSE,
                SL_BOOLEAN_FALSE,
                SL_BOOLEAN_FALSE,
                SL_BOOLEAN_FALSE,
                SL_BOOLEAN_TRUE,
            };
            SLuint32 presetValue = SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;

            if ((*thiz.engineEngine)->CreateAudioRecorder(thiz.engineEngine, &thiz.recorderObject, &audioSource, &audioSink, 5, ids, req) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderObject)->GetInterface(thiz.recorderObject, AOpenSLES_SL_IID_ANDROIDCONFIGURATION, &thiz.recorderConfig) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderConfig)->SetConfiguration(thiz.recorderConfig, SL_ANDROID_KEY_RECORDING_PRESET, &presetValue, sizeof(SLuint32)) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderObject)->Realize(thiz.recorderObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
                break;
            if (AOpenSLES_SL_IID_ANDROIDACOUSTICECHOCANCELLATION)
                (*thiz.recorderObject)->GetInterface(thiz.recorderObject, AOpenSLES_SL_IID_ANDROIDACOUSTICECHOCANCELLATION, &thiz.recorderAEC);
            if (AOpenSLES_SL_IID_ANDROIDAUTOMATICGAINCONTROL)
                (*thiz.recorderObject)->GetInterface(thiz.recorderObject, AOpenSLES_SL_IID_ANDROIDAUTOMATICGAINCONTROL, &thiz.recorderAGC);
            if (AOpenSLES_SL_IID_ANDROIDNOISESUPPRESSION)
                (*thiz.recorderObject)->GetInterface(thiz.recorderObject, AOpenSLES_SL_IID_ANDROIDNOISESUPPRESSION, &thiz.recorderNS);
            if (thiz.recorderAEC)
                (*thiz.recorderAEC)->SetEnabled(thiz.recorderAEC, SL_BOOLEAN_TRUE);
            if (thiz.recorderAGC)
                (*thiz.recorderAGC)->SetEnabled(thiz.recorderAGC, SL_BOOLEAN_TRUE);
            if (thiz.recorderNS)
                (*thiz.recorderNS)->SetEnabled(thiz.recorderNS, SL_BOOLEAN_TRUE);
            if ((*thiz.recorderObject)->GetInterface(thiz.recorderObject, AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, (void*)&thiz.recorderBufferQueue) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderObject)->GetInterface(thiz.recorderObject, AOpenSLES_SL_IID_RECORD, (void*)&thiz.recorderRecord) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderBufferQueue)->RegisterCallback(thiz.recorderBufferQueue, recorderCallback, openSLES) != SL_RESULT_SUCCESS)
                break;
#if 0
            if ((*thiz.recorderRecord)->SetMarkerPosition(thiz.recorderRecord, 2000) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderRecord)->SetPositionUpdatePeriod(thiz.recorderRecord, 500) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderRecord)->SetCallbackEventsMask(thiz.recorderRecord, SL_RECORDEVENT_HEADATMARKER | SL_RECORDEVENT_HEADATNEWPOS) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.recorderRecord)->RegisterCallback(thiz.recorderRecord, [](SLRecordItf, void*, SLuint32){}, nullptr) != SL_RESULT_SUCCESS)
                break;
#endif
            if ((*thiz.recorderRecord)->SetRecordState(thiz.recorderRecord, SL_RECORDSTATE_RECORDING) != SL_RESULT_SUCCESS)
                break;
        }
        else
        {
            if ((*thiz.engineEngine)->CreateOutputMix(thiz.engineEngine, &thiz.outputMixObject, 0, nullptr, nullptr) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.outputMixObject)->Realize(thiz.outputMixObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
                break;

            SLDataLocator_OutputMix locMix = { SL_DATALOCATOR_OUTPUTMIX, thiz.outputMixObject };
            SLDataSource audioSource = { &locatorQueue, &formatPCM };
            SLDataSink audioSink = { &locMix, nullptr };
            SLInterfaceID ids[1] =
            {
                AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE
            };
            SLboolean req[1] =
            {
                SL_BOOLEAN_TRUE
            };

            if ((*thiz.engineEngine)->CreateAudioPlayer(thiz.engineEngine, &thiz.playerObject, &audioSource, &audioSink, 1, ids, req) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.playerObject)->Realize(thiz.playerObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.playerObject)->GetInterface(thiz.playerObject, AOpenSLES_SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &thiz.playerBufferQueue) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.playerObject)->GetInterface(thiz.playerObject, AOpenSLES_SL_IID_PLAY, &thiz.playerPlay) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.playerBufferQueue)->RegisterCallback(thiz.playerBufferQueue, playerCallback, openSLES) != SL_RESULT_SUCCESS)
                break;
            if ((*thiz.playerPlay)->SetPlayState(thiz.playerPlay, SL_PLAYSTATE_PLAYING) != SL_RESULT_SUCCESS)
                break;
        }

        thiz.channel = channel;
        thiz.sampleRate = sampleRate;
        thiz.bytesPerSecond = sampleRate * sizeof(int16_t) * channel;
        thiz.volume = record ? 1.0f : 0.0f;
        thiz.record = record;

        return openSLES;
    }
    AOpenSLESDestroy(openSLES);

    return nullptr;
}
//------------------------------------------------------------------------------
uint64_t AOpenSLESQueue(struct AOpenSLES* openSLES, uint64_t now, uint64_t timestamp, int64_t adjust, const void* buffer, size_t bufferSize, int gap)
{
    if (openSLES == nullptr)
        return 0;
    AOpenSLES& thiz = (*openSLES);
    if (thiz.record)
        return 0;
    if (bufferSize == 0)
        return 0;

    if (thiz.ready)
    {
        if (thiz.bufferQueueSend < thiz.bufferQueuePick || thiz.bufferQueueSend > thiz.bufferQueuePick + thiz.bytesPerSecond / 2)
        {
            thiz.bufferQueueSend = 0;
            timestamp = now + thiz.bufferQueuePickAdjust;
        }
    }

    if (thiz.bufferQueueSend == 0 || thiz.bufferQueueSendAdjust != adjust)
    {
        thiz.bufferQueueSend = (timestamp + adjust) * thiz.bytesPerSecond / 1000000;
        thiz.bufferQueueSend = thiz.bufferQueueSend - (thiz.bufferQueueSend % bufferSize);
        thiz.bufferQueueSendAdjust = adjust;
    }
    thiz.bufferQueueSend += thiz.bufferQueue.Scatter(thiz.bufferQueueSend, buffer, bufferSize);

    if (thiz.ready == false)
    {
        thiz.ready = true;

        adjust = 0;
        if (now > timestamp)
        {
            adjust = timestamp - now;
        }

        thiz.bufferSize = bufferSize;
        thiz.bufferQueuePick = (now + adjust) * thiz.bytesPerSecond / 1000000 - bufferSize * gap;
        thiz.bufferQueuePick = thiz.bufferQueuePick - (thiz.bufferQueuePick % bufferSize);
        thiz.bufferQueuePickAdjust = adjust;

        (*thiz.playerPlay)->SetPlayState(thiz.playerPlay, SL_PLAYSTATE_PLAYING);
        (*thiz.playerBufferQueue)->Enqueue(thiz.playerBufferQueue, thiz.temp, sizeof(short) * thiz.channel);
    }
    else
    {
        thiz.go = true;
    }

    return thiz.bufferQueuePick * 1000000 / thiz.bytesPerSecond;
}
//------------------------------------------------------------------------------
size_t AOpenSLESDequeue(struct AOpenSLES* openSLES, void* buffer, size_t bufferSize, bool drop)
{
    if (openSLES == nullptr)
        return 0;
    AOpenSLES& thiz = (*openSLES);
    if (thiz.record == false)
        return 0;

    if (thiz.ready == false)
    {
        thiz.ready = true;

        (*thiz.recorderRecord)->SetRecordState(thiz.recorderRecord, SL_RECORDSTATE_RECORDING);
        (*thiz.recorderBufferQueue)->Enqueue(thiz.recorderBufferQueue, thiz.temp, sizeof(short) * thiz.channel);
    }

    if (drop)
    {
        uint64_t available = thiz.bufferQueueSend - thiz.bufferQueuePick;
        while (available > thiz.bytesPerSecond)
        {
            thiz.bufferQueuePick += thiz.bytesPerSecond;
            available = thiz.bufferQueueSend - thiz.bufferQueuePick;
        }
    }

    if (thiz.bufferQueueSend < thiz.bufferQueuePick + bufferSize)
        return 0;
    thiz.bufferQueuePick += thiz.bufferQueue.Gather(thiz.bufferQueuePick, buffer, bufferSize, true);

    return bufferSize;
}
//------------------------------------------------------------------------------
void AOpenSLESReset(struct AOpenSLES* openSLES)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    if (thiz.record)
    {
        (*thiz.recorderRecord)->SetRecordState(thiz.recorderRecord, SL_RECORDSTATE_STOPPED);
        thiz.ready = false;
        thiz.go = false;
    }
    else
    {
        (*thiz.playerPlay)->SetPlayState(thiz.playerPlay, SL_PLAYSTATE_STOPPED);
        thiz.ready = false;
        thiz.go = false;
    }
}
//------------------------------------------------------------------------------
void AOpenSLESVolume(struct AOpenSLES* openSLES, float volume)
{
    if (openSLES == nullptr)
        return;
    AOpenSLES& thiz = (*openSLES);

    if (thiz.record)
    {
        thiz.volume = volume;
    }
    else
    {
        thiz.volume = volume;
    }
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
    }

    if (thiz.recorderObject != nullptr)
    {
        (*thiz.recorderObject)->Destroy(thiz.recorderObject);
        thiz.recorderObject = nullptr;
        thiz.recorderRecord = nullptr;
        thiz.recorderBufferQueue = nullptr;
        thiz.recorderConfig = nullptr;
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

    delete openSLES;
}
//------------------------------------------------------------------------------
