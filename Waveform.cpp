//==============================================================================
// Waveform
//
// Copyright (c) 2020 TAiGA
// https://github.com/metarutaiga/StreamAL
//==============================================================================
#if defined(__ARM_NEON__) || defined(__ARM_NEON) || defined(_M_ARM) || defined(_M_ARM64)
#   include <arm_neon.h>
#elif defined(_M_IX86) || defined(_M_AMD64) || defined(__i386__) || defined(__amd64__)
#   include <immintrin.h>
#endif
#include <limits.h>
#include <string.h>
#include "Waveform.h"

//------------------------------------------------------------------------------
void scaleWaveform(int16_t* waveform, size_t count, float scale)
{
    if (scale == 1.0f)
        return;
    if (scale <= 0.0f)
    {
        memset(waveform, 0, count);
        return;
    }

#if defined(__ARM_NEON__) || defined(__ARM_NEON) || defined(_M_ARM) || defined(_M_ARM64)
    float32x4_t vScale = vdupq_n_f32(scale);
    for (size_t i = 0, size = count / sizeof(int16_t); i < size; i += 4)
    {
        int16x4_t s16 = vld1_s16(waveform + i);
        int32x4_t s32 = vmovl_s16(s16);
        float32x4_t f32 = vcvtq_f32_s32(s32);
        f32 = vmulq_f32(f32, vScale);
        s32 = vcvtq_s32_f32(f32);
        s16 = vqmovn_s32(s32);
        vst1_s16(waveform + i, s16);
    }
#elif defined(_M_IX86) || defined(_M_AMD64) || defined(__i386__) || defined(__amd64__)
    __m128 vScale = _mm_set1_ps(scale);
    for (size_t i = 0, size = count / sizeof(int16_t); i < size; i += 4)
    {
        __m128i s16 = _mm_loadu_si64(waveform + i);
        __m128i s32 = _mm_srai_epi32(_mm_unpacklo_epi16(s16, s16), 16);
        __m128 f32 = _mm_cvtepi32_ps(s32);
        f32 = _mm_mul_ps(f32, vScale);
        s32 = _mm_cvtps_epi32(f32);
        s16 = _mm_packs_epi32(s32, s32);
        _mm_storeu_si64(waveform + i, s16);
    }
#else
    for (size_t i = 0, size = count / sizeof(int16_t); i < size; ++i)
    {
        float scaled = waveform[i] * scale;
        waveform[i] = fminf(fmaxf(scaled, SHRT_MIN), SHRT_MAX);
    }
#endif
}
//------------------------------------------------------------------------------
