/******************************************************************************\
* JamonyFader — VST 风格推子 (v0 移植版, 水平/垂直合一)                         *
\******************************************************************************/

#include "jamonyfader.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFont>
#include <QColor>
#include <QRadialGradient>
#include <QSizePolicy>

JamonyFader::JamonyFader ( Qt::Orientation orient, QWidget* parent ) :
    QWidget ( parent ), m_orient ( orient )
{
    setFocusPolicy ( Qt::NoFocus );
    setCursor ( orient == Qt::Horizontal ? Qt::PointingHandCursor : Qt::PointingHandCursor );
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    m_revertTimer.setSingleShot ( true );
    QObject::connect ( &m_revertTimer, &QTimer::timeout, this, [this] () {
        m_bShowValue = false;
        update();
    } );

    if ( orient == Qt::Horizontal )
    {
        setSizePolicy ( QSizePolicy::Expanding, QSizePolicy::Fixed );
    }
    else
    {
        setSizePolicy ( QSizePolicy::Fixed, QSizePolicy::Fixed );
    }
}

void JamonyFader::setRange ( int iMin, int iMax )
{
    m_iMin = iMin;
    m_iMax = iMax;
    if ( m_iValue < m_iMin ) m_iValue = m_iMin;
    if ( m_iValue > m_iMax ) m_iValue = m_iMax;
    update();
}

void JamonyFader::setValue ( int iValue )
{
    if ( iValue < m_iMin ) iValue = m_iMin;
    if ( iValue > m_iMax ) iValue = m_iMax;
    if ( iValue != m_iValue )
    {
        m_iValue = iValue;
        update();
    }
}

QSize JamonyFader::sizeHint() const
{
    if ( m_orient == Qt::Horizontal )
    {
        return QSize ( 120, 24 );
    }
    return QSize ( 16, m_iGrooveLen + 14 );
}

QRectF JamonyFader::grooveRect() const
{
    if ( m_orient == Qt::Horizontal )
    {
        const int iLabelW = m_strLabel.isEmpty() ? 0 : 44;
        const qreal gx = iLabelW;
        const qreal gw = width() - iLabelW;
        const qreal gy = height() / 2.0 - 1.5;
        return QRectF ( gx, gy, gw, 3 );
    }
    const qreal gx = width() / 2.0 - 1.5;
    return QRectF ( gx, 0, 3, m_iGrooveLen );
}

void JamonyFader::setFromPos ( int pos )
{
    const QRectF g = grooveRect();
    qreal fRatio = 0;
    if ( m_orient == Qt::Horizontal )
    {
        const qreal gw = g.width();
        fRatio = gw > 0 ? ( pos - g.left() ) / gw : 0;
    }
    else
    {
        const qreal gh = g.height();
        fRatio = gh > 0 ? 1.0 - ( pos - g.top() ) / gh : 0; // 底=0, 顶=1
    }
    if ( fRatio < 0 ) fRatio = 0;
    if ( fRatio > 1 ) fRatio = 1;
    const int iNew = m_iMin + static_cast<int> ( fRatio * ( m_iMax - m_iMin ) + 0.5 );
    if ( iNew != m_iValue )
    {
        m_iValue = iNew;
        update();
        emit valueChanged ( m_iValue );
    }
}

