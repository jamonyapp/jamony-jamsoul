/******************************************************************************\
* PedalWidget — 效果器机架单元 (v0 移植版)                                     *
\******************************************************************************/
/*
    自绘容器：圆角面板 + 顶部栏(LED/电源/名称/折叠) + 可折叠 body + 涂鸦装饰。
    对应 v0 components/rack/pedal.tsx + neon-decor.tsx。
    涂鸦用 QPainter 自绘（不依赖 QtSvg），7 种 variant。
*/

#pragma once
#include <QWidget>
#include <QColor>
#include <QVBoxLayout>

class LedWidget;
class PowerSwitch;
class FoldButton;
class QLabel;
class QPropertyAnimation;

class PedalWidget : public QWidget
{
    Q_OBJECT

public:
    enum Decor { None, Splash, Lines, Drips, Grid, Wave, Dots, Arc };

    explicit PedalWidget ( QWidget* parent = nullptr );

    void setName ( const QString& s );
    void setAccent ( const QColor& c );
    void setDecor ( Decor d ) { m_decor = d; update(); }
    void setPowerOn ( bool on );
    bool isPowerOn() const;
    void setActiveByPower ( bool on ); // 电源状态驱动 LED/涂鸦/body 透明度

    QVBoxLayout* bodyLayout() const { return m_pBodyLayout; } // 外部 addWidget 放旋钮/推子

signals:
    void powerToggled ( bool on );

protected:
    void paintEvent ( QPaintEvent* ) override;
    void showEvent ( QShowEvent* ) override; // 显示时同步 body 控件 active(构造时 body 空未同步)

private slots:
    void onFoldClicked();
    void onPowerToggled ( bool on );

private:
    void paintDecor ( QPainter& p );

    QWidget*             m_pHeader = nullptr;
    LedWidget*           m_pLed = nullptr;
    PowerSwitch*         m_pPower = nullptr;
    QLabel*              m_pNameLabel = nullptr;
    FoldButton*          m_pFoldBtn = nullptr;
    QWidget*             m_pBody = nullptr;
    QVBoxLayout*         m_pBodyLayout = nullptr;
    QPropertyAnimation*  m_pFoldAnim = nullptr;

    QString m_strName;
    QColor  m_accent = QColor ( "#00aaff" );
    Decor   m_decor = None;
    bool    m_bPowerOn = true;
    bool    m_bFolded = false;
};
