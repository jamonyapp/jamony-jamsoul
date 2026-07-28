/******************************************************************************\
* JamonyKnob — VST 风格旋钮控件 (v0 移植版)                                    *
\******************************************************************************/

#include "jamonyknob.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFont>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QRadialGradient>

JamonyKnob::JamonyKnob ( QWidget* parent ) : QWidget ( parent )
{
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( Qt::SizeVerCursor );
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    m_revertTimer.setSingleShot ( true );
    QObject::connect ( &m_revertTimer, &QTimer::timeout, this, [this] () {
        m_bShowValue = false;
        update();
    } );
}

void JamonyKnob::setRange ( int iMin, int iMax )
{
    m_iMin = iMin;
    m_iMax = iMax;
    if ( m_iValue < m_iMin ) m_iValue = m_iMin;
    if ( m_iValue > m_iMax ) m_iValue = m_iMax;
    update();
}

void JamonyKnob::setValue ( int iValue )
{
    if ( iValue < m_iMin ) iValue = m_iMin;
    if ( iValue > m_iMax ) iValue = m_iMax;
    if ( iValue != m_iValue )
    {
        m_iValue = iValue;
        update();
    }
}

void JamonyKnob::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    const QRectF r = rect();
    const int    iKnobAreaH = height() - 14;       // 底部 14px 留给标签
    const QPointF c ( r.center().x(), iKnobAreaH / 2.0 );
    const qreal   rOuter = qMin ( width(), iKnobAreaH ) / 2.0 - 4;
    const qreal   rKnob = qMax ( qreal ( 4 ), rOuter - 6 );

    const QRectF arcRect ( c.x() - rOuter, c.y() - rOuter, rOuter * 2, rOuter * 2 );

    const qreal tRatio = ( m_iMax > m_iMin ) ? static_cast<qreal> ( m_iValue - m_iMin ) / ( m_iMax - m_iMin ) : 0;
    // Qt drawArc: 0°=3点方向, 逆时针正, 单位 1/16 度
    // 7点钟=225°, 顺时针经12点到5点钟(315°) = spanAngle -270°
    const int iArcStart = 225 * 16;
    const int iArcFullSpan = -270 * 16;
    const int iArcValueSpan = static_cast<int> ( iArcFullSpan * tRatio );

    // 1. etch 弧形底（暗刻线, 全程 270°）
    {
        QPen pen ( QColor ( "#26262a" ) );
        pen.setWidthF ( 2.5 );
        pen.setCapStyle ( Qt::RoundCap );
        p.setPen ( pen );
        p.setBrush ( Qt::NoBrush );
        p.drawArc ( arcRect, iArcStart, iArcFullSpan );
    }

    // 2. 多层描边发光弧（仅 active 且有值, 对应 v0 三层描边避免方形滤镜区）
    if ( m_bActive && tRatio > 0 )
    {
        QColor c1 = m_accent; c1.setAlphaF ( 0.14 );
        QPen g1 ( c1 ); g1.setWidthF ( 7 ); g1.setCapStyle ( Qt::RoundCap );
        p.setPen ( g1 ); p.drawArc ( arcRect, iArcStart, iArcValueSpan );

        QColor c2 = m_accent; c2.setAlphaF ( 0.28 );
        QPen g2 ( c2 ); g2.setWidthF ( 4.5 ); g2.setCapStyle ( Qt::RoundCap );
        p.setPen ( g2 ); p.drawArc ( arcRect, iArcStart, iArcValueSpan );
    }

    // 3. 实心弧（active 全亮, 失活 0.25 变暗不变灰）
    {
        QColor c = m_accent; c.setAlphaF ( m_bActive ? 1.0 : 0.25 );
        QPen pen ( c ); pen.setWidthF ( 2.5 ); pen.setCapStyle ( Qt::RoundCap );
        p.setPen ( pen );
        p.drawArc ( arcRect, iArcStart, iArcValueSpan );
    }

    // 4. 圆盘（radial gradient + 顶部 inset 高光）
    const QRectF knobRect ( c.x() - rKnob, c.y() - rKnob, rKnob * 2, rKnob * 2 );
    {
        QRadialGradient grad ( QPointF ( knobRect.left() + rKnob * 0.4, knobRect.top() + rKnob * 0.3 ), rKnob * 1.3 );
        grad.setColorAt ( 0, QColor ( "#3a3a3d" ) );
        grad.setColorAt ( 0.55, QColor ( "#1e1e21" ) );
        grad.setColorAt ( 1, QColor ( "#0d0d0f" ) );
        p.setBrush ( QBrush ( grad ) );
        p.setPen ( QPen ( QColor ( 255, 255, 255, 46 ), 1 ) );
        p.drawEllipse ( knobRect );
    }

    // 5. 旋转指针（12点为 0°, 顺时针; value=0→-135°=7点, value=max→+135°=5点）
    {
        const qreal angle = -135.0 + tRatio * 270.0;
        p.save();
        p.translate ( c );
        p.rotate ( angle );
        const QColor ptrColor = m_bActive ? m_accent : QColor ( "#6b6b70" );
        p.setPen ( Qt::NoPen );
        p.setBrush ( ptrColor );
        p.drawRoundedRect ( QRectF ( -1.0, -rKnob + 3, 2.0, rKnob * 0.5 ), 1.0, 1.0 );
        p.restore();
    }

    // 6. 底部标签/数值（hover 或拖动时切换为数值, 否则显示名字）
    {
        QString text = m_strLabel;
        if ( m_bShowValue && m_display )
        {
            text = m_display ( m_iValue );
        }
        if ( !text.isEmpty() )
        {
            const QColor labelColor = ( m_bShowValue && m_bActive ) ? m_accent : QColor ( "#8f9096" );
            p.setPen ( labelColor );
            QFont f = p.font();
            f.setPointSize ( 8 );
            if ( m_bShowValue )
            {
                f.setStyleHint ( QFont::Monospace );
                f.setFamily ( "Menlo" );
            }
            p.setFont ( f );
            p.drawText ( QRectF ( 0, height() - 14, width(), 14 ), Qt::AlignCenter, text );
        }
    }
}

