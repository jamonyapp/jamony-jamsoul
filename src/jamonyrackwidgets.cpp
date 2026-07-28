/******************************************************************************\
* JamonyRackWidgets — LED / 电源开关 / L-R 单选 (v0 移植版)                    *
\******************************************************************************/

#include "jamonyrackwidgets.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QRadialGradient>

/* --------------------------------- LED --------------------------------- */

LedWidget::LedWidget ( QWidget* parent ) : QWidget ( parent )
{
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    setFocusPolicy ( Qt::NoFocus );
}

void LedWidget::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );
    const QPointF c = rect().center();
    const qreal   r = 4;

    if ( m_bOn )
    {
        // 实体红
        p.setBrush ( QColor ( "#ff2d3f" ) );
        p.setPen ( QPen ( QColor ( 0, 0, 0, 150 ), 0.5 ) );
        p.drawEllipse ( c, r, r );
        // 顶部高光
        p.setBrush ( QColor ( 255, 255, 255, 100 ) );
        p.setPen ( Qt::NoPen );
        p.drawEllipse ( QPointF ( c.x() - 1, c.y() - 1.5 ), 1.2, 1.2 );
    }
    else
    {
        p.setBrush ( QColor ( "#2a1214" ) );
        p.setPen ( QPen ( QColor ( 0, 0, 0, 120 ), 0.5 ) );
        p.drawEllipse ( c, r, r );
        // 内阴影
        p.setBrush ( QColor ( 0, 0, 0, 80 ) );
        p.setPen ( Qt::NoPen );
        p.drawEllipse ( QPointF ( c.x(), c.y() + 0.8 ), r - 1, r - 1 );
    }
}

/* ----------------------------- PowerSwitch ----------------------------- */

PowerSwitch::PowerSwitch ( QWidget* parent ) : QWidget ( parent )
{
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( Qt::PointingHandCursor );
    setStyleSheet ( "background: transparent; border: none;" );
}

void PowerSwitch::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    // icon viewBox 0 0 24 24, 圆心(12,12) 对齐 widget 中心 → 垂直居中
    const qreal s = qMin ( width(), height() ) / 24.0;
    p.save();
    p.translate ( width() / 2.0, height() / 2.0 );
    p.scale ( s, s );
    p.translate ( -12, -12 );

    const QColor color = m_bOn ? m_accent : QColor ( "#5c5d63" );

    // 实体图标：竖线 + 顶部开口圆弧（电源符号 ⏻）
    QPen pen ( color );
    pen.setWidthF ( 2.6 );
    pen.setCapStyle ( Qt::RoundCap );
    p.setPen ( pen );
    p.drawLine ( QPointF ( 12, 3 ), QPointF ( 12, 12 ) );
    p.drawArc ( QRectF ( 3, 3, 18, 18 ), 135 * 16, 270 * 16 ); // 顶部 90° 开口
    p.restore();
}

void PowerSwitch::mousePressEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        m_bOn = !m_bOn;
        update();
        emit toggled ( m_bOn );
        e->accept();
    }
}

/* ------------------------------- LrSelect ------------------------------ */

LrSelect::LrSelect ( QWidget* parent ) : QWidget ( parent )
{
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( Qt::PointingHandCursor );
}

void LrSelect::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    const qreal cy = height() / 2.0;
    // L 在左半, R 在右半
    const QPointF cL ( 8, cy );
    const QPointF cR ( width() - 24, cy );

    for ( int i = 0; i < 2; i++ )
    {
        const bool   isL = ( i == 0 );
        const bool   sel = ( isL == m_bLeft );
        const QPointF c = isL ? cL : cR;
        const QString txt = isL ? QStringLiteral ( "L" ) : QStringLiteral ( "R" );

        // 发光（选中且 active）
        if ( sel && m_bActive )
        {
            QColor g = m_accent; g.setAlphaF ( 0.30 );
            p.setBrush ( g ); p.setPen ( Qt::NoPen );
            p.drawEllipse ( c, 7, 7 );
        }
        // indicator 圆
        p.setBrush ( QColor ( "#0d0d0f" ) );
        p.setPen ( QPen ( sel && m_bActive ? m_accent : QColor ( "#4a4a4f" ), 1 ) );
        p.drawEllipse ( c, 4.5, 4.5 );
        // 选中实心点
        if ( sel )
        {
            p.setBrush ( m_bActive ? m_accent : QColor ( "#6b6b70" ) );
            p.setPen ( Qt::NoPen );
            p.drawEllipse ( c, 2, 2 );
        }
        // 文字
        p.setPen ( QColor ( "#8f9096" ) );
        QFont f = p.font();
        f.setPointSize ( 7 );
        f.setFamily ( "Menlo" );
        p.setFont ( f );
        p.drawText ( QRectF ( c.x() + 7, 0, 16, height() ), Qt::AlignLeft | Qt::AlignVCenter, txt );
    }
}

void LrSelect::mousePressEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        const bool left = ( e->x() < width() / 2 );
        if ( left != m_bLeft )
        {
            m_bLeft = left;
            update();
            emit valueChanged ( m_bLeft );
        }
        e->accept();
    }
}

/* ------------------------------ FoldButton ----------------------------- */

FoldButton::FoldButton ( QWidget* parent ) : QWidget ( parent )
{
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( Qt::PointingHandCursor );
    setStyleSheet ( "background: transparent; border: none;" );
}

void FoldButton::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );
    // viewBox 16x16, 三角 (4,6)-(8,10)-(12,6) 朝下, stroke 1.5 纤细
    const qreal s = qMin ( width(), height() ) / 16.0;
    p.save();
    p.translate ( width() / 2.0, height() / 2.0 );
    p.scale ( s, s );
    p.rotate ( m_bFolded ? -90.0 : 0.0 ); // 折叠朝右
    p.translate ( -8, -8 );
    QPen pen ( QColor ( "#8f9096" ), 1.5 );
    pen.setCapStyle ( Qt::RoundCap );
    pen.setJoinStyle ( Qt::RoundJoin );
    p.setPen ( pen );
    QPainterPath path;
    path.moveTo ( 4, 6 );
    path.lineTo ( 8, 10 );
    path.lineTo ( 12, 6 );
    p.drawPath ( path );
    p.restore();
}

void FoldButton::mousePressEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton ) { emit clicked(); e->accept(); }
}
