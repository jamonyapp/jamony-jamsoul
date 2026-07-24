/******************************************************************************\
* Audio Boost (clean boost with soft-knee limiter)                            *
\******************************************************************************/
/*
    jamsoul 自有效果器：clean boost。

    信号链位置：电平表后、混响前（推前级，饱满信号再进混响）。
    - 0~+18 dB 连续增益（只提升不衰减）。
    - 超过阈值用 soft-knee 软限幅防硬削波：阈值以下完全线性（零染色，
      clean boost 的灵魂），阈值以上软压缩趋近上限，永不硬 clip。

    算法为公开 DSP 知识，照 jamulus 抄 STK JCRev 之先例自行实现。
*/

#pragma once
#include "util.h"

class CAudioBoost
{
public:
    CAudioBoost() : iStereoBlockSizeSam ( 0 ) {}

    // 签名照 CAudioReverb::Init，便于效果器链统一调用。采样率参数当前
    // 不依赖（soft-knee 无记忆），保留供未来效果器扩展。
    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate );

    // fBoostLevelNorm: 归一化增益 [0,1]，内部映射到 0~+18 dB。
    // 0 时调用方应跳过 Process（见 client 信号链），此处再兜底一次。
    void Process ( CVector<int16_t>& vecsStereoInOut, const float fBoostLevelNorm );

protected:
    // soft-knee 软限幅：|x|<=T 线性，|x|>T 软压趋近 1.0
    inline float SoftKnee ( const float x );

    EAudChanConf eAudioChannelConf;
    int          iStereoBlockSizeSam;
};
