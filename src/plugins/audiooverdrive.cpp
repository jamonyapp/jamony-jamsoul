/******************************************************************************\
* Audio Overdrive (tanh soft-clip + tone LPF + level)                         *
\******************************************************************************/

#include "audiooverdrive.h"
#include <cmath>

void CAudioOverdrive::Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate )
{
    (void) eNAudioChannelConf; // 两路独立处理，声道配置不影响

    iStereoBlockSizeSam = iNStereoBlockSizeSam;
    fSampleRate = static_cast<float> ( iSampleRate );
    fToneStateL = 0.0f;
    fToneStateR = 0.0f;
}

void CAudioOverdrive::Process ( CVector<int16_t>& vecsStereoInOut,
                                const float fDriveNorm, const float fLevelNorm, const float fToneNorm )
{
    // drive 增益：0~+40 dB（覆盖主流过载踏板 TS9~OCD/SD-1 范围）
    const float fDriveDb = fDriveNorm * 40.0f;
    const float fGain = powf ( 10.0f, fDriveDb / 20.0f );

    // tone 一阶低通：截止 800Hz~12kHz（暗↔亮）
    const float fc = 800.0f + fToneNorm * 11200.0f;
    const float fAlpha = 1.0f - expf ( -2.0f * 3.14159265f * fc / fSampleRate );

    for ( int i = 0; i < iStereoBlockSizeSam; i += 2 )
    {
        // ---- L ----
        float xl = vecsStereoInOut[i] / 32768.0f;
        // tanh 软 clip（gain=1 小信号单位增益，gain↑ 小信号放大、大信号软压饱和）
        float yl = tanhf ( fGain * xl );
        // tone 一阶 LPF
        fToneStateL += fAlpha * ( yl - fToneStateL );
        yl = fToneStateL;
        // level 输出补偿
        yl *= fLevelNorm;
        vecsStereoInOut[i] = Float2Short ( yl * 32768.0f );

        // ---- R ----
        float xr = vecsStereoInOut[i + 1] / 32768.0f;
        float yr = tanhf ( fGain * xr );
        fToneStateR += fAlpha * ( yr - fToneStateR );
        yr = fToneStateR;
        yr *= fLevelNorm;
        vecsStereoInOut[i + 1] = Float2Short ( yr * 32768.0f );
    }
}
