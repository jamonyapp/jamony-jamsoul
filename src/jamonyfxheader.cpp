/******************************************************************************\
* JamonyFxHeader — 机架标题卡 (v0 移植版)                                      *
\******************************************************************************/

#include "jamonyfxheader.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QColor>

JamonyFxHeader::JamonyFxHeader ( QWidget* parent ) : QWidget ( parent )
{
    setAttribute ( Qt::WA_OpaquePaintEvent, false );
    setSizePolicy ( QSizePolicy::Preferred, QSizePolicy::Fixed );
    m_logo.load ( ":/png/main/res/fronticon.png" );
    if ( !m_logo.isNull() )
    {
        m_logo = m_logo.scaledToHeight ( 22, Qt::SmoothTransformation );
    }
}

void JamonyFxHeader::paintEvent ( QPaintEvent* )
{
    QPainter p ( this );
    p.setRenderHint ( QPainter::Antialiasing );
    const QRectF r = rect().adjusted ( 0.5, 0.5, -0.5, -0.5 );

    // panel 背景
    p.setPen ( QPen ( QColor ( 255, 255, 255, 26 ), 1 ) );
    p.setBrush ( QColor ( "#0d0d0d" ) );
    p.drawRoundedRect ( r, 6, 6 );

    // 顶部 inset 高光
    p.setPen ( QPen ( QColor ( 255, 255, 255, 15 ), 1 ) );
    p.drawLine ( QPointF ( r.left() + 1, r.top() + 1 ), QPointF ( r.right() - 1, r.top() + 1 ) );

    // logo
    qreal x = r.left() + 10;
    if ( !m_logo.isNull() )
    {
        p.drawPixmap ( QPointF ( x, r.center().y() - m_logo.height() / 2.0 ), m_logo );
        x += m_logo.width() + 8;
    }

    // 文字 "jamony FX RACK"
    QFont f = p.font();
    f.setBold ( true );
    f.setPointSize ( 10 );
    f.setLetterSpacing ( QFont::AbsoluteSpacing, 2.4 );
    p.setFont ( f );
    p.setPen ( QColor ( "#ffffff" ) );
    p.drawText ( QRectF ( x, r.top(), 190, r.height() ), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral ( "jamony FX RACK" ) );

    // 8 段品牌渐变彩条
    static const char* colors[] = {
        "#00aaff", "#4c6eff", "#9933ff", "#cc33d4",
        "#ff33aa", "#f06a55", "#dcac33", "#bbee00"
    };
    const qreal barH = 16, barW = 3, gap = 4;
    const qreal totalW = 8 * barW + 7 * gap;
    qreal bx = r.right() - 10 - totalW;
    const qreal by = r.center().y() - barH / 2;
    for ( int i = 0; i < 8; i++ )
    {
        QColor c ( colors[i] );
        // 发光
        QColor g = c; g.setAlphaF ( 0.30 );
        p.setBrush ( g ); p.setPen ( Qt::NoPen );
        p.drawRoundedRect ( QRectF ( bx - 1.5, by - 1.5, barW + 3, barH + 3 ), 2, 2 );
        // 实体
        p.setBrush ( c );
        p.drawRoundedRect ( QRectF ( bx, by, barW, barH ), 1.5, 1.5 );
        bx += barW + gap;
    }
}
