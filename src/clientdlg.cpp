/******************************************************************************\
 * Copyright (c) 2004-2026
 *
 * Author(s):
 *  Volker Fischer
 *
 * As of Jamulus 3.12.1dev (commit eb172d47): All new source code contributions must be licensed
 * under AGPL 3.0 or any later version.
 *
 * Existing code: Code contributed before 3.12.1dev (commit eb172d47) was licensed under GPL 2.0+.
 * This code will be licensed under GPL 3.0 (or any later version) from
 * 3.12.1dev (commit eb172d47).  When distributed as part of Jamulus, the AGPL 3.0 terms govern
 * the combined work, including network use provisions.
 *
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * ---------------------------------------------------------------------------
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
\******************************************************************************/

#include "clientdlg.h"
#include "util.h"
#include "jamonyfxheader.h"
#include "jamonypedal.h"
#include "jamonyfader.h"
#include "jamonyrackwidgets.h"

#include <QWindow>
#ifdef Q_OS_MACOS
#include <objc/runtime.h>
#include <objc/message.h>
#endif

// jamony: 布局 dump 工具所需头文件（UI 调试基建）
#include <QFile>
#include <QTextStream>
#include <QBoxLayout>
#include <QGridLayout>
#include <QStackedLayout>
#include <QSpacerItem>
#include <QScrollArea>

