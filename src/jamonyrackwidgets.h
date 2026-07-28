/******************************************************************************\
* JamonyRackWidgets — LED / 电源开关 / L-R 单选 (v0 移植版)                    *
\******************************************************************************/
/*
    对应 v0 controls.tsx 的 Led / PowerSwitch / LRSelect。
    颜色全用 v0 的：LED 红 #ff2d3f, 不改。
*/

#pragma once
#include <QWidget>
#include <QColor>

// LED 指示灯：On 红 #ff2d3f 多层发光, Off 暗红
class LedWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LedWidget ( QWidget* parent = nullptr );
    void setOn ( bool on ) { m_bOn = on; update(); }
    QSize sizeHint() const override { return QSize ( 10, 10 ); }

protected:
    void paintEvent ( QPaintEvent* ) override;

private:
    bool m_bOn = false;
};

// 电源开关：电源图标, On=accent+发光, Off=灰
class PowerSwitch : public QWidget
{
    Q_OBJECT
public:
    explicit PowerSwitch ( QWidget* parent = nullptr );
    void setOn ( bool on ) { m_bOn = on; update(); }
    bool isOn() const { return m_bOn; }
    void setAccent ( const QColor& c ) { m_accent = c; update(); }
    QSize sizeHint() const override { return QSize ( 14, 14 ); }

signals:
    void toggled ( bool on );

protected:
    void paintEvent ( QPaintEvent* ) override;
    void mousePressEvent ( QMouseEvent* ) override;

private:
    bool   m_bOn = false;
    QColor m_accent = QColor ( "#00aaff" );
};

// L/R 单选：两个圆形 indicator + 文字
class LrSelect : public QWidget
{
    Q_OBJECT
public:
    explicit LrSelect ( QWidget* parent = nullptr );
    void setValue ( bool left ) { m_bLeft = left; update(); } // true=L false=R
    bool isLeft() const { return m_bLeft; }
    void setAccent ( const QColor& c ) { m_accent = c; update(); }
    void setActive ( bool on ) { m_bActive = on; update(); }
    QSize sizeHint() const override { return QSize ( 56, 18 ); }

signals:
    void valueChanged ( bool left ); // true=L false=R

protected:
    void paintEvent ( QPaintEvent* ) override;
    void mousePressEvent ( QMouseEvent* ) override;

private:
    bool   m_bLeft = true;
    bool   m_bActive = true;
    QColor m_accent = QColor ( "#00aaff" );
};

// 折叠箭头：纤细线条三角, 展开朝下 / 折叠朝右
class FoldButton : public QWidget
{
    Q_OBJECT

public:
    explicit FoldButton ( QWidget* parent = nullptr );
    void setFolded ( bool f ) { m_bFolded = f; update(); }
    QSize sizeHint() const override { return QSize ( 14, 14 ); }

signals:
    void clicked();

protected:
    void paintEvent ( QPaintEvent* ) override;
    void mousePressEvent ( QMouseEvent* ) override;

private:
    bool m_bFolded = false;
};
