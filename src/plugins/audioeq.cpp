/******************************************************************************\
* Audio EQ (7-band graphic equalizer + input/output level, biquad peaking)     *
\******************************************************************************/

#include "audioeq.h"
#include <cmath>

void CAudioEq::Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate )
{
    (void) eNAudioChannelConf; // 两路独立处理，声道配置不影响

    iStereoBlockSizeSam = iNStereoBlockSizeSam;
    iMonoBlockSizeSam   = iNStereoBlockSizeSam / 2;
    fSampleRate = static_cast<float> ( iSampleRate );

    for ( int i = 0; i < AUD_EQ_BANDS; i++ ) { biquadL[i].Reset(); biquadR[i].Reset(); }

    vecfInL.Init ( iMonoBlockSizeSam );
    vecfInR.Init ( iMonoBlockSizeSam );
}

void CAudioEq::Process ( CVector<int16_t>& vecsStereoInOut,
                         const float fInNorm, const float fOutNorm,
                         const float fBandNorm[AUD_EQ_BANDS] )
{
    // input/output level: 归一化 0.5=0dB, 范围 ±12dB
    const float fInGainDb  = ( fInNorm  - 0.5f ) * 24.0f;
    const float fOutGainDb = ( fOutNorm - 0.5f ) * 24.0f;
    const float fInGain  = powf ( 10.0f, fInGainDb  / 20.0f );
    const float fOutGain = powf ( 10.0f, fOutGainDb / 20.0f );

    // 每块重算 7 段 biquad 系数（旋钮变化即反映，每块一次 CPU 微不足道）
    for ( int i = 0; i < AUD_EQ_BANDS; i++ )
    {
        const float fBandDb = ( fBandNorm[i] - 0.5f ) * 24.0f; // ±12dB
        biquadL[i].SetPeak ( fSampleRate, fBandFreq[i], fBandDb, fBandQ );
        biquadR[i].SetPeak ( fSampleRate, fBandFreq[i], fBandDb, fBandQ );
    }

    const int N = iMonoBlockSizeSam;

    // 拆 L/R 交错 int16 → 单路 float，并乘 input gain
    for ( int n = 0; n < N; n++ )
    {
        vecfInL[n] = ( vecsStereoInOut[2 * n]     / 32768.0f ) * fInGain;
        vecfInR[n] = ( vecsStereoInOut[2 * n + 1] / 32768.0f ) * fInGain;
    }

    // 7 段 biquad 串联（每段逐样本处理）
    for ( int i = 0; i < AUD_EQ_BANDS; i++ )
    {
        for ( int n = 0; n < N; n++ )
        {
            vecfInL[n] = biquadL[i].Process ( vecfInL[n] );
            vecfInR[n] = biquadR[i].Process ( vecfInR[n] );
        }
    }

    // output gain + 写回交错 int16
    for ( int n = 0; n < N; n++ )
    {
        vecsStereoInOut[2 * n]     = Float2Short ( vecfInL[n] * fOutGain * 32768.0f );
        vecsStereoInOut[2 * n + 1] = Float2Short ( vecfInR[n] * fOutGain * 32768.0f );
    }
}
