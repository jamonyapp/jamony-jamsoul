/******************************************************************************\
* Audio Delay (干净数字延迟 + 反馈 + Level, dry 直通, 立体声)                  *
\******************************************************************************/

#include "audiodelay.h"

void CAudioDelay::Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate )
{
    (void) eNAudioChannelConf;

    iMonoBlockSizeSam = iNStereoBlockSizeSam / 2;
    fSampleRate = static_cast<float> ( iSampleRate );

    // 延迟线容量: fMaxMs + 余量
    const int iMaxDelaySamples = static_cast<int> ( ( fMaxMs + 50.0f ) * fSampleRate / 1000.0f );
    delayL.Init ( iMaxDelaySamples );
    delayR.Init ( iMaxDelaySamples );
}

void CAudioDelay::Process ( CVector<int16_t>& vecsStereoInOut,
                            const float fTimeNorm, const float fFeedbackNorm, const float fLevelNorm )
{
    // Time: fMinMs-fMaxMs → 样本 (float, 分数延迟平滑过渡)
    const float fDelayMs     = fMinMs + fTimeNorm * ( fMaxMs - fMinMs );
    const float fDelaySamples = fDelayMs * fSampleRate / 1000.0f;

    // Feedback: 0-0.95 (防自激)
    const float fFeedback = fFeedbackNorm * 0.95f;

    // Level: 湿声音量, dry 直通
    const float fLevel = fLevelNorm;

    for ( int n = 0; n < iMonoBlockSizeSam; n++ )
    {
        const float xl = vecsStereoInOut[2 * n]     / 32768.0f;
        const float xr = vecsStereoInOut[2 * n + 1] / 32768.0f;

        // 读延迟 (先读后写)
        const float wetL = delayL.Read ( fDelaySamples );
        const float wetR = delayR.Read ( fDelaySamples );

        // 写: dry + feedback * wet (反馈回灌)
        delayL.Write ( xl + fFeedback * wetL );
        delayR.Write ( xr + fFeedback * wetR );

        // 输出: dry 直通 + level * wet
        const float yl = xl + fLevel * wetL;
        const float yr = xr + fLevel * wetR;
        vecsStereoInOut[2 * n]     = Float2Short ( yl * 32768.0f );
        vecsStereoInOut[2 * n + 1] = Float2Short ( yr * 32768.0f );
    }
}
