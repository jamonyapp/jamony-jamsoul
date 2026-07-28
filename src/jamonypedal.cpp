/******************************************************************************\
* PedalWidget — 效果器机架单元 (v0 移植版)                                     *
\******************************************************************************/

#include "jamonypedal.h"
#include "jamonyrackwidgets.h"
#include "jamonyknob.h"
#include "jamonyfader.h"

#include <QPainter>
#include <QPaintEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QFont>
#include <QPainterPath>

PedalWidget::PedalWidget ( QWidget* parent ) : QWidget ( parent )
{
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    setSizePolicy ( QSizePolicy::Preferred, QSizePolicy::Maximum );

    auto* pMain = new QVBoxLayout ( this );
    pMain->setContentsMargins ( 0, 0, 0, 0 );
    pMain->setSpacing ( 0 );

    // ---- 顶部栏 ----
    m_pHeader = new QWidget;
    m_pHeader->setFixedHeight ( 28 );
    auto* pHdr = new QHBoxLayout ( m_pHeader );
    pHdr->setContentsMargins ( 10, 0, 8, 0 );
    pHdr->setSpacing ( 6 );

    m_pLed = new LedWidget;
    m_pPower = new PowerSwitch;
    m_pPower->setOn ( true );
    m_pNameLabel = new QLabel;
    QFont nf = m_pNameLabel->font();
    nf.setBold ( true );
    nf.setPointSize ( 8 );
    nf.setLetterSpacing ( QFont::AbsoluteSpacing, 1.6 );
    m_pNameLabel->setFont ( nf );
    m_pNameLabel->setStyleSheet ( "color: #8f9096;" );

    m_pFoldBtn = new FoldButton;
    m_pFoldBtn->setFixedSize ( 14, 14 );

    pHdr->addWidget ( m_pLed, 0, Qt::AlignVCenter );
    pHdr->addWidget ( m_pPower, 0, Qt::AlignVCenter );
    pHdr->addStretch();
    pHdr->addWidget ( m_pNameLabel, 0, Qt::AlignVCenter );
    pHdr->addWidget ( m_pFoldBtn, 0, Qt::AlignVCenter );

    // ---- body (可折叠) ----
    m_pBody = new QWidget;
    m_pBodyLayout = new QVBoxLayout ( m_pBody );
    m_pBodyLayout->setContentsMargins ( 10, 8, 10, 8 );
    m_pBodyLayout->setSpacing ( 6 );

    pMain->addWidget ( m_pHeader );
    pMain->addWidget ( m_pBody );

    // 折叠动画
    m_pFoldAnim = new QPropertyAnimation ( m_pBody, "maximumHeight", this );
    m_pFoldAnim->setDuration ( 200 );
    m_pFoldAnim->setEasingCurve ( QEasingCurve::OutQuad );

    connect ( m_pFoldBtn, &FoldButton::clicked, this, &PedalWidget::onFoldClicked );
    connect ( m_pPower, &PowerSwitch::toggled, this, &PedalWidget::onPowerToggled );
}

void PedalWidget::setName ( const QString& s )
{
    m_strName = s.toUpper();
    m_pNameLabel->setText ( m_strName );
}

void PedalWidget::setAccent ( const QColor& c )
{
    m_accent = c;
    m_pPower->setAccent ( c );
    if ( m_bPowerOn )
    {
        m_pNameLabel->setStyleSheet ( QString ( "color: %1;" ).arg ( c.name() ) );
    }
    update();
}

void PedalWidget::setPowerOn ( bool on )
{
    m_pPower->setOn ( on );
    onPowerToggled ( on );
}

bool PedalWidget::isPowerOn() const { return m_bPowerOn; }

void PedalWidget::setActiveByPower ( bool on )
{
    onPowerToggled ( on );
}

void PedalWidget::onPowerToggled ( bool on )
{
    m_bPowerOn = on;
    m_pLed->setOn ( on );
    m_pNameLabel->setStyleSheet ( QString ( "color: %1;" ).arg ( on ? m_accent.name() : "#8f9096" ) );
    // body 内旋钮/推子跟随 active (off 时变暗不变灰)
    const auto knobs = m_pBody->findChildren<JamonyKnob*>();
    for ( auto* k : knobs ) { k->setActive ( on ); }
    const auto faders = m_pBody->findChildren<JamonyFader*>();
    for ( auto* f : faders ) { f->setActive ( on ); }
    update();
}