/* Implementation *************************************************************/
CClientDlg::CClientDlg ( CClient*         pNCliP,
                         CClientSettings* pNSetP,
                         const QString&   strConnOnStartupAddress,
                         const bool       bNewShowComplRegConnList,
                         const bool       bShowAnalyzerConsole,
                         const bool       bMuteStream,
                         const bool       bNEnableIPv6,
                         QWidget*         parent ) :
    CBaseDlg ( parent, Qt::Window ), // use Qt::Window to get min/max window buttons
    pClient ( pNCliP ),
    pSettings ( pNSetP ),
    bConnectDlgWasShown ( false ),
    bDetectFeedback ( false ),
    bEnableIPv6 ( bNEnableIPv6 ),
    eLastRecorderState ( RS_UNDEFINED ), // for SetMixerBoardDeco
    eLastDesign ( GD_DEFAULT ),          //          "
    ClientSettingsDlg ( pNCliP, pNSetP, parent ),
    ChatDlg ( parent ),
    ConnectDlg ( pNSetP, bNewShowComplRegConnList, bNEnableIPv6, parent ),
    AnalyzerConsole ( pNCliP, parent ),
    m_pIpc ( nullptr )
{
    setupUi ( this );

    // jamony: A 栏 spacing 统一 6 (与 m_pRackLayout 效果器间距一致, 等距)
    verticalLayout_3->setSpacing ( 6 );

    // jamony: 效果器滚动区 (7 Pedal; 标题卡/pingWrap/butAutoAdjust 固定不滚动)
    m_pRackScroll = new QScrollArea ( this );
    m_pRackScroll->setWidgetResizable ( true );
    m_pRackScroll->setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );
    m_pRackScroll->setVerticalScrollBarPolicy ( Qt::ScrollBarAsNeeded );
    m_pRackScroll->setFrameShape ( QFrame::NoFrame );
    QWidget* pRackContent = new QWidget ( m_pRackScroll );
    m_pRackLayout = new QVBoxLayout ( pRackContent );
    m_pRackLayout->setContentsMargins ( 0, 0, 0, 0 );
    m_pRackLayout->setSpacing ( 6 );
    m_pRackScroll->setWidget ( pRackContent );
    m_pRackScroll->setMaximumHeight ( 634 ); // 下沿 720 = B 栏 L/R 文案底(对齐)

    // 机架标题卡 (固定, 不滚动)
    JamonyFxHeader* pFxHeader = new JamonyFxHeader ( this );
    verticalLayout_3->insertWidget ( 1, pFxHeader );
    pxlLogo->setVisible ( false );
    verticalLayout_3->insertWidget ( 2, m_pRackScroll ); // 标题卡后
    // 移除 verticalLayout_3 的弹性 spacer, 让 QScrollArea 占满
    for ( int i = verticalLayout_3->count() - 1; i >= 0; --i )
    {
        QLayoutItem* it = verticalLayout_3->itemAt ( i );
        if ( it && it->spacerItem() ) { delete verticalLayout_3->takeAt ( i ); }
    }

    // ping 1 行 + 边框, 移 A 栏最下 (参考标题卡样式; ping 控件 reparent 从 horizontalLayoutPingWrap)
    QFrame* pPingPanel = new QFrame ( this );
    pPingPanel->setObjectName ( "pingPanel" );
    pPingPanel->setStyleSheet ( "QFrame#pingPanel { background: #0d0d0d; border: 1px solid rgba(255,255,255,26); border-radius: 6px; }" );
    pPingPanel->setFixedHeight ( 22 ); // jamony: 卡片高=M按钮22, 底齐 frameLocalMute(底y=753)
    QHBoxLayout* pPingLay = new QHBoxLayout ( pPingPanel );
    pPingLay->setContentsMargins ( 8, 2, 8, 2 ); // 上下 2: 卡片高 22 = M 按钮高, 内部 16 一致
    pPingLay->setSpacing ( 4 );
    // LED 调小 (参考栏头 LED 10x10)
    ledDelay->setFixedSize ( 10, 10 );
    ledBuffers->setFixedSize ( 10, 10 );
    // 三组数据平均分布 (addStretch 首中尾)
    pPingLay->addStretch ( 1 );
    lblPing->setText ( "Ping:" );
    pPingLay->addWidget ( lblPing );
    pPingLay->addWidget ( lblPingVal );
    pPingLay->addWidget ( lblPingUnit );
    pPingLay->addStretch ( 1 );
    QLabel* pSep1 = new QLabel ( "│", pPingPanel ); pSep1->setStyleSheet ( "color: #444;" );
    pPingLay->addWidget ( pSep1 );
    pPingLay->addStretch ( 1 );
    lblDelay->setText ( "延迟:" );
    pPingLay->addWidget ( lblDelay );
    pPingLay->addWidget ( lblDelayVal );
    pPingLay->addWidget ( lblDelayUnit );
    pPingLay->addWidget ( ledDelay );
    pPingLay->addStretch ( 1 );
    QLabel* pSep2 = new QLabel ( "│", pPingPanel ); pSep2->setStyleSheet ( "color: #444;" );
    pPingLay->addWidget ( pSep2 );
    pPingLay->addStretch ( 1 );
    lblBuffers->setText ( "抖动:" ); // 原 "Jitter", 改中文; reparent 从 horizontalLayoutPingWrap
    pPingLay->addWidget ( lblBuffers );
    pPingLay->addWidget ( ledBuffers );
    pPingLay->addStretch ( 1 );
    verticalLayout_3->addWidget ( pPingPanel, 0, Qt::AlignBottom ); // jamony: 贴底(底齐B栏M按钮753), 高22
    // 清空 gridLayout 的 spacer (减 horizontalLayoutPingWrap 高 20→0; 保留 gridLayout 对象给 line ~2164)
    while ( gridLayout->count() > 0 ) { QLayoutItem* it = gridLayout->takeAt ( 0 ); if ( it ) { delete it; } }
    // 删两条水平分割线
    lineUpperLowerLeft->setVisible ( false );
    lineUpperLowerLeft_2->setVisible ( false );
    // butAutoAdjust 移到 C 区顶部 (与 A 栏 chbSettings 水平对齐 y=12; C 区 verticalLayout item 0)
    verticalLayout->insertWidget ( 0, butAutoAdjust, 0, Qt::AlignLeft );

    // jamony: Boost 接入 PedalWidget (首个效果器, 全链路验证)
    frameBoost->setVisible ( false );
    PedalWidget* pBoostPedal = new PedalWidget ( this );
    pBoostPedal->setName ( QStringLiteral ( "Clean Boost" ) );
    pBoostPedal->setAccent ( QColor ( "#00aaff" ) );
    pBoostPedal->setDecor ( PedalWidget::Splash );
    pBoostPedal->setPowerOn ( pClient->GetBoostEnabled() );
    JamonyFader* pBoostGain = new JamonyFader ( Qt::Horizontal, pBoostPedal );
    pBoostGain->setRange ( 0, AUD_BOOST_MAX );
    pBoostGain->setValue ( pClient->GetBoostLevel() );
    pBoostGain->setLabel ( tr ( "Gain" ) );
    pBoostGain->setAccent ( QColor ( "#00aaff" ) );
    pBoostGain->setDisplay ( [] ( int v ) -> QString {
        if ( v == 0 ) { return QStringLiteral ( "0 dB" ); }
        return QStringLiteral ( "+%1 dB" ).arg ( v * 18 / AUD_BOOST_MAX );
    } );
    pBoostPedal->bodyLayout()->addWidget ( pBoostGain );
    connect ( pBoostGain, &JamonyFader::valueChanged, this, &CClientDlg::OnAudioBoostValueChanged );
    connect ( pBoostPedal, &PedalWidget::powerToggled, this, &CClientDlg::OnBoostOnOffToggled );
    m_pRackLayout->addWidget ( pBoostPedal );

    // jamony: Overdrive 接入 PedalWidget (3 旋钮 Drive/Tone/Level, 全 0-100)
    frameOverdrive->setVisible ( false );
    PedalWidget* pOdPedal = new PedalWidget ( this );
    pOdPedal->setName ( QStringLiteral ( "Overdrive" ) );
    pOdPedal->setAccent ( QColor ( "#4c6eff" ) );
    pOdPedal->setDecor ( PedalWidget::Lines );
    pOdPedal->setPowerOn ( pClient->GetOverdriveEnabled() );
    QWidget*     pOdRow = new QWidget ( pOdPedal );
    QHBoxLayout* pOdLay = new QHBoxLayout ( pOdRow );
    pOdLay->setContentsMargins ( 0, 0, 0, 0 );
    pOdLay->setSpacing ( 8 );
    auto addOdKnob = [&] ( const QString& label, int value, int defVal, void ( CClientDlg::*slot )( int ) ) {
        JamonyKnob* k = new JamonyKnob ( pOdRow );
        k->setRange ( 0, AUD_OVERDRIVE_MAX );
        k->setValue ( value );
        k->setDefaultValue ( defVal );
        k->setLabel ( label );
        k->setAccent ( QColor ( "#4c6eff" ) );
        k->setDisplay ( [] ( int v ) { return QString::number ( v ); } );
        pOdLay->addWidget ( k );
        connect ( k, &JamonyKnob::valueChanged, this, slot );
    };
    addOdKnob ( tr ( "Drive" ), pClient->GetOverdriveDrive(), 0, &CClientDlg::OnOverdriveDriveChanged );
    addOdKnob ( tr ( "Tone" ), pClient->GetOverdriveTone(), AUD_OVERDRIVE_MAX / 2, &CClientDlg::OnOverdriveToneChanged );
    addOdKnob ( tr ( "Level" ), pClient->GetOverdriveLevel(), AUD_OVERDRIVE_MAX, &CClientDlg::OnOverdriveLevelChanged );
    pOdPedal->bodyLayout()->addWidget ( pOdRow );
    connect ( pOdPedal, &PedalWidget::powerToggled, this, &CClientDlg::OnOverdriveOnOffToggled );
    m_pRackLayout->addWidget ( pOdPedal );

    // jamony: Distortion 接入 PedalWidget (3 旋钮 Dis/Tone/Level, 全 0-100)
    frameDistortion->setVisible ( false );
    PedalWidget* pDistPedal = new PedalWidget ( this );
    pDistPedal->setName ( QStringLiteral ( "Distortion" ) );
    pDistPedal->setAccent ( QColor ( "#ff2d55" ) );
    pDistPedal->setDecor ( PedalWidget::Drips );
    pDistPedal->setPowerOn ( pClient->GetDistortionEnabled() );
    QWidget*     pDistRow = new QWidget ( pDistPedal );
    QHBoxLayout* pDistLay = new QHBoxLayout ( pDistRow );
    pDistLay->setContentsMargins ( 0, 0, 0, 0 );
    pDistLay->setSpacing ( 8 );
    auto addDistKnob = [&] ( const QString& label, int value, int defVal, void ( CClientDlg::*slot )( int ) ) {
        JamonyKnob* k = new JamonyKnob ( pDistRow );
        k->setRange ( 0, AUD_DISTORTION_MAX );
        k->setValue ( value );
        k->setDefaultValue ( defVal );
        k->setLabel ( label );
        k->setAccent ( QColor ( "#ff2d55" ) );
        k->setDisplay ( [] ( int v ) { return QString::number ( v ); } );
        pDistLay->addWidget ( k );
        connect ( k, &JamonyKnob::valueChanged, this, slot );
    };
    addDistKnob ( tr ( "Dis" ), pClient->GetDistortionDrive(), 0, &CClientDlg::OnDistortionDriveChanged );
    addDistKnob ( tr ( "Tone" ), pClient->GetDistortionTone(), AUD_DISTORTION_MAX / 2, &CClientDlg::OnDistortionToneChanged );
    addDistKnob ( tr ( "Level" ), pClient->GetDistortionLevel(), AUD_DISTORTION_MAX, &CClientDlg::OnDistortionLevelChanged );
    pDistPedal->bodyLayout()->addWidget ( pDistRow );
    connect ( pDistPedal, &PedalWidget::powerToggled, this, &CClientDlg::OnDistortionOnOffToggled );
    m_pRackLayout->addWidget ( pDistPedal );

    // jamony: EQ 接入 PedalWidget (9 垂直推子: 7 频段 + IN/OUT, ±dB 显示)
    frameEq->setVisible ( false );
    PedalWidget* pEqPedal = new PedalWidget ( this );
    pEqPedal->setName ( QStringLiteral ( "Equalizer" ) );
    pEqPedal->setAccent ( QColor ( "#cc33d4" ) );
    pEqPedal->setDecor ( PedalWidget::Grid );
    pEqPedal->setPowerOn ( pClient->GetEqEnabled() );
    QWidget*     pEqRow = new QWidget ( pEqPedal );
    QHBoxLayout* pEqLay = new QHBoxLayout ( pEqRow );
    pEqLay->setContentsMargins ( 0, 0, 0, 0 );
    // 7 段用 stretch 均匀填满左侧, IN/OUT 紧靠右, 不设固定 spacing
    auto eqDbDisplay = [] ( int v ) {
        const qreal db = ( v - 50 ) * 0.24; // 中 50=0dB, ±12dB
        if ( db >= 0 ) { return QStringLiteral ( "+%1" ).arg ( db, 0, 'f', 1 ); }
        return QString::number ( db, 'f', 1 );
    };
    static const char* eqBands[] = { "50", "120", "250", "500", "1.2k", "3k", "6k" };
    for ( int i = 0; i < 7; i++ )
    {
        JamonyFader* f = new JamonyFader ( Qt::Vertical, pEqRow );
        f->setRange ( 0, AUD_EQ_MAX );
        f->setValue ( pClient->GetEqBand ( i ) );
        f->setLabel ( QString::fromLatin1 ( eqBands[i] ) );
        f->setAccent ( QColor ( "#cc33d4" ) );
        f->setDisplay ( eqDbDisplay );
        f->setGrooveLength ( 96 );
        pEqLay->addWidget ( f );
        const int bandIdx = i;
        connect ( f, &JamonyFader::valueChanged, this, [this, bandIdx] ( int v ) { pClient->SetEqBand ( bandIdx, v ); } );
        if ( i < 6 ) { pEqLay->addStretch ( 2 ); } // 段间均匀
    }
    pEqLay->addStretch ( 3 ); // 7 段与 IN 之间明显间隔 (1.5 倍段间距)
    auto addEqIo = [&] ( const QString& label, int value, void ( CClient::*setter )( int ) ) {
        JamonyFader* f = new JamonyFader ( Qt::Vertical, pEqRow );
        f->setRange ( 0, AUD_EQ_MAX );
        f->setValue ( value );
        f->setLabel ( label );
        f->setAccent ( QColor ( "#cc33d4" ) );
        f->setDisplay ( eqDbDisplay );
        f->setGrooveLength ( 96 );
        pEqLay->addWidget ( f );
        connect ( f, &JamonyFader::valueChanged, this, [this, setter] ( int v ) { ( pClient->*setter )( v ); } );
    };
    addEqIo ( tr ( "IN" ), pClient->GetEqIn(), &CClient::SetEqIn );
    pEqLay->addStretch ( 1 ); // IN-OUT 间小间距
    addEqIo ( tr ( "OUT" ), pClient->GetEqOut(), &CClient::SetEqOut );
    pEqPedal->bodyLayout()->addWidget ( pEqRow );
    connect ( pEqPedal, &PedalWidget::powerToggled, this, &CClientDlg::OnEqOnOffToggled );
    m_pRackLayout->addWidget ( pEqPedal );

    // jamony: Chorus 接入 PedalWidget (3 旋钮 Rate[Hz]/Depth[ms]/Mix[0-100])
    frameChorus->setVisible ( false );
    PedalWidget* pChPedal = new PedalWidget ( this );
    pChPedal->setName ( QStringLiteral ( "Chorus" ) );
    pChPedal->setAccent ( QColor ( "#ff33aa" ) );
    pChPedal->setDecor ( PedalWidget::Wave );
    pChPedal->setPowerOn ( pClient->GetChorusEnabled() );
    QWidget*     pChRow = new QWidget ( pChPedal );
    QHBoxLayout* pChLay = new QHBoxLayout ( pChRow );
    pChLay->setContentsMargins ( 0, 0, 0, 0 );
    pChLay->setSpacing ( 8 );
    auto addChKnob = [&] ( const QString& label, int value, int defVal,
                           std::function<QString ( int )> display,
                           void ( CClientDlg::*slot )( int ) ) {
        JamonyKnob* k = new JamonyKnob ( pChRow );
        k->setRange ( 0, AUD_CHORUS_MAX );
        k->setValue ( value );
        k->setDefaultValue ( defVal );
        k->setLabel ( label );
        k->setAccent ( QColor ( "#ff33aa" ) );
        k->setDisplay ( display );
        pChLay->addWidget ( k );
        connect ( k, &JamonyKnob::valueChanged, this, slot );
    };
    addChKnob ( tr ( "Rate" ), pClient->GetChorusRate(), 40,
                [] ( int v ) { return QString::number ( 0.1 + v * 0.019, 'f', 2 ) + "Hz"; },
                &CClientDlg::OnChorusRateChanged );
    addChKnob ( tr ( "Depth" ), pClient->GetChorusDepth(), 50,
                [] ( int v ) { return QString::number ( v * 0.05, 'f', 1 ) + "ms"; },
                &CClientDlg::OnChorusDepthChanged );
    addChKnob ( tr ( "Mix" ), pClient->GetChorusMix(), 50,
                [] ( int v ) { return QString::number ( v ); },
                &CClientDlg::OnChorusMixChanged );
    pChPedal->bodyLayout()->addWidget ( pChRow );
    connect ( pChPedal, &PedalWidget::powerToggled, this, &CClientDlg::OnChorusOnOffToggled );
    m_pRackLayout->addWidget ( pChPedal );

    // jamony: Delay 接入 PedalWidget (3 旋钮 Time[ms]/Feedback[0-100]/Level[0-100])
    frameDelay->setVisible ( false );
    PedalWidget* pDelayPedal = new PedalWidget ( this );
    pDelayPedal->setName ( QStringLiteral ( "Delay" ) );
    pDelayPedal->setAccent ( QColor ( "#dd9055" ) );
    pDelayPedal->setDecor ( PedalWidget::Dots );
    pDelayPedal->setPowerOn ( pClient->GetDelayEnabled() );
    QWidget*     pDelayRow = new QWidget ( pDelayPedal );
    QHBoxLayout* pDelayLay = new QHBoxLayout ( pDelayRow );
    pDelayLay->setContentsMargins ( 0, 0, 0, 0 );
    pDelayLay->setSpacing ( 8 );
    auto addDelayKnob = [&] ( const QString& label, int value, int defVal,
                              std::function<QString ( int )> display,
                              void ( CClientDlg::*slot )( int ) ) {
        JamonyKnob* k = new JamonyKnob ( pDelayRow );
        k->setRange ( 0, AUD_DELAY_MAX );
        k->setValue ( value );
        k->setDefaultValue ( defVal );
        k->setLabel ( label );
        k->setAccent ( QColor ( "#dd9055" ) );
        k->setDisplay ( display );
        pDelayLay->addWidget ( k );
        connect ( k, &JamonyKnob::valueChanged, this, slot );
    };
    addDelayKnob ( tr ( "Time" ), pClient->GetDelayTime(), 35,
                   [] ( int v ) { return QString::number ( 50 + v * 550 / 100 ) + "ms"; },
                   &CClientDlg::OnDelayTimeChanged );
    addDelayKnob ( tr ( "Feedback" ), pClient->GetDelayFeedback(), 40,
                   [] ( int v ) { return QString::number ( v ); },
                   &CClientDlg::OnDelayFeedbackChanged );
    addDelayKnob ( tr ( "Level" ), pClient->GetDelayLevel(), 50,
                   [] ( int v ) { return QString::number ( v ); },
                   &CClientDlg::OnDelayLevelChanged );
    pDelayPedal->bodyLayout()->addWidget ( pDelayRow );
    connect ( pDelayPedal, &PedalWidget::powerToggled, this, &CClientDlg::OnDelayOnOffToggled );
    m_pRackLayout->addWidget ( pDelayPedal );

    // jamony: Reverb 接入 PedalWidget (3 旋钮 Pre[ms]/Decay[s]/Damp + Mix 推子 + L/R)
    // Reverb 在效果器链最后 → m_pRackLayout addWidget 顺序末尾(Delay 后)
    frameReverb->setVisible ( false );
    PedalWidget* pReverbPedal = new PedalWidget ( this );
    pReverbPedal->setName ( QStringLiteral ( "Reverb" ) );
    pReverbPedal->setAccent ( QColor ( "#bbee00" ) );
    pReverbPedal->setDecor ( PedalWidget::Arc );
    pReverbPedal->setPowerOn ( pClient->GetReverbEnabled() );
    QWidget*     pReverbKnobRow = new QWidget ( pReverbPedal );
    QHBoxLayout* pReverbKnobLay = new QHBoxLayout ( pReverbKnobRow );
    pReverbKnobLay->setContentsMargins ( 0, 0, 0, 0 );
    pReverbKnobLay->setSpacing ( 8 );
    auto addReverbKnob = [&] ( const QString& label, int value, int defVal, int maxVal,
                               std::function<QString ( int )> display,
                               void ( CClientDlg::*slot )( int ) ) {
        JamonyKnob* k = new JamonyKnob ( pReverbKnobRow );
        k->setRange ( 0, maxVal );
        k->setValue ( value );
        k->setDefaultValue ( defVal );
        k->setLabel ( label );
        k->setAccent ( QColor ( "#bbee00" ) );
        k->setDisplay ( display );
        pReverbKnobLay->addWidget ( k );
        connect ( k, &JamonyKnob::valueChanged, this, slot );
    };
    addReverbKnob ( tr ( "Pre" ), pClient->GetReverbPreDelay(), 0, AUD_REVERB_PREDELAY_MAX,
                    [] ( int v ) { return QString::number ( v * 150 / 100 ) + "ms"; },
                    &CClientDlg::OnReverbPreDelayChanged );
    addReverbKnob ( tr ( "Decay" ), pClient->GetReverbDecay(), 14, AUD_REVERB_DECAY_MAX,
                    [] ( int v ) { return QString::number ( 0.3 + v * 5.7 / 100, 'f', 1 ) + "s"; },
                    &CClientDlg::OnReverbDecayChanged );
    addReverbKnob ( tr ( "Damp" ), pClient->GetReverbDamping(), 24, AUD_REVERB_DAMPING_MAX,
                    [] ( int v ) { return QString::number ( v ); },
                    &CClientDlg::OnReverbDampingChanged );
    pReverbPedal->bodyLayout()->addWidget ( pReverbKnobRow );
    // Mix 推子 + L/R 单选 行
    QWidget*     pReverbMixRow = new QWidget ( pReverbPedal );
    QHBoxLayout* pReverbMixLay = new QHBoxLayout ( pReverbMixRow );
    pReverbMixLay->setContentsMargins ( 0, 0, 0, 0 );
    pReverbMixLay->setSpacing ( 12 );
    JamonyFader* pReverbMix = new JamonyFader ( Qt::Horizontal, pReverbMixRow );
    pReverbMix->setRange ( 0, AUD_REVERB_MAX );
    pReverbMix->setValue ( pClient->GetReverbLevel() );
    pReverbMix->setLabel ( tr ( "Mix" ) );
    pReverbMix->setAccent ( QColor ( "#bbee00" ) );
    pReverbMix->setDisplay ( [] ( int v ) { return QString::number ( v ); } );
    pReverbMixLay->addWidget ( pReverbMix );
    LrSelect* pReverbLr = new LrSelect ( pReverbMixRow );
    pReverbLr->setAccent ( QColor ( "#bbee00" ) );
    pReverbLr->setValue ( pClient->IsReverbOnLeftChan() );
    pReverbMixLay->addWidget ( pReverbLr );
    connect ( pReverbMix, &JamonyFader::valueChanged, this, &CClientDlg::OnAudioReverbValueChanged );
    connect ( pReverbLr, &LrSelect::valueChanged, this, [this] ( bool left ) { pClient->SetReverbOnLeftChan ( left ); } );
    pReverbPedal->bodyLayout()->addWidget ( pReverbMixRow );
    connect ( pReverbPedal, &PedalWidget::powerToggled, this, &CClientDlg::OnReverbOnOffToggled );
    m_pRackLayout->addWidget ( pReverbPedal ); // m_pRackLayout 末尾 = 效果器链最后
    m_pRackLayout->addStretch(); // 末尾弹性: 折叠后栏头集中, 下方空余(不被 layout 拉伸填充)

    // jamony: 窗口跟随 IPC (stdin 监听 jamony focus 指令, 跟随前置不抢焦点)
    m_pIpc = new JamsoulIpc ( this );
    connect ( m_pIpc, &JamsoulIpc::RaiseRequested, this, &CClientDlg::OnIpcRaise );
    connect ( m_pIpc, &JamsoulIpc::MoveRequested, this, &CClientDlg::OnIpcMove );
    m_pIpc->Start();

    // Add help text to controls -----------------------------------------------
    // input level meter
    QString strInpLevH = "<b>" + tr ( "Input Level Meter" ) + ":</b> " +
                         tr ( "This shows "
                              "the level of the two stereo channels "
                              "for your audio input." ) +
                         "<br>" +
                         tr ( "Make sure not to clip the input signal to avoid distortions of the "
                              "audio signal." );

    QString strInpLevHTT = tr ( "If the application "
                                "is connected to a server and "
                                "you play your instrument/sing into the microphone, the VU "
                                "meter should flicker. If this is not the case, you have "
                                "probably selected the wrong input channel (e.g. 'line in' instead "
                                "of the microphone input) or set the input gain too low in the "
                                "(Windows) audio mixer." ) +
                           "<br>" +
                           tr ( "For proper usage of the "
                                "application, you should not hear your singing/instrument through "
                                "the loudspeaker or your headphone when the software is not connected. "
                                "This can be achieved by muting your input audio channel in the "
                                "Playback mixer (not the Recording mixer!)." ) +
                           TOOLTIP_COM_END_TEXT;

    QString strInpLevHAccText  = tr ( "Input level meter" );
    QString strInpLevHAccDescr = tr ( "Simulates an analog LED level meter." );

    lblInputLEDMeter->setWhatsThis ( strInpLevH );
    lblInputLEDMeter->setText ( "Input" ); // jamony: 英文覆盖翻译(原"输入")
    lblInputLEDMeter->setFixedHeight ( 22 ); // jamony: 高=A栏音频设置按钮(22), 视觉等高
    lblInputLEDMeter->setAlignment ( Qt::AlignHCenter | Qt::AlignTop ); // jamony: 文案顶齐 A栏"音频设置"按钮文案(两文字顶均y=15: Input widget top15 == chbSettings widget12+border1+padding2)
    lblLevelMeterLeft->setWhatsThis ( strInpLevH );
    lblLevelMeterRight->setWhatsThis ( strInpLevH );
    lblLevelMeterLeft->setText ( "L" ); // jamony: 强制英文 L/R, 覆盖中文翻译(左/右)
    lblLevelMeterRight->setText ( "R" );
    lbrInputLevelL->setWhatsThis ( strInpLevH );
    lbrInputLevelL->setAccessibleName ( strInpLevHAccText );
    lbrInputLevelL->setAccessibleDescription ( strInpLevHAccDescr );
    lbrInputLevelL->setToolTip ( strInpLevHTT );
    lbrInputLevelL->setEnabled ( false );
    lbrInputLevelL->setFixedWidth ( 18 ); // jamony: 类 UI 同步 C 区分轨电平槽 18（原 sizeHint 20）
    lbrInputLevelR->setWhatsThis ( strInpLevH );
    lbrInputLevelR->setAccessibleName ( strInpLevHAccText );
    lbrInputLevelR->setAccessibleDescription ( strInpLevHAccDescr );
    lbrInputLevelR->setToolTip ( strInpLevHTT );
    lbrInputLevelR->setEnabled ( false );
    lbrInputLevelR->setFixedWidth ( 18 ); // jamony: 类 UI 同步 C 区分轨电平槽 18

    // reverberation level
    QString strAudReverb = "<b>" + tr ( "Reverb effect" ) + ":</b> " +
                           tr ( "Reverb can be applied to one local mono audio channel or to both "
                                "channels in stereo mode. The mono channel selection and the "
                                "reverb level can be modified. For example, if "
                                "a microphone signal is fed in to the right audio channel of the "
                                "sound card and a reverb effect needs to be applied, set the "
                                "channel selector to right and move the fader upwards until the "
                                "desired reverb level is reached." );

    lblAudioReverb->setWhatsThis ( strAudReverb );
    sldAudioReverb->setWhatsThis ( strAudReverb );

    sldAudioReverb->setAccessibleName ( tr ( "Reverb effect level setting" ) );

    // boost effect
    QString strAudBoost = "<b>" + tr ( "Boost effect" ) + ":</b> " +
                          tr ( "Clean boost applied to the signal before the reverb. "
                               "Continuous gain from 0 to +18 dB; a soft-knee limiter "
                               "prevents hard clipping as the signal approaches full scale." );

    lblAudioBoost->setWhatsThis ( strAudBoost );
    sldAudioBoost->setWhatsThis ( strAudBoost );
    lblBoostDb->setWhatsThis ( strAudBoost );

    sldAudioBoost->setAccessibleName ( tr ( "Boost effect level setting" ) );

    // reverberation channel selection
    QString strRevChanSel = "<b>" + tr ( "Reverb Channel Selection" ) + ":</b> " +
                            tr ( "With these radio buttons the audio input channel on which the "
                                 "reverb effect is applied can be chosen. Either the left "
                                 "or right input channel can be selected." );

    rbtReverbSelL->setWhatsThis ( strRevChanSel );
    rbtReverbSelL->setAccessibleName ( tr ( "Left channel selection for reverb" ) );
    rbtReverbSelR->setWhatsThis ( strRevChanSel );
    rbtReverbSelR->setAccessibleName ( tr ( "Right channel selection for reverb" ) );

    // delay LED
    QString strLEDDelay = "<b>" + tr ( "Delay Status LED" ) + ":</b> " + tr ( "Shows the current audio delay status:" ) +
                          "<ul>"
                          "<li>"
                          "<b>" +
                          tr ( "Green" ) + ":</b> " +
                          tr ( "The delay is perfect for a jam "
                               "session." ) +
                          "</li>"
                          "<li>"
                          "<b>" +
                          tr ( "Yellow" ) + ":</b> " +
                          tr ( "A session is still possible "
                               "but it may be harder to play." ) +
                          "</li>"
                          "<li>"
                          "<b>" +
                          tr ( "Red" ) + ":</b> " +
                          tr ( "The delay is too large for "
                               "jamming." ) +
                          "</li>"
                          "</ul>";

    lblDelay->setWhatsThis ( strLEDDelay );
    ledDelay->setWhatsThis ( strLEDDelay );
    ledDelay->setToolTip ( tr ( "If this LED indicator turns red, "
                                "you will not have much fun using %1." )
                               .arg ( APP_NAME ) +
                           TOOLTIP_COM_END_TEXT );

    ledDelay->setAccessibleName ( tr ( "Delay status LED indicator" ) );

    // buffers LED
    QString strLEDBuffers = "<b>" + tr ( "Local Jitter Buffer Status LED" ) + ":</b> " +
                            tr ( "The local jitter buffer status LED shows the current audio/streaming "
                                 "status. If the light is red, the audio stream is interrupted. "
                                 "This is caused by one of the following problems:" ) +
                            "<ul>"
                            "<li>" +
                            tr ( "The network jitter buffer is not large enough for the current "
                                 "network/audio interface jitter." ) +
                            "</li>"
                            "<li>" +
                            tr ( "The sound card's buffer delay (buffer size) is too small "
                                 "(see Settings window)." ) +
                            "</li>"
                            "<li>" +
                            tr ( "The upload or download stream rate is too high for your "
                                 "internet bandwidth." ) +
                            "</li>"
                            "<li>" +
                            tr ( "The CPU of the client or server is at 100%." ) +
                            "</li>"
                            "</ul>";

    lblBuffers->setWhatsThis ( strLEDBuffers );
    ledBuffers->setWhatsThis ( strLEDBuffers );
    ledBuffers->setToolTip ( tr ( "If this LED indicator turns red, "
                                  "the audio stream is interrupted." ) +
                             TOOLTIP_COM_END_TEXT );

    ledBuffers->setAccessibleName ( tr ( "Local Jitter Buffer status LED indicator" ) );

    // current connection status details
    QString strConnStats = "<b>" + tr ( "Current Connection Status" ) + ":</b> " +
                           tr ( "The Ping Time is the time required for the audio "
                                "stream to travel from the client to the server and back again. This "
                                "delay is introduced by the network and should be about "
                                "20-30 ms. If this delay is higher than about 50 ms, your distance to "
                                "the server is too large or your internet connection is not "
                                "sufficient." ) +
                           "<br>" +
                           tr ( "Overall Delay is calculated from the current Ping Time and the "
                                "delay introduced by the current buffer settings." );

    lblPing->setWhatsThis ( strConnStats );
    lblPingVal->setWhatsThis ( strConnStats );
    lblDelay->setWhatsThis ( strConnStats );
    lblDelayVal->setWhatsThis ( strConnStats );
    lblPingVal->setText ( "---" );
    lblPingUnit->setText ( "" );
    lblDelayVal->setText ( "---" );
    lblDelayUnit->setText ( "" );

    // init GUI design
    SetGUIDesign ( pClient->GetGUIDesign() );

    // MeterStyle init
    SetMeterStyle ( pClient->GetMeterStyle() );

    // set the settings pointer to the mixer board (must be done early)
    MainMixerBoard->SetSettingsPointer ( pSettings );
    MainMixerBoard->SetNumMixerPanelRows ( pSettings->iNumMixerPanelRows );

    // Pass through flag for MIDICtrlUsed
    MainMixerBoard->SetMIDICtrlUsed ( pSettings->bUseMIDIController );

    // reset mixer board
    MainMixerBoard->HideAll();

    // init status label
    OnTimerStatus();

    // init input level meter bars
    lbrInputLevelL->SetValue ( 0 );
    lbrInputLevelR->SetValue ( 0 );

    // init status LEDs
    ledBuffers->Reset();
    ledDelay->Reset();

    // init audio reverberation
    sldAudioReverb->setRange ( 0, AUD_REVERB_MAX );
    const int iCurAudReverb = pClient->GetReverbLevel();
    sldAudioReverb->setValue ( iCurAudReverb );
    sldAudioReverb->setTickInterval ( AUD_REVERB_MAX / 5 );

    // jamony: reverb 扩展旋钮 (Decay/PreDelay/Damping)
    knobReverbDecay->setRange ( 0, AUD_REVERB_DECAY_MAX );
    knobReverbDecay->setValue ( pClient->GetReverbDecay() );
    knobReverbDecay->setDefaultValue ( 14 ); // -> 1.1s (保持原听感, 0-100 下 14≈1.1s)
    knobReverbDecay->setLabel ( tr ( "Decay" ) );
    knobReverbPreDelay->setRange ( 0, AUD_REVERB_PREDELAY_MAX );
    knobReverbPreDelay->setValue ( pClient->GetReverbPreDelay() );
    knobReverbPreDelay->setDefaultValue ( 0 );
    knobReverbPreDelay->setLabel ( tr ( "Pre" ) );
    knobReverbDamping->setRange ( 0, AUD_REVERB_DAMPING_MAX );
    knobReverbDamping->setValue ( pClient->GetReverbDamping() );
    knobReverbDamping->setDefaultValue ( 24 ); // -> pole≈0.2 (保持原听感)
    knobReverbDamping->setLabel ( tr ( "Damp" ) );

    // init audio boost
    sldAudioBoost->setRange ( 0, AUD_BOOST_MAX );
    const int iCurAudBoost = pClient->GetBoostLevel();
    sldAudioBoost->setValue ( iCurAudBoost );
    sldAudioBoost->setTickInterval ( AUD_BOOST_MAX / 5 );
    if ( iCurAudBoost == 0 )
        lblBoostDb->setText ( "0 dB" );
    else
        lblBoostDb->setText ( QString ( "+%1 dB" ).arg ( iCurAudBoost * 18 / AUD_BOOST_MAX ) );

    // init audio overdrive knobs
    knobOverdriveDrive->setRange ( 0, AUD_OVERDRIVE_MAX );
    knobOverdriveDrive->setValue ( pClient->GetOverdriveDrive() );
    knobOverdriveDrive->setDefaultValue ( 0 );
    knobOverdriveDrive->setLabel ( tr ( "Drive" ) );
    knobOverdriveLevel->setRange ( 0, AUD_OVERDRIVE_MAX );
    knobOverdriveLevel->setValue ( pClient->GetOverdriveLevel() );
    knobOverdriveLevel->setDefaultValue ( AUD_OVERDRIVE_MAX ); // Level 默认 100%
    knobOverdriveLevel->setLabel ( tr ( "Level" ) );
    knobOverdriveTone->setRange ( 0, AUD_OVERDRIVE_MAX );
    knobOverdriveTone->setValue ( pClient->GetOverdriveTone() );
    knobOverdriveTone->setDefaultValue ( AUD_OVERDRIVE_MAX / 2 ); // Tone 默认中
    knobOverdriveTone->setLabel ( tr ( "Tone" ) );

    // init audio distortion knobs
    knobDistortionDrive->setRange ( 0, AUD_DISTORTION_MAX );
    knobDistortionDrive->setValue ( pClient->GetDistortionDrive() );
    knobDistortionDrive->setDefaultValue ( 0 );
    knobDistortionDrive->setLabel ( tr ( "dis" ) );
    knobDistortionLevel->setRange ( 0, AUD_DISTORTION_MAX );
    knobDistortionLevel->setValue ( pClient->GetDistortionLevel() );
    knobDistortionLevel->setDefaultValue ( AUD_DISTORTION_MAX ); // Level 默认 100%
    knobDistortionLevel->setLabel ( tr ( "Level" ) );
    knobDistortionTone->setRange ( 0, AUD_DISTORTION_MAX );
    knobDistortionTone->setValue ( pClient->GetDistortionTone() );
    knobDistortionTone->setDefaultValue ( AUD_DISTORTION_MAX / 2 ); // Tone 默认中
    knobDistortionTone->setLabel ( tr ( "Tone" ) );

    // init audio eq faders (7 bands + in/out, 垂直 QSlider, 中点 50=0dB)
    QSlider* eqBandSliders[AUD_EQ_BANDS] = { sldEqBand0, sldEqBand1, sldEqBand2, sldEqBand3, sldEqBand4, sldEqBand5, sldEqBand6 };
    for ( int i = 0; i < AUD_EQ_BANDS; i++ ) { eqBandSliders[i]->setValue ( pClient->GetEqBand ( i ) ); }
    sldEqIn->setValue ( pClient->GetEqIn() );
    sldEqOut->setValue ( pClient->GetEqOut() );

    // init audio chorus knobs (Rate/Depth/Mix)
    knobChorusRate->setRange ( 0, AUD_CHORUS_MAX );
    knobChorusRate->setValue ( pClient->GetChorusRate() );
    knobChorusRate->setDefaultValue ( 40 );
    knobChorusRate->setLabel ( tr ( "Rate" ) );
    knobChorusDepth->setRange ( 0, AUD_CHORUS_MAX );
    knobChorusDepth->setValue ( pClient->GetChorusDepth() );
    knobChorusDepth->setDefaultValue ( AUD_CHORUS_MAX / 2 );
    knobChorusDepth->setLabel ( tr ( "Depth" ) );
    knobChorusMix->setRange ( 0, AUD_CHORUS_MAX );
    knobChorusMix->setValue ( pClient->GetChorusMix() );
    knobChorusMix->setDefaultValue ( AUD_CHORUS_MAX / 2 );
    knobChorusMix->setLabel ( tr ( "Mix" ) );

    // init audio delay knobs (Time/Feedback/Level)
    knobDelayTime->setRange ( 0, AUD_DELAY_MAX );
    knobDelayTime->setValue ( pClient->GetDelayTime() );
    knobDelayTime->setDefaultValue ( 35 );
    knobDelayTime->setLabel ( tr ( "Time" ) );
    knobDelayFeedback->setRange ( 0, AUD_DELAY_MAX );
    knobDelayFeedback->setValue ( pClient->GetDelayFeedback() );
    knobDelayFeedback->setDefaultValue ( 40 );
    knobDelayFeedback->setLabel ( tr ( "Fb" ) );
    knobDelayLevel->setRange ( 0, AUD_DELAY_MAX );
    knobDelayLevel->setValue ( pClient->GetDelayLevel() );
    knobDelayLevel->setDefaultValue ( AUD_DELAY_MAX / 2 );
    knobDelayLevel->setLabel ( tr ( "Level" ) );

    // init effect on/off buttons + LED (boost / overdrive / reverb)
    btnBoostOnOff->setChecked ( pClient->GetBoostEnabled() );
    ledBoost->SetLight ( pClient->GetBoostEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
    btnOverdriveOnOff->setChecked ( pClient->GetOverdriveEnabled() );
    ledOverdrive->SetLight ( pClient->GetOverdriveEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
    btnDistortionOnOff->setChecked ( pClient->GetDistortionEnabled() );
    ledDistortion->SetLight ( pClient->GetDistortionEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
    btnEqOnOff->setChecked ( pClient->GetEqEnabled() );
    ledEq->SetLight ( pClient->GetEqEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
    btnChorusOnOff->setChecked ( pClient->GetChorusEnabled() );
    ledChorus->SetLight ( pClient->GetChorusEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
    btnDelayOnOff->setChecked ( pClient->GetDelayEnabled() );
    ledDelayEff->SetLight ( pClient->GetDelayEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
    btnReverbOnOff->setChecked ( pClient->GetReverbEnabled() );
    ledReverb->SetLight ( pClient->GetReverbEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );

    // init input boost
    pClient->SetInputBoost ( pSettings->iInputBoost );

    // init reverb channel
    UpdateRevSelection();

    // init connect dialog
    ConnectDlg.SetShowAllMusicians ( pSettings->bConnectDlgShowAllMusicians );

    // set window title (with no clients connected -> "0")
    SetMyWindowTitle ( 0 );

    // track number of clients to detect joins/leaves for audio alerts
    iClients = 0;

    // prepare Mute Myself info label (invisible by default)
    lblGlobalInfoLabel->setStyleSheet ( ".QLabel { color: #FF3366; font: bold 13px; }" ); // 取消背景框, 红字直接显示
    lblGlobalInfoLabel->setAlignment ( Qt::AlignLeft | Qt::AlignVCenter ); // 和 C 区左对齐(butAutoAdjust 一致)
    lblGlobalInfoLabel->hide();

    // prepare update check info label (invisible by default)
    lblUpdateCheck->setOpenExternalLinks ( true ); // enables opening a web browser if one clicks on a html link
    lblUpdateCheck->setText ( "<font color=\"red\"><b>" + APP_UPGRADE_AVAILABLE_MSG_TEXT.arg ( APP_NAME ).arg ( VERSION ) + "</b></font>" );
    lblUpdateCheck->hide();

    // setup timers
    TimerCheckAudioDeviceOk.setSingleShot ( true ); // only check once after connection
    TimerDetectFeedback.setSingleShot ( true );

    // Connect on startup ------------------------------------------------------
    if ( !strConnOnStartupAddress.isEmpty() )
    {
        // initiate connection (always show the address in the mixer board
        // (no alias))
        Connect ( strConnOnStartupAddress, strConnOnStartupAddress );
    }

    // File menu  --------------------------------------------------------------
    QMenu* pFileMenu = new QMenu ( tr ( "&File" ), this );

    //pFileMenu->addAction ( tr ( "&Connection Setup..." ), this, SLOT ( OnOpenConnectionSetupDialog() ), QKeySequence ( Qt::CTRL + Qt::Key_C ) );

    pFileMenu->addSeparator();

    pFileMenu->addAction ( tr ( "&Load Mixer Channels Setup..." ), this, SLOT ( OnLoadChannelSetup() ) );

    pFileMenu->addAction ( tr ( "&Save Mixer Channels Setup..." ), this, SLOT ( OnSaveChannelSetup() ) );

    pFileMenu->addSeparator();

    pFileMenu->addAction ( tr ( "E&xit" ), this, SLOT ( close() ), QKeySequence ( Qt::CTRL + Qt::Key_Q ) );

    // Edit menu  --------------------------------------------------------------
    QMenu* pEditMenu = new QMenu ( tr ( "&Edit" ), this );

    pEditMenu->addAction ( tr ( "Clear &All Stored Solo and Mute Settings" ), this, SLOT ( OnClearAllStoredSoloMuteSettings() ) );

    pEditMenu->addAction ( tr ( "Set All Faders to New Client &Level" ),
                           this,
                           SLOT ( OnSetAllFadersToNewClientLevel() ),
                           QKeySequence ( Qt::CTRL + Qt::Key_L ) );

    pEditMenu->addAction ( tr ( "Auto-Adjust all &Faders" ), this, SLOT ( OnAutoAdjustAllFaderLevels() ), QKeySequence ( Qt::CTRL + Qt::Key_F ) );

    // View menu  --------------------------------------------------------------
    QMenu* pViewMenu = new QMenu ( tr ( "&View" ), this );

    // own fader first option: works from server version 3.5.5 which supports sending client ID back to client
    QAction* OwnFaderFirstAction =
        pViewMenu->addAction ( tr ( "O&wn Fader First" ), this, SLOT ( OnOwnFaderFirst() ), QKeySequence ( Qt::CTRL + Qt::Key_W ) );

    pViewMenu->addSeparator();

    QAction* NoSortAction =
        pViewMenu->addAction ( tr ( "N&o User Sorting" ), this, SLOT ( OnNoSortChannels() ), QKeySequence ( Qt::CTRL + Qt::Key_O ) );

    QAction* ByNameAction =
        pViewMenu->addAction ( tr ( "Sort Users by &Name" ), this, SLOT ( OnSortChannelsByName() ), QKeySequence ( Qt::CTRL + Qt::Key_N ) );

    QAction* ByInstrAction = pViewMenu->addAction ( tr ( "Sort Users by &Instrument" ),
                                                    this,
                                                    SLOT ( OnSortChannelsByInstrument() ),
                                                    QKeySequence ( Qt::CTRL + Qt::Key_I ) );

    QAction* ByGroupAction =
        pViewMenu->addAction ( tr ( "Sort Users by &Group" ), this, SLOT ( OnSortChannelsByGroupID() ), QKeySequence ( Qt::CTRL + Qt::Key_G ) );

    QAction* ByCityAction =
        pViewMenu->addAction ( tr ( "Sort Users by &City" ), this, SLOT ( OnSortChannelsByCity() ), QKeySequence ( Qt::CTRL + Qt::Key_T ) );

    QAction* ByChannelAction =
        pViewMenu->addAction ( tr ( "Sort Users by Chann&el" ), this, SLOT ( OnSortChannelsByChannel() ), QKeySequence ( Qt::CTRL + Qt::Key_E ) );

    OwnFaderFirstAction->setCheckable ( true );
    OwnFaderFirstAction->setChecked ( pSettings->bOwnFaderFirst );

    // the sorting menu entries shall be checkable and exclusive
    QActionGroup* SortActionGroup = new QActionGroup ( this );
    SortActionGroup->setExclusive ( true );
    NoSortAction->setCheckable ( true );
    SortActionGroup->addAction ( NoSortAction );
    ByNameAction->setCheckable ( true );
    SortActionGroup->addAction ( ByNameAction );
    ByInstrAction->setCheckable ( true );
    SortActionGroup->addAction ( ByInstrAction );
    ByGroupAction->setCheckable ( true );
    SortActionGroup->addAction ( ByGroupAction );
    ByCityAction->setCheckable ( true );
    SortActionGroup->addAction ( ByCityAction );
    ByChannelAction->setCheckable ( true );
    SortActionGroup->addAction ( ByChannelAction );

    // initialize sort type setting (i.e., recover stored setting)
    switch ( pSettings->eChannelSortType )
    {
    case ST_BY_NAME:
        ByNameAction->setChecked ( true );
        break;
    case ST_BY_INSTRUMENT:
        ByInstrAction->setChecked ( true );
        break;
    case ST_BY_GROUPID:
        ByGroupAction->setChecked ( true );
        break;
    case ST_BY_CITY:
        ByCityAction->setChecked ( true );
        break;
    case ST_BY_SERVER_CHANNEL:
        ByChannelAction->setChecked ( true );
        break;
    default: // ST_NO_SORT
        NoSortAction->setChecked ( true );
        break;
    }
    MainMixerBoard->SetFaderSorting ( pSettings->eChannelSortType );

    pViewMenu->addSeparator();

    //pViewMenu->addAction ( tr ( "C&hat..." ), this, SLOT ( OnOpenChatDialog() ), QKeySequence ( Qt::CTRL + Qt::Key_H ) );

    // optionally show analyzer console entry
    if ( bShowAnalyzerConsole )
    {
        pViewMenu->addAction ( tr ( "&Analyzer Console..." ), this, SLOT ( OnOpenAnalyzerConsole() ) );
    }

    pViewMenu->addSeparator();

    // Settings menu  --------------------------------------------------------------
    QMenu* pSettingsMenu = new QMenu ( tr ( "Sett&ings" ), this );

    // jamony: 我的信息由 jamony 用户系统管理，移除 Profile 菜单项
    //pSettingsMenu->addAction ( tr ( "My &Profile..." ), this, SLOT ( OnOpenUserProfileSettings() ), QKeySequence ( Qt::CTRL + Qt::Key_P ) );

    pSettingsMenu->addAction ( tr ( "Audio/Network &Settings..." ), this, SLOT ( OnOpenAudioNetSettings() ), QKeySequence ( Qt::CTRL + Qt::Key_S ) );

    pSettingsMenu->addAction ( tr ( "A&dvanced Settings..." ), this, SLOT ( OnOpenAdvancedSettings() ), QKeySequence ( Qt::CTRL + Qt::Key_D ) );

    pSettingsMenu->addAction (
        tr ( "&MIDI Control Settings..." ),
        this,
        [this] { ShowGeneralSettings ( SETTING_TAB_MIDI ); },
        QKeySequence ( Qt::CTRL + Qt::Key_M ) );

    // jamony: 菜单栏全删（File/Edit/View/Settings 通过主界面按钮直达；减法 + LSUIElement 隐藏 dock 无菜单栏副作用）
    // QMenuBar* pMenu = new QMenuBar ( this );
    // pMenu->setStyleSheet ( ... );
    // pMenu->addMenu ( pFileMenu/pEditMenu/pViewMenu/pSettingsMenu );
    // layout()->setMenuBar ( pMenu );

    // Window positions --------------------------------------------------------
    // main window
    if ( !pSettings->vecWindowPosMain.isEmpty() && !pSettings->vecWindowPosMain.isNull() )
    {
        restoreGeometry ( pSettings->vecWindowPosMain );
    }

    // settings window
    if ( !pSettings->vecWindowPosSettings.isEmpty() && !pSettings->vecWindowPosSettings.isNull() )
    {
        ClientSettingsDlg.restoreGeometry ( pSettings->vecWindowPosSettings );
    }

    if ( pSettings->bWindowWasShownSettings )
    {
        ShowGeneralSettings ( pSettings->iSettingsTab );
    }

    // chat window
    if ( !pSettings->vecWindowPosChat.isEmpty() && !pSettings->vecWindowPosChat.isNull() )
    {
        ChatDlg.restoreGeometry ( pSettings->vecWindowPosChat );
    }

    if ( pSettings->bWindowWasShownChat )
    {
        ShowChatWindow();
    }

    // connection setup window
    if ( !pSettings->vecWindowPosConnect.isEmpty() && !pSettings->vecWindowPosConnect.isNull() )
    {
        ConnectDlg.restoreGeometry ( pSettings->vecWindowPosConnect );
    }

    // Connections -------------------------------------------------------------
    // push buttons
    QObject::connect ( butAutoAdjust, &QPushButton::clicked, this, &CClientDlg::OnAutoAdjustAllFaderLevels ); // jamony: 自动调整分轨信号电平

    // check boxes
    QObject::connect ( chbSettings, &QCheckBox::stateChanged, this, &CClientDlg::OnSettingsStateChanged );

    QObject::connect ( chbLocalMute, &QPushButton::toggled, this, [this](bool c){ OnLocalMuteStateChanged(c?Qt::Checked:0); } ); // jamony: QCheckBox→QPushButton, lambda适配int信号

    // timers
    QObject::connect ( &TimerSigMet, &QTimer::timeout, this, &CClientDlg::OnTimerSigMet );

    QObject::connect ( &TimerBuffersLED, &QTimer::timeout, this, &CClientDlg::OnTimerBuffersLED );

    QObject::connect ( &TimerStatus, &QTimer::timeout, this, &CClientDlg::OnTimerStatus );

    QObject::connect ( &TimerPing, &QTimer::timeout, this, &CClientDlg::OnTimerPing );

    QObject::connect ( &TimerCheckAudioDeviceOk, &QTimer::timeout, this, &CClientDlg::OnTimerCheckAudioDeviceOk );

    QObject::connect ( &TimerDetectFeedback, &QTimer::timeout, this, &CClientDlg::OnTimerDetectFeedback );

    QObject::connect ( sldAudioReverb, &QSlider::valueChanged, this, &CClientDlg::OnAudioReverbValueChanged );

    // jamony: reverb 扩展旋钮 (Decay/PreDelay/Damping)
    QObject::connect ( knobReverbDecay,    &JamonyKnob::valueChanged, this, &CClientDlg::OnReverbDecayChanged );
    QObject::connect ( knobReverbPreDelay, &JamonyKnob::valueChanged, this, &CClientDlg::OnReverbPreDelayChanged );
    QObject::connect ( knobReverbDamping,  &JamonyKnob::valueChanged, this, &CClientDlg::OnReverbDampingChanged );

    QObject::connect ( sldAudioBoost, &QSlider::valueChanged, this, &CClientDlg::OnAudioBoostValueChanged );

    // overdrive knobs
    QObject::connect ( knobOverdriveDrive, &JamonyKnob::valueChanged, this, &CClientDlg::OnOverdriveDriveChanged );
    QObject::connect ( knobOverdriveLevel, &JamonyKnob::valueChanged, this, &CClientDlg::OnOverdriveLevelChanged );
    QObject::connect ( knobOverdriveTone, &JamonyKnob::valueChanged, this, &CClientDlg::OnOverdriveToneChanged );

    // distortion knobs
    QObject::connect ( knobDistortionDrive, &JamonyKnob::valueChanged, this, &CClientDlg::OnDistortionDriveChanged );
    QObject::connect ( knobDistortionLevel, &JamonyKnob::valueChanged, this, &CClientDlg::OnDistortionLevelChanged );
    QObject::connect ( knobDistortionTone, &JamonyKnob::valueChanged, this, &CClientDlg::OnDistortionToneChanged );

    // eq faders (7 bands + in/out, lambda 带索引)
    QObject::connect ( sldEqBand0, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqBand ( 0, v ); } );
    QObject::connect ( sldEqBand1, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqBand ( 1, v ); } );
    QObject::connect ( sldEqBand2, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqBand ( 2, v ); } );
    QObject::connect ( sldEqBand3, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqBand ( 3, v ); } );
    QObject::connect ( sldEqBand4, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqBand ( 4, v ); } );
    QObject::connect ( sldEqBand5, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqBand ( 5, v ); } );
    QObject::connect ( sldEqBand6, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqBand ( 6, v ); } );
    QObject::connect ( sldEqIn, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqIn ( v ); } );
    QObject::connect ( sldEqOut, &QSlider::valueChanged, this, [this]( int v ){ pClient->SetEqOut ( v ); } );

    // chorus knobs
    QObject::connect ( knobChorusRate, &JamonyKnob::valueChanged, this, &CClientDlg::OnChorusRateChanged );
    QObject::connect ( knobChorusDepth, &JamonyKnob::valueChanged, this, &CClientDlg::OnChorusDepthChanged );
    QObject::connect ( knobChorusMix, &JamonyKnob::valueChanged, this, &CClientDlg::OnChorusMixChanged );

    // delay knobs
    QObject::connect ( knobDelayTime, &JamonyKnob::valueChanged, this, &CClientDlg::OnDelayTimeChanged );
    QObject::connect ( knobDelayFeedback, &JamonyKnob::valueChanged, this, &CClientDlg::OnDelayFeedbackChanged );
    QObject::connect ( knobDelayLevel, &JamonyKnob::valueChanged, this, &CClientDlg::OnDelayLevelChanged );

    // effect on/off buttons (boost / overdrive / reverb)
    QObject::connect ( btnBoostOnOff, &QPushButton::toggled, this, &CClientDlg::OnBoostOnOffToggled );
    QObject::connect ( btnOverdriveOnOff, &QPushButton::toggled, this, &CClientDlg::OnOverdriveOnOffToggled );
    QObject::connect ( btnDistortionOnOff, &QPushButton::toggled, this, &CClientDlg::OnDistortionOnOffToggled );
    QObject::connect ( btnEqOnOff, &QPushButton::toggled, this, &CClientDlg::OnEqOnOffToggled );
    QObject::connect ( btnChorusOnOff, &QPushButton::toggled, this, &CClientDlg::OnChorusOnOffToggled );
    QObject::connect ( btnDelayOnOff, &QPushButton::toggled, this, &CClientDlg::OnDelayOnOffToggled );
    QObject::connect ( btnReverbOnOff, &QPushButton::toggled, this, &CClientDlg::OnReverbOnOffToggled );

    // radio buttons
    QObject::connect ( rbtReverbSelL, &QRadioButton::clicked, this, &CClientDlg::OnReverbSelLClicked );

    QObject::connect ( rbtReverbSelR, &QRadioButton::clicked, this, &CClientDlg::OnReverbSelRClicked );

    // other
    QObject::connect ( pClient, &CClient::ConClientListMesReceived, this, &CClientDlg::OnConClientListMesReceived );

    QObject::connect ( pClient, &CClient::Disconnected, this, &CClientDlg::OnDisconnected );

    QObject::connect ( pClient, &CClient::ChatTextReceived, this, &CClientDlg::OnChatTextReceived );

    QObject::connect ( pClient, &CClient::ClientIDReceived, this, &CClientDlg::OnClientIDReceived );

    QObject::connect ( pClient, &CClient::MuteStateHasChangedReceived, this, &CClientDlg::OnMuteStateHasChangedReceived );

    QObject::connect ( pClient, &CClient::RecorderStateReceived, this, &CClientDlg::OnRecorderStateReceived );

    // This connection is a special case. On receiving a licence required message via the
    // protocol, a modal licence dialog is opened. Since this blocks the thread, we need
    // a queued connection to make sure the core protocol mechanism is not blocked, too.
    qRegisterMetaType<ELicenceType> ( "ELicenceType" );
    QObject::connect ( pClient, &CClient::LicenceRequired, this, &CClientDlg::OnLicenceRequired, Qt::QueuedConnection );

    QObject::connect ( pClient, &CClient::PingTimeReceived, this, &CClientDlg::OnPingTimeResult );

    QObject::connect ( pClient, &CClient::CLServerListReceived, this, &CClientDlg::OnCLServerListReceived );

    QObject::connect ( pClient, &CClient::CLRedServerListReceived, this, &CClientDlg::OnCLRedServerListReceived );

    QObject::connect ( pClient, &CClient::CLConnClientsListMesReceived, this, &CClientDlg::OnCLConnClientsListMesReceived );

    QObject::connect ( pClient, &CClient::CLPingTimeWithNumClientsReceived, this, &CClientDlg::OnCLPingTimeWithNumClientsReceived );

    QObject::connect ( pClient, &CClient::ControllerInFaderLevel, this, &CClientDlg::OnControllerInFaderLevel );

    QObject::connect ( pClient, &CClient::ControllerInPanValue, this, &CClientDlg::OnControllerInPanValue );

    QObject::connect ( pClient, &CClient::ControllerInFaderIsSolo, this, &CClientDlg::OnControllerInFaderIsSolo );

    QObject::connect ( pClient, &CClient::ControllerInFaderIsMute, this, &CClientDlg::OnControllerInFaderIsMute );

    QObject::connect ( pClient, &CClient::ControllerInMuteMyself, this, &CClientDlg::OnControllerInMuteMyself );

    QObject::connect ( pClient, &CClient::CLChannelLevelListReceived, this, &CClientDlg::OnCLChannelLevelListReceived );

    QObject::connect ( pClient, &CClient::VersionAndOSReceived, this, &CClientDlg::OnVersionAndOSReceived );

    QObject::connect ( pClient, &CClient::CLVersionAndOSReceived, this, &CClientDlg::OnCLVersionAndOSReceived );

    QObject::connect ( pClient, &CClient::SoundDeviceChanged, this, &CClientDlg::OnSoundDeviceChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::GUIDesignChanged, this, &CClientDlg::OnGUIDesignChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::MeterStyleChanged, this, &CClientDlg::OnMeterStyleChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::AudioChannelsChanged, this, &CClientDlg::OnAudioChannelsChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::CustomDirectoriesChanged, &ConnectDlg, &CConnectDlg::OnCustomDirectoriesChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::NumMixerPanelRowsChanged, this, &CClientDlg::OnNumMixerPanelRowsChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::MIDIControllerUsageChanged, this, &CClientDlg::OnMIDIControllerUsageChanged );

    QObject::connect ( this, &CClientDlg::SendTabChange, &ClientSettingsDlg, &CClientSettingsDlg::OnMakeTabChange );

    QObject::connect ( MainMixerBoard, &CAudioMixerBoard::ChangeChanGain, this, &CClientDlg::OnChangeChanGain );

    QObject::connect ( MainMixerBoard, &CAudioMixerBoard::ChangeChanPan, this, &CClientDlg::OnChangeChanPan );

    QObject::connect ( MainMixerBoard, &CAudioMixerBoard::NumClientsChanged, this, &CClientDlg::OnNumClientsChanged );

    QObject::connect ( &ChatDlg, &CChatDlg::NewLocalInputText, this, &CClientDlg::OnNewLocalInputText );

    QObject::connect ( &ConnectDlg, &CConnectDlg::ReqServerListQuery, this, &CClientDlg::OnReqServerListQuery );

    // note that this connection must be a queued connection, otherwise the server list ping
    // times are not accurate and the client list may not be retrieved for all servers listed
    // (it seems the sendto() function needs to be called from different threads to fire the
    // packet immediately and do not collect packets before transmitting)
    QObject::connect ( &ConnectDlg, &CConnectDlg::CreateCLServerListPingMes, this, &CClientDlg::OnCreateCLServerListPingMes, Qt::QueuedConnection );

    QObject::connect ( &ConnectDlg, &CConnectDlg::CreateCLServerListReqVerAndOSMes, this, &CClientDlg::OnCreateCLServerListReqVerAndOSMes );

    QObject::connect ( &ConnectDlg,
                       &CConnectDlg::CreateCLServerListReqConnClientsListMes,
                       this,
                       &CClientDlg::OnCreateCLServerListReqConnClientsListMes );

    QObject::connect ( &ConnectDlg, &CConnectDlg::accepted, this, &CClientDlg::OnConnectDlgAccepted );

    // Initializations which have to be done after the signals are connected ---
    // start timer for status bar
    TimerStatus.start ( LED_BAR_UPDATE_TIME_MS );

    // restore connect dialog
    if ( pSettings->bWindowWasShownConnect )
    {
        ShowConnectionSetupDialog();
    }

    // mute stream on startup (must be done after the signal connections)
    if ( bMuteStream )
    {
        chbLocalMute->setChecked ( true );
    }

    // jamony: 移除 jamulus 自动更新检查（jamsoul 版本由 jamony 管理，不连 jamulus 更新服务器）
    // CHostAddress UpdateServerHostAddress;
    // if ( NetworkUtil::ParseNetworkAddressBare ( UPDATECHECK1_ADDRESS, UpdateServerHostAddress, bEnableIPv6 ) )
    // {
    //     pClient->CreateCLServerListReqVerAndOSMes ( UpdateServerHostAddress );
    // }
    // if ( NetworkUtil::ParseNetworkAddressBare ( UPDATECHECK2_ADDRESS, UpdateServerHostAddress, bEnableIPv6 ) )
    // {
    //     pClient->CreateCLServerListReqVerAndOSMes ( UpdateServerHostAddress );
    // }
}

