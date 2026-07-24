/******************************************************************************\
* Audio Overdrive (tanh soft-clip + tone LPF + level)                         *
\******************************************************************************/
/*
    jamsoul 自有效果器：过载。
    信号链位置：boost 后、混响前（失真位预留中间）。
    - Drive: 0~+18 dB 前置增益，推信号进软 clip。
    - 软 clip: y = tanh(gain*x)。gain=1 时小信号单位增益近直通、大信号软压；
      gain↑ 小信号放大、更早饱和（过载特性）。
    - Tone: 一阶低通，截止 800Hz~12kHz（暗↔亮），衰减过载产生的高频谐波。
    - Level: 输出音量补偿。
    无过采样（软 clip 高次谐波衰减快，44.1kHz 够）。
    照 jamulus 抄 STK JCRev 之先例自行实现，算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"

class CAudioOverdrive
{
public:
    CAudioOverdrive() : iStereoBlockSizeSam ( 0 ), fSampleRate ( 48000.0f ), fToneStateL ( 0 ), fToneStateR ( 0 ) {}

    // 签名照 CAudioBoost/CAudioReverb::Init，便于效果器链统一调用
    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate );

    // fDriveNorm/fLevelNorm/fToneNorm: 归一化 [0,1]。
    // bypass 与 drive=0 由调用方判断（client 信号链 if enabled && drive!=0）。
    void Process ( CVector<int16_t>& vecsStereoInOut,
                   const float fDriveNorm, const float fLevelNorm, const float fToneNorm );

protected:
    int   iStereoBlockSizeSam;
    float fSampleRate;
    float fToneStateL; // 一阶 LPF 状态（左）
    float fToneStateR; // 一阶 LPF 状态（右）
};
