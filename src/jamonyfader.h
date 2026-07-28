/******************************************************************************\
* JamonyFader — VST 风格推子 (v0 移植版, 水平/垂直合一)                         *
\******************************************************************************/
/*
    自绘推子：groove + accent 填充 + radial-gradient handle + 标签/数值互换。
    水平用于 Boost Gain / Reverb Mix；垂直用于 EQ 9 推子。
    对应 v0 controls.tsx 的 HFader / VFader。
*/

#pragma once
#include <QWidget>
#include <QString>
#include <QColor>
#include <QTimer>
#include <QEnterEvent>
#include <functional>

class JamonyFader : public QWidget
{
    Q_OBJECT

public:
    explicit JamonyFader ( Qt::Orientation orient, QWidget* parent = nullptr );

    void setRange ( int iMin, int iMax );
    void setValue ( int iValue );
    int  value() const { return m_iValue; }
    void setLabel ( const QString& s ) { m_strLabel = s; update(); }
    void setAccent ( const QColor& c ) { m_accent = c; update(); }
    void setActive ( bool on ) { m_bActive = on; update(); }
    void setDisplay ( std::function<QString(int)> f ) { m_display = f; }
    void setGrooveLength ( int iLen ) { m_iGrooveLen = iLen; updateGeometry(); } // V: groove 像素高度

signals:
    void valueChanged ( int iValue );

protected:
    void paintEvent ( QPaintEvent* ) override;
    void mousePressEvent ( QMouseEvent* ) override;
    void mouseMoveEvent ( QMouseEvent* ) override;
    void mouseReleaseEvent ( QMouseEvent* ) override;
    void enterEvent ( QEnterEvent* ) override;
    void leaveEvent ( QEvent* ) override;
    QSize sizeHint() const override;

private:
    void showReadout();
    void scheduleRevert();
    void setFromPos ( int pos );     // pos = 鼠标像素位置(相对控件)
    QRectF grooveRect() const;       // groove 几何(不含 label)

    Qt::Orientation m_orient;
    int     m_iMin = 0;
    int     m_iMax = 100;
    int     m_iValue = 0;
    int     m_iGrooveLen = 96;       // V: groove 高度; H: 忽略(用宽度)
    int     m_iPressPos = 0;
    int     m_iPressValue = 0;
    QString m_strLabel;
    QColor  m_accent = QColor ( "#00aaff" );
    bool    m_bActive = true;
    bool    m_bHovering = false;
    bool    m_bDragging = false;
    bool    m_bShowValue = false;
    QTimer  m_revertTimer;
    std::function<QString(int)> m_display;
};
