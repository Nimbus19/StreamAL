//==============================================================================
// Windows WaveOut Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/WWaveOut
//==============================================================================
#include <stdlib.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmeapi.h>
#include "WWaveOut.h"

//------------------------------------------------------------------------------
struct WWaveOut
{
    WAVEFORMATEX waveFormat;
    HWAVEOUT waveOut;
    WAVEHDR waveHeader[8];
    int waveHeaderIndex;

    char* ringBuffer;
    size_t ringBufferSize;
    uint64_t bufferQueueSend;
    uint64_t bufferQueuePick;

    uint32_t channel;
    uint32_t sampleRate;
    uint32_t bytesPerSecond;

    bool sync;
    int bufferSize;

    HANDLE thread;
    HANDLE semaphore;
    bool threadCancel;
};
//------------------------------------------------------------------------------
DWORD WINAPI WWaveOutThread(LPVOID arg)
{
    WWaveOut& thiz = *(WWaveOut*)arg;

    waveOutOpen(&thiz.waveOut, WAVE_MAPPER, &thiz.waveFormat, 0, 0, CALLBACK_NULL);

    while (thiz.waveOut)
    {
        if (thiz.threadCancel)
            break;
        WaitForSingleObject(thiz.semaphore, INFINITE);
        if (thiz.threadCancel)
            break;

        thiz.waveHeaderIndex++;
        if (thiz.waveHeaderIndex >= _countof(thiz.waveHeader))
            thiz.waveHeaderIndex = 0;

        if (thiz.sync)
        {
            if (thiz.bufferQueuePick < thiz.bufferQueueSend - thiz.bytesPerSecond / 2)
                thiz.bufferQueuePick = thiz.bufferQueueSend - thiz.bytesPerSecond / 8;
            else if (thiz.bufferQueuePick > thiz.bufferQueueSend)
                thiz.bufferQueuePick = thiz.bufferQueueSend - thiz.bytesPerSecond / 8;
        }

        int offset = thiz.bufferQueuePick % thiz.ringBufferSize;
        int size = thiz.bufferSize;
        if (size > thiz.ringBufferSize - offset)
        {
            size = thiz.ringBufferSize - offset;
        }

        thiz.waveHeader[thiz.waveHeaderIndex].lpData = thiz.ringBuffer + offset;
        thiz.waveHeader[thiz.waveHeaderIndex].dwBufferLength = size;
        thiz.bufferQueuePick += size;

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
    if (thiz.ringBuffer)
    {
        free(thiz.ringBuffer);
        thiz.ringBuffer = nullptr;
    }

    return 0;
}
//------------------------------------------------------------------------------
struct WWaveOut* WWaveOutCreate(int channel, int sampleRate)
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

        thiz.ringBufferSize = sampleRate * sizeof(int16_t) * channel * 8;
        thiz.ringBuffer = (char*)malloc(thiz.ringBufferSize);
        if (thiz.ringBuffer == nullptr)
            break;
        memset(thiz.ringBuffer, 0, thiz.ringBufferSize);

        thiz.waveFormat.nSamplesPerSec = sampleRate;
        thiz.waveFormat.wBitsPerSample = 16;
        thiz.waveFormat.nChannels = channel;
        thiz.waveFormat.cbSize = 0;
        thiz.waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        thiz.waveFormat.nBlockAlign = (thiz.waveFormat.wBitsPerSample * thiz.waveFormat.nChannels) >> 3;
        thiz.waveFormat.nAvgBytesPerSec = thiz.waveFormat.nBlockAlign * thiz.waveFormat.nSamplesPerSec;
        if (waveOutOpen(nullptr, WAVE_MAPPER, &thiz.waveFormat, 0, 0, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
            break;

        thiz.channel = channel;
        thiz.sampleRate = sampleRate;
        thiz.bytesPerSecond = sampleRate * sizeof(int16_t) * channel;

        thiz.semaphore = CreateSemaphoreA(nullptr, 0, 1, "WWaveOut");
        if (thiz.semaphore == nullptr)
            break;

        thiz.thread = CreateThread(nullptr, 0, WWaveOutThread, &thiz, 0, nullptr);
        if (thiz.thread == nullptr)
            break;

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

    if (thiz.thread)
    {
        thiz.threadCancel = true;
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
void WWaveOutQueue(struct WWaveOut* waveOut, uint64_t timestamp, const void* buffer, size_t bufferSize, bool sync)
{
    if (waveOut == nullptr)
        return;
    WWaveOut& thiz = (*waveOut);

    uint64_t queueOffset = timestamp * thiz.bytesPerSecond / 1000000;

    if (bufferSize)
    {
        queueOffset = queueOffset + bufferSize / 2 - 1;
        queueOffset = queueOffset - queueOffset % bufferSize;
    }

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

    thiz.bufferSize = bufferSize;
    thiz.sync = sync;
    ReleaseSemaphore(thiz.semaphore, 1, nullptr);
}
//------------------------------------------------------------------------------
