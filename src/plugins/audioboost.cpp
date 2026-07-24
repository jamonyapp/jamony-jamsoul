/******************************************************************************\
* Audio Boost (clean boost with soft-knee limiter)                            *
\******************************************************************************/

#include "audioboost.h"

void CAudioBoost::Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate )
{
    // store parameters
    eAudioChannelConf   = eNAudioChannelConf;
    iStereoBlockSizeSam = iNStereoBlockSizeSam;

    // soft-knee is stateless (no delay lines), sample rate not needed;
    // kept to match the effect Init signature for uniform call-site usage.
    (void) iSampleRate;
}

float CAudioBoost::SoftKnee ( const float x )
{
    // threshold below which the signal passes through untouched (zero coloration)
    const float T = 0.95f;

    float ax = fabsf ( x );

    if ( ax <= T )
    {
        // pure linear region — clean boost stays transparent
        return x;
    }

    // soft-knee region: compress gently toward the ceiling (1.0), never hard clip
    const float over = ( ax - T ) / ( 1.0f - T );          // 0..inf
    const float y = T + ( 1.0f - T ) * tanhf ( over );     // asymptote -> 1.0

    return copysignf ( y, x );
}

void CAudioBoost::Process ( CVector<int16_t>& vecsStereoInOut, const float fBoostLevelNorm )
{
    // 0 dB -> no boost, bail out (caller also guards with iBoostLevel != 0)
    if ( fBoostLevelNorm <= 0.0f )
    {
        return;
    }

    // map normalized [0,1] to 0..+18 dB, then to linear gain
    const float fBoostDb = fBoostLevelNorm * 18.0f;
    const float fGain = powf ( 10.0f, fBoostDb / 20.0f );

    // two independent channels (no cross-channel mix): boost is a per-channel
    // gain, safe for both stereo and mono channel configurations.
    for ( int i = 0; i < iStereoBlockSizeSam; i += 2 )
    {
        float xl = vecsStereoInOut[i] / 32768.0f * fGain;
        float xr = vecsStereoInOut[i + 1] / 32768.0f * fGain;

        vecsStereoInOut[i]     = Float2Short ( SoftKnee ( xl ) * 32768.0f );
        vecsStereoInOut[i + 1] = Float2Short ( SoftKnee ( xr ) * 32768.0f );
    }
}
