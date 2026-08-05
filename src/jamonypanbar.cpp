/******************************************************************************\
* JamonyPanBar — v0 Pan 横条控件 (Cubase 风格, 1:1 复刻 mixer-channel.tsx)       *
\******************************************************************************/

#include "jamonypanbar.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFont>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <cmath>

JamonyPanBar::JamonyPanBar ( QWidget* parent ) : QWidget ( parent )
{
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( Qt::SizeHorCursor );     // v0: cursor-ew-resize
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
}

void JamonyPanBar::setRange ( int iMin, int iMax )
{
    m_iMin = iMin;
    m_iMax = iMax;
    if ( m_iValue < m_iMin ) m_iValue = m_iMin;
    if ( m_iValue > m_iMax ) m_iValue = m_iMax;
    update();
}

void JamonyPanBar::setValue ( int iValue )
{
    if ( iValue < m_iMin ) iValue = m_iMin;
    if ( iValue > m_iMax ) iValue = m_iMax;
    if ( iValue != m_iValue )
    {
        m_iValue = iValue;
        update();
    }
}

// v0 updatePanFromPointer (mixer-channel.tsx:93-99):
//   frac = (clientX - rect.left) / rect.width; setPanValue(frac * 100)
void JamonyPanBar::setFromPosX ( int x )
{
    const int w = width();
    if ( w <= 0 ) return;
    qreal frac = static_cast<qreal> ( x ) / w;
    frac = qBound ( 0.0, frac, 1.0 );
    const int iNewValue = m_iMin + static_cast<int> ( std::lround ( frac * ( m_iMax - m_iMin ) ) );
    if ( iNewValue != m_iValue )
    {
        m_iValue = iNewValue;
        update();
        emit valueChanged ( m_iValue );
    }
}

void JamonyPanBar::mousePressEvent ( QMouseEvent* e )
{
    // v0 onPointerDown: 立即跳到点击位置（点击跳变）
    if ( e->button() == Qt::LeftButton )
    {
        m_bDragging = true;
        setFromPosX ( e->pos().x() );
        e->accept();
    }
}

void JamonyPanBar::mouseMoveEvent ( QMouseEvent* e )
{
    // v0 pointermove: 拖拽跟随
    if ( e->buttons() & Qt::LeftButton )
    {
        setFromPosX ( e->pos().x() );
        e->accept();
    }
}

void JamonyPanBar::mouseReleaseEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        m_bDragging = false;
        e->accept();
    }
}

void JamonyPanBar::mouseDoubleClickEvent ( QMouseEvent* e )
{
    // v0 onDoubleClick: 复位中点 (mixer-channel.tsx:278)
    const int iCenter = ( m_iMin + m_iMax ) / 2;
    if ( m_iValue != iCenter )
    {
        m_iValue = iCenter;
        update();
        emit valueChanged ( m_iValue );
    }
    e->accept();
}

void JamonyPanBar::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    const QRectF r = rect();

    // 1. 背景：圆角 3，#141414 (v0:280 backgroundColor #141414, rounded-[3px])
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( "#141414" ) );
    p.drawRoundedRect ( r, 3, 3 );

    // panDev = (value-50)*2 ∈ [-100,100] (v0:120-121)
    const int iCenter = ( m_iMin + m_iMax ) / 2;                          // 50
    const int iHalf = ( m_iMax - m_iMin ) / 2;                            // 50
    const int panDev = iHalf > 0 ? ( m_iValue - iCenter ) * 100 / iHalf : 0; // -100..100

    const qreal cx = r.center().x();                                      // 50% 中点
    const qreal halfW = r.width() / 2.0;

    // 2. 中心向偏移侧填充 rgba(187,238,0,0.22) (v0:283-294 PAN_FILL)
    QColor fill ( "#BBEE00" );
    fill.setAlphaF ( 0.22 );
    p.setBrush ( fill );
    p.setPen ( Qt::NoPen );
    if ( panDev < 0 )
    {
        // 左：从中心向左生长，width = |panDev|/100 * halfW
        const qreal w = ( static_cast<qreal> ( -panDev ) / 100.0 ) * halfW;
        p.drawRect ( QRectF ( cx - w, r.top(), w, r.height() ) );
    }
    else if ( panDev > 0 )
    {
        // 右：从中心向右生长
        const qreal w = ( static_cast<qreal> ( panDev ) / 100.0 ) * halfW;
        p.drawRect ( QRectF ( cx, r.top(), w, r.height() ) );
    }

    // 3. 中点标记 1px #555 (v0:296-299)
    p.setPen ( QPen ( QColor ( "#555555" ), 1 ) );
    p.drawLine ( QPointF ( cx, r.top() ), QPointF ( cx, r.bottom() ) );

    // 4. 数值读数 C / L{n} / R{n} (v0:118-121, 301-311)
    QString text;
    if ( panDev == 0 )      text = "C";
    else if ( panDev < 0 )  text = QString ( "L%1" ).arg ( -panDev );
    else                    text = QString ( "R%1" ).arg ( panDev );

    // 数值水平位置：panDev==0→50%, <0→25%, >0→75% (v0:304)
    qreal textX;
    if ( panDev == 0 )      textX = 0.50 * r.width();
    else if ( panDev < 0 )  textX = 0.25 * r.width();
    else                    textX = 0.75 * r.width();

    QFont f = p.font();
    f.setBold ( true );                    // v0: font-bold
    f.setPixelSize ( 13 );                 // v0: fontSize 13
    p.setFont ( f );
    p.setPen ( QColor ( "#ffffff" ) );     // v0: color #ffffff
    // transform: translate(-50%,-50%) → 用 QRectF 居中绘制（宽 60 富余，垂直居中）
    p.drawText ( QRectF ( textX - 30, r.top(), 60, r.height() ), Qt::AlignCenter, text );
}
