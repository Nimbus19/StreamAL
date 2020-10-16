//==============================================================================
// Windows WaveOut Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#include <stdlib.h>
#include <math.h>
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "winmm.lib")
#include <Windows.h>
#include <mmeapi.h>
#include "RingBuffer.h"
#include "Waveform.h"
#include "WWaveOut.h"

//------------------------------------------------------------------------------
struct WWaveOut
{
    WAVEFORMATEX waveFormat;
    HWAVEOUT waveOut;
    WAVEHDR waveHeader[8];
    int waveHeaderIndex;

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
    HANDLE thread;
    HANDLE semaphore;
};
//------------------------------------------------------------------------------
DWORD WINAPI WWaveOutThread(LPVOID arg)
{
    WWaveOut& thiz = *(WWaveOut*)arg;

    waveOutOpen(&thiz.waveOut, WAVE_MAPPER, &thiz.waveFormat, 0, 0, CALLBACK_NULL);

    while (thiz.waveOut)
    {
        if (thiz.cancel)
            break;
        WaitForSingleObject(thiz.semaphore, INFINITE);
        if (thiz.cancel)
            break;

        thiz.waveHeaderIndex++;
        if (thiz.waveHeaderIndex >= _countof(thiz.waveHeader))
            thiz.waveHeaderIndex = 0;

        size_t outputSize = thiz.bufferSize;
        short* output = (short*)thiz.bufferQueue.Address(thiz.bufferQueuePick, &outputSize);
        scaleWaveform(output, outputSize, thiz.volume);

        thiz.waveHeader[thiz.waveHeaderIndex].lpData = (LPSTR)output;
        thiz.waveHeader[thiz.waveHeaderIndex].dwBufferLength = outputSize;
        thiz.bufferQueuePick += outputSize;

        if (thiz.waveHeader[thiz.waveHeaderIndex].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(thiz.waveOut, &thiz.waveHeader[thiz.waveHeaderIndex], sizeof(WAVEHDR));
        waveOutPrepareHeader(thiz.waveOut, &thiz.waveHeader[thiz.waveHeaderIndex], sizeof(WAVEHDR));
        waveOutWrite(thiz.waveOut, &thiz.waveHeader[thiz.waveHeaderIndex], sizeof(WAVEHDR));
    }

    if (thiz.waveOut)
    {
        waveOutPause(thiz.waveOut);
        for (int i = 0; i < _countof(thiz.waveHeader); ++i)
        {
            if (thiz.waveHeader[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(thiz.waveOut, &thiz.waveHeader[i], sizeof(WAVEHDR));
            waveOutClose(thiz.waveOut);
        }
        waveOutClose(thiz.waveOut);
        thiz.waveOut = nullptr;
    }

    return 0;
}
//------------------------------------------------------------------------------
struct WWaveOut* WWaveOutCreate(int channel, int sampleRate, int secondPerBuffer, bool record)
{
    WWaveOut* waveOut = nullptr;

    switch (0) case 0: default:
    {
        if (channel == 0)
            break;
        if (sampleRate == 0)
            break;

        waveOut = new WWaveOut{};
        if (waveOut == nullptr)
            break;
        WWaveOut& thiz = (*waveOut);

        if (thiz.bufferQueue.Startup(sampleRate * sizeof(int16_t) * channel * secondPerBuffer) == false)
            break;

        thiz.waveFormat.nSamplesPerSec = sampleRate;
        thiz.waveFormat.wBitsPerSample = 16;
        thiz.waveFormat.nChannels = channel;
        thiz.waveFormat.cbSize = 0;
        thiz.waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        thiz.waveFormat.nBlockAlign = (thiz.waveFormat.wBitsPerSample * thiz.waveFormat.nChannels) >> 3;
        thiz.waveFormat.nAvgBytesPerSec = thiz.waveFormat.nBlockAlign * thiz.waveFormat.nSamplesPerSec;
        if (waveOutOpen(nullptr, WAVE_MAPPER, &thiz.waveFormat, 0, 0, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
            break;

        thiz.semaphore = CreateSemaphoreA(nullptr, 0, LONG_MAX, nullptr);
        if (thiz.semaphore == nullptr)
            break;

        thiz.channel = channel;
        thiz.sampleRate = sampleRate;
        thiz.bytesPerSecond = sampleRate * sizeof(int16_t) * channel;
        thiz.volume = 0.0f;
        thiz.record = record;

        return waveOut;
    }
    WWaveOutDestroy(waveOut);

    return nullptr;
}
//------------------------------------------------------------------------------
void WWaveOutDestroy(struct WWaveOut* waveOut)
{
    if (waveOut == nullptr)
        return;
    WWaveOut& thiz = (*waveOut);

    thiz.cancel = true;

    if (thiz.thread)
    {
        ReleaseSemaphore(thiz.semaphore, 1, nullptr);
        WaitForSingleObject(thiz.thread, INFINITE);
        CloseHandle(thiz.thread);
    }
    else
    {
        WWaveOutThread(&thiz);
    }

    delete& thiz;
}
//------------------------------------------------------------------------------
uint64_t WWaveOutQueue(struct WWaveOut* waveOut, uint64_t now, uint64_t timestamp, int64_t adjust, const void* buffer, size_t bufferSize)
{
    if (waveOut == nullptr)
        return 0;
    WWaveOut& thiz = (*waveOut);
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
        thiz.bufferQueuePick = (now + adjust) * thiz.bytesPerSecond / 1000000 - bufferSize;
        thiz.bufferQueuePick = thiz.bufferQueuePick - (thiz.bufferQueuePick % bufferSize);
        thiz.bufferQueuePickAdjust = adjust;

        if (thiz.thread == nullptr)
            thiz.thread = CreateThread(nullptr, 0, WWaveOutThread, &thiz, 0, nullptr);
    }

    ReleaseSemaphore(thiz.semaphore, 1, nullptr);
    return thiz.bufferQueuePick * 1000000 / thiz.bytesPerSecond;
}
//------------------------------------------------------------------------------
void WWaveOutVolume(struct WWaveOut* waveOut, float volume)
{
    if (waveOut == nullptr)
        return;
    WWaveOut& thiz = (*waveOut);

    thiz.volume = volume;
}
//------------------------------------------------------------------------------
