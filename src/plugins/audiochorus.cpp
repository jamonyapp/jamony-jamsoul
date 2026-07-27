/******************************************************************************\
* Audio Chorus (单 delay line + sine LFO + BBD 味 LPF + Mix, 立体声)           *
\******************************************************************************/

#include "audiochorus.h"
#include <cmath>

void CAudioChorus::Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate )
{
    (void) eNAudioChannelConf; // 两路独立处理

    iMonoBlockSizeSam = iNStereoBlockSizeSam / 2;
    fSampleRate = static_cast<float> ( iSampleRate );

    // 延迟线容量: center(20ms) + depth(15ms) + 余量(15ms) = 50ms
    const int iMaxDelaySamples = static_cast<int> ( 50.0f * fSampleRate / 1000.0f );
    delayL.Init ( iMaxDelaySamples );
    delayR.Init ( iMaxDelaySamples );

    fPhaseL = 0.0f;
    fPhaseR = 3.14159265f / 2.0f; // L/R 相位差 90°
    fBbdStateL = 0.0f;
    fBbdStateR = 0.0f;
}

void CAudioChorus::Process ( CVector<int16_t>& vecsStereoInOut,
                             const float fRateNorm, const float fDepthNorm, const float fMixNorm )
{
    // Rate: 0.1-2 Hz (合唱慢调制, >2Hz 进颤音区会变鬼叫)
    const float fRateHz   = 0.1f + fRateNorm * 1.9f;
    const float fPhaseInc = 2.0f * 3.14159265f * fRateHz / fSampleRate;

    // Depth: 0-fMaxDepthMs → 样本
    const float fDepthSamples  = fDepthNorm * ( fMaxDepthMs * fSampleRate / 1000.0f );
    const float fCenterSamples = fCenterMs * fSampleRate / 1000.0f;

    // BBD 味一阶 LPF 系数
    const float fBbdAlpha = 1.0f - expf ( -2.0f * 3.14159265f * fBbdCutoffHz / fSampleRate );

    const float fMix = fMixNorm;

    for ( int n = 0; n < iMonoBlockSizeSam; n++ )
    {
        const float xl = vecsStereoInOut[2 * n]     / 32768.0f;
        const float xr = vecsStereoInOut[2 * n + 1] / 32768.0f;

        // LFO (sine), L/R 相位差 90°
        const float lfoL = sinf ( fPhaseL );
        const float lfoR = sinf ( fPhaseR );
        const float fDelayL = fCenterSamples + fDepthSamples * lfoL;
        const float fDelayR = fCenterSamples + fDepthSamples * lfoR;

        // 读延迟 (先读后写)
        float wetL = delayL.Read ( fDelayL );
        float wetR = delayR.Read ( fDelayR );
        delayL.Write ( xl );
        delayR.Write ( xr );

        // BBD 味 LPF (高频衰减, 模拟 BBD 暗暖)
        fBbdStateL += fBbdAlpha * ( wetL - fBbdStateL );
        wetL = fBbdStateL;
        fBbdStateR += fBbdAlpha * ( wetR - fBbdStateR );
        wetR = fBbdStateR;

        // Mix
        const float yl = xl * ( 1.0f - fMix ) + wetL * fMix;
        const float yr = xr * ( 1.0f - fMix ) + wetR * fMix;
        vecsStereoInOut[2 * n]     = Float2Short ( yl * 32768.0f );
        vecsStereoInOut[2 * n + 1] = Float2Short ( yr * 32768.0f );

        // LFO 相位累加 (L 主, R = L + π/2 保持恒定相位差)
        fPhaseL += fPhaseInc;
        if ( fPhaseL > 6.2831853f ) { fPhaseL -= 6.2831853f; }
        fPhaseR = fPhaseL + 1.5707963f;
        if ( fPhaseR > 6.2831853f ) { fPhaseR -= 6.2831853f; }
    }
}