void JamonyKnob::showReadout()
{
    m_revertTimer.stop();
    if ( m_bHovering || m_bDragging )
    {
        m_bShowValue = true;
        update();
    }
}

void JamonyKnob::scheduleRevert()
{
    m_revertTimer.stop();
    if ( !m_bHovering && !m_bDragging )
    {
        m_revertTimer.start ( 600 );
    }
}

void JamonyKnob::enterEvent ( QEnterEvent* )
{
    m_bHovering = true;
    showReadout();
}

void JamonyKnob::leaveEvent ( QEvent* )
{
    m_bHovering = false;
    scheduleRevert();
}

void JamonyKnob::mousePressEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        m_iPressY = e->globalY();
        m_iPressValue = m_iValue;
        m_bDragging = true;
        showReadout();
        e->accept();
    }
}

void JamonyKnob::mouseMoveEvent ( QMouseEvent* e )
{
    if ( e->buttons() & Qt::LeftButton )
    {
        const int iDeltaY = m_iPressY - e->globalY();
        qreal fSensitivity = 200.0;
        if ( e->modifiers() & Qt::ShiftModifier )
        {
            fSensitivity *= 4.0;
        }
        const int iRange = m_iMax - m_iMin;
        int iNewValue = m_iPressValue + static_cast<int> ( iDeltaY * iRange / fSensitivity );
        if ( iNewValue < m_iMin ) iNewValue = m_iMin;
        if ( iNewValue > m_iMax ) iNewValue = m_iMax;
        if ( iNewValue != m_iValue )
        {
            m_iValue = iNewValue;
            update();
            emit valueChanged ( m_iValue );
        }
        e->accept();
    }
}

void JamonyKnob::mouseReleaseEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        m_bDragging = false;
        scheduleRevert();
        e->accept();
    }
}

void JamonyKnob::mouseDoubleClickEvent ( QMouseEvent* e )
{
    // 双击回默认值（VST 惯例）
    if ( m_iValue != m_iDefaultValue )
    {
        m_iValue = m_iDefaultValue;
        update();
        emit valueChanged ( m_iValue );
    }
    e->accept();
}
