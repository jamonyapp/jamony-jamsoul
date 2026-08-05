/******************************************************************************\
* JamonyChannelSlider — C 区分轨推子 (QSlider 子类, paintEvent 1:1 自绘 v0)      *
\******************************************************************************/

#include "jamonychannelslider.h"

#include <QPainter>
#include <QPaintEvent>
#include <QColor>

JamonyChannelSlider::JamonyChannelSlider ( Qt::Orientation orient, QWidget* parent )
    : QSlider ( orient, parent )
{
}

void JamonyChannelSlider::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    // v0 GROOVE_CENTER=9, groove width=6 (mixer-channel.tsx:25-26)
    const qreal GROOVE_CENTER = 9.0;
    const qreal GROOVE_W = 6.0;
    const QRectF grooveRect ( GROOVE_CENTER - GROOVE_W / 2, 0, GROOVE_W, height() );

    // 1. groove: #232323 6px 圆角槽 (v0:180-183 inset-y-0 rounded-full)
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( "#232323" ) );
    p.drawRoundedRect ( grooveRect, 3, 3 );

    // 2. handle 位置: v0 faderFrac 0(顶,0dB)..1(底,-∞); QSlider value max=顶 → faderFrac=1-value/max
    const qreal faderFrac = ( maximum() > minimum() )
        ? 1.0 - static_cast<qreal> ( value() - minimum() ) / ( maximum() - minimum() ) : 0;
    const QPointF handleCenter ( GROOVE_CENTER, faderFrac * height() );

    // 3. handle 阴影 (v0:248 boxShadow 0 1px 2px rgba(0,0,0,0.6)) — 偏下 1px + 半透明稍大圆模拟
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( 0, 0, 0, 153 ) );                          // rgba alpha 0.6
    p.drawEllipse ( handleCenter + QPointF ( 0, 1 ), 9, 9 );         // 半径 9 (handle 8 + 模糊 1)

    // 4. handle 实体 16×16 圆 #e5e5e5 (v0:244-247 roundFader)
    p.setBrush ( QColor ( "#e5e5e5" ) );
    p.drawEllipse ( handleCenter, 8, 8 );                             // 半径 8 = 直径 16

    // 5. 中心 6×6 #8f9096 点 (v0:251-258 roundFader)
    p.setBrush ( QColor ( "#8f9096" ) );
    p.drawEllipse ( handleCenter, 3, 3 );                             // 半径 3 = 直径 6

    // TODO G2: dB 刻度 0/-6/-12/-18/-24/-∞ + groove 左右刻度线 (v0:187-225)
}
