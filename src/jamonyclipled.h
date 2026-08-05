/******************************************************************************\
* JamonyClipLed — v0 消波指示灯 (1:1 复刻 mixer-channel.tsx:138-149)            *
\******************************************************************************/
/*
    v0 消波灯：
      - 6px 高粉色条，全宽
      - clipped(On)：#FF33AA + boxShadow 0 0 6px #FF33AA 发光
      - 未消波(Off)：#1a1a1a 暗
      - 点击熄灭（复位）
    接 CLevelMeter：clip 检测命中→setOn(true)；20s 自动或点击→setOn(false)。
    形态从 LedWidget（圆形）改成圆角条（drawRoundedRect），色 #ff2d3f→#FF33AA，
    加 mousePressEvent（点击熄灭）+ QGraphicsDropShadowEffect（发光）。
*/

#pragma once
#include <QWidget>

class QGraphicsDropShadowEffect;

class JamonyClipLed : public QWidget
{
    Q_OBJECT

public:
    explicit JamonyClipLed ( QWidget* parent = nullptr );
    void setOn ( bool on );
    bool isOn() const { return m_bOn; }
    QSize sizeHint() const override { return QSize ( 76, 6 ); }

protected:
    void paintEvent ( QPaintEvent* ) override;
    void mousePressEvent ( QMouseEvent* ) override;     // 点击熄灭

signals:
    void clicked();    // 点击通知（CLevelMeter 接 → ClipReset 统一停 timer/清状态）

private:
    bool                       m_bOn = false;
    QGraphicsDropShadowEffect* m_glow = nullptr;
};
