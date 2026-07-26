/******************************************************************************\
* Audio Distortion (hard-clip + pre HP + noise gate + post HC + tone, 2x os)   *
\******************************************************************************/
/*
    jamsoul 自有效果器：失真（金属节奏向，Laney 式"切菜"palm mute）。
    信号链位置：过载后、混响前（失真位预留中间）。
    - Drive: 0~+60 dB 前置增益（比过载 +40dB 多 20dB 余量，到过载到不了的猛区）。
    - 预高通: clip 前一阶 high-pass ~60Hz，切 palm mute 低频隆隆 → 制音紧凑有边界
      = "切菜感"关键。60Hz 比 90Hz 少切，保留低频力度。
    - Noise gate: clip 前包络跟随 + 软门限。静音/制音衰减时压底噪（高增益放大底噪
      的固有问题），有信号时全通过。软曲线避免吃音尾、避免开关咔哒。
    - 硬 clip: y = clamp(gain*x, -1, 1)。对称硬削波（整齐），方波化高频谐波丰富。
      谐波远超奈奎斯特 → 混叠严重 → **2x 过采样必须**（治混叠，见 audiooversample.h）。
    - 2x 过采样：硬 clip 在 2x 采样率跑（COversampler2x 上/下采样）。
    - 后高切: clip 后二阶低通 ~6kHz（两个一阶串联，-12dB/oct），去 fizz 刺耳高频，
      金属音色标准做法（保留中频咬合，削掉毛刺）。
    - Tone: 一阶低通，截止 800Hz~12kHz（暗↔亮），用户可调音色。
    - Level: 输出音量补偿。
    全湿串联（整信号过硬 clip，不合并干声，取代式处理）。
    照 jamulus 抄 STK JCRev 之先例自行实现，算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"
#include "audiooversample.h"

class CAudioDistortion
{
public:
    CAudioDistortion() : iStereoBlockSizeSam ( 0 ), iMonoBlockSizeSam ( 0 ),
                         fSampleRate ( 48000.0f ),
                         fHpAlpha ( 0.0f ), fHpStateL ( 0 ), fHpStateR ( 0 ),
                         fGateBeta ( 0.0f ), fGateThr ( 0.0f ), fEnvL ( 0 ), fEnvR ( 0 ),
                         fHcAlpha ( 0.0f ),
                         fHcS1L ( 0 ), fHcS2L ( 0 ), fHcS1R ( 0 ), fHcS2R ( 0 ),
                         fToneStateL ( 0 ), fToneStateR ( 0 ) {}

    // 签名照 CAudioBoost/CAudioOverdrive/CAudioReverb::Init，便于效果器链统一调用
    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate );

    // fDriveNorm/fLevelNorm/fToneNorm: 归一化 [0,1]。
    // bypass 与 drive=0 由调用方判断（client 信号链 if enabled && drive!=0）。
    void Process ( CVector<int16_t>& vecsStereoInOut,
                   const float fDriveNorm, const float fLevelNorm, const float fToneNorm );

protected:
    int            iStereoBlockSizeSam;
    int            iMonoBlockSizeSam;      // = iStereoBlockSizeSam/2, 单路样本数
    float          fSampleRate;
    float          fHpAlpha;               // 预高通一阶系数 (fc~60Hz)
    float          fHpStateL;              // 预高通 LPF 状态（左）, y=x-lpf
    float          fHpStateR;              // 预高通 LPF 状态（右）
    float          fGateBeta;              // noise gate 包络跟随系数 (fc~150Hz)
    float          fGateThr;               // noise gate 软门限阈值
    float          fEnvL;                  // 包络跟随状态（左）
    float          fEnvR;                  // 包络跟随状态（右）
    float          fHcAlpha;               // 后高切一阶系数 (fc~6kHz, 串联两级=二阶)
    float          fHcS1L, fHcS2L;         // 后高切两级 LPF 状态（左）
    float          fHcS1R, fHcS2R;         // 后高切两级 LPF 状态（右）
    float          fToneStateL;            // tone 一阶 LPF 状态（左）
    float          fToneStateR;            // tone 一阶 LPF 状态（右）
    COversampler2x oversamplerL;           // 2x 过采样（左）
    COversampler2x oversamplerR;           // 2x 过采样（右）
    CVector<float> vecfInL, vecfInR;       // 单路输入 float (N)
    CVector<float> vecfWorkL, vecfWorkR;   // 2x 工作缓冲 (2N)
    CVector<float> vecfOutL, vecfOutR;     // 下采样后单路输出 (N)
};
