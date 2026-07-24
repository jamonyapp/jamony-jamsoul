/******************************************************************************\
* JamonyKnob — VST 风格旋钮控件                                                *
\******************************************************************************/
/*
    jamsoul 自定义旋钮：圆盘 + 圆点指针，270° 可调区间（7点钟→5点钟）。
    鼠标垂直拖动改值（上增下减），Shift 细调，双击回默认值。
    用于过载/失真等效果器的 Drive/Level/Tone 参数。一次做好复用。
*/

#pragma once
#include <QWidget>
#include <QString>

class JamonyKnob : public QWidget
{
    Q_OBJECT

public:
    explicit JamonyKnob ( QWidget* parent = nullptr );

    void setRange ( int iMin, int iMax );
    void setValue ( int iValue );        // 程序设值，不 emit 信号
    int  value() const { return m_iValue; }
    void setDefaultValue ( int iValue ) { m_iDefaultValue = iValue; }
    int  defaultValue() const { return m_iDefaultValue; }
    void setLabel ( const QString& strLabel ) { m_strLabel = strLabel; update(); }

signals:
    void valueChanged ( int iValue );    // 仅用户交互 emit

protected:
    void paintEvent ( QPaintEvent* ) override;
    void mousePressEvent ( QMouseEvent* ) override;
    void mouseMoveEvent ( QMouseEvent* ) override;
    void mouseDoubleClickEvent ( QMouseEvent* ) override;
    QSize sizeHint() const override { return QSize ( 54, 54 ); }

private:
    int     m_iMin = 0;
    int     m_iMax = 100;
    int     m_iValue = 0;
    int     m_iDefaultValue = 0;
    int     m_iPressY = 0;
    int     m_iPressValue = 0;
    QString m_strLabel;
};
