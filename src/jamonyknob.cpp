/******************************************************************************\
* JamonyKnob — VST 风格旋钮控件                                                *
\******************************************************************************/

#include "jamonyknob.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QFont>
#include <QColor>
#include <QPen>
#include <QBrush>

JamonyKnob::JamonyKnob ( QWidget* parent ) : QWidget ( parent )
{
    // 旋钮不需要焦点框，鼠标光标为垂直调整
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( Qt::SizeVerCursor );
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
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
    const QPointF c = r.center();
    const int    iRadius = qMin ( width(), height() ) / 2 - 6;

    // 圆盘：深色填充 + 暗边框（和机架 #0f0f0f/#333 配色一致）
    p.setPen ( QPen ( QColor ( "#333333" ), 1 ) );
    p.setBrush ( QColor ( "#1a1a1a" ) );
    p.drawEllipse ( c, iRadius, iRadius );

    // 圆点指针：jamony 色，角度随 value 映射 270° 弧
    // value=min → -135°（7点钟），value=max → +135°（5点钟），顺时针经过 12 点
    qreal tRatio = ( m_iMax > m_iMin ) ? static_cast<qreal> ( m_iValue - m_iMin ) / ( m_iMax - m_iMin ) : 0;
    qreal fAngle = -135.0 + tRatio * 270.0;

    p.save();
    p.translate ( c );
    p.rotate ( fAngle );
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( "#BBEE00" ) ); // jamony 亮绿（同 LED RL_GREEN）
    p.drawEllipse ( QPointF ( 0, -iRadius + 5 ), 3.0, 3.0 );
    p.restore();

    // 底部标签文字（Drive/Level/Tone）
    if ( !m_strLabel.isEmpty() )
    {
        p.setPen ( QColor ( "#aaaaaa" ) );
        QFont f = p.font();
        f.setPointSize ( 8 );
        p.setFont ( f );
        p.drawText ( QRectF ( 0, height() - 14, width(), 14 ), Qt::AlignCenter, m_strLabel );
    }
}

void JamonyKnob::mousePressEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        m_iPressY = e->globalY();
        m_iPressValue = m_iValue;
        e->accept();
    }
}

void JamonyKnob::mouseMoveEvent ( QMouseEvent* e )
{
    if ( e->buttons() & Qt::LeftButton )
    {
        // 垂直拖动：鼠标上移值增（dy>0），下移值减
        const int iDeltaY = m_iPressY - e->globalY();

        // 200 像素扫全程；Shift 细调 ×4（更慢更精）
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

void JamonyKnob::mouseDoubleClickEvent ( QMouseEvent* e )
{
    // 双击回中间 12 点位置（VST 惯例：双击归位到中间值）
    const int iMid = ( m_iMin + m_iMax ) / 2;

    if ( m_iValue != iMid )
    {
        m_iValue = iMid;
        update();
        emit valueChanged ( m_iValue );
    }
    e->accept();
}
