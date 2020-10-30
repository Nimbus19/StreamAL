//==============================================================================
// iOS AudioUnit Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#include <stdio.h>
#include <TargetConditionals.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AVFoundation/AVFoundation.h>
#include "RingBuffer.h"
#include "Waveform.h"
#include "iAudioUnit.h"

#define kBusSpeaker     0
#define kBusMicrophone  1

bool iAudioUnitAvailable = true;
//==============================================================================
// AudioUnit Utility
//==============================================================================
struct iAudioUnit
{
    AudioComponentInstance instance;

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
    bool record;

    int bufferSize;
};
//------------------------------------------------------------------------------
static OSStatus playerCallback(void* inRefCon,
                               AudioUnitRenderActionFlags* ioActionFlags,
                               const AudioTimeStamp* inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList* ioData)
{
    iAudioUnit& thiz = *(iAudioUnit*)inRefCon;

    if (thiz.cancel == false)
    {
        short* output = (short*)ioData->mBuffers[0].mData;
        size_t outputSize = ioData->mBuffers[0].mDataByteSize;
        thiz.bufferQueuePick += thiz.bufferQueue.Gather(thiz.bufferQueuePick, output, outputSize, true);
        scaleWaveform(output, outputSize, thiz.volume);

        return noErr;
    }

    thiz.ready = false;
    return noErr;
};
//------------------------------------------------------------------------------
static OSStatus recorderCallback(void* inRefCon,
                                 AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList* ioData)
{
    iAudioUnit& thiz = *(iAudioUnit*)inRefCon;

    if (thiz.cancel == false)
    {
        AudioBufferList bufferList;
        bufferList.mNumberBuffers = 1;
        bufferList.mBuffers[0].mData = nullptr;
        bufferList.mBuffers[0].mDataByteSize = 0;
        OSStatus status = AudioUnitRender(thiz.instance,
                                          ioActionFlags,
                                          inTimeStamp,
                                          kBusMicrophone,
                                          inNumberFrames,
                                          &bufferList);
        if (status == noErr)
        {
            short* input = (short*)bufferList.mBuffers[0].mData;
            size_t inputSize = bufferList.mBuffers[0].mDataByteSize;
            scaleWaveform(input, inputSize, thiz.volume);
            thiz.bufferQueueSend += thiz.bufferQueue.Scatter(thiz.bufferQueueSend, input, inputSize);
        }

        return noErr;
    }

    thiz.ready = false;
    return noErr;
}
//------------------------------------------------------------------------------
static OSStatus dummyCallback(void* inRefCon,
                              AudioUnitRenderActionFlags* ioActionFlags,
                              const AudioTimeStamp* inTimeStamp,
                              UInt32 inBusNumber,
                              UInt32 inNumberFrames,
                              AudioBufferList* ioData)
{
    return noErr;
}
//------------------------------------------------------------------------------
struct iAudioUnit* iAudioUnitCreate(int channel, int sampleRate, int secondPerBuffer, bool record)
{
    iAudioUnit* audioUnit = nullptr;

    switch (0) case 0: default:
    {
        if (iAudioUnitAvailable == false)
            break;

        if (channel == 0)
            break;
        if (sampleRate == 0)
            break;

        audioUnit = new iAudioUnit{};
        if (audioUnit == nullptr)
            break;
        iAudioUnit& thiz = (*audioUnit);

        if (thiz.bufferQueue.Startup(sampleRate * sizeof(int16_t) * channel * secondPerBuffer) == false)
            break;

        if (record)
        {
            AudioComponentDescription desc = {};
            desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
            desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
#else
            desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
            desc.componentFlags = 0;
            desc.componentFlagsMask = 0;
            desc.componentManufacturer = kAudioUnitManufacturer_Apple;
            AudioComponent inputComponent = AudioComponentFindNext(nullptr, &desc);
            if (inputComponent == nullptr)
                break;
            if (AudioComponentInstanceNew(inputComponent, &thiz.instance) != noErr)
                break;

            AudioStreamBasicDescription format = {};
            format.mSampleRate = sampleRate;
            format.mFormatID = kAudioFormatLinearPCM;
            format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
            format.mFramesPerPacket = 1;
            format.mChannelsPerFrame = channel;
            format.mBitsPerChannel = 16;
            format.mBytesPerFrame = format.mBitsPerChannel / 8 * format.mChannelsPerFrame;
            format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioUnitProperty_StreamFormat,
                                     kAudioUnitScope_Output,
                                     kBusMicrophone,
                                     &format,
                                     sizeof(format)) != noErr)
                break;

            UInt32 enable = 1;
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioOutputUnitProperty_EnableIO,
                                     kAudioUnitScope_Input,
                                     kBusMicrophone,
                                     &enable,
                                     sizeof(enable)) != noErr)
                break;

            UInt32 disable = 0;
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioOutputUnitProperty_EnableIO,
                                     kAudioUnitScope_Output,
                                     kBusSpeaker,
                                     &disable,
                                     sizeof(disable)) != noErr)
                break;

            AURenderCallbackStruct recorderCallbackStruct = { recorderCallback, audioUnit };
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioOutputUnitProperty_SetInputCallback,
                                     kAudioUnitScope_Input,
                                     kBusMicrophone,
                                     &recorderCallbackStruct,
                                     sizeof(recorderCallbackStruct)) != noErr)
                break;

            AURenderCallbackStruct playerCallbackStruct = { dummyCallback, nullptr };
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioUnitProperty_SetRenderCallback,
                                     kAudioUnitScope_Output,
                                     kBusSpeaker,
                                     &playerCallbackStruct,
                                     sizeof(playerCallbackStruct)) != noErr)
                break;

            AudioUnitInitialize(thiz.instance);
            AudioOutputUnitStart(thiz.instance);

