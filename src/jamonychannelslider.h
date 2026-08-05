/******************************************************************************\
* JamonyChannelSlider — C 区分轨推子 (QSlider 子类, paintEvent 1:1 自绘 v0)      *
\******************************************************************************/
/*
    pFader 的自绘渲染层。继承 QSlider，保留全部值/信号槽/交互
    (valueChanged→OnLevelValueChanged→SendFaderLevelToServer 红线零碰)，
    只重写 paintEvent，按 v0 components/mixer-channel.tsx:179-260 (roundFader) 逐笔自绘：
      - groove: 中心 x=9, #232323, 6px, 圆角, 竖满 (v0:180-183)
      - handle: 中心 x=9, 16×16 圆 #e5e5e5 + boxShadow 阴影 + 中心 6×6 #8f9096 点 (v0:227-258)
      - (dB 刻度 G2 阶段在同一 paintEvent 加)
    借鉴 JamonyFader (jamonyfader.cpp) 的自绘思路，规格按 v0 C 区推子。
    v0 faderFrac: 0(顶,0dB)..1(底,-∞); QSlider value max=顶 → faderFrac = 1-value/max。
*/

#pragma once
#include <QSlider>

class JamonyChannelSlider : public QSlider
{
public:
    explicit JamonyChannelSlider ( Qt::Orientation orient, QWidget* parent = nullptr );

protected:
    void paintEvent ( QPaintEvent* ) override;
};