void PedalWidget::onFoldClicked()
{
    m_bFolded = !m_bFolded;
    m_pFoldBtn->setFolded ( m_bFolded );
    if ( m_bFolded )
    {
        m_pFoldAnim->setStartValue ( m_pBody->height() );
        m_pFoldAnim->setEndValue ( 0 );
        m_pBody->setMaximumHeight ( 0 );
    }
    else
    {
        m_pBody->setMaximumHeight ( QWIDGETSIZE_MAX );
        m_pFoldAnim->setStartValue ( 0 );
        m_pFoldAnim->setEndValue ( m_pBody->sizeHint().height() );
    }
    m_pFoldAnim->start();
}

void PedalWidget::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    const QRectF r = rect().adjusted ( 0.5, 0.5, -0.5, -0.5 );

    // 1. panel 背景
    p.setPen ( QPen ( QColor ( 255, 255, 255, 26 ), 1 ) ); // border-white/10
    p.setBrush ( QColor ( "#0d0d0d" ) );
    p.drawRoundedRect ( r, 6, 6 );

    // 2. 涂鸦（slice 等比缩放居中裁切, 不变形; clip 到 body 区域避免透过 header 半透明背景溢出淡色）
    if ( m_decor != None )
    {
        p.save();
        p.setClipRect ( QRectF ( r.left(), 28, r.width(), r.height() - 28 ) );
        paintDecor ( p );
        p.restore();
    }

    // 3. 顶部条背景 + 底部分隔线
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( 0, 0, 0, 102 ) ); // bg-black/40
    p.drawRoundedRect ( QRectF ( r.left(), r.top(), r.width(), 28 ), 6, 6 );
    p.drawRect ( QRectF ( r.left(), r.top() + 22, r.width(), 6 ) ); // 盖住下半圆角
    p.setPen ( QPen ( QColor ( 255, 255, 255, 26 ), 1 ) );
    p.drawLine ( QPointF ( r.left(), r.top() + 28 ), QPointF ( r.right(), r.top() + 28 ) );

    // 4. 顶部 inset 高光
    p.setPen ( QPen ( QColor ( 255, 255, 255, 15 ), 1 ) );
    p.drawLine ( QPointF ( r.left() + 1, r.top() + 1 ), QPointF ( r.right() - 1, r.top() + 1 ) );
}

