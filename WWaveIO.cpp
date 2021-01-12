//==============================================================================
// Windows WaveIO Wrapper
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#include <stdlib.h>
#include <math.h>
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "winmm.lib")
#include <windows.h>
#include <mmeapi.h>
#include "RingBuffer.h"
#include "Waveform.h"
#include "WWaveIO.h"

//------------------------------------------------------------------------------
struct WWaveIO
{
    WAVEFORMATEX waveFormat;
    HWAVEIN waveIn;
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
    bool go;
    bool record;

    HANDLE thread;
    HANDLE semaphore;

    int bufferSize;
    short temp[8192];
};
//------------------------------------------------------------------------------
static DWORD WINAPI WWaveOutThread(LPVOID arg)
{
    WWaveIO& thiz = *(WWaveIO*)arg;

    if (thiz.cancel == false)
    {
        waveOutOpen(&thiz.waveOut, WAVE_MAPPER, &thiz.waveFormat, 0, 0, CALLBACK_NULL);
    }

    while (thiz.waveOut)
    {
        if (thiz.cancel)
            break;
        WaitForSingleObject(thiz.semaphore, INFINITE);
        if (thiz.cancel)
            break;

        size_t outputSize = thiz.bufferSize;
        for (int i = 0; i < 2; ++i)
        {
            thiz.waveHeaderIndex++;
            if (thiz.waveHeaderIndex >= _countof(thiz.waveHeader))
                thiz.waveHeaderIndex = 0;

            short* output = (short*)thiz.bufferQueue.Address(thiz.bufferQueuePick, &outputSize);
            scaleWaveform(output, outputSize, thiz.volume);

            thiz.waveHeader[thiz.waveHeaderIndex].lpData = (LPSTR)output;
            thiz.waveHeader[thiz.waveHeaderIndex].dwBufferLength = outputSize;
            if (thiz.go)
            {
                thiz.bufferQueuePick += outputSize;
            }
            else
            {
                memset(output, 0, outputSize);
            }

            if (thiz.waveHeader[thiz.waveHeaderIndex].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(thiz.waveOut, &thiz.waveHeader[thiz.waveHeaderIndex], sizeof(WAVEHDR));
            waveOutPrepareHeader(thiz.waveOut, &thiz.waveHeader[thiz.waveHeaderIndex], sizeof(WAVEHDR));
            waveOutWrite(thiz.waveOut, &thiz.waveHeader[thiz.waveHeaderIndex], sizeof(WAVEHDR));

            outputSize = thiz.bufferSize - outputSize;
            if (outputSize == 0)
                break;
        }
    }

    if (thiz.waveOut)
    {
        waveOutPause(thiz.waveOut);
        for (int i = 0; i < _countof(thiz.waveHeader); ++i)
        {
            if (thiz.waveHeader[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(thiz.waveOut, &thiz.waveHeader[i], sizeof(WAVEHDR));
        }
        waveOutClose(thiz.waveOut);
        thiz.waveOut = nullptr;
    }

    return 0;
}
//------------------------------------------------------------------------------
static DWORD WINAPI WWaveInCallback(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    WWaveIO& thiz = *(WWaveIO*)dwInstance;

    switch (uMsg)
    {
    case WIM_OPEN:
        break;
    case WIM_DATA:
    {
        PWAVEHDR hdr = (PWAVEHDR)dwParam1;

        short* input = (short*)hdr->lpData;
        size_t inputSize = hdr->dwBufferLength;
        scaleWaveform(input, inputSize, thiz.volume);
        thiz.bufferQueueSend += thiz.bufferQueue.Scatter(thiz.bufferQueueSend, input, inputSize);

        waveInAddBuffer(hWaveIn, hdr, sizeof(WAVEHDR));
        break;
    }
    case WIM_CLOSE:
        break;
    default:
        break;
    }

    return 0;
};
//------------------------------------------------------------------------------
static DWORD WINAPI WWaveInThread(LPVOID arg)
{
    WWaveIO& thiz = *(WWaveIO*)arg;

    if (thiz.cancel == false)
    {
        waveInOpen(&thiz.waveIn, WAVE_MAPPER, &thiz.waveFormat, (DWORD_PTR)WWaveInCallback, (DWORD_PTR)&thiz, CALLBACK_FUNCTION);
    }

    if (thiz.waveIn)
    {
        thiz.waveHeader[0] = {};
        thiz.waveHeader[0].lpData = (LPSTR)&thiz.temp[0];
        thiz.waveHeader[0].dwBufferLength = thiz.bufferSize;
        thiz.waveHeader[0].dwLoops = TRUE;

        thiz.waveHeader[1] = {};
        thiz.waveHeader[1].lpData = (LPSTR)&thiz.temp[thiz.bufferSize];
        thiz.waveHeader[1].dwBufferLength = thiz.bufferSize;
        thiz.waveHeader[1].dwLoops = TRUE;

        waveInPrepareHeader(thiz.waveIn, &thiz.waveHeader[0], sizeof(WAVEHDR));
        waveInPrepareHeader(thiz.waveIn, &thiz.waveHeader[1], sizeof(WAVEHDR));
        waveInAddBuffer(thiz.waveIn, &thiz.waveHeader[0], sizeof(WAVEHDR));
        waveInAddBuffer(thiz.waveIn, &thiz.waveHeader[1], sizeof(WAVEHDR));
        waveInStart(thiz.waveIn);
    }

    while (thiz.waveIn)
    {
        if (thiz.cancel)
            break;
        WaitForSingleObject(thiz.semaphore, INFINITE);
        if (thiz.cancel)
            break;
    }

    if (thiz.waveIn)
    {
        waveInStop(thiz.waveIn);
        waveInUnprepareHeader(thiz.waveIn, &thiz.waveHeader[0], sizeof(WAVEHDR));
        waveInUnprepareHeader(thiz.waveIn, &thiz.waveHeader[1], sizeof(WAVEHDR));
        waveInClose(thiz.waveIn);
        thiz.waveIn = nullptr;
    }

    return 0;
}
//------------------------------------------------------------------------------
struct WWaveIO* WWaveIOCreate(int channel, int sampleRate, int secondPerBuffer, bool record)
{
    WWaveIO* waveOut = nullptr;

    switch (0) case 0: default:
    {
        if (channel == 0)
            break;
        if (sampleRate == 0)
            break;

        waveOut = new WWaveIO{};
        if (waveOut == nullptr)
            break;
        WWaveIO& thiz = (*waveOut);

        if (thiz.bufferQueue.Startup(sampleRate * sizeof(int16_t) * channel * secondPerBuffer) == false)
            break;

        thiz.waveFormat.nSamplesPerSec = sampleRate;
        thiz.waveFormat.wBitsPerSample = 16;
        thiz.waveFormat.nChannels = channel;
        thiz.waveFormat.cbSize = 0;
        thiz.waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        thiz.waveFormat.nBlockAlign = (thiz.waveFormat.wBitsPerSample * thiz.waveFormat.nChannels) >> 3;
        thiz.waveFormat.nAvgBytesPerSec = thiz.waveFormat.nBlockAlign * thiz.waveFormat.nSamplesPerSec;
        if (record)
        {
            if (waveInOpen(nullptr, WAVE_MAPPER, &thiz.waveFormat, 0, 0, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
                break;
        }
        else
        {
            if (waveOutOpen(nullptr, WAVE_MAPPER, &thiz.waveFormat, 0, 0, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
                break;
        }

        thiz.semaphore = CreateSemaphoreA(nullptr, 0, LONG_MAX, nullptr);
        if (thiz.semaphore == nullptr)
            break;

        thiz.channel = channel;
        thiz.sampleRate = sampleRate;
        thiz.bytesPerSecond = sampleRate * sizeof(int16_t) * channel;
        thiz.volume = record ? 1.0f : 0.0f;
        thiz.record = record;

        return waveOut;
    }
    WWaveIODestroy(waveOut);

    return nullptr;
}
//------------------------------------------------------------------------------
void WWaveIODestroy(struct WWaveIO* waveOut)
{
    if (waveOut == nullptr)
        return;
    WWaveIO& thiz = (*waveOut);

    thiz.cancel = true;

    if (thiz.thread)
    {
        ReleaseSemaphore(thiz.semaphore, 1, nullptr);
        WaitForSingleObject(thiz.thread, INFINITE);
        CloseHandle(thiz.thread);
    }
    else
    {
        WWaveInThread(&thiz);
        WWaveOutThread(&thiz);
    }

    delete& thiz;
}
//------------------------------------------------------------------------------
uint64_t WWaveIOQueue(struct WWaveIO* waveOut, uint64_t now, uint64_t timestamp, int64_t adjust, const void* buffer, size_t bufferSize, int gap)
{
    if (waveOut == nullptr)
        return 0;
    WWaveIO& thiz = (*waveOut);
    if (bufferSize == 0)
        return 0;
    if (thiz.record)
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

        if (thiz.thread == nullptr)
            thiz.thread = CreateThread(nullptr, 0, WWaveOutThread, &thiz, 0, nullptr);
    }
    else
    {
        thiz.go = true;
    }

    ReleaseSemaphore(thiz.semaphore, 1, nullptr);
    return thiz.bufferQueuePick * 1000000 / thiz.bytesPerSecond;
}
//------------------------------------------------------------------------------
size_t WWaveIODequeue(struct WWaveIO* waveOut, void* buffer, size_t bufferSize, bool drop)
{
    if (waveOut == nullptr)
        return 0;
    WWaveIO& thiz = (*waveOut);
    if (thiz.record == false)
        return 0;

    if (thiz.ready == false)
    {
        thiz.ready = true;

        thiz.bufferSize = bufferSize;
        thiz.bufferQueueSend = 0;

        if (thiz.thread == nullptr)
            thiz.thread = CreateThread(nullptr, 0, WWaveInThread, &thiz, 0, nullptr);
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
void WWaveIOPlay(struct WWaveIO* waveOut)
{
    if (waveOut == nullptr)
        return;
    WWaveIO& thiz = (*waveOut);

    thiz.go = true;
}
//------------------------------------------------------------------------------
void WWaveIOStop(struct WWaveIO* waveOut)
{
    if (waveOut == nullptr)
        return;
    WWaveIO& thiz = (*waveOut);

    thiz.go = false;
}
//------------------------------------------------------------------------------
void WWaveIOPause(struct WWaveIO* waveOut)
{
    if (waveOut == nullptr)
        return;
    WWaveIO& thiz = (*waveOut);

    thiz.go = false;
}
//------------------------------------------------------------------------------
void WWaveIOReset(struct WWaveIO* waveOut)
{
    if (waveOut == nullptr)
        return;
    WWaveIO& thiz = (*waveOut);

    thiz.ready = false;
    thiz.go = false;
}
//------------------------------------------------------------------------------
void WWaveIOVolume(struct WWaveIO* waveOut, float volume)
{
    if (waveOut == nullptr)
        return;
    WWaveIO& thiz = (*waveOut);

    thiz.volume = volume;
}
//------------------------------------------------------------------------------
