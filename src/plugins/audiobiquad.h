/******************************************************************************\
* Audio Biquad (RBJ cookbook peaking EQ, Direct Form II Transposed)            *
\******************************************************************************/
/*
    jamsoul 共享 biquad 滤波器：供 EQ 等需要频段塑形的效果器用。

    RBJ Audio EQ Cookbook（Robert Bristow-Johnson, 1994）是数字 EQ 行业标准。
    这里实现 peaking EQ（峰/谷，提升或削减某中心频率，Q 控制带宽）：
      A = 10^(gainDb/40)
      w0 = 2π·freq/fs,  alpha = sin(w0)/(2·Q)
      b0 = (1+αA)/a0, b1 = -2cos(w0)/a0, b2 = (1-αA)/a0
      a1 = -2cos(w0)/a0, a2 = (1-α/A)/a0,  a0 = 1+α/A
    实现 Direct Form II Transposed（数值最稳定，低频高 Q 不爆）。

    用法：每个 biquad = 一段 EQ。SetPeak() 设系数，Process() 逐样本处理。
    系数依赖 (fs, freq, gainDb, Q)，旋钮变化时重算（每块一次足够，CPU 微不足道）。
    0dB 增益时退化为近似单位增益（信号几乎直通）。

    照 jamulus 抄 STK JCRev 之先例自行实现，DSP 算法公开，AGPL 无障碍。
*/

#pragma once
#include <cmath>

class CBiquad
{
public:
    CBiquad() : b0 ( 1.0f ), b1 ( 0.0f ), b2 ( 0.0f ), a1 ( 0.0f ), a2 ( 0.0f ), z1 ( 0.0f ), z2 ( 0.0f ) {}

    void Reset() { z1 = 0.0f; z2 = 0.0f; }

    // RBJ peaking EQ: peakGainDb @ freq Hz, Q. fSampleRate=采样率
    void SetPeak ( const float fSampleRate, const float fFreq, const float fPeakGainDb, const float fQ )
    {
        const float A     = powf ( 10.0f, fPeakGainDb / 40.0f );
        const float w0    = 2.0f * 3.14159265f * fFreq / fSampleRate;
        const float cosw0 = cosf ( w0 );
        const float sinw0 = sinf ( w0 );
        const float alpha = sinw0 / ( 2.0f * fQ );

        const float a0 = 1.0f + alpha / A;

        b0 = ( 1.0f + alpha * A ) / a0;
        b1 = ( -2.0f * cosw0 ) / a0;
        b2 = ( 1.0f - alpha * A ) / a0;
        a1 = ( -2.0f * cosw0 ) / a0;
        a2 = ( 1.0f - alpha / A ) / a0;
    }

    // Direct Form II Transposed
    inline float Process ( const float x )
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    float b0, b1, b2, a1, a2; // 分子/分母系数（a0 已归一化为 1）
    float z1, z2;             // 状态（DF II Transposed 延迟线）
};
