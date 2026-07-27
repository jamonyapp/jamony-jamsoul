/******************************************************************************\
* Audio Delay (干净数字延迟 + 反馈 + Level, dry 直通, 立体声)                  *
\******************************************************************************/
/*
    jamsoul 自有效果器：延迟（干净数字向，仿 Boss DD-3 / TC Flashback 数字模式）。
    信号链位置：合唱后、混响前（时间类）。
    - 延迟线 + 反馈：输出回灌输入做多次回声。反馈 0-0.95（防自激）。
    - 干净数字：无调制、无高频退化，回声清晰（区别于磁带/模拟 BBD 味）。
    - Level 控制：dry 信号直通全量，Level 只调湿声（延迟回声）音量。
      output = dry + level * wet （非 Mix blend，仿 Flashback 的 Level 模式）。
    - Time: 50-600ms。Feedback: 0-95%。Level: 0-100%。
    - 分数延迟读取（CDelayLine 线性插值），Time 旋钮变化时延迟长度平滑过渡无 click。
    - 立体声：L/R 独立延迟线，同延迟时间。
    照 jamulus 抄 STK JCRev 之先例自行实现，算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"
#include "audiodelayline.h"

#define AUD_DELAY_MAX 100

class CAudioDelay
{
public:
    CAudioDelay() : iMonoBlockSizeSam ( 0 ), fSampleRate ( 48000.0f ) {}

    // 签名照其他效果器 Init
    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate );

    // fTimeNorm/fFeedbackNorm/fLevelNorm: 归一化 [0,1]
    // bypass 由调用方判断（client 信号链 if enabled）。
    void Process ( CVector<int16_t>& vecsStereoInOut,
                   const float fTimeNorm, const float fFeedbackNorm, const float fLevelNorm );

protected:
    int        iMonoBlockSizeSam;
    float      fSampleRate;
    CDelayLine delayL, delayR;

    static constexpr float fMinMs = 50.0f;   // 最小延迟
    static constexpr float fMaxMs = 600.0f;  // 最大延迟
};