void JamonyFader::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );

    const QRectF g = grooveRect();
    const qreal  fRatio = ( m_iMax > m_iMin ) ? static_cast<qreal> ( m_iValue - m_iMin ) / ( m_iMax - m_iMin ) : 0;
    const qreal  fAlpha = m_bActive ? 1.0 : 0.25;

    // groove 底（etch 暗刻线）
    p.setPen ( Qt::NoPen );
    p.setBrush ( QColor ( "#26262a" ) );
    p.drawRoundedRect ( g, 1.5, 1.5 );

    // accent 填充
    {
        QColor fill = m_accent; fill.setAlphaF ( fAlpha );
        p.setBrush ( fill );
        QRectF fillRect;
        if ( m_orient == Qt::Horizontal )
        {
            fillRect = QRectF ( g.left(), g.top(), g.width() * fRatio, g.height() );
        }
        else
        {
            const qreal fh = g.height() * fRatio;
            fillRect = QRectF ( g.left(), g.bottom() - fh, g.width(), fh );
        }
        if ( fillRect.width() > 0 && fillRect.height() > 0 )
        {
            p.drawRoundedRect ( fillRect, 1.5, 1.5 );
        }
    }

    // handle 位置
    const QPointF handleCenter = ( m_orient == Qt::Horizontal )
        ? QPointF ( g.left() + g.width() * fRatio, g.center().y() )
        : QPointF ( g.center().x(), g.bottom() - g.height() * fRatio );

    // handle 发光（active 时, 多层半透明圆模拟 boxShadow）
    if ( m_bActive )
    {
        QColor glow = m_accent; glow.setAlphaF ( 0.20 );
        p.setBrush ( glow );
        p.setPen ( Qt::NoPen );
        p.drawEllipse ( handleCenter, 11, 11 );
        glow.setAlphaF ( 0.30 );
        p.setBrush ( glow );
        p.drawEllipse ( handleCenter, 8, 8 );
    }

    // handle 实体（radial gradient + accent 边框）
    {
        const qreal hr = 6;
        QRadialGradient grad ( QPointF ( handleCenter.x() - hr * 0.3, handleCenter.y() - hr * 0.4 ), hr * 1.3 );
        grad.setColorAt ( 0, QColor ( "#45454a" ) );
        grad.setColorAt ( 1, QColor ( "#1a1a1d" ) );
        p.setBrush ( QBrush ( grad ) );
        p.setPen ( QPen ( m_bActive ? m_accent : QColor ( "#4a4a4f" ), 1 ) );
        p.drawEllipse ( handleCenter, hr, hr );
    }

    // 标签/数值
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
            f.setPointSize ( m_orient == Qt::Horizontal ? 8 : 7 );
            if ( m_bShowValue )
            {
                f.setStyleHint ( QFont::Monospace );
                f.setFamily ( "Menlo" );
            }
            p.setFont ( f );
            if ( m_orient == Qt::Horizontal )
            {
                p.drawText ( QRectF ( 0, 0, 44, height() ), Qt::AlignLeft | Qt::AlignVCenter, text );
            }
            else
            {
                p.drawText ( QRectF ( 0, m_iGrooveLen, width(), 14 ), Qt::AlignCenter, text );
            }
        }
    }
}

void JamonyFader::showReadout()
{
    m_revertTimer.stop();
    if ( m_bHovering || m_bDragging )
    {
        m_bShowValue = true;
        update();
    }
}

void JamonyFader::scheduleRevert()
{
    m_revertTimer.stop();
    if ( !m_bHovering && !m_bDragging )
    {
        m_revertTimer.start ( 600 );
    }
}

void JamonyFader::enterEvent ( QEnterEvent* )
{
    m_bHovering = true;
    showReadout();
}

void JamonyFader::leaveEvent ( QEvent* )
{
    m_bHovering = false;
    scheduleRevert();
}

void JamonyFader::mousePressEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        m_iPressPos = ( m_orient == Qt::Horizontal ) ? e->x() : e->y();
        m_iPressValue = m_iValue;
        m_bDragging = true;
        showReadout();
        setFromPos ( m_iPressPos ); // 点击即跳到位置
        e->accept();
    }
}

void JamonyFader::mouseMoveEvent ( QMouseEvent* e )
{
    if ( e->buttons() & Qt::LeftButton )
    {
        const int pos = ( m_orient == Qt::Horizontal ) ? e->x() : e->y();
        setFromPos ( pos );
        e->accept();
    }
}

void JamonyFader::mouseReleaseEvent ( QMouseEvent* e )
{
    if ( e->button() == Qt::LeftButton )
    {
        m_bDragging = false;
        scheduleRevert();
        e->accept();
    }
}
