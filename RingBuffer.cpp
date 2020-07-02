//==============================================================================
// RingBuffer
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/RingBuffer
//==============================================================================
#include <memory>
#include "RingBuffer.h"

//------------------------------------------------------------------------------
RingBuffer::RingBuffer()
{
    RingBuffer& thiz = (*this);

    thiz.buffer = nullptr;
    thiz.bufferSize = 0;
}
//------------------------------------------------------------------------------
RingBuffer::~RingBuffer()
{
    Shutdown();
}
//------------------------------------------------------------------------------
bool RingBuffer::Startup(size_t size)
{
    RingBuffer& thiz = (*this);

    thiz.buffer = (char*)realloc(thiz.buffer, size);
    if (thiz.buffer == nullptr)
        return false;
    thiz.bufferSize = size;

    memset(thiz.buffer, 0, size);
    return true;
}
//------------------------------------------------------------------------------
void RingBuffer::Shutdown()
{
    RingBuffer& thiz = (*this);

    free(thiz.buffer);
    thiz.buffer = nullptr;
    thiz.bufferSize = 0;
}
//------------------------------------------------------------------------------
uint64_t RingBuffer::Gather(uint64_t index, void* data, size_t dataSize, bool clear)
{
    RingBuffer& thiz = (*this);

    uint64_t offset = index % thiz.bufferSize;
    uint64_t size = dataSize;
    if (size > thiz.bufferSize - offset)
    {
        size = thiz.bufferSize - offset;
        memcpy(data, thiz.buffer + offset, size);
        if (clear)
        {
            memset(thiz.buffer + offset, 0, size);
        }
        index += size;

        data = (char*)data + size;
        offset = index % thiz.bufferSize;
        size = dataSize - size;
    }
    memcpy(data, thiz.buffer + offset, size);
    if (clear)
    {
        memset(thiz.buffer + offset, 0, size);
    }

    return dataSize;
}
//------------------------------------------------------------------------------
uint64_t RingBuffer::Scatter(uint64_t index, const void* data, size_t dataSize)
{
    RingBuffer& thiz = (*this);

    uint64_t offset = index % thiz.bufferSize;
    uint64_t size = dataSize;
    if (size > thiz.bufferSize - offset)
    {
        size = thiz.bufferSize - offset;
        memcpy(thiz.buffer + offset, data, size);
        index += size;

        data = (char*)data + size;
        offset = index % thiz.bufferSize;
        size = dataSize - size;
    }
    memcpy(thiz.buffer + offset, data, size);

    return dataSize;
}
//------------------------------------------------------------------------------