void PedalWidget::paintDecor ( QPainter& p )
{
    // slice: 等比缩放覆盖整个 widget, 居中裁切
    const qreal sx = qMax ( width() / 360.0, height() / 120.0 );
    const qreal ox = ( width() - 360 * sx ) / 2;
    const qreal oy = ( height() - 120 * sx ) / 2;
    p.translate ( ox, oy );
    p.scale ( sx, sx );

    p.setOpacity ( m_bPowerOn ? 0.46 : 0.16 );
    p.setPen ( Qt::NoPen );

    // 画 circle (cx,cy,r, alpha)
    auto circ = [&p, this] ( qreal cx, qreal cy, qreal rad, qreal fa ) {
        QColor c = m_accent; c.setAlphaF ( fa );
        p.setBrush ( c );
        p.drawEllipse ( QPointF ( cx, cy ), rad, rad );
    };
    // 画 rounded rect (x,y,w,h,rx, alpha)
    auto rrect = [&p, this] ( qreal x, qreal y, qreal w, qreal h, qreal rx, qreal fa ) {
        QColor c = m_accent; c.setAlphaF ( fa );
        p.setBrush ( c );
        p.drawRoundedRect ( QRectF ( x, y, w, h ), rx, rx );
    };

    switch ( m_decor )
    {
    case Splash:
        circ ( 302, 38, 26, 0.55 );
        circ ( 336, 72, 10, 0.7 );
        circ ( 264, 76, 6, 0.8 );
        circ ( 320, 22, 4, 0.9 );
        rrect ( 228, 84, 72, 9, 4.5, 0.6 );
        rrect ( 308, 90, 40, 6, 3, 0.45 );
        rrect ( 16, 24, 34, 10, 5, 0.4 );
        break;
    case Lines:
        p.save();
        p.rotate ( -14 );
        rrect ( -12, 70, 150, 12, 6, 0.6 );
        rrect ( 20, 88, 104, 8, 4, 0.4 );
        p.restore();
        p.save();
        p.rotate ( 20 );
        rrect ( 292, 26, 82, 11, 5.5, 0.55 );
        p.restore();
        circ ( 330, 78, 20, 0.35 );
        circ ( 288, 42, 7, 0.7 );
        rrect ( 200, 24, 46, 8, 4, 0.45 );
        break;
    case Drips:
    {
        QPainterPath path;
        path.moveTo ( 0, 22 );
        path.lineTo ( 104, 22 );
        path.cubicTo ( 104, 44, 88, 52, 86, 70 );
        path.cubicTo ( 84, 88, 66, 84, 64, 62 );
        path.cubicTo ( 62, 44, 44, 48, 36, 28 );
        path.closeSubpath();
        QColor c = m_accent; c.setAlphaF ( 0.55 );
        p.setBrush ( c );
        p.drawPath ( path );
        circ ( 88, 84, 6, 0.8 );
        circ ( 52, 76, 3.5, 0.9 );
        rrect ( 176, 84, 130, 11, 5.5, 0.5 );
        circ ( 326, 44, 16, 0.4 );
        circ ( 300, 26, 5, 0.65 );
        break;
    }
    case Grid:
        for ( int i = 0; i < 7; i++ )
        {
            rrect ( 18 + i * 48, 86 - i * 5, 26, 12 + i * 3, 6, 0.28 + ( i % 3 ) * 0.14 );
        }
        rrect ( 236, 24, 96, 12, 6, 0.5 );
        circ ( 216, 28, 7, 0.7 );
        circ ( 338, 52, 16, 0.3 );
        break;
    case Wave:
    {
        QPainterPath w1;
        w1.moveTo ( 0, 76 );
        w1.cubicTo ( 40, 54, 80, 96, 124, 76 );
        w1.cubicTo ( 168, 56, 210, 96, 252, 76 );
        QColor c = m_accent; c.setAlphaF ( 0.5 );
        QPen pen ( c, 11, Qt::SolidLine, Qt::RoundCap );
        p.setPen ( pen );
        p.setBrush ( Qt::NoBrush );
        p.drawPath ( w1 );

        QPainterPath w2;
        w2.moveTo ( 128, 30 );
        w2.cubicTo ( 168, 10, 208, 50, 250, 30 );
        w2.cubicTo ( 292, 12, 324, 48, 360, 30 );
        QColor c2 = m_accent; c2.setAlphaF ( 0.35 );
        QPen pen2 ( c2, 7, Qt::SolidLine, Qt::RoundCap );
        p.setPen ( pen2 );
        p.drawPath ( w2 );
        p.setPen ( Qt::NoPen );
        circ ( 326, 80, 15, 0.4 );
        circ ( 296, 58, 5, 0.75 );
        break;
    }
    case Dots:
    {
        const qreal dots[][3] = { { 290, 30, 7 }, { 314, 50, 4.5 }, { 338, 32, 11 },
                                  { 266, 48, 3.5 }, { 348, 64, 5.5 }, { 300, 70, 3 } };
        for ( const auto& d : dots ) { circ ( d[0], d[1], d[2], 0.7 ); }
        rrect ( -6, 86, 120, 11, 5.5, 0.5 );
        rrect ( 14, 24, 66, 9, 4.5, 0.35 );
        circ ( 104, 32, 9, 0.4 );
        break;
    }
    case Arc:
    {
        QColor c = m_accent; c.setAlphaF ( 0.3 );
        QPen pen ( c, 10, Qt::SolidLine, Qt::RoundCap );
        p.setPen ( pen );
        p.setBrush ( Qt::NoBrush );
        p.drawArc ( QRectF ( 20, 22, 76, 76 ), 0, 360 * 16 );
        c.setAlphaF ( 0.45 );
        QPen pen2 ( c, 8, Qt::SolidLine, Qt::RoundCap );
        p.setPen ( pen2 );
        p.drawArc ( QRectF ( 38, 40, 40, 40 ), 0, 360 * 16 );
        p.setPen ( Qt::NoPen );
        rrect ( 150, 26, 104, 12, 6, 0.5 );
        rrect ( 172, 86, 72, 9, 4.5, 0.35 );
        circ ( 266, 28, 6, 0.75 );
        circ ( 140, 88, 12, 0.3 );
        break;
    }
    default: break;
    }
}
