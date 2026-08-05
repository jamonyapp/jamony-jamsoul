/******************************************************************************\
* JamonyPanBar — v0 Pan 横条控件 (Cubase 风格, 1:1 复刻 mixer-channel.tsx)       *
\******************************************************************************/
/*
    交互（欢哥体验后调整：偏离 v0 的点击跳变，改相对拖拽微调友好）：
      - 点击只选中不跳变（避免想微调时点击突然变位置）
      - 水平相对拖拽（handle 从原位随拖动相对移，像旋钮）
      - 双击复位中点 50
    值域 0-100（与 pPan / AUD_MIX_PAN_MAX 契约一致），50=中点。
    左右语义：0=全左，100=全右（与 jamsoul 一致，不反转）。
    内部 m_dValue 用 double（像 v0 panValue float），panDev=(value-50)*2 取整显示步长 1。
    对外 value()/valueChanged 仍 int（0-100 协议不变）。
*/

#pragma once
#include <QWidget>
#include <cmath>

class JamonyPanBar : public QWidget
{
    Q_OBJECT

public:
    explicit JamonyPanBar ( QWidget* parent = nullptr );

    void setRange ( int iMin, int iMax );
    void setValue ( int iValue );        // 程序设值，不 emit（SetPanValue 回填用）
    int  value() const { return static_cast<int> ( std::lround ( m_dValue ) ); }

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
    int    m_iMin = 0;
    int    m_iMax = 100;
    double m_dValue = 50.0;       // 内部 double（让 panDev 显示步长 1，像 v0 panValue float）
    double m_dPressValue = 50.0;  // 拖拽起始（double）
    int    m_iPressX = 0;
    int    m_iLastEmitted = 50;   // 上次 emit 的 int 值（避免重复上行）
    bool   m_bDragging = false;
};
