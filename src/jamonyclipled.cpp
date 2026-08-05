/******************************************************************************\
* JamonyClipLed — v0 消波指示灯 (1:1 复刻 mixer-channel.tsx:138-149)            *
\******************************************************************************/

#include "jamonyclipled.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QColor>
#include <QGraphicsDropShadowEffect>

JamonyClipLed::JamonyClipLed ( QWidget* parent ) : QWidget ( parent )
{
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( Qt::PointingHandCursor );
    // 宽随父布局（Ignored 不主导 CLevelMeter 宽度），高固定 6（sizeHint）
    setSizePolicy ( QSizePolicy::Ignored, QSizePolicy::Fixed );

    // v0 boxShadow: 0 0 6px #FF33AA（仅 On 时启用发光）
    m_glow = new QGraphicsDropShadowEffect ( this );
    m_glow->setColor ( QColor ( "#FF33AA" ) );
    m_glow->setBlurRadius ( 6 );
    m_glow->setOffset ( 0, 0 );
    m_glow->setEnabled ( false );
    setGraphicsEffect ( m_glow );
}

void JamonyClipLed::setOn ( bool on )
{
    m_bOn = on;
    if ( m_glow ) m_glow->setEnabled ( on );   // On→发光，Off→关闭
    update();
}

void JamonyClipLed::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );
    const QRectF r = rect();

    // v0: clipped→#FF33AA, else→#1a1a1a, rounded-[2px]
    p.setPen ( QPen ( QColor ( 0, 0, 0, 150 ), 0.5 ) );
    p.setBrush ( m_bOn ? QColor ( "#FF33AA" ) : QColor ( "#1a1a1a" ) );
    p.drawRoundedRect ( r, 2, 2 );

    if ( m_bOn )
    {
        // 顶部高光（沿用 LedWidget 画法）
        p.setPen ( Qt::NoPen );
        p.setBrush ( QColor ( 255, 255, 255, 100 ) );
        p.drawRoundedRect ( QRectF ( r.left() + 1, r.top() + 0.5, r.width() - 2,
                                     qMax ( qreal ( 1 ), r.height() * 0.35 ) ), 1, 1 );
    }
}

void JamonyClipLed::mousePressEvent ( QMouseEvent* e )
{
    // v0 onClick: 点击熄灭（复位）
    if ( e->button() == Qt::LeftButton )
    {
        if ( m_bOn ) setOn ( false );
        emit clicked();   // 通知外部（CLevelMeter→ClipReset 停 timer + 清 bClip）
        e->accept();
    }
}
