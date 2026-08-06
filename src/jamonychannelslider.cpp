/******************************************************************************\
* JamonyChannelSlider — C 区分轨推子 (QSlider 子类, paintEvent 1:1 自绘 v0)      *
\******************************************************************************/

#include "jamonychannelslider.h"

#include <QPainter>
#include <QPaintEvent>
#include <QColor>
#include <QFont>

JamonyChannelSlider::JamonyChannelSlider ( Qt::Orientation orient, QWidget* parent )
    : QSlider ( orient, parent )
{
}

void JamonyChannelSlider::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    // jamony v0: pFader 46 宽（含左刻度线），groove 中心 x=15
    const qreal GROOVE_CENTER = 15.0;
    const qreal GROOVE_W = 4.0; // jamony: 推子槽 4px（原 6）
    // jamony: groove 顶离 pFader 顶 6px（与消波灯下沿顶齐；0 刻度中心 groove 顶，上半在 pFader 内避裁切）
    const qreal GROOVE_TOP = 6.0;
    const qreal grooveH = height() - GROOVE_TOP;
    const QRectF grooveRect ( GROOVE_CENTER - GROOVE_W / 2, GROOVE_TOP, GROOVE_W, grooveH );

    // 1. groove: #232323 6px 圆角槽 (v0:180-183)
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( "#232323" ) );
    p.drawRoundedRect ( grooveRect, 3, 3 );

    // 2. dB 刻度 (v0:185-225 MARKS，离散对数 frac；y = GROOVE_TOP + frac×grooveH)
    struct Mark { QString label; qreal frac; };
    const Mark marks[] = {
        { "0",   0.0 },
        { "-6",  0.13 },
        { "-12", 0.28 },
        { "-18", 0.46 },
        { "-24", 0.68 },
        { QString::fromUtf8("-\xe2\x88\x9e"), 1.0 } // -∞
    };
    const int   TICK_W = 6, TICK_H = 2;
    const qreal leftTickX  = GROOVE_CENTER - 11;   // jamony: 内收 4px（原 -15）
    const qreal rightTickX = GROOVE_CENTER + 5;    // jamony: 内收 4px（原 +9）
    const qreal valueX     = GROOVE_CENTER + 12;   // jamony: 内收 4px（原 +16）
    const QColor TICK ( "#8f9096" );

    QFont valueFont = p.font();
    valueFont.setPixelSize ( 10 );   // v0 fontSize 10
    p.setFont ( valueFont );

    for ( const Mark& m : marks )
    {
        const bool isBottom = ( m.frac >= 1.0 );
        // 刻度中心 y：非-∞ = GROOVE_TOP + frac×grooveH；-∞ = pFader 底
        const qreal cy = isBottom ? height() : ( GROOVE_TOP + m.frac * grooveH );

        // 左刻度线 + 右刻度线（6×2 TICK；左无值右有值）
        p.setPen ( Qt::NoPen );
        p.setBrush ( TICK );
        p.drawRect ( QRectF ( leftTickX,  cy - TICK_H / 2.0, TICK_W, TICK_H ) );
        p.drawRect ( QRectF ( rightTickX, cy - TICK_H / 2.0, TICK_W, TICK_H ) );

        // 右刻度值（fontSize 10 TICK；非-∞ 中心对齐刻度点，-∞ 底对齐）
        p.setPen ( TICK );
        if ( isBottom )
            p.drawText ( QRectF ( valueX, height() - 16, 30, 16 ), Qt::AlignBottom | Qt::AlignLeft, m.label );
        else
            p.drawText ( QRectF ( valueX, cy - 8, 30, 16 ), Qt::AlignVCenter | Qt::AlignLeft, m.label );
    }

    // 3. handle 位置: v0 faderFrac 0(顶,0dB)..1(底,-∞); 相对 groove
    const qreal faderFrac = ( maximum() > minimum() )
        ? 1.0 - static_cast<qreal> ( value() - minimum() ) / ( maximum() - minimum() ) : 0;
    const QPointF handleCenter ( GROOVE_CENTER, GROOVE_TOP + faderFrac * grooveH );

    // 4. handle 阴影 (v0:248 boxShadow 0 1px 2px rgba(0,0,0,0.6))
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( 0, 0, 0, 153 ) );
    p.drawEllipse ( handleCenter + QPointF ( 0, 1 ), 9, 9 );

    // 5. handle 实体 16×16 圆 #e5e5e5 (v0:244-247 roundFader)
    p.setBrush ( QColor ( "#e5e5e5" ) );
    p.drawEllipse ( handleCenter, 8, 8 );

    // 6. 中心 6×6 #8f9096 点 (v0:251-258 roundFader)
    p.setBrush ( QColor ( "#8f9096" ) );
    p.drawEllipse ( handleCenter, 3, 3 );
}
