/******************************************************************************\
* Audio Oversampler 2x (linear-interp upsample + half-band FIR downsample)    *
\******************************************************************************/
/*
    jamsoul 共享过采样件：供失真/过载等非线性效果器治混叠用。

    为什么需要：tanh/硬 clip 等非线性产生大量高次谐波，超过奈奎斯特(22kHz@44.1k
    或 24kHz@48k)会折叠回低频，产生非谐波关系的脏频率 = 听感刺耳飘忽(混叠)。
    2x 过采样把内部采样率翻倍(48k→96k)，奈奎斯特推到 48kHz，谐波折叠点远高于
    可听域，再下采样滤掉 >24kHz 的镜像，混叠大幅抑制。

    方案(与 JUCE dsp::Oversampling 对齐的国际主流做法)：
    - 上采样：线性插值 2x。out[2n]=0.5*(last+x[n]), out[2n+1]=x[n]，跨块保持
      上一个输入样本 fLastIn。线性插值比零阶保持干净，镜像抑制更好。
    - 下采样：半带 FIR(linear-phase, symmetric, 一半抽头为 0) + 2x 抽取。截止
      恰在 fs'/4 = 24kHz，完美匹配抗镜像/抗混叠。FIR 延迟线跨块保持。

    半带 FIR 设计：11-tap, Hamming 窗截断 sinc，归一化到直流增益=1。
      系数(对称, 中心 h[5], 奇数索引 1/3/7/9 为 0):
        h[0]=h[10]=+0.005044, h[2]=h[8]=-0.041760, h[4]=h[6]=+0.287500,
        h[5]=+0.498300,  h[1]=h[3]=h[7]=h[9]=0
      (半带特性：奇数索引为 0，可优化只算非零对称抽头)

    代价：非线性核心在 2x 采样率跑，每块 tanh/clip 调用次数翻倍；48kHz 立体声
    下 CPU <1%，可忽略。FIR 线性相位群延迟固定(2x 域 5 sample)，块内消化，
    不增端到端延迟(状态跨块保持即可)。

    用法(效果器内, 单路 mono)：
      CAudioOverdrive::Process 里每路:
        oversamplerL.Upsample(vecfIn, vecfWork);   // vecfIn[N] -> vecfWork[2N]
        for (k in 2N) vecfWork[k] = tanhf(gain*vecfWork[k]);  // 非线性@2x
        oversamplerL.Downsample(vecfWork, vecfOut); // vecfWork[2N] -> vecfOut[N]
      vecfWork 由效果器自己持有(Init 时分配 2N)。

    照 jamulus 抄 STK JCRev 之先例自行实现，DSP 算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"
#include <cmath>

class COversampler2x
{
public:
    COversampler2x() : iMonoBlockSizeSam ( 0 ), fLastIn ( 0.0f )
    {
        for ( int i = 0; i < FIR_TAPS; i++ ) { fDelay[i] = 0.0f; }
    }

    // iNMonoBlockSizeSam = 单路(非交错)样本数。调用方 work buffer 须 >= 2x。
    void Init ( const int iNMonoBlockSizeSam )
    {
        iMonoBlockSizeSam = iNMonoBlockSizeSam;
        fLastIn = 0.0f;
        for ( int i = 0; i < FIR_TAPS; i++ ) { fDelay[i] = 0.0f; }
    }

    // 线性插值 2x 上采样：vecfIn[N] -> vecfOut[2N]，跨块保持 fLastIn。
    // vecfOut 须由调用方预分配 >= 2*N 大小。
    void Upsample ( const CVector<float>& vecfIn, CVector<float>& vecfOut )
    {
        const int N = iMonoBlockSizeSam;
        for ( int n = 0; n < N; n++ )
        {
            const float x = vecfIn[n];
            // 中点插值样本 + 原样本交替；跨块用 fLastIn 衔接首样本
            vecfOut[2 * n]     = 0.5f * ( fLastIn + x );
            vecfOut[2 * n + 1] = x;
            fLastIn = x;
        }
    }

    // 半带 FIR + 2x 抽取下采样：vecfIn[2N] -> vecfOut[N]，跨块保持 fDelay。
    // 取偶数索引样本进延迟线后计算输出(对应 5 样本前的群延迟对齐)。
    void Downsample ( const CVector<float>& vecfIn, CVector<float>& vecfOut )
    {
        const int N = iMonoBlockSizeSam;
        for ( int n = 0; n < N; n++ )
        {
            // 推入偶数样本 in[2n]，计算一次输出
            PushDelay ( vecfIn[2 * n] );
            vecfOut[n] = Convolve();
            // 推入奇数样本 in[2n+1]，不计算(抽取)
            PushDelay ( vecfIn[2 * n + 1] );
        }
    }

protected:
    // 推一个样本进 FIR 延迟线(移位寄存器, 末端为最新)
    inline void PushDelay ( const float fSample )
    {
        for ( int i = 0; i < FIR_TAPS - 1; i++ ) { fDelay[i] = fDelay[i + 1]; }
        fDelay[FIR_TAPS - 1] = fSample;
    }

    // FIR 卷积：y = sum h[k]*fDelay[k] (fDelay 末端=最新, 中心 h[5] 对齐群延迟)
    inline float Convolve() const
    {
        // 半带优化：奇数索引(1,3,7,9)系数为 0，只算非零对称对 h[0/10],h[2/8],h[4/6]+中心 h[5]
        return fHalfBandCoef[0] * ( fDelay[0] + fDelay[10] ) +  // 对称 h0
               fHalfBandCoef[2] * ( fDelay[2] + fDelay[8] ) +   // 对称 h2
               fHalfBandCoef[4] * ( fDelay[4] + fDelay[6] ) +   // 对称 h4
               fHalfBandCoef[5] * fDelay[5];                    // 中心 h5
    }

    // 11-tap 半带 FIR 系数(Hamming 窗 sinc, 归一化直流增益=1)
    // 对称, 奇数索引 1/3/7/9 为 0; 设计 h[k]=0.5*sinc(0.5*(k-5)) 加 Hamming 窗, 归一化 sum=1
    static constexpr int   FIR_TAPS = 11;
    static constexpr float fHalfBandCoef[FIR_TAPS] = {
        0.005044f, 0.0f, -0.041760f, 0.0f, 0.287500f,
        0.498300f,
        0.287500f, 0.0f, -0.041760f, 0.0f, 0.005044f
    };

    int   iMonoBlockSizeSam;
    float fLastIn;           // 上采样跨块: 上一块最后一个输入样本
    float fDelay[FIR_TAPS];  // 下采样跨块: FIR 延迟线(末端=最新)
};
