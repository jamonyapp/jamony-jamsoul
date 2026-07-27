/******************************************************************************\
* Audio Chorus (单 delay line + sine LFO + BBD 味 LPF + Mix, 立体声)           *
\******************************************************************************/
/*
    jamsoul 自有效果器：合唱（Analogman Bi Chorus 模拟味向）。
    信号链位置：EQ 后、延迟前（调制类）。
    - 延迟线 + sine LFO 调制延迟时间 → 延迟周期性微变 → 音高周期性微漂 →
      "多人同奏微微走调叠加"的厚度感。
    - 中心延迟 20ms，深度 0-15ms（LFO 调制幅度）。
    - L/R 立体声：两套延迟线，LFO 相位差 90°（轻立体感，不抢戏）。
    - BBD 味：输出过一阶 LPF（~3kHz），模拟 BBD 芯片高频衰减，暗暖模拟味
      （Analogman/CE-2 那种老味，区别于干净数字合唱）。
    - Rate: 0.1-5 Hz（LFO 速率）。Depth: 0-15ms（调制深度）。Mix: 0-100%（干湿）。
    分数延迟读取（CDelayLine 线性插值），LFO 调制无 click。
    照 jamulus 抄 STK JCRev 之先例自行实现，算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"
#include "audiodelayline.h"
#include <cmath>

#define AUD_CHORUS_MAX 100

class CAudioChorus
{
public:
    CAudioChorus() : iMonoBlockSizeSam ( 0 ), fSampleRate ( 48000.0f ),
                     fPhaseL ( 0.0f ), fPhaseR ( 1.5707963f ),
                     fBbdStateL ( 0.0f ), fBbdStateR ( 0.0f ) {}

    // 签名照 CAudioBoost/CAudioOverdrive/CAudioDistortion/CAudioEq/CAudioReverb::Init
    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate );

    // fRateNorm/fDepthNorm/fMixNorm: 归一化 [0,1]
    // bypass 由调用方判断（client 信号链 if enabled）。
    void Process ( CVector<int16_t>& vecsStereoInOut,
                   const float fRateNorm, const float fDepthNorm, const float fMixNorm );

protected:
    int        iMonoBlockSizeSam;
    float      fSampleRate;
    CDelayLine delayL, delayR;     // L/R 延迟线
    float      fPhaseL, fPhaseR;   // LFO 相位（L/R 相位差 π/2）
    float      fBbdStateL, fBbdStateR; // BBD 味一阶 LPF 状态

    static constexpr float fCenterMs     = 20.0f;  // 中心延迟
    static constexpr float fMaxDepthMs   = 5.0f;   // 最大调制深度 (合唱 1-5ms, 太大变颤音)
    static constexpr float fBbdCutoffHz  = 3000.0f; // BBD 高频衰减截止
};
