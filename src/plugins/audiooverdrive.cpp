/******************************************************************************\
* Audio Overdrive (tanh soft-clip + tone LPF + level, 2x oversampled)         *
\******************************************************************************/

#include "audiooverdrive.h"
#include <cmath>

void CAudioOverdrive::Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate )
{
    (void) eNAudioChannelConf; // 两路独立处理，声道配置不影响

    iStereoBlockSizeSam = iNStereoBlockSizeSam;
    iMonoBlockSizeSam   = iNStereoBlockSizeSam / 2;
    fSampleRate = static_cast<float> ( iSampleRate );

    fToneStateL = 0.0f;
    fToneStateR = 0.0f;

    // 过采样器 + 工作缓冲（单路 N → 2N → N）
    const int iTwoN = iMonoBlockSizeSam * 2;
    oversamplerL.Init ( iMonoBlockSizeSam );
    oversamplerR.Init ( iMonoBlockSizeSam );
    vecfInL.Init ( iMonoBlockSizeSam );
    vecfInR.Init ( iMonoBlockSizeSam );
    vecfWorkL.Init ( iTwoN );
    vecfWorkR.Init ( iTwoN );
    vecfOutL.Init ( iMonoBlockSizeSam );
    vecfOutR.Init ( iMonoBlockSizeSam );
}

void CAudioOverdrive::Process ( CVector<int16_t>& vecsStereoInOut,
                                const float fDriveNorm, const float fLevelNorm, const float fToneNorm )
{
    // drive 增益：0~+40 dB（覆盖主流过载踏板 TS9~OCD/SD-1 范围）
    const float fDriveDb = fDriveNorm * 40.0f;
    const float fGain = powf ( 10.0f, fDriveDb / 20.0f );

    // tone 一阶低通：截止 800Hz~12kHz（暗↔亮），在原始采样率跑（线性环节不必过采样）
    const float fc = 800.0f + fToneNorm * 11200.0f;
    const float fAlpha = 1.0f - expf ( -2.0f * 3.14159265f * fc / fSampleRate );

    const int N  = iMonoBlockSizeSam;
    const int N2 = iMonoBlockSizeSam * 2;

    // ---- 拆 L/R 交错 int16 → 单路 float ----
    for ( int n = 0; n < N; n++ )
    {
        vecfInL[n] = vecsStereoInOut[2 * n]     / 32768.0f;
        vecfInR[n] = vecsStereoInOut[2 * n + 1] / 32768.0f;
    }

    // ---- L: 2x 上采样 → tanh 软 clip @2x → 下采样 ----
    oversamplerL.Upsample ( vecfInL, vecfWorkL );
    for ( int k = 0; k < N2; k++ ) { vecfWorkL[k] = tanhf ( fGain * vecfWorkL[k] ); }
    oversamplerL.Downsample ( vecfWorkL, vecfOutL );

    // ---- R: 同 ----
    oversamplerR.Upsample ( vecfInR, vecfWorkR );
    for ( int k = 0; k < N2; k++ ) { vecfWorkR[k] = tanhf ( fGain * vecfWorkR[k] ); }
    oversamplerR.Downsample ( vecfWorkR, vecfOutR );

    // ---- tone LPF + level + 写回交错 int16（原采样率） ----
    for ( int n = 0; n < N; n++ )
    {
        float yl = vecfOutL[n];
        fToneStateL += fAlpha * ( yl - fToneStateL );
        yl = fToneStateL * fLevelNorm;
        vecsStereoInOut[2 * n] = Float2Short ( yl * 32768.0f );

        float yr = vecfOutR[n];
        fToneStateR += fAlpha * ( yr - fToneStateR );
        yr = fToneStateR * fLevelNorm;
        vecsStereoInOut[2 * n + 1] = Float2Short ( yr * 32768.0f );
    }
}