// jamony: 全量布局 dump 辅助（file-local，UI 调试基建）——递归打印 layout 树
//         每个控件打印 geometry/sizeHint/min/max/sizePolicy，每个 layout 打印 stretch/margin/spacing
namespace {

void dumpIndent ( QTextStream& s, int depth )
{
    for ( int i = 0; i < depth; ++i ) s << "  ";
}

void dumpLayoutRecursive ( QTextStream& s, QLayout* pLayout, int depth, QWidget* pRoot );

void dumpWidget ( QTextStream& s, QWidget* pWidget, int depth, QWidget* pRoot )
{
    if ( !pWidget ) return;
    const QRect        g  ( pWidget->geometry() );
    const QPoint       tl ( pWidget->mapTo ( pRoot, QPoint ( 0, 0 ) ) );
    const QSize        sh ( pWidget->sizeHint() );
    const QSize        mn ( pWidget->minimumSize() );
    const QSize        mx ( pWidget->maximumSize() );
    const QSizePolicy  sp ( pWidget->sizePolicy() );
    const QColor       winCol ( pWidget->palette().color ( pWidget->backgroundRole() ) ); // jamony: dump 诊断背景色

    dumpIndent ( s, depth );
    s << "[W] " << pWidget->metaObject()->className()
      << " name=\"" << pWidget->objectName() << "\""
      << " parent_geom=" << g.x() << "," << g.y() << " " << g.width() << "x" << g.height()
      << " root_xy=" << tl.x() << "," << tl.y()
      << " sizeHint=" << sh.width() << "x" << sh.height()
      << " minSize=" << mn.width() << "x" << mn.height()
      << " maxSize=" << mx.width() << "x" << mx.height()
      << " sizePolicy(hpol=" << static_cast<int> ( sp.horizontalPolicy() )
      << " vpol=" << static_cast<int> ( sp.verticalPolicy() )
      << " hstr=" << sp.horizontalStretch() << " vstr=" << sp.verticalStretch() << ")"
      << " visible=" << pWidget->isVisible() << " hidden=" << pWidget->isHidden()
      << " autoFillBg=" << pWidget->autoFillBackground()
      << " styledBg=" << pWidget->testAttribute ( Qt::WA_StyledBackground )
      << " winRgb=" << winCol.red() << "," << winCol.green() << "," << winCol.blue()
      << " ownSS=" << ( pWidget->styleSheet().isEmpty() ? 0 : 1 )
      << "\n";

    if ( pWidget->layout() )
    {
        dumpLayoutRecursive ( s, pWidget->layout(), depth + 1, pRoot );
    }

    // jamony: QScrollArea 的 widget() 不在 layout 里, 单独遍历(fader列)
    if ( QScrollArea* pSA = qobject_cast<QScrollArea*> ( pWidget ) )
    {
        if ( pSA->widget() )
        {
            dumpWidget ( s, pSA->widget(), depth + 1, pRoot );
        }
    }
}

void dumpLayoutRecursive ( QTextStream& s, QLayout* pLayout, int depth, QWidget* pRoot )
{
    if ( !pLayout ) return;
    const QRect     g ( pLayout->geometry() );
    const QMargins  m ( pLayout->contentsMargins() );

    dumpIndent ( s, depth );
    s << "[L] " << pLayout->metaObject()->className()
      << " name=\"" << pLayout->objectName() << "\""
      << " count=" << pLayout->count()
      << " spacing=" << pLayout->spacing()
      << " margins=" << m.left() << "/" << m.top() << "/" << m.right() << "/" << m.bottom()
      << " geom=" << g.x() << "," << g.y() << " " << g.width() << "x" << g.height()
      << "\n";

    QBoxLayout*     pBox   = qobject_cast<QBoxLayout*> ( pLayout );
    QGridLayout*    pGrid  = qobject_cast<QGridLayout*> ( pLayout );
    QStackedLayout* pStack = qobject_cast<QStackedLayout*> ( pLayout );

    if ( pStack )
    {
        dumpIndent ( s, depth + 1 );
        s << "(stack currentIndex=" << pStack->currentIndex() << ")\n";
    }

    for ( int i = 0; i < pLayout->count(); ++i )
    {
        QLayoutItem* pItem = pLayout->itemAt ( i );
        if ( !pItem ) continue;

        if ( pItem->widget() )
        {
            dumpWidget ( s, pItem->widget(), depth + 1, pRoot );
        }
        else if ( pItem->spacerItem() )
        {
            QSpacerItem* pSp = pItem->spacerItem();
            dumpIndent ( s, depth + 1 );
            s << "[S] spacer sizeHint=" << pSp->sizeHint().width() << "x" << pSp->sizeHint().height()
              << " sizePolicy=" << static_cast<int> ( pSp->sizePolicy().horizontalPolicy() )
              << "/" << static_cast<int> ( pSp->sizePolicy().verticalPolicy() ) << "\n";
        }
        else if ( pItem->layout() )
        {
            dumpLayoutRecursive ( s, pItem->layout(), depth + 1, pRoot );
        }

        if ( pBox )
        {
            dumpIndent ( s, depth + 1 );
            s << "(item " << i << " stretch=" << pBox->stretch ( i ) << ")\n";
        }
        if ( pGrid )
        {
            int row, col, rowSpan, colSpan;
            pGrid->getItemPosition ( i, &row, &col, &rowSpan, &colSpan );
            dumpIndent ( s, depth + 1 );
            s << "(grid item " << i << " row=" << row << " col=" << col
              << " rowSpan=" << rowSpan << " colSpan=" << colSpan << ")\n";
        }
    }
}

} // namespace

