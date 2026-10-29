/******************************************************************************\
* JamonyChannelSlider — C 区分轨推子 (QSlider 子类, paintEvent 1:1 自绘 v0)      *
\******************************************************************************/

#include "jamonychannelslider.h"

#include <QPainter>
#include <QPaintEvent>
#include <QColor>
#include <QFont>
#include <QStyle>
#include <QStyleOptionSlider>

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

    // ⭐ 1029 隐形天花板根治：paint 与交互同源。
    //   旧版自定 GROOVE_TOP=6 画槽,与 QSlider 内部(QStyle groove+handle 算拖拽行程)脱节——
    //   拖到头仍差 ~32px 够不到画出来的 0dB 槽顶(欢哥案发:手柄像被 IN/OUT 文案挡住)。
    //   现槽/刻度/手柄全部沿 QStyle 真轨画:手柄中心行程 = groove.top()+handleH/2 → groove.bottom()-handleH/2
    //   （与 QSlider 官方 pixelPosFromValue 映射逐项一致）——画轨=拖轨,死区归零,顶底两端对称。
    QStyleOptionSlider opt;
    initStyleOption ( &opt );
    const QRect grooveR = style()->subControlRect ( QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this );
    const QRect handleR = style()->subControlRect ( QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this );
    const qreal travelTop = grooveR.top() + handleR.height() / 2.0;    // 手柄中心最顶（=value 最大）
    // 底端基线（1029 欢哥定稿）：-∞ 刻度与 IN 电平槽底端底齐（=控件底边）——
    // 不用 groove.bottom()-handleH/2（样式槽底上方 ~5px，首轮验收被欢哥抓出）；顶底基线从此各归其位
    const qreal travelBot = height();
    const qreal grooveH   = travelBot - travelTop;                      // 手柄中心行程长
    const QRectF grooveRect ( GROOVE_CENTER - GROOVE_W / 2, travelTop, GROOVE_W, grooveH );

    // 1. groove: #232323 4px 圆角槽 (v0:180-183)
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( "#232323" ) );
    p.drawRoundedRect ( grooveRect, 3, 3 );

    // 2. dB 刻度 (v0:185-225 MARKS，离散对数 frac；y = travelTop + frac×grooveH)
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
        // 刻度中心 y（1029 统一走真轨：-∞ 也在行程末端，不再特判贴 widget 底）
        const qreal cy = travelTop + m.frac * grooveH;

        // 左刻度线 + 右刻度线（6×2 TICK；左无值右有值）
        p.setPen ( Qt::NoPen );
        p.setBrush ( TICK );
        p.drawRect ( QRectF ( leftTickX,  cy - TICK_H / 2.0, TICK_W, TICK_H ) );
        p.drawRect ( QRectF ( rightTickX, cy - TICK_H / 2.0, TICK_W, TICK_H ) );

        // 右刻度值（fontSize 10；中心对齐刻度点；-∞ 底部钳制防越界）
        p.setPen ( TICK );
        const bool isBottom = ( m.frac >= 1.0 );
        if ( isBottom )
            p.drawText ( QRectF ( valueX, cy - 16, 30, 16 ), Qt::AlignBottom | Qt::AlignLeft, m.label );
        else
            p.drawText ( QRectF ( valueX, cy - 8, 30, 16 ), Qt::AlignVCenter | Qt::AlignLeft, m.label );
    }

    // 3. handle 位置: v0 faderFrac 0(顶,0dB)..1(底,-∞); 相对真轨（与交互同源）
    const qreal faderFrac = ( maximum() > minimum() )
        ? 1.0 - static_cast<qreal> ( value() - minimum() ) / ( maximum() - minimum() ) : 0;
    const QPointF handleCenter ( GROOVE_CENTER, travelTop + faderFrac * grooveH );

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
