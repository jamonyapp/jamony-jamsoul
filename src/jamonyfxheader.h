/******************************************************************************\
* JamonyFxHeader — 机架标题卡 (v0 移植版)                                      *
\******************************************************************************/
/*
    "jamony FX RACK" 标题 + 缩小 logo 前置 + 8 段品牌渐变彩条。
    对应 v0 app/page.tsx 的 header。
*/

#pragma once
#include <QWidget>
#include <QPixmap>

class JamonyFxHeader : public QWidget
{
    Q_OBJECT

public:
    explicit JamonyFxHeader ( QWidget* parent = nullptr );
    QSize sizeHint() const override { return QSize ( 360, 40 ); }

protected:
    void paintEvent ( QPaintEvent* ) override;

private:
    QPixmap m_logo;
};
