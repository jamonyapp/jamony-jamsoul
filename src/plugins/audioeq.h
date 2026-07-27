/******************************************************************************\
* Audio EQ (7-band graphic equalizer + input/output level, biquad peaking)     *
\******************************************************************************/
/*
    jamsoul 自有效果器：7 段图形均衡（吉他/bass 兼顾）。
    信号链位置：失真后、混响前（EQ 整形后再进空间类）。
    - 7 段 peaking biquad，频率 50/120/250/500/1.2k/3k/6k Hz：
        50Hz: bass sub / 吉他低 E(82Hz) 以下
        120Hz: bass punch / 吉他厚度
        250Hz: 浑浊区
        500Hz: 低中木感
        1.2k: 中频咬合
        3k: presence
        6k: brightness/air
      覆盖 bass 50Hz sub 到吉他 6kHz air，兼顾两类乐器。
    - Q=1.41（倍频程间隔标准，段间衔接平滑）。
    - 每段 ±12 dB（fader 0..100，中点 50=0dB）。
    - Input level ±12dB：前置增益，给 EQ headroom（distortion 后满信号，EQ 提升会 clip，
      input 降一点防）；也匹配被动吉他 vs 主动 bass 电平差异。
    - Output level ±12dB：EQ 后 makeup 增益补偿。
    - 线性环节，无谐波产生，不需过采样。
    - 每块开始重算 7 段 biquad 系数（RBJ 公式，CPU 微不足道）。
    照 jamulus 抄 STK JCRev 之先例自行实现，算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"
#include "audiobiquad.h"

#define AUD_EQ_BANDS 7
#define AUD_EQ_MAX 100 // 每段/level 0..100, 中点 50 = 0dB

class CAudioEq
{
public:
    CAudioEq() : iStereoBlockSizeSam ( 0 ), iMonoBlockSizeSam ( 0 ), fSampleRate ( 48000.0f ) {}

    // 签名照 CAudioBoost/CAudioOverdrive/CAudioDistortion/CAudioReverb::Init
    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate );

    // fInNorm/fOutNorm: input/output level 归一化 [0,1], 中点 0.5=0dB, 范围 ±12dB
    // fBandNorm[7]: 7 段增益归一化 [0,1], 中点 0.5=0dB, 范围 ±12dB
    // bypass 由调用方判断（client 信号链 if enabled）。
    void Process ( CVector<int16_t>& vecsStereoInOut,
                   const float fInNorm, const float fOutNorm,
                   const float fBandNorm[AUD_EQ_BANDS] );

protected:
    int            iStereoBlockSizeSam;
    int            iMonoBlockSizeSam;      // = iStereoBlockSizeSam/2, 单路样本数
    float          fSampleRate;
    CBiquad        biquadL[AUD_EQ_BANDS];  // 7 段 biquad（左）
    CBiquad        biquadR[AUD_EQ_BANDS];  // 7 段 biquad（右）
    CVector<float> vecfInL, vecfInR;       // 单路工作缓冲 (N)

    // 7 段中心频率 + Q
    static constexpr float fBandFreq[AUD_EQ_BANDS] = { 50.0f, 120.0f, 250.0f, 500.0f, 1200.0f, 3000.0f, 6000.0f };
    static constexpr float fBandQ = 1.41f;
};
