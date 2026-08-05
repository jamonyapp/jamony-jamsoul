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
    if ( m_dValue < m_iMin ) m_dValue = m_iMin;
    if ( m_dValue > m_iMax ) m_dValue = m_iMax;
    update();
}

void JamonyPanBar::setValue ( int iValue )
{
    if ( iValue < m_iMin ) iValue = m_iMin;
    if ( iValue > m_iMax ) iValue = m_iMax;
    if ( static_cast<int> ( std::lround ( m_dValue ) ) != iValue )
    {
        m_dValue = iValue;
        m_iLastEmitted = iValue;
        update();
    }
}

void JamonyPanBar::mousePressEvent ( QMouseEvent* e )
{
    // jamony: 点击不跳变（避免想微调时点击突然变位置），只记录拖拽起始
    if ( e->button() == Qt::LeftButton )
    {
        m_iPressX = e->globalX();
        m_dPressValue = m_dValue;
        m_bDragging = true;
        e->accept();
    }
}

void JamonyPanBar::mouseMoveEvent ( QMouseEvent* e )
{
    // jamony: 相对拖拽（handle 从原位随拖动相对移，微调友好，像旋钮）
    // m_dValue 用 double 连续变化 → panDev 显示取整步长 1（v0 panValue float 语义）
    if ( e->buttons() & Qt::LeftButton )
    {
        const int    iDeltaX = e->globalX() - m_iPressX;    // 水平向右为正
        const qreal  fSensitivity = 400.0;                   // 拖 400px 改满 range
        const double fRange = m_iMax - m_iMin;
        double fNewValue = m_dPressValue + iDeltaX * fRange / fSensitivity;
        if ( fNewValue < m_iMin ) fNewValue = m_iMin;
        if ( fNewValue > m_iMax ) fNewValue = m_iMax;
        m_dValue = fNewValue;
        update();                                           // 重画（panDev 平滑/步长1）
        const int iRound = static_cast<int> ( std::lround ( m_dValue ) );
        if ( iRound != m_iLastEmitted )                     // 只在 int 值变化时上行
        {
            m_iLastEmitted = iRound;
            emit valueChanged ( iRound );
        }
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
    if ( static_cast<int> ( std::lround ( m_dValue ) ) != iCenter )
    {
        m_dValue = iCenter;
        m_iLastEmitted = iCenter;
        update();
        emit valueChanged ( iCenter );
    }
    e->accept();
}

void JamonyPanBar::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    const QRectF r = rect();

    // 1. 背景：圆角 3，#141414 (v0:280)
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( "#141414" ) );
    p.drawRoundedRect ( r, 3, 3 );

    // panDev = (m_dValue-50)*2 ∈ [-100,100] (v0:120-121)，double 计算 → 取整显示步长 1
    const double dCenter = ( m_iMin + m_iMax ) / 2.0;                          // 50
    const double dHalf   = ( m_iMax - m_iMin ) / 2.0;                          // 50
    const double panDevD = dHalf > 0 ? ( m_dValue - dCenter ) * 100.0 / dHalf : 0; // -100..100 double
    const int    panDev = static_cast<int> ( std::lround ( panDevD ) );        // 显示取整（步长1）

    const qreal cx = r.center().x();                                           // 50% 中点
    const qreal halfW = r.width() / 2.0;

    // 2. 中心向偏移侧填充 rgba(187,238,0,0.22) (v0:283-294 PAN_FILL)，用 panDevD 平滑
    QColor fill ( "#BBEE00" );
    fill.setAlphaF ( 0.22 );
    p.setBrush ( fill );
    p.setPen ( Qt::NoPen );
    if ( panDevD < 0 )
    {
        const qreal w = ( -panDevD / 100.0 ) * halfW;
        p.drawRect ( QRectF ( cx - w, r.top(), w, r.height() ) );
    }
    else if ( panDevD > 0 )
    {
        const qreal w = ( panDevD / 100.0 ) * halfW;
        p.drawRect ( QRectF ( cx, r.top(), w, r.height() ) );
    }

    // 3. 中点标记 1px #555 (v0:296-299)
    p.setPen ( QPen ( QColor ( "#555555" ), 1 ) );
    p.drawLine ( QPointF ( cx, r.top() ), QPointF ( cx, r.bottom() ) );

    // 4. 数值读数 C / L{n} / R{n} (v0:118-121, 301-311)，panDev 取整步长 1
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
    p.drawText ( QRectF ( textX - 30, r.top(), 60, r.height() ), Qt::AlignCenter, text );
}
