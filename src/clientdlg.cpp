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
    AnalyzerConsole ( pNCliP, parent )
{
    setupUi ( this );

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
    lblLevelMeterLeft->setWhatsThis ( strInpLevH );
    lblLevelMeterRight->setWhatsThis ( strInpLevH );
    lbrInputLevelL->setWhatsThis ( strInpLevH );
    lbrInputLevelL->setAccessibleName ( strInpLevHAccText );
    lbrInputLevelL->setAccessibleDescription ( strInpLevHAccDescr );
    lbrInputLevelL->setToolTip ( strInpLevHTT );
    lbrInputLevelL->setEnabled ( false );
    lbrInputLevelR->setWhatsThis ( strInpLevH );
    lbrInputLevelR->setAccessibleName ( strInpLevHAccText );
    lbrInputLevelR->setAccessibleDescription ( strInpLevHAccDescr );
    lbrInputLevelR->setToolTip ( strInpLevHTT );
    lbrInputLevelR->setEnabled ( false );

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

    // init effect on/off buttons + LED (boost / overdrive / reverb)
    btnBoostOnOff->setChecked ( pClient->GetBoostEnabled() );
    ledBoost->SetLight ( pClient->GetBoostEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
    btnOverdriveOnOff->setChecked ( pClient->GetOverdriveEnabled() );
    ledOverdrive->SetLight ( pClient->GetOverdriveEnabled() ? CMultiColorLED::RL_GREEN : CMultiColorLED::RL_GREY );
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
    lblGlobalInfoLabel->setStyleSheet ( ".QLabel { background: #FF3366; color: white; }" );
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

    QObject::connect ( sldAudioBoost, &QSlider::valueChanged, this, &CClientDlg::OnAudioBoostValueChanged );

    // overdrive knobs
    QObject::connect ( knobOverdriveDrive, &JamonyKnob::valueChanged, this, &CClientDlg::OnOverdriveDriveChanged );
    QObject::connect ( knobOverdriveLevel, &JamonyKnob::valueChanged, this, &CClientDlg::OnOverdriveLevelChanged );
    QObject::connect ( knobOverdriveTone, &JamonyKnob::valueChanged, this, &CClientDlg::OnOverdriveToneChanged );

    // effect on/off buttons (boost / overdrive / reverb)
    QObject::connect ( btnBoostOnOff, &QPushButton::toggled, this, &CClientDlg::OnBoostOnOffToggled );
    QObject::connect ( btnOverdriveOnOff, &QPushButton::toggled, this, &CClientDlg::OnOverdriveOnOffToggled );
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
    // jamony: 连接中时弹确认（防误关 jamsoul）
    if ( pClient->IsRunning() )
    {
        QMessageBox::StandardButton reply = QMessageBox::question ( this, tr ( "退出 jamsoul" ),
            tr ( "退出 jamsoul 将断开音频连接，确认退出？" ), QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
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

    // jamony: 隐藏Pan旋钮(不需要)
    MainMixerBoard->SetDisplayPans ( false );
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

    // show/hide info label
    if ( value == Qt::Checked )
    {
        lblGlobalInfoLabel->show();
    }
    else
    {
        lblGlobalInfoLabel->hide();
    }
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
    chbLocalMute->setFixedWidth ( 53 ); // jamony: M按钮撑满B栏宽(=frameLocalMute宽), 去静音文案+外框
    chbLocalMute->setFixedHeight ( 22 ); // jamony: M按钮高=butAutoAdjust(22)
    frameLocalMute->setStyleSheet ( "" ); // jamony: 去外框, 只M按钮

    // jamony: butAutoAdjust padding 0 + 紧凑边框, 视觉底框 = geometry 底(对齐 frameLocalMute)
    butAutoAdjust->setStyleSheet (
        "QPushButton { color: rgb(255,255,255); font: bold 13px; padding: 2px 8px;"
        "              border: 1px solid #444; border-radius: 3px; background: #1a1a1a; }"
        "QPushButton:hover { border: 1px solid #888; }" );

    // jamony: 混响效果器机架UI(横向前置)
    frameReverb->setStyleSheet ( "QFrame#frameReverb { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: Boost效果器机架UI(横向，同混响风格)
    frameBoost->setStyleSheet ( "QFrame#frameBoost { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

    // jamony: 过载效果器机架UI(横向，3旋钮+LED+开关)
    frameOverdrive->setStyleSheet ( "QFrame#frameOverdrive { border: 1px solid #333; border-radius: 4px; background: #0f0f0f; }" );

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

    MainMixerBoard->setStyleSheet ( sTitleStyle );
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
