/******************************************************************************\
* Audio Reverberation                                                          *
\******************************************************************************/
/*
    The following code is based on "JCRev: John Chowning's reverberator class"
    by Perry R. Cook and Gary P. Scavone, 1995 - 2004
    which is in "The Synthesis ToolKit in C++ (STK)"
    http://ccrma.stanford.edu/software/stk

    Original description:
    This class is derived from the CLM JCRev function, which is based on the use
    of networks of simple allpass and comb delay filters. This class implements
    three series allpass units, followed by four parallel comb filters, and two
    decorrelation delay lines in parallel at the output.
*/

#pragma once
#include "util.h"

class CAudioReverb
{
public:
    CAudioReverb() {}

    void Init ( const EAudChanConf eNAudioChannelConf, const int iNStereoBlockSizeSam, const int iSampleRate, const float fT60 = 1.1f );

    void Clear();
    void Process ( CVector<int16_t>& vecsStereoInOut, const bool bReverbOnLeftChan, const float fAttenuation );

    // jamony: 运行时可调参数 (用户拧旋钮时调用, 未 Init 时仅存值, Init 时生效)
    void SetDecay     ( const float fT60Sec );   // T60 衰减时间 (0.3~3.0s)
    void SetPreDelay  ( const float fMs );       // pre-delay 湿声延迟 (0~150ms)
    void SetDamping   ( const float fPole );     // comb 高频阻尼极点 (0~0.85)

protected:
    void setT60 ( const float fT60, const int iSampleRate );
    bool isPrime ( const int number );

    class COnePole
    {
    public:
        COnePole() : fA ( 0 ), fB ( 0 ) { Reset(); }
        void  setPole ( const float fPole );
        float Calc ( const float fIn );
        void  Reset() { fLastSample = 0; }

    protected:
        float fA;
        float fB;
        float fLastSample;
    };

    EAudChanConf eAudioChannelConf;
    int          iStereoBlockSizeSam;
    CFIFO<float> allpassDelays[3];
    CFIFO<float> combDelays[4];
    COnePole     combFilters[4];
    CFIFO<float> outLeftDelay;
    CFIFO<float> outRightDelay;
    float        allpassCoefficient;
    float        combCoefficient[4];

    // jamony: 可调参数状态
    int          iReverbSampleRate    = 0;     // 0 = 未 Init
    float        fT60                 = 1.1f;  // 当前衰减时间
    float        fDampingPole         = 0.2f;  // 当前阻尼极点
    float        fPreDelayMs          = 0.0f;  // 当前 pre-delay
    int          iPreDelayMaxSamples  = 0;     // 缓冲容量 (200ms)
    int          iPreDelayCurSamples  = 0;     // 当前延迟样本数
    int          iPreDelayWriteIdx    = 0;     // 环形写指针
    CVector<float> preDelayBuf;                // pre-delay 环形缓冲
};
