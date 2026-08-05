/******************************************************************************\
* JamonyPanBar — v0 Pan 横条控件 (Cubase 风格, 1:1 复刻 mixer-channel.tsx)       *
\******************************************************************************/
/*
    交互完全照 v0 (components/mixer-channel.tsx:264-312, 93-99)：
      - 点击即跳到点击位置 (onPointerDown→updatePanFromPointer)
      - 水平拖拽跟随          (pointermove→updatePanFromPointer)
      - 双击复位中点 50        (onDoubleClick→setPanValue(50))
      - 无 Shift
    值域 0-100（与 pPan / AUD_MIX_PAN_MAX 契约一致），50=中点。
    左右语义：0=全左，100=全右（与 jamsoul 一致，不反转）。
    显示：panDev=(value-50)*2 ∈ [-100,100] → C / L{n} / R{n}（左右最大 100）。
    JamonyKnob 仅作 QPainter 画法参考（圆角/半透明填充），交互逻辑按 v0 重写。
*/

#pragma once
#include <QWidget>

class JamonyPanBar : public QWidget
{
    Q_OBJECT

public:
    explicit JamonyPanBar ( QWidget* parent = nullptr );

    void setRange ( int iMin, int iMax );
    void setValue ( int iValue );        // 程序设值，不 emit（SetPanValue 回填用）
    int  value() const { return m_iValue; }

signals:
    void valueChanged ( int iValue );    // 仅用户交互 emit（接 OnPanValueChanged）

protected:
    void paintEvent ( QPaintEvent* ) override;
    void mousePressEvent ( QMouseEvent* ) override;
    void mouseMoveEvent ( QMouseEvent* ) override;
    void mouseReleaseEvent ( QMouseEvent* ) override;
    void mouseDoubleClickEvent ( QMouseEvent* ) override;
    QSize sizeHint() const override { return QSize ( 76, 20 ); }

private:
    void setFromPosX ( int x );          // v0 updatePanFromPointer: frac=x/width → value

    int  m_iMin = 0;
    int  m_iMax = 100;
    int  m_iValue = 50;                  // 默认中点
    bool m_bDragging = false;
};
