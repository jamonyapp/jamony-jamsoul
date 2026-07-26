/******************************************************************************\
* Audio Overdrive (tanh soft-clip + tone LPF + level, 2x oversampled)         *
\******************************************************************************/
/*
    jamsoul 自有效果器：过载。
    信号链位置：boost 后、混响前（失真位预留中间）。
    - Drive: 0~+40 dB 前置增益，推信号进软 clip。覆盖主流过载踏板 TS9~OCD/SD-1。
    - 软 clip: y = tanh(gain*x)。gain=1 时小信号单位增益近直通、大信号软压；
      gain↑ 小信号放大、更早饱和（过载特性）。
    - 2x 过采样：tanh 非线性在 2x 采样率跑（COversampler2x 上/下采样），治高 drive
      下高次谐波折叠回低频的混叠（刺耳飘忽根因）。详见 audiooversample.h。
    - Tone: 一阶低通，截止 800Hz~12kHz（暗↔亮），衰减过载产生的高频谐波。
    - Level: 输出音量补偿。
    全湿串联（整信号过 tanh，不合并干声，TS9/SD-1/OCD 标准取代式处理）。
    照 jamulus 抄 STK JCRev 之先例自行实现，算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"
#include "audiooversample.h"

class CAudioOverdrive
{
public:
    CAudioOverdrive() : iStereoBlockSizeSam ( 0 ), iMonoBlockSizeSam ( 0 ),
                        fSampleRate ( 48000.0f ), fToneStateL ( 0 ), fToneStateR ( 0 ) {}

    // 签名照 CAudioBoost/CAudioReverb::Init，便于效果器链统一调用
    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate );

    // fDriveNorm/fLevelNorm/fToneNorm: 归一化 [0,1]。
    // bypass 与 drive=0 由调用方判断（client 信号链 if enabled && drive!=0）。
    void Process ( CVector<int16_t>& vecsStereoInOut,
                   const float fDriveNorm, const float fLevelNorm, const float fToneNorm );

protected:
    int            iStereoBlockSizeSam;
    int            iMonoBlockSizeSam;      // = iStereoBlockSizeSam/2, 单路样本数
    float          fSampleRate;
    float          fToneStateL;            // 一阶 LPF 状态（左）
    float          fToneStateR;            // 一阶 LPF 状态（右）
    COversampler2x oversamplerL;           // 2x 过采样（左）
    COversampler2x oversamplerR;           // 2x 过采样（右）
    CVector<float> vecfInL, vecfInR;       // 单路输入 float (N)
    CVector<float> vecfWorkL, vecfWorkR;   // 2x 工作缓冲 (2N)
    CVector<float> vecfOutL, vecfOutR;     // 下采样后单路输出 (N)
};