// jamony: 跟随前置, 用原生 NSWindow orderFront: (前置不 makeKey, 不抢焦点, jamony 可操作)
void CClientDlg::OnIpcRaise()
{
#ifdef Q_OS_MACOS
    if ( QWindow* w = windowHandle() )
    {
        id nsView = reinterpret_cast<id> ( w->winId() );
        // [nsView window] → NSWindow
        id nsWindow = reinterpret_cast<id ( * )( id, SEL )> ( objc_msgSend ) ( nsView, sel_registerName ( "window" ) );
        // [nsWindow orderFront:nil] — 前置但不 makeKey (不抢焦点)
        reinterpret_cast<void ( * )( id, SEL, id )> ( objc_msgSend ) ( nsWindow, sel_registerName ( "orderFront:" ), nil );
    }
#else
    raise();
#endif
}

void CClientDlg::showEvent ( QShowEvent* Event )
{
    // jamony: 首次显示后 dump 完整布局树到 /tmp/jamsoul-layout-dump.txt（UI 调试基建）
    // 用 singleShot(0) 等首帧 layout 完成，此时 geometry/sizeHint 是真实值
    Q_UNUSED ( Event )
    QTimer::singleShot ( 3000, this, [this]() { // jamony: 延迟3秒等fader创建完再dump
        QFile f ( "/tmp/jamsoul-layout-dump.txt" );
        if ( f.open ( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
        {
            QTextStream s ( &f );
            s << "=== jamsoul layout dump (root=" << metaObject()->className() << ") ===\n";
            if ( this->layout() )
            {
                dumpLayoutRecursive ( s, this->layout(), 0, this );
            }
            s << "=== end ===\n";
            s.flush();
        }
    } );
}

void CClientDlg::closeEvent ( QCloseEvent* Event )
{
    // jamony: 连接中时弹确认（防误关 jamsoul）；统一文案告知后果（唯一合奏者将解散，路由由前端判定）
    if ( pClient->IsRunning() )
    {
        QMessageBox::StandardButton reply = QMessageBox::question ( this, tr ( "退出 jamsoul" ),
            tr ( "退出 jamsoul 将断开音频连接，若你是唯一合奏者将解散房间，确认退出？" ), QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
        if ( reply != QMessageBox::Yes )
        {
            Event->ignore();
            return;
        }
    }

    // store window positions
    pSettings->vecWindowPosMain     = saveGeometry();
    pSettings->vecWindowPosSettings = ClientSettingsDlg.saveGeometry();
    pSettings->vecWindowPosChat     = ChatDlg.saveGeometry();
    pSettings->vecWindowPosConnect  = ConnectDlg.saveGeometry();

    pSettings->bWindowWasShownSettings = ClientSettingsDlg.isVisible();
    pSettings->bWindowWasShownChat     = ChatDlg.isVisible();
    pSettings->bWindowWasShownConnect  = ConnectDlg.isVisible();

    // if settings/connect dialog or chat dialog is open, close it
    ClientSettingsDlg.close();
    ChatDlg.close();
    ConnectDlg.close();
    AnalyzerConsole.close();

    // if connected, terminate connection
    if ( pClient->IsRunning() )
    {
        pClient->Stop();
    }

    // make sure all current fader settings are applied to the settings struct
    MainMixerBoard->StoreAllFaderSettings();

    pSettings->bConnectDlgShowAllMusicians = ConnectDlg.GetShowAllMusicians();
    pSettings->eChannelSortType            = MainMixerBoard->GetFaderSorting();
    pSettings->iNumMixerPanelRows          = MainMixerBoard->GetNumMixerPanelRows();

    // default implementation of this event handler routine
    Event->accept();
}

void CClientDlg::ManageDragNDrop ( QDropEvent* Event, const bool bCheckAccept )
{
    // we only want to use drag'n'drop with file URLs
    QListIterator<QUrl> UrlIterator ( Event->mimeData()->urls() );

    while ( UrlIterator.hasNext() )
    {
        const QString strFileNameWithPath = UrlIterator.next().toLocalFile();

        // check all given URLs and if any has the correct suffix
        if ( !strFileNameWithPath.isEmpty() && ( QFileInfo ( strFileNameWithPath ).suffix() == MIX_SETTINGS_FILE_SUFFIX ) )
        {
            if ( bCheckAccept )
            {
                // only accept drops of supports file types
                Event->acceptProposedAction();
            }
            else
            {
                // load the first valid settings file and leave the loop
                pSettings->LoadFaderSettings ( strFileNameWithPath );
                MainMixerBoard->LoadAllFaderSettings();
                break;
            }
        }
    }
}

void CClientDlg::UpdateRevSelection()
{
    if ( pClient->GetAudioChannels() == CC_STEREO )
    {
        // for stereo make channel selection invisible since
        // reverberation effect is always applied to both channels
        rbtReverbSelL->setVisible ( false );
        rbtReverbSelR->setVisible ( false );
    }
    else
    {
        // make radio buttons visible
        rbtReverbSelL->setVisible ( true );
        rbtReverbSelR->setVisible ( true );

        // update value
        if ( pClient->IsReverbOnLeftChan() )
        {
            rbtReverbSelL->setChecked ( true );
        }
        else
        {
            rbtReverbSelR->setChecked ( true );
        }
    }

    // jamony: 显示Pan旋钮(控制该分轨本地播放的左右声像)
    MainMixerBoard->SetDisplayPans ( true );
}

void CClientDlg::OnConnectDlgAccepted()
{
    // We had an issue that the accepted signal was emit twice if a list item was double
    // clicked in the connect dialog. To avoid this we introduced a flag which makes sure
    // we process the accepted signal only once after the dialog was initially shown.
    if ( bConnectDlgWasShown )
    {
        // get the address from the connect dialog
        QString strSelectedAddress = ConnectDlg.GetSelectedAddress();

        // only store new host address in our data base if the address is
        // not empty and it was not a server list item (only the addresses
        // typed in manually are stored by definition)
        if ( !strSelectedAddress.isEmpty() && !ConnectDlg.GetServerListItemWasChosen() )
        {
            // store new address at the top of the list, if the list was already
            // full, the last element is thrown out
            pSettings->vstrIPAddress.StringFiFoWithCompare ( strSelectedAddress );
        }

        // get name to be set in audio mixer group box title
        QString strMixerBoardLabel;

        if ( ConnectDlg.GetServerListItemWasChosen() )
        {
            // in case a server in the server list was chosen,
            // display the server name of the server list
            strMixerBoardLabel = ConnectDlg.GetSelectedServerName();
        }
        else
        {
            // an item of the server address combo box was chosen,
            // just show the address string as it was entered by the
            // user
            strMixerBoardLabel = strSelectedAddress;

            // special case: if the address is empty, we substitute the default
            // directory address so that a user who just pressed the connect
            // button without selecting an item in the table or manually entered an
            // address gets a successful connection
            if ( strSelectedAddress.isEmpty() )
            {
                strSelectedAddress = DEFAULT_SERVER_ADDRESS;
                strMixerBoardLabel = tr ( "%1 Directory" ).arg ( DirectoryTypeToString ( AT_DEFAULT ) );
            }
        }

        // first check if we are already connected, if this is the case we have to
        // disconnect the old server first
        if ( pClient->IsRunning() )
        {
            Disconnect();
        }

        // initiate connection
        Connect ( strSelectedAddress, strMixerBoardLabel );

        // reset flag
        bConnectDlgWasShown = false;
    }
}

void CClientDlg::OnConnectDisconBut()
{
    // the connect/disconnect button implements a toggle functionality
    if ( pClient->IsRunning() )
    {
        Disconnect();
        SetMixerBoardDeco ( RS_UNDEFINED, pClient->GetGUIDesign() );
    }
    else
    {
        ShowConnectionSetupDialog();
    }
}

void CClientDlg::OnClearAllStoredSoloMuteSettings()
{
    // if we are in an active connection, we first have to store all fader settings in
    // the settings struct, clear the solo and mute states and then apply the settings again
    MainMixerBoard->StoreAllFaderSettings();
    pSettings->vecStoredFaderIsSolo.Reset ( false );
    pSettings->vecStoredFaderIsMute.Reset ( false );
    MainMixerBoard->LoadAllFaderSettings();
}

void CClientDlg::OnLoadChannelSetup()
{
    QString strFileName = QFileDialog::getOpenFileName ( this, tr ( "Select Channel Setup File" ), "", QString ( "*." ) + MIX_SETTINGS_FILE_SUFFIX );

    if ( !strFileName.isEmpty() )
    {
        // first update the settings struct and then update the mixer panel
        pSettings->LoadFaderSettings ( strFileName );
        MainMixerBoard->LoadAllFaderSettings();
    }
}

void CClientDlg::OnSaveChannelSetup()
{
    QString strFileName = QFileDialog::getSaveFileName ( this, tr ( "Select Channel Setup File" ), "", QString ( "*." ) + MIX_SETTINGS_FILE_SUFFIX );

    if ( !strFileName.isEmpty() )
    {
        // first store all current fader settings (in case we are in an active connection
        // right now) and then save the information in the settings struct in the file
        MainMixerBoard->StoreAllFaderSettings();
        pSettings->SaveFaderSettings ( strFileName );
    }
}

void CClientDlg::OnVersionAndOSReceived ( COSUtil::EOpSystemType, QString strVersion )
{
    // check if Pan is supported by the server (minimum version is 3.5.4)
#if QT_VERSION >= QT_VERSION_CHECK( 5, 6, 0 )
    if ( QVersionNumber::compare ( QVersionNumber::fromString ( strVersion ), QVersionNumber ( 3, 5, 4 ) ) >= 0 )
    {
        MainMixerBoard->SetPanIsSupported();
    }
#endif
}

void CClientDlg::OnCLVersionAndOSReceived ( CHostAddress InetAddr, COSUtil::EOpSystemType, QString strVersion )
{
    // if connect dialog showing, pass version to it, and don't do update check
    if ( bConnectDlgWasShown )
    {
        // display version in connect dialog
        ConnectDlg.SetServerVersionResult ( InetAddr, strVersion );
    }
    else
    {
        // update check
#if ( QT_VERSION >= QT_VERSION_CHECK( 5, 6, 0 ) ) && !defined( DISABLE_VERSION_CHECK )
        int            mySuffixIndex;
        QVersionNumber myVersion = QVersionNumber::fromString ( VERSION, &mySuffixIndex );

        int            serverSuffixIndex;
        QVersionNumber serverVersion = QVersionNumber::fromString ( strVersion, &serverSuffixIndex );

        // only compare if the server version has no suffix (such as dev or beta)
        if ( strVersion.size() == serverSuffixIndex && QVersionNumber::compare ( serverVersion, myVersion ) > 0 )
        {
            // jamony: 移除更新提醒（不 show lblUpdateCheck）
            // lblUpdateCheck->show();
            // QTimer::singleShot ( 60000, [this]() { lblUpdateCheck->hide(); } );
        }
#endif
    }
}

void CClientDlg::OnChatTextReceived ( QString strChatText )
{
    if ( pSettings->bEnableAudioAlerts )
    {
        QSoundEffect* sf = new QSoundEffect();
        sf->setSource ( QUrl::fromLocalFile ( ":sounds/res/sounds/new_message.wav" ) );
        sf->play();
    }
    ChatDlg.AddChatText ( strChatText );

    // Open chat dialog. If a server welcome message is received, we force
    // the dialog to be upfront in case a licence text is shown. For all
    // other new chat texts we do not want to force the dialog to be upfront
    // always when a new message arrives since this is annoying.
    ShowChatWindow ( ( strChatText.indexOf ( WELCOME_MESSAGE_PREFIX ) == 0 ) );

    UpdateDisplay();
}

void CClientDlg::OnLicenceRequired ( ELicenceType eLicenceType )
{
    // right now only the creative common licence is supported
    if ( eLicenceType == LT_CREATIVECOMMONS )
    {
        CLicenceDlg LicenceDlg;

        // mute the client output stream
        pClient->SetMuteOutStream ( true );

        // Open the licence dialog and check if the licence was accepted. In
        // case the dialog is just closed or the decline button was pressed,
        // disconnect from that server.
        if ( !LicenceDlg.exec() )
        {
            Disconnect();
        }

        // unmute the client output stream if local mute button is not pressed
        if ( !chbLocalMute->isChecked() )
        {
            pClient->SetMuteOutStream ( false );
        }
    }
}

void CClientDlg::OnConClientListMesReceived ( CVector<CChannelInfo> vecChanInfo )
{
    // update mixer board with the additional client infos
    MainMixerBoard->ApplyNewConClientList ( vecChanInfo );
}

void CClientDlg::OnNumClientsChanged ( int iNewNumClients )
{
    if ( pSettings->bEnableAudioAlerts && iNewNumClients > iClients )
    {
        QSoundEffect* sf = new QSoundEffect();
        sf->setSource ( QUrl::fromLocalFile ( ":sounds/res/sounds/new_user.wav" ) );
        sf->play();
    }

    // iNewNumClients will be zero on the first trigger of this signal handler when connecting to a new server.
    // Subsequent triggers will thus sound the alert (if enabled).
    iClients = iNewNumClients;

    // update window title
    SetMyWindowTitle ( iNewNumClients );
}

void CClientDlg::OnOpenAudioNetSettings() { ShowGeneralSettings ( SETTING_TAB_AUDIONET ); }

void CClientDlg::OnOpenAdvancedSettings() { ShowGeneralSettings ( SETTING_TAB_ADVANCED ); }

void CClientDlg::OnOpenUserProfileSettings() { ShowGeneralSettings ( SETTING_TAB_USER ); }

void CClientDlg::SetMyWindowTitle ( const int iNumClients )
{
    // jamony: 排除 jamony-looper（系统录音客户端，不计入用户数显示）
    const int iDisplayClients = std::max ( 0, iNumClients - 1 );

    // set the window title (and therefore also the task bar icon text of the OS)
    // according to the following specification (#559):
    // <ServerName> - <N> users - Jamulus
    QString    strWinTitle;
    const bool bClientNameIsUsed = !pClient->strClientName.isEmpty();

    if ( bClientNameIsUsed )
    {
        // if --clientname is used, the APP_NAME must be the very first word in
        // the title, otherwise some user scripts do not work anymore, see #789
        strWinTitle += QString ( APP_NAME ) + " - " + pClient->strClientName + " ";
    }

    if ( iDisplayClients == 0 )
    {
        // only application name
        if ( !bClientNameIsUsed ) // if --clientname is used, the APP_NAME is the first word in title
        {
            strWinTitle += QString ( APP_NAME );
        }
    }
    else
    {
        // jamony: 不显示服务器地址（安全，不暴露 IP:port）
        // strWinTitle += MainMixerBoard->GetServerName();

        if ( iDisplayClients == 1 )
        {
            strWinTitle += " - 1 " + tr ( "user" );
        }
        else if ( iDisplayClients > 1 )
        {
            strWinTitle += " - " + QString::number ( iDisplayClients ) + " " + tr ( "users" );
        }

        if ( !bClientNameIsUsed ) // if --clientname is used, the APP_NAME is the first word in title
        {
            strWinTitle += " - " + QString ( APP_NAME );
        }
    }

    setWindowTitle ( strWinTitle );

#if defined( Q_OS_MACOS )
    // for MacOS only we show the number of connected clients as a
    // badge label text if more than one user is connected
    if ( iDisplayClients > 1 )
    {
        // show the number of connected clients
        SetMacBadgeLabelText ( QString ( "%1" ).arg ( iDisplayClients ) );
    }
    else
    {
        // clear the text (apply an empty string)
        SetMacBadgeLabelText ( "" );
    }
#endif
}

void CClientDlg::ShowConnectionSetupDialog()
{
    // show connect dialog
    bConnectDlgWasShown = true;
    ConnectDlg.show();
    ConnectDlg.setWindowTitle ( MakeClientNameTitle ( tr ( "Connect" ), pClient->strClientName ) );

    // make sure dialog is upfront and has focus
    ConnectDlg.raise();
    ConnectDlg.activateWindow();
}

void CClientDlg::ShowGeneralSettings ( int iTab )
{
    // open general settings dialog
    emit SendTabChange ( iTab );
    ClientSettingsDlg.show();
    ClientSettingsDlg.setWindowTitle ( MakeClientNameTitle ( tr ( "Settings" ), pClient->strClientName ) );

    // make sure dialog is upfront and has focus
    ClientSettingsDlg.raise();
    ClientSettingsDlg.activateWindow();
}

void CClientDlg::ShowChatWindow ( const bool bForceRaise )
{
    ChatDlg.show();
    ChatDlg.setWindowTitle ( MakeClientNameTitle ( tr ( "Chat" ), pClient->strClientName ) );

    if ( bForceRaise )
    {
        // make sure dialog is upfront and has focus
        ChatDlg.showNormal();
        ChatDlg.raise();
        ChatDlg.activateWindow();
    }

    UpdateDisplay();
}

void CClientDlg::ShowAnalyzerConsole()
{
    // open analyzer console dialog
    AnalyzerConsole.show();

    // make sure dialog is upfront and has focus
    AnalyzerConsole.raise();
    AnalyzerConsole.activateWindow();
}

void CClientDlg::OnSettingsStateChanged ( int value )
{
    if ( value == Qt::Checked )
    {
        ShowGeneralSettings ( SETTING_TAB_AUDIONET );
    }
    else
    {
        ClientSettingsDlg.hide();
    }
}

void CClientDlg::OnChatStateChanged ( int value )
{
    if ( value == Qt::Checked )
    {
        ShowChatWindow();
    }
    else
    {
        ChatDlg.hide();
    }
}

void CClientDlg::OnLocalMuteStateChanged ( int value )
{
    pClient->SetMuteOutStream ( value == Qt::Checked );

    // jamony: 隐藏"已静音"文案(M按钮悬停已有 tooltip 说明, C 区不再显示)
    lblGlobalInfoLabel->hide();
}

void CClientDlg::OnTimerSigMet()
{
    // show current level
    lbrInputLevelL->SetValue ( pClient->GetLevelForMeterdBLeft() );
    lbrInputLevelR->SetValue ( pClient->GetLevelForMeterdBRight() );

    if ( bDetectFeedback &&
         ( pClient->GetLevelForMeterdBLeft() > NUM_STEPS_LED_BAR - 0.5 || pClient->GetLevelForMeterdBRight() > NUM_STEPS_LED_BAR - 0.5 ) )
    {
        // mute locally and mute channel
        chbLocalMute->setChecked ( true );
        MainMixerBoard->MuteMyChannel();

        // show message box about feedback issue
        QCheckBox* chb = new QCheckBox ( tr ( "Enable feedback detection" ) );
        chb->setCheckState ( pSettings->bEnableFeedbackDetection ? Qt::Checked : Qt::Unchecked );
        QMessageBox msgbox;
        msgbox.setText ( tr ( "Audio feedback or loud signal detected.\n\n"
                              "We muted your channel and activated 'Mute Myself'. Please solve "
                              "the feedback issue first and unmute yourself afterwards." ) );
        msgbox.setIcon ( QMessageBox::Icon::Warning );
        msgbox.addButton ( QMessageBox::Ok );
        msgbox.setDefaultButton ( QMessageBox::Ok );
        msgbox.setCheckBox ( chb );

        QObject::connect ( chb, &QCheckBox::stateChanged, this, &CClientDlg::OnFeedbackDetectionChanged );

        msgbox.exec();
    }
}

void CClientDlg::OnTimerBuffersLED()
{
    CMultiColorLED::ELightColor eCurStatus;

    // get and reset current buffer state and set LED from that flag
    if ( pClient->GetAndResetbJitterBufferOKFlag() )
    {
        eCurStatus = CMultiColorLED::RL_GREEN;
    }
    else
    {
        eCurStatus = CMultiColorLED::RL_RED;
    }

    // update the buffer LED and the general settings dialog, too
    ledBuffers->SetLight ( eCurStatus );
}

void CClientDlg::OnTimerPing()
{
    // send ping message to the server
    pClient->CreateCLPingMes();
}

void CClientDlg::OnPingTimeResult ( int iPingTime )
{
    // calculate overall delay
    const int iOverallDelayMs = pClient->EstimatedOverallDelay ( iPingTime );

    // color definition: <= 43 ms green, <= 68 ms yellow, otherwise red
    CMultiColorLED::ELightColor eOverallDelayLEDColor;

    if ( iOverallDelayMs <= 43 )
    {
        eOverallDelayLEDColor = CMultiColorLED::RL_GREEN;
    }
    else
    {
        if ( iOverallDelayMs <= 68 )
        {
            eOverallDelayLEDColor = CMultiColorLED::RL_YELLOW;
        }
        else
        {
            eOverallDelayLEDColor = CMultiColorLED::RL_RED;
        }
    }

    // only update delay information on settings dialog if it is visible to
    // avoid CPU load on working thread if not necessary
    if ( ClientSettingsDlg.isVisible() )
    {
        // set ping time result to general settings dialog
        ClientSettingsDlg.UpdateUploadRate();
    }
    SetPingTime ( iPingTime, iOverallDelayMs, eOverallDelayLEDColor );

    // update delay LED on the main window
    ledDelay->SetLight ( eOverallDelayLEDColor );
}

void CClientDlg::OnTimerCheckAudioDeviceOk()
{
    // check if the audio device entered the audio callback after a pre-defined
    // timeout to check if a valid device is selected and if we do not have
    // fundamental settings errors (in which case the GUI would only show that
    // it is trying to connect the server which does not help to solve the problem (#129))
    if ( !pClient->IsCallbackEntered() )
    {
        QMessageBox::warning ( this,
                               APP_NAME,
                               tr ( "Your sound card is not working correctly. "
                                    "Please open the settings dialog and check the device selection and the driver settings." ) );
    }
}

void CClientDlg::OnTimerDetectFeedback() { bDetectFeedback = false; }

void CClientDlg::OnSoundDeviceChanged ( QString strError )
{
    if ( !strError.isEmpty() )
    {
        // the sound device setup has a problem, disconnect any active connection
        if ( pClient->IsRunning() )
        {
            Disconnect();
        }

        // show the error message of the device setup
        QMessageBox::critical ( this, APP_NAME, strError, tr ( "Ok" ), nullptr );
    }

    // if the check audio device timer is running, it must be restarted on a device change
    if ( TimerCheckAudioDeviceOk.isActive() )
    {
        TimerCheckAudioDeviceOk.start ( CHECK_AUDIO_DEV_OK_TIME_MS );
    }

    if ( pSettings->bEnableFeedbackDetection && TimerDetectFeedback.isActive() )
    {
        TimerDetectFeedback.start ( DETECT_FEEDBACK_TIME_MS );
        bDetectFeedback = true;
    }

    // update the settings dialog
    ClientSettingsDlg.UpdateSoundDeviceChannelSelectionFrame();
}

void CClientDlg::OnCLPingTimeWithNumClientsReceived ( CHostAddress InetAddr, int iPingTime, int iNumClients )
{
    // update connection dialog server list
    ConnectDlg.SetPingTimeAndNumClientsResult ( InetAddr, iPingTime, iNumClients );
}

void CClientDlg::Connect ( const QString& strSelectedAddress, const QString& strMixerBoardLabel )
{
    // set address and check if address is valid
    if ( pClient->SetServerAddr ( strSelectedAddress ) )
    {
        // try to start client, if error occurred, do not go in
        // running state but show error message
        try
        {
            if ( !pClient->IsRunning() )
            {
                pClient->Start();
            }
        }

        catch ( const CGenErr& generr )
        {
            // show error message and return the function
            QMessageBox::critical ( this, APP_NAME, generr.GetErrorText(), "Close", nullptr );
            return;
        }

        // hide label connect to server
        lblConnectToServer->hide();
        lbrInputLevelL->setEnabled ( true );
        lbrInputLevelR->setEnabled ( true );

        // set server name in audio mixer group box title
        MainMixerBoard->SetServerName ( strMixerBoardLabel );

        // start timer for level meter bar and ping time measurement
        TimerSigMet.start ( LEVELMETER_UPDATE_TIME_MS );
        TimerBuffersLED.start ( BUFFER_LED_UPDATE_TIME_MS );
        TimerPing.start ( PING_UPDATE_TIME_MS );
        TimerCheckAudioDeviceOk.start ( CHECK_AUDIO_DEV_OK_TIME_MS ); // is single shot timer

        // audio feedback detection
        if ( pSettings->bEnableFeedbackDetection )
        {
            TimerDetectFeedback.start ( DETECT_FEEDBACK_TIME_MS ); // single shot timer
            bDetectFeedback = true;
        }
    }
}

void CClientDlg::Disconnect()
{
    // only stop client if currently running, in case we received
    // the stopped message, the client is already stopped but the
    // connect/disconnect button and other GUI controls must be
    // updated
    if ( pClient->IsRunning() )
    {
        pClient->Stop();
    }

    // reset server name in audio mixer group box title
    MainMixerBoard->SetServerName ( "" );

    // stop timer for level meter bars and reset them
    TimerSigMet.stop();
    lbrInputLevelL->setEnabled ( false );
    lbrInputLevelR->setEnabled ( false );
    lbrInputLevelL->SetValue ( 0 );
    lbrInputLevelR->SetValue ( 0 );

    // show connect to server message
    lblConnectToServer->show();

    // stop other timers
    TimerBuffersLED.stop();
    TimerPing.stop();
    TimerCheckAudioDeviceOk.stop();
    TimerDetectFeedback.stop();
    bDetectFeedback = false;

    //### TODO: BEGIN ###//
    // is this still required???
    // immediately update status bar
    OnTimerStatus();
    //### TODO: END ###//

    // reset LEDs
    ledBuffers->Reset();
    ledDelay->Reset();

    // clear text labels with client parameters
    lblPingVal->setText ( "---" );
    lblPingUnit->setText ( "" );
    lblDelayVal->setText ( "---" );
    lblDelayUnit->setText ( "" );

    // clear mixer board (remove all faders)
    MainMixerBoard->HideAll();
}

void CClientDlg::UpdateDisplay()
{
    // update settings/chat buttons (do not fire signals since it is an update)
    if ( chbSettings->isChecked() && !ClientSettingsDlg.isVisible() )
    {
        chbSettings->blockSignals ( true );
        chbSettings->setChecked ( false );
        chbSettings->blockSignals ( false );
    }
    if ( !chbSettings->isChecked() && ClientSettingsDlg.isVisible() )
    {
        chbSettings->blockSignals ( true );
        chbSettings->setChecked ( true );
        chbSettings->blockSignals ( false );
    }
}

void CClientDlg::SetGUIDesign ( const EGUIDesign eNewDesign )
{
    // remove any styling from the mixer board - reapply after changing skin
    MainMixerBoard->setStyleSheet ( "" );

    // apply GUI design to current window
    switch ( eNewDesign )
    {
    case GD_ORIGINAL:
        backgroundFrame->setStyleSheet (
            "QFrame#backgroundFrame { background: #000;"
            "                         border: none; }"
            "QLabel {                 color:          rgb(255, 255, 255);"
            "                         font:           bold 13px; }"
            "QRadioButton {           color:          rgb(255, 255, 255);"
            "                         font:           bold 13px; }"
            "QScrollArea {            background:     transparent; }"
            ".QWidget {               background:     transparent; }" // note: matches instances of QWidget, but not of its subclasses
            "QGroupBox {              background:     #000; }"
            "QGroupBox::title {       color:          rgb(255, 255, 255); }"
            "QCheckBox::indicator {   width:          38px;"
            "                         height:         21px; }"
            "QCheckBox::indicator:unchecked {"
            "                         image:          url(:/png/fader/res/ledbuttonnotpressed.png); }"
            "QCheckBox::indicator:checked {"
            "                         image:          url(:/png/fader/res/ledbuttonpressed.png); }"
            "QCheckBox {              color:          rgb(255, 255, 255);"
            "                         font:           bold 13px; }" );
#ifdef _WIN32
        // Workaround QT-Windows problem: This should not be necessary since in the
        // background frame the style sheet for QRadioButton was already set. But it
        // seems that it is only applied if the style was set to default and then back
        // to GD_ORIGINAL. This seems to be a QT related issue...
        rbtReverbSelL->setStyleSheet ( "color: rgb(255, 255, 255);"
                                       "font:  bold 13px;" );
        rbtReverbSelR->setStyleSheet ( "color: rgb(255, 255, 255);"
                                       "font:  bold 13px;" );
#endif

        ledBuffers->SetType ( CMultiColorLED::MT_LED );
        ledDelay->SetType ( CMultiColorLED::MT_LED );
        break;

    default:
        // reset style sheet and set original parameters
        backgroundFrame->setStyleSheet ( "" );

#ifdef _WIN32
        // Workaround QT-Windows problem: See above description
        rbtReverbSelL->setStyleSheet ( "" );
        rbtReverbSelR->setStyleSheet ( "" );
#endif

        ledBuffers->SetType ( CMultiColorLED::MT_INDICATOR );
        ledDelay->SetType ( CMultiColorLED::MT_INDICATOR );
        break;
    }

    // also apply GUI design to child GUI controls
    MainMixerBoard->SetGUIDesign ( eNewDesign );

    // jamony: chbSettings 移到 A栏顶部, 隐藏 checkbox 灯 + 紧密边框 + h=16(=输入标签高)
    chbSettings->setStyleSheet (
        "QCheckBox { color: rgb(255,255,255); font: bold 13px; padding: 2px 8px;"
        "             border: 1px solid #444; border-radius: 3px; background: #1a1a1a; }"
        "QCheckBox::indicator { width: 0px; height: 0px; }"
        "QCheckBox:checked { color: #FF33AA; border: 1px solid #FF33AA; }"
        "QCheckBox:hover { border: 1px solid #888; }" );
    chbSettings->setFixedHeight ( 22 ); // jamony: 内部高度=butAutoAdjust(22-4padding-2border=16)

    // jamony: chbLocalMute 移到 B栏底部 frameLocalMute(M+静音), M 字母灯样式 + 边框
    chbLocalMute->setStyleSheet (
        "QPushButton { font: bold 13px; color: #999999; background: #262626;"
        "              border-radius: 3px; padding: 2px 6px; text-align: center; }"
        "QPushButton:checked { background: #FF3366; color: #ffffff; }" );
    chbLocalMute->setFixedWidth ( 37 ); // jamony: 对齐电平表（L17+R17+spacing3=37）
    frameLocalMute->setFixedWidth ( 37 ); // jamony: frameLocalMute 同步 37
    chbLocalMute->setFixedHeight ( 22 ); // jamony: M按钮高=butAutoAdjust(22)
    frameLocalMute->setStyleSheet ( "" ); // jamony: 去外框, 只M按钮
    // jamony: B栏内容水平居中于 AB线(293)..BC线(339)。原左隙3/右隙6偏左；
    // vboxLayout 左margin+2 → 内容左缘296→298，左隙5/右隙4（Fixed内容顺溢2px进333..339空白，不撞BC线）
    vboxLayout->setContentsMargins ( 2, 3, 0, 0 );

    // jamony: butAutoAdjust padding 0 + 紧凑边框, 视觉底框 = geometry 底(对齐 frameLocalMute)
    butAutoAdjust->setStyleSheet (
        "QPushButton { color: rgb(255,255,255); font: bold 13px; padding: 2px 4px;"
        "              border: 1px solid #444; border-radius: 3px; background: #1a1a1a; }"
        "QPushButton:hover { border: 1px solid #888; }" );
    butAutoAdjust->setSizePolicy ( QSizePolicy::Fixed, QSizePolicy::Fixed ); // 宽度自适应文案+padding, 不被 C 区拉伸

    // jamony: 混响效果器机架UI(横向前置)
    frameReverb->setStyleSheet ( "QFrame#frameReverb { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: Boost效果器机架UI(横向，同混响风格)
    frameBoost->setStyleSheet ( "QFrame#frameBoost { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: 过载效果器机架UI(横向，3旋钮+LED+开关)
    frameOverdrive->setStyleSheet ( "QFrame#frameOverdrive { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: 失真效果器机架UI(横向，3旋钮+LED+开关，同过载风格)
    frameDistortion->setStyleSheet ( "QFrame#frameDistortion { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: EQ效果器机架UI(横向，9垂直fader+LED+开关，同风格)
    frameEq->setStyleSheet ( "QFrame#frameEq { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: 合唱/延迟效果器机架UI(横向，3旋钮+LED+开关，同风格)
    frameChorus->setStyleSheet ( "QFrame#frameChorus { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );
    frameDelay->setStyleSheet ( "QFrame#frameDelay { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: 效果器 On/Off 按钮统一 stylesheet (失活保持颜色, 与 LED 逻辑一致; macOS native 按钮失活会变灰)
    const QString sOnOffBtn =
        "QPushButton { background: #1a1a1a; color: #888888; border: 1px solid #444; border-radius: 3px; padding: 2px 10px; font: bold 12px; }"
        "QPushButton:checked { background: #BBEE00; color: #1a1a1a; border: 1px solid #BBEE00; }"
        "QPushButton:hover { border: 1px solid #888; }";
    btnBoostOnOff->setStyleSheet ( sOnOffBtn );
    btnOverdriveOnOff->setStyleSheet ( sOnOffBtn );
    btnDistortionOnOff->setStyleSheet ( sOnOffBtn );
    btnEqOnOff->setStyleSheet ( sOnOffBtn );
    btnChorusOnOff->setStyleSheet ( sOnOffBtn );
    btnDelayOnOff->setStyleSheet ( sOnOffBtn );
    btnReverbOnOff->setStyleSheet ( sOnOffBtn );

    // jamony: 推子(QSlider) stylesheet (失活保持颜色, 非 native 渲染)
    const QString sVSlider =
        "QSlider::groove:vertical { background: #333333; width: 4px; border-radius: 2px; }"
        "QSlider::add-page:vertical { background: qlineargradient(x1:0, y1:1, x2:0, y2:0, stop:0 #00aaff, stop:0.35 #9933ff, stop:0.7 #ff33aa, stop:1 #bbee00); border-radius: 2px; }"
        "QSlider::handle:vertical { background: #ffffff; height: 12px; margin: 0 -4px; border-radius: 6px; }"
        "QSlider::handle:vertical:hover { background: #e0e0e0; }";
    QSlider* const eqFaders[] = { sldEqBand0, sldEqBand1, sldEqBand2, sldEqBand3, sldEqBand4, sldEqBand5, sldEqBand6, sldEqIn, sldEqOut };
    for ( QSlider* s : eqFaders ) { s->setStyleSheet ( sVSlider ); }
    const QString sHSlider =
        "QSlider::groove:horizontal { background: #333333; height: 4px; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00aaff, stop:0.35 #9933ff, stop:0.7 #ff33aa, stop:1 #bbee00); border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #ffffff; width: 12px; margin: -4px 0; border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: #e0e0e0; }";
    sldAudioReverb->setStyleSheet ( sHSlider );
    sldAudioBoost->setStyleSheet ( sHSlider );

    // jamony: 混响左右选项(QRadioButton) stylesheet (失活保持颜色, 非 native 渲染)
    const QString sRadio =
        "QRadioButton { color: #ffffff; }"
        "QRadioButton::indicator { width: 10px; height: 10px; border-radius: 5px; border: 1px solid #666666; background: #1a1a1a; }"
        "QRadioButton::indicator:checked { background: #BBEE00; border: 1px solid #BBEE00; }";
    rbtReverbSelL->setStyleSheet ( sRadio );
    rbtReverbSelR->setStyleSheet ( sRadio );

    // jamony: 四条分割线灰色(低调) — NoFrame + 1px高/宽 + background-color
    lineMeter->setFrameShape ( QFrame::NoFrame );
    lineMeter->setStyleSheet ( "background-color: #444; border: none;" );
    lineMeter->setFixedWidth ( 1 );
    linePanReverb->setFrameShape ( QFrame::NoFrame );
    linePanReverb->setStyleSheet ( "background-color: #444; border: none;" );
    linePanReverb->setFixedWidth ( 1 );
    lineUpperLowerLeft->setFrameShape ( QFrame::NoFrame );
    lineUpperLowerLeft->setStyleSheet ( "background-color: #444; border: none;" );
    lineUpperLowerLeft->setFixedHeight ( 1 );
    lineUpperLowerLeft_2->setFrameShape ( QFrame::NoFrame );
    lineUpperLowerLeft_2->setStyleSheet ( "background-color: #444; border: none;" );
    lineUpperLowerLeft_2->setFixedHeight ( 1 );

    // jamony: gridLayout 占满A栏宽 + 列均匀分布(删HBox右侧spacer + 6列stretch=1)
    static bool bGridAdjusted = false;
    if ( !bGridAdjusted )
    {
        QLayoutItem* sp = horizontalLayoutPingWrap->takeAt ( 1 );
        if ( sp ) delete sp;
        for ( int i = 0; i < 6; i++ ) gridLayout->setColumnStretch ( i, 1 );
        bGridAdjusted = true;
    }
}

void CClientDlg::SetMeterStyle ( const EMeterStyle eNewMeterStyle )
{
    // apply MeterStyle to current window
    // Note: input meter uses big LED and wide bar even in narrow mode
    switch ( eNewMeterStyle )
    {
    case MT_LED_STRIPE:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_LED_STRIPE );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_LED_STRIPE );
        break;

    case MT_LED_ROUND_BIG:
    case MT_LED_ROUND_SMALL:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_BIG );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_BIG );
        break;

    case MT_BAR_WIDE:
    case MT_BAR_NARROW:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_BAR_WIDE );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_BAR_WIDE );
        break;
    }

    // also apply MeterStyle to child GUI controls
    MainMixerBoard->SetMeterStyle ( eNewMeterStyle );
}

void CClientDlg::OnRecorderStateReceived ( const ERecorderState newRecorderState )
{
    MainMixerBoard->SetRecorderState ( newRecorderState );
    SetMixerBoardDeco ( newRecorderState, pClient->GetGUIDesign() );
}

void CClientDlg::OnGUIDesignChanged()
{
    SetGUIDesign ( pClient->GetGUIDesign() );
    SetMixerBoardDeco ( MainMixerBoard->GetRecorderState(), pClient->GetGUIDesign() );
}

void CClientDlg::OnMeterStyleChanged() { SetMeterStyle ( pClient->GetMeterStyle() ); }

void CClientDlg::SetMixerBoardDeco ( const ERecorderState newRecorderState, const EGUIDesign eNewDesign )
{
    // return if no change
    if ( ( newRecorderState == eLastRecorderState ) && ( eNewDesign == eLastDesign ) )
        return;
    eLastRecorderState = newRecorderState;
    eLastDesign        = eNewDesign;

    // set base style
    // jamony: 隐藏 title 展示(连接状态3态文案+录音红底), 逻辑代码全保留, 去掉 display:none 即恢复
    QString sTitleStyle = "QGroupBox::title { subcontrol-origin: margin;"
                          "                   subcontrol-position: left top;"
                          "                   left: 7px;"
                          "                   display: none;";

    if ( newRecorderState == RS_RECORDING )
    {
        sTitleStyle += "color: rgb(255,255,255);"
                       "background-color: rgb(255,0,0); }";
    }
    else
    {
        if ( eNewDesign == GD_ORIGINAL )
        {
            // no need to set the background color for dark mode in fancy skin, as the background is already dark.
            sTitleStyle += "color: rgb(255,255,255); }";
        }
        else
        {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 5, 0 )
            // for Qt 6.5.0 or later, we use the inbuilt cross platform color scheme picker.
            if ( QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark )
#else
            // for earlier versions, check darkmode as proposed in https://www.qt.io/blog/dark-mode-on-windows-11-with-qt-6.5
            const QPalette defaultPalette;
            if ( defaultPalette.color ( QPalette::WindowText ).lightness() > defaultPalette.color ( QPalette::Window ).lightness() )
#endif
            {
                // Dark mode needs a light color

                sTitleStyle += "color: rgb(255,255,255); }";
            }
            else
            {
                sTitleStyle += "color: rgb(255,255,255); }";
            }
        }
    }

    MainMixerBoard->setStyleSheet ( sTitleStyle + " QGroupBox { border: none; background: #000000; }" );
}

void CClientDlg::SetPingTime ( const int iPingTime, const int iOverallDelayMs, const CMultiColorLED::ELightColor eOverallDelayLEDColor )
{
    // apply values to GUI labels, take special care if ping time exceeds
    // a certain value
    if ( iPingTime > 500 )
    {
        const QString sErrorText = "<font color=\"red\"><b>&#62;500</b></font>";
        lblPingVal->setText ( sErrorText );
        lblDelayVal->setText ( sErrorText );
    }
    else
    {
        lblPingVal->setText ( QString().setNum ( iPingTime ) );
        lblDelayVal->setText ( QString().setNum ( iOverallDelayMs ) );
    }
    lblPingUnit->setText ( "ms" );
    lblDelayUnit->setText ( "ms" );

    // set current LED status
    ledDelay->SetLight ( eOverallDelayLEDColor );
}

// OnOpenMidiSettings slot removed; lambda is used in menu action
void CClientDlg::OnMIDIControllerUsageChanged ( bool bEnabled )
{
    // Update the mixer board's MIDI flag to trigger proper user numbering display
    MainMixerBoard->SetMIDICtrlUsed ( bEnabled );

    // Enable/disable runtime MIDI via the sound interface through the public CClient interface
    pClient->EnableMIDI ( bEnabled );
}