#if TARGET_OS_IPHONE
            [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayAndRecord
                                             withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker
                                                   error:nil];
            [[AVAudioSession sharedInstance] setMode:AVAudioSessionModeVoiceChat
                                               error:nil];
            [[AVAudioSession sharedInstance] setInputGain:1.0
                                                    error:nil];
#endif
        }
        else
        {
            AudioComponentDescription desc = {};
            desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
            desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
            desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
            desc.componentFlags = 0;
            desc.componentFlagsMask = 0;
            desc.componentManufacturer = kAudioUnitManufacturer_Apple;
            AudioComponent inputComponent = AudioComponentFindNext(nullptr, &desc);
            if (inputComponent == nullptr)
                break;
            if (AudioComponentInstanceNew(inputComponent, &thiz.instance) != noErr)
                break;

            AudioStreamBasicDescription outputFormat = {};
            outputFormat.mSampleRate = sampleRate;
            outputFormat.mFormatID = kAudioFormatLinearPCM;
            outputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
            outputFormat.mFramesPerPacket = 1;
            outputFormat.mChannelsPerFrame = channel;
            outputFormat.mBitsPerChannel = 16;
            outputFormat.mBytesPerFrame = outputFormat.mBitsPerChannel / 8 * outputFormat.mChannelsPerFrame;
            outputFormat.mBytesPerPacket = outputFormat.mBytesPerFrame * outputFormat.mFramesPerPacket;
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioUnitProperty_StreamFormat,
                                     kAudioUnitScope_Input,
                                     kBusSpeaker,
                                     &outputFormat,
                                     sizeof(outputFormat)) != noErr)
                break;

#if TARGET_OS_IPHONE
            UInt32 enable = 1;
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioOutputUnitProperty_EnableIO,
                                     kAudioUnitScope_Output,
                                     kBusSpeaker,
                                     &enable,
                                     sizeof(enable)) != noErr)
                break;
#endif

            AURenderCallbackStruct callbackStruct = { playerCallback, audioUnit };
            if (AudioUnitSetProperty(thiz.instance,
                                     kAudioUnitProperty_SetRenderCallback,
                                     kAudioUnitScope_Output,
                                     kBusSpeaker,
                                     &callbackStruct,
                                     sizeof(callbackStruct)) != noErr)
                break;

            AudioUnitInitialize(thiz.instance);
            AudioOutputUnitStart(thiz.instance);

#if TARGET_OS_IPHONE
            [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback
                                                   error:nil];
            [[AVAudioSession sharedInstance] overrideOutputAudioPort:AVAudioSessionPortOverrideSpeaker
                                                               error:nil];
            [[AVAudioSession sharedInstance] setActive:YES
                                                 error:nil];
#endif
        }

        thiz.channel = channel;
        thiz.sampleRate = sampleRate;
        thiz.bytesPerSecond = sampleRate * sizeof(int16_t) * channel;
        thiz.volume = 0.0f;
        thiz.record = record;

        return audioUnit;
    }
    iAudioUnitDestroy(audioUnit);

    return nullptr;
}
//------------------------------------------------------------------------------
uint64_t iAudioUnitQueue(struct iAudioUnit* audioUnit, uint64_t now, uint64_t timestamp, int64_t adjust, const void* buffer, size_t bufferSize, int gap)
{
    if (audioUnit == nullptr)
        return 0;
    iAudioUnit& thiz = (*audioUnit);
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

        if (thiz.instance)
            AudioOutputUnitStart(thiz.instance);
    }

    return thiz.bufferQueuePick * 1000000 / thiz.bytesPerSecond;
}
//------------------------------------------------------------------------------
size_t iAudioUnitDequeue(struct iAudioUnit* audioUnit, void* buffer, size_t bufferSize, bool drop)
{
    if (audioUnit == nullptr)
        return 0;
    iAudioUnit& thiz = (*audioUnit);
    if (thiz.record == false)
        return 0;

    if (thiz.ready == false)
    {
        thiz.ready = true;

        AudioOutputUnitStart(thiz.instance);
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
void iAudioUnitPlay(struct iAudioUnit* audioUnit)
{
    if (audioUnit == nullptr)
        return;
    iAudioUnit& thiz = (*audioUnit);

    if (thiz.record)
    {

    }
    else
    {
        AudioOutputUnitStart(thiz.instance);
    }
}
//------------------------------------------------------------------------------
void iAudioUnitStop(struct iAudioUnit* audioUnit)
{
    if (audioUnit == nullptr)
        return;
    iAudioUnit& thiz = (*audioUnit);

    if (thiz.record)
    {

    }
    else
    {
        AudioOutputUnitStop(thiz.instance);
    }
}
//------------------------------------------------------------------------------
void iAudioUnitPause(struct iAudioUnit* audioUnit)
{
    if (audioUnit == nullptr)
        return;
    iAudioUnit& thiz = (*audioUnit);

    if (thiz.record)
    {

    }
    else
    {
        AudioOutputUnitStop(thiz.instance);
    }
}
//------------------------------------------------------------------------------
void iAudioUnitVolume(struct iAudioUnit* audioUnit, float volume)
{
    if (audioUnit == nullptr)
        return;
    iAudioUnit& thiz = (*audioUnit);

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
void iAudioUnitDestroy(struct iAudioUnit* audioUnit)
{
    if (audioUnit == nullptr)
        return;
    iAudioUnit& thiz = (*audioUnit);

    thiz.cancel = true;

    if (thiz.instance)
    {
        AudioOutputUnitStop(thiz.instance);
        AudioUnitUninitialize(thiz.instance);
        AudioComponentInstanceDispose(thiz.instance);
        thiz.instance = nullptr;
    }

    delete audioUnit;
}
//------------------------------------------------------------------------------
