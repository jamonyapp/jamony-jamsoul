/******************************************************************************\
* Audio Delay Line (环形缓冲 + 分数延迟线性插值读取)                            *
\******************************************************************************/
/*
    jamsoul 共享延迟线：供合唱/延迟等需要"存历史样本"的效果器用。

    原理：环形缓冲 (FIFO)，写指针前进，读指针在写指针后 N 个样本。
    - Write(x): buf[writeIdx] = x; writeIdx 前进
    - Read(N):  读 writeIdx 之前 N 个样本。N 可为分数（LFO 调制/时间平滑），
      线性插值取相邻两样本加权，避免延迟长度变化时 click。

    用法：
      wet = delay.Read(delaySamples)        // 读延迟样本 (先读)
      delay.Write(input + feedback * wet)   // 写新样本 (后写)
    Read 在 Write 前：writeIdx 指向"下一个写位置"，Read(N) 读 (writeIdx-N) 即 N 样本前。

    容量由 Init(iMaxDelaySamples) 指定，需 >= 最大延迟样本数 + 2（插值余量）。
    照 jamulus 抄 STK JCRev 之先例自行实现，算法公开，AGPL 无障碍。
*/

#pragma once
#include "util.h"

class CDelayLine
{
public:
    CDelayLine() : iSize ( 0 ), iWriteIdx ( 0 ) {}

    void Init ( const int iMaxDelaySamples )
    {
        iSize = iMaxDelaySamples;
        vecfBuf.Init ( iSize, 0.0f );
        iWriteIdx = 0;
    }

    void Reset()
    {
        vecfBuf.Reset ( 0.0f );
        iWriteIdx = 0;
    }

    // 写一个样本到延迟线
    inline void Write ( const float fSample )
    {
        vecfBuf[iWriteIdx] = fSample;
        iWriteIdx++;
        if ( iWriteIdx >= iSize ) { iWriteIdx = 0; }
    }

    // 读 fDelaySamples 前的样本（分数延迟，线性插值）。先 Read 后 Write。
    inline float Read ( const float fDelaySamples ) const
    {
        float fReadPos = static_cast<float> ( iWriteIdx ) - fDelaySamples;
        // 归一化到 [0, iSize)
        while ( fReadPos < 0.0f ) { fReadPos += static_cast<float> ( iSize ); }
        while ( fReadPos >= static_cast<float> ( iSize ) ) { fReadPos -= static_cast<float> ( iSize ); }

        const int   i0   = static_cast<int> ( fReadPos );
        const float fFrac = fReadPos - static_cast<float> ( i0 );
        const int   i1   = ( i0 + 1 < iSize ) ? i0 + 1 : 0;
        return vecfBuf[i0] * ( 1.0f - fFrac ) + vecfBuf[i1] * fFrac;
    }

protected:
    CVector<float> vecfBuf;
    int            iSize;
    int            iWriteIdx;
};
