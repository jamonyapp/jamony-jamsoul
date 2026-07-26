/******************************************************************************\
* Audio Distortion (hard-clip + tone LPF + level, 2x oversampled)             *
\******************************************************************************/

#include "audiodistortion.h"
#include <cmath>

void CAudioDistortion::Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate )
{
    (void) eNAudioChannelConf; // 两路独立处理，声道配置不影响

    iStereoBlockSizeSam = iNStereoBlockSizeSam;
    iMonoBlockSizeSam   = iNStereoBlockSizeSam / 2;
    fSampleRate = static_cast<float> ( iSampleRate );

    // 预高通一阶 LPF 系数（fc~60Hz，切 palm mute 低频隆隆，60Hz 少切保低频力度）
    const float fHpFc = 60.0f;
    fHpAlpha = 1.0f - expf ( -2.0f * 3.14159265f * fHpFc / fSampleRate );

    // noise gate 包络跟随系数（fc~150Hz，攻击/释放足够快跟 palm mute 包络）
    const float fGateFc = 150.0f;
    fGateBeta = 1.0f - expf ( -2.0f * 3.14159265f * fGateFc / fSampleRate );
    // 软门限阈值：原始信号约 -44dB（0.006）。低于此压底噪，高于此全通过。
    fGateThr = 0.006f;

    // 后高切一阶系数（fc~6kHz，两个串联 = 二阶 -12dB/oct，去 fizz）
    const float fHcFc = 6000.0f;
    fHcAlpha = 1.0f - expf ( -2.0f * 3.14159265f * fHcFc / fSampleRate );

    fHpStateL = 0.0f; fHpStateR = 0.0f;
    fEnvL = 0.0f;    fEnvR = 0.0f;
    fHcS1L = 0.0f;   fHcS2L = 0.0f; fHcS1R = 0.0f; fHcS2R = 0.0f;
    fToneStateL = 0.0f; fToneStateR = 0.0f;

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

void CAudioDistortion::Process ( CVector<int16_t>& vecsStereoInOut,
                                 const float fDriveNorm, const float fLevelNorm, const float fToneNorm )
{
    // drive 增益：0~+60 dB（比过载 +40dB 多 20dB 余量，金属节奏猛区）
    const float fDriveDb = fDriveNorm * 60.0f;
    const float fGain = powf ( 10.0f, fDriveDb / 20.0f );

    // tone 一阶低通：截止 800Hz~12kHz（暗↔亮），在原始采样率跑（线性环节不必过采样）
    const float fc = 800.0f + fToneNorm * 11200.0f;
    const float fAlpha = 1.0f - expf ( -2.0f * 3.14159265f * fc / fSampleRate );

    const int N  = iMonoBlockSizeSam;
    const int N2 = iMonoBlockSizeSam * 2;

    // ---- 拆 L/R 交错 int16 → 单路 float → 预高通 → noise gate ----
    for ( int n = 0; n < N; n++ )
    {
        // 预高通（切低频隆隆）
        float xl = vecsStereoInOut[2 * n]     / 32768.0f;
        fHpStateL += fHpAlpha * ( xl - fHpStateL );
        xl -= fHpStateL;

        // noise gate（软门限，压底噪）：包络跟随 → 低于阈值按比例衰减
        const float absL = xl < 0.0f ? -xl : xl;
        fEnvL += fGateBeta * ( absL - fEnvL );
        float gL = fEnvL < fGateThr ? ( fEnvL / fGateThr ) : 1.0f; // 软曲线 0→1
        vecfInL[n] = xl * gL;

        // R 同
        float xr = vecsStereoInOut[2 * n + 1] / 32768.0f;
        fHpStateR += fHpAlpha * ( xr - fHpStateR );
        xr -= fHpStateR;

        const float absR = xr < 0.0f ? -xr : xr;
        fEnvR += fGateBeta * ( absR - fEnvR );
        float gR = fEnvR < fGateThr ? ( fEnvR / fGateThr ) : 1.0f;
        vecfInR[n] = xr * gR;
    }

    // ---- L: 2x 上采样 → 硬 clip @2x → 下采样 ----
    // 硬 clip: y = clamp(gain*x, -1, 1)。对称硬削波（整齐），谐波陡，过采样治混叠必须。
    oversamplerL.Upsample ( vecfInL, vecfWorkL );
    for ( int k = 0; k < N2; k++ )
    {
        float x = fGain * vecfWorkL[k];
        vecfWorkL[k] = x > 1.0f ? 1.0f : ( x < -1.0f ? -1.0f : x );
    }
    oversamplerL.Downsample ( vecfWorkL, vecfOutL );

    // ---- R: 同 ----
    oversamplerR.Upsample ( vecfInR, vecfWorkR );
    for ( int k = 0; k < N2; k++ )
    {
        float x = fGain * vecfWorkR[k];
        vecfWorkR[k] = x > 1.0f ? 1.0f : ( x < -1.0f ? -1.0f : x );
    }
    oversamplerR.Downsample ( vecfWorkR, vecfOutR );

    // ---- 后高切(二阶) + tone LPF + level + 写回交错 int16（原采样率） ----
    for ( int n = 0; n < N; n++ )
    {
        // 后高切：两个一阶低通串联 @6kHz = 二阶 -12dB/oct，去 fizz
        float yl = vecfOutL[n];
        fHcS1L += fHcAlpha * ( yl - fHcS1L );
        yl = fHcS1L;
        fHcS2L += fHcAlpha * ( yl - fHcS2L );
        yl = fHcS2L;

        // tone LPF
        fToneStateL += fAlpha * ( yl - fToneStateL );
        yl = fToneStateL * fLevelNorm;
        vecsStereoInOut[2 * n] = Float2Short ( yl * 32768.0f );

        // R 同
        float yr = vecfOutR[n];
        fHcS1R += fHcAlpha * ( yr - fHcS1R );
        yr = fHcS1R;
        fHcS2R += fHcAlpha * ( yr - fHcS2R );
        yr = fHcS2R;

        fToneStateR += fAlpha * ( yr - fToneStateR );
        yr = fToneStateR * fLevelNorm;
        vecsStereoInOut[2 * n + 1] = Float2Short ( yr * 32768.0f );
    }
}
