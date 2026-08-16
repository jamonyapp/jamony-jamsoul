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

#include "audiomixerboard.h"
#include "jamonychannelslider.h"
#include "jamonypanbar.h"
#include <QScrollBar>
#include <QStyleFactory>
#include <chrono>
#include <deque>

namespace
{
// Per-channel MIDI pickup state
struct MidiPickupState
{
    std::deque<int>                       recentFader;
    std::deque<int>                       recentPan;
    std::chrono::steady_clock::time_point lastMidiTimeFader;
    std::chrono::steady_clock::time_point lastMidiTimePan;
};

std::vector<MidiPickupState> g_midiPickupStates ( MAX_NUM_CHANNELS );
std::vector<bool>            g_midiPickupInitialized ( MAX_NUM_CHANNELS, false );
std::vector<bool>            g_midiPickupWaitingForPickup ( MAX_NUM_CHANNELS, false );

// Check for inactivity and reset pickup state if needed
static void midiPickupInactivityCheck ( int                                    iChannelIdx,
                                        std::chrono::steady_clock::time_point& lastMidiTime,
                                        std::deque<int>&                       pickupBuffer,
                                        std::vector<bool>&                     waitingFlag )
{
    auto now = std::chrono::steady_clock::now();
    if ( lastMidiTime.time_since_epoch().count() > 0 )
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds> ( now - lastMidiTime ).count();
        if ( elapsed > MIDI_PICKUP_INACTIVITY_TIMEOUT_MS )
        {
            // Reset pickup state after inactivity
            waitingFlag[iChannelIdx] = true;
            pickupBuffer.clear();
        }
    }
    lastMidiTime = now;
}

// Determine if MIDI value should be applied based on pickup logic
template<typename T>
static bool midiPickupShouldApply ( int midiValue, int currentValue, int tolerance, const std::deque<T>& recentMidiValues )
{
    // Accept if within tolerance
    if ( std::abs ( midiValue - currentValue ) <= tolerance )
        return true;

    // If buffer has at least 2 values, check if we're crossing the current value
    // Handles the case where rapid movement causes MIDI values to "skip over" the software value
    if ( recentMidiValues.size() >= 2 )
    {
        int prevMidi = recentMidiValues.back();
        // Check if current and previous MIDI values bracket the software value
        if ( ( prevMidi <= currentValue && midiValue >= currentValue ) || ( prevMidi >= currentValue && midiValue <= currentValue ) )
            return true;
    }

    return false;
}

// Try to apply MIDI value with pickup logic
template<typename T>
static bool midiPickupTryApply ( int midiValue, int currentValue, int tolerance, std::deque<T>& pickupBuffer, bool waitingForPickup )
{
    if ( waitingForPickup )
    {
        // Create temp buffer with new value to test pickup logic
        std::deque<int> tempPickup = pickupBuffer;
        if ( tempPickup.size() >= MIDI_PICKUP_HISTORY )
            tempPickup.pop_front();
        tempPickup.push_back ( midiValue );

        if ( !midiPickupShouldApply ( midiValue, currentValue, tolerance, tempPickup ) )
            return true; // Still waiting for pickup

        // Picked up. Stop waiting
        waitingForPickup = false;
    }

    // Update the pickup buffer
    if ( pickupBuffer.size() >= MIDI_PICKUP_HISTORY )
        pickupBuffer.pop_front();
    pickupBuffer.push_back ( midiValue );

    return waitingForPickup;
}
} // namespace

/******************************************************************************\
* CChanneFader                                                                 *
\******************************************************************************/
CChannelFader::CChannelFader ( QWidget* pNW ) :
    eDesign ( GD_STANDARD ),
    BitmapMutedIcon ( QString::fromUtf8 ( ":/png/fader/res/mutediconorange.png" ) ),
    bMIDICtrlUsed ( false )
{
    // create new GUI control objects and store pointers to them (note that
    // QWidget takes the ownership of the pMainGrid so that this only has
    // to be created locally in this constructor)
    pFrame = new QFrame ( pNW );
    pFrame->setFixedWidth ( 64 ); // jamony: fader 64（=模块宽64, 贴满不偏台; 欢哥定64）
    pFrame->setFixedHeight ( 690 ); // jamony v0: 分轨总高 690

    pLevelsBox       = new QWidget ( pFrame );
    plbrChannelLevel = new CLevelMeter ( pLevelsBox );
    pFader           = new JamonyChannelSlider ( Qt::Vertical, pLevelsBox ); // jamony v0: 自绘推子 (paintEvent 1:1 v0, 保留 QSlider 值/信号槽)
    pPan             = new JamonyPanBar ( pLevelsBox ); // jamony v0: Pan 横条（替 QDial 圆盘）
    pPanLabel        = new QLabel ( "Pan", pLevelsBox ); // jamony: 英文(不tr), 覆盖翻译"声像"
    pInfoLabel       = new QLabel ( "", pLevelsBox );
    // jamony v0: IN/OUT 标签（绝对定位 pFrame，不进 pMainGrid，对照 mixer-channel.tsx:130-178）
    plblInTag  = new QLabel ( "IN", pFrame );
    plblOutTag = new QLabel ( "OUT", pFrame );
    plblInTag->setStyleSheet  ( "QLabel { color: #8f9096; font: bold 13px; }" );
    plblOutTag->setStyleSheet ( "QLabel { color: #8f9096; font: bold 13px; }" );
    plblInTag->setAlignment  ( Qt::AlignCenter );
    plblOutTag->setAlignment ( Qt::AlignCenter );
    plblInTag->setGeometry  ( 0, -2, 18, 13 );   // jamony: x=0 对齐电平槽0（A列左缘0=Pan左缘；A+B列整体左移3）
    plblOutTag->setGeometry ( 19, -2, 30, 13 );  // jamony: x=19 中心 34 对齐 groove（pFader 左 19+15; B列再左移6, 电平表-刻度线间距5）
    // jamony v0: pPanLabel(Pan文案)/pInfoLabel(静音icon) 舍弃显示，始终隐藏
    pPanLabel->hide();
    pInfoLabel->hide();

    pMuteSoloBox = new QWidget ( pFrame );
    pcbMute      = new QPushButton ( tr ( "Mute" ), pMuteSoloBox );
    pcbMute->setCheckable ( true );
    pcbSolo      = new QPushButton ( tr ( "Solo" ), pMuteSoloBox );
    pcbSolo->setCheckable ( true );
    pcbGroup     = new QPushButton ( "", pMuteSoloBox );
    pcbGroup->setCheckable ( true );

    pLabelInstBox   = new QGroupBox ( pFrame );
    pLabelInstBox->setFixedWidth ( 64 ); // jamony: 用户名框 64（欢哥定; bold13px双行 iBreakPos8, 英文每行8字符56px≤58 够用）
    plblLabel       = new QLabel ( "", pFrame );
    plblInstrument  = new QLabel ( pFrame );
    plblCountryFlag = new QLabel ( pFrame );

    QVBoxLayout* pMainGrid     = new QVBoxLayout ( pFrame );
    QHBoxLayout* pLevelsGrid   = new QHBoxLayout ( pLevelsBox );
    QVBoxLayout* pMuteSoloGrid = new QVBoxLayout ( pMuteSoloBox );
    pLabelGrid                 = new QHBoxLayout ( pLabelInstBox );
    pLabelPictGrid             = new QVBoxLayout();

    // define the popup menu for the group checkbox
    pGroupPopupMenu = new QMenu ( "", pcbGroup );
    pGroupPopupMenu->addAction ( tr ( "&No grouping" ), this, [=] { OnGroupMenuGrp ( INVALID_INDEX ); } );
    for ( int iGrp = 0; iGrp < MAX_NUM_FADER_GROUPS; iGrp++ )
    {
        pGroupPopupMenu->addAction ( tr ( "Assign to group" ) + ( QString ( " &%1" ).arg ( iGrp + 1 ) ), this, [=] { OnGroupMenuGrp ( iGrp ); } );
    }
#if ( MAX_NUM_FADER_GROUPS != 8 )
#    error "MAX_NUM_FADER_GROUPS must be set to 8, see implementation in CChannelFader()"
#endif

    // setup channel level
    plbrChannelLevel->setContentsMargins ( 0, 0, 0, 0 ); // jamony v0: margin 全 0，电平条/消波灯占满 CLevelMeter
    plbrChannelLevel->setFixedWidth ( 18 ); // jamony: 电平列 18 宽（原 21，缩 3；消波灯 Ignored 自动跟随；类 UI B 栏 input 同步）

    // setup slider
    pFader->setPageStep ( 1 );
    pFader->setRange ( 0, AUD_MIX_FADER_MAX );
    pFader->setTickInterval ( AUD_MIX_FADER_MAX / 9 );
    pFader->setFixedWidth ( 46 ); // jamony v0: 推子列 46（40 + 左刻度线 6，含左刻度线 = v0 视觉宽；groove x=15 绝对不变）

    // setup panning control and info label
    pPan->setRange ( 0, AUD_MIX_PAN_MAX );
    pPan->setValue ( AUD_MIX_PAN_MAX / 2 );
    pInfoLabel->setMinimumHeight ( 14 ); // prevents jitter when muting/unmuting (#811)
    pInfoLabel->setAlignment ( Qt::AlignTop );
    // jamony v0: pPanLabel(Pan文案)/pInfoLabel(静音icon) 舍弃显示；pPan 移到 pMainGrid（电平推子下方）

    // setup fader tag label (black bold text which is centered)
    plblLabel->setTextFormat ( Qt::PlainText );
    plblLabel->setAlignment ( Qt::AlignHCenter | Qt::AlignVCenter );

    // set margins of the layouts to zero to get maximum space for the controls
    pMainGrid->setContentsMargins ( 0, 0, 0, 0 );
    pMainGrid->setSpacing ( 5 ); // jamony 08-10: 段间距 6→5 (Pan/Grp/M/S/用户名框 全5px)

    pLevelsGrid->setContentsMargins ( 0, 0, 0, 0 );
    pLevelsGrid->setSpacing ( 0 ); // only minimal space

    pMuteSoloGrid->setContentsMargins ( 0, 0, 0, 0 );
    pMuteSoloGrid->setSpacing ( 4 ); // jamony: 分组与MS两行之间留间隙

    pLabelGrid->setContentsMargins ( 0, 0, 0, 0 );
    pLabelGrid->setSpacing ( 2 ); // only minimal space between picture and text

    // add user controls to the grids
    pLabelPictGrid->addWidget ( plblCountryFlag, 0, Qt::AlignHCenter );
    pLabelPictGrid->addWidget ( plblInstrument, 0, Qt::AlignHCenter );

    pLabelGrid->addLayout ( pLabelPictGrid );
    pLabelGrid->addWidget ( plblLabel, 0, Qt::AlignVCenter ); // note: just initial add, may be changed later

    // jamony v0: 电平列(21) + 推子列(40) justify-between（间距 15，左刻度线在间距里）
    pLevelsGrid->addSpacing ( 0 );             // jamony: A列左缘 0（电平槽对齐 Pan 左缘 0；原3致A列距BC 6-7 且不对齐Pan）
    pLevelsGrid->addWidget ( plbrChannelLevel );
    pLevelsGrid->addSpacing ( 1 );             // jamony: A列电平表右→B列刻度线左 间距 11→5 (欢哥要5px; 电平表-pFader间距 7→1, pFader左移6)
    pLevelsGrid->addSpacing ( 3 );             // jamony: B列整体右移 3px
    pLevelsGrid->addWidget ( pFader );
    pLevelsGrid->addStretch ();

    // jamony v0: pcbGroup 移到 pMainGrid 全宽行；pMuteSoloBox 只留 M/S（半宽 flex-1, gap 4）
    QHBoxLayout* pMSLayout = new QHBoxLayout();
    pMSLayout->setSpacing ( 5 ); // jamony 08-10: M/S 水平间距 4→5
    pMSLayout->setContentsMargins ( 0, 0, 0, 0 );
    pcbMute->setFixedHeight ( 20 ); // jamony v0: ROW_HEIGHT 20（等高 Pan/Grp）
    pcbSolo->setFixedHeight ( 20 );
    pMSLayout->addWidget ( pcbMute, 1 );
    pMSLayout->addWidget ( pcbSolo, 1 );
    pMuteSoloGrid->addLayout ( pMSLayout );

    // jamony v0 段顺序：电平推子(549) → Pan(20) → Grp(20) → M/S(20) → 用户名(40)，spacing 6，总 690
    pLevelsBox->setFixedHeight ( 553 );   // jamony 08-10: 549→553 (+4, 间距5px化Pan下移, 电平/推子下沿增长补5px; 刻度paintEvent按height自适应)
    pPan->setFixedHeight ( 20 );          // v0 ROW_HEIGHT
    pPan->setFixedWidth ( 64 );           // jamony: 模块宽 64（欢哥定, 与用户名框64对齐）
    pcbGroup->setFixedHeight ( 20 );      // v0 ROW_HEIGHT
    pcbGroup->setFixedWidth ( 64 );       // jamony: 模块宽 64（与用户名框对齐）
    pMuteSoloBox->setFixedHeight ( 20 );  // v0 ROW_HEIGHT
    pMuteSoloBox->setFixedWidth ( 64 );   // jamony: 模块宽 64（M/S半宽, 与用户名框对齐）
    pMainGrid->addSpacing ( 16 ); // jamony v0: IN/OUT 空间（16 + spacer-widget spacing1 = 17 = 标签13 + 间距4，pLevelsBox y=17）
    pMainGrid->addWidget ( pLevelsBox ); // 549 高（电平+推子）
    pMainGrid->addWidget ( pPan, 0, Qt::AlignLeft ); // jamony: 64 左对齐 pFrame 左缘(距BC5, 与用户名框对齐)
    pMainGrid->addWidget ( pcbGroup, 0, Qt::AlignLeft ); // jamony: 64 左对齐(与用户名框对齐)
    pMainGrid->addWidget ( pMuteSoloBox, 0, Qt::AlignLeft ); // jamony: 64 左对齐(与用户名框对齐)
    pMainGrid->addWidget ( pLabelInstBox, 0, Qt::AlignLeft ); // jamony: 用户名框 64 左对齐 pFrame 左缘(距BC5 不变; 原70贴满AlignHCenter, 改64后AlignLeft保左缘=Pan左缘)
    pMainGrid->addStretch(); // jamony 08-10: 底部弹性吸收多余空间(pFrame690 - items684 = 6px), 防 Qt 撑大 widget spacing(实测 5→6), 让段间距真5px + 用户名框下沿684=root726 对齐A区滚动区

    // reset current fader
    strGroupBaseText  = "Grp";         // this will most probably overwritten by SetGUIDesign()
    iInstrPicMaxWidth = INVALID_INDEX; // this will most probably overwritten by SetGUIDesign()
    Reset();

    // add help text to controls
    // jamony 08-14: C区悬停 toolTip 统一（移除 WhatsThis），定稿见 jamsoul说明文本.txt
    QString strC12 = QStringLiteral(
        "<b>IN电平</b>：该用户的输入音量，由该用户本地输入信号强度决定，不受控于右侧推子。<br>"
        "<b>OUT推子</b>：调整该用户在本地听到的音量，不影响其他用户。<br>"
        "• <font color=\"#FF33AA\">●</font> 如果IN电平顶部的消波灯亮起，则应提醒该用户降低其本地输入增益，削波灯点击可熄灭，或20s后自动熄灭。<br>"
        "• 该用户有信号输入时，<font color=\"#BBEE00\">●</font> 电平应跳动；如果未看到跳动，应提醒该用户检查输入连接、是否本地静音或提高增益。" );
    QString strC3 = QStringLiteral(
        "<b>Pan（声像）</b>：左右拖动可调整该用户在本地播放时的声像。" );
    QString strC45 = QStringLiteral(
        "<font color=\"#FF33AA\">●</font> <b>M</b>：在本地静音这位用户，不影响其他用户听到的。<br>"
        "<font color=\"#BBEE00\">●</font> <b>S</b>：在本地独奏这位用户，不影响其他用户听到的。<br>"
        "• M与S如同时点亮，则静音优先。" );
    QString strC6 = QStringLiteral(
        "<b>Grp（分组）</b>：把多个用户编为一组，拖动组内任一推子，其他推子同步联动。" );

    // C1+C2 电平表+推子（IN/OUT 标签也弹同一条）
    plbrChannelLevel->setToolTip ( strC12 );
    plbrChannelLevel->setWhatsThis ( "" );
    plbrChannelLevel->setAccessibleName ( tr ( "Input level of the current audio channel at the server" ) );
    pFader->setToolTip ( strC12 );
    pFader->setWhatsThis ( "" );
    pFader->setAccessibleName ( tr ( "Local mix level setting of the current audio channel at the server" ) );
    plblInTag->setToolTip ( strC12 );
    plblOutTag->setToolTip ( strC12 );
    plbrChannelLevel->setToolTipDuration ( 15000 );
    pFader->setToolTipDuration ( 15000 );
    plblInTag->setToolTipDuration ( 15000 );
    plblOutTag->setToolTipDuration ( 15000 );

    // pInfoLabel 保持隐藏，清 WhatsThis
    pInfoLabel->setWhatsThis ( "" );
    pInfoLabel->setAccessibleName ( tr ( "Status indicator label" ) );

    // C3 Pan
    pPan->setToolTip ( strC3 );
    pPan->setWhatsThis ( "" );
    pPan->setAccessibleName ( tr ( "Local panning position of the current audio channel at the server" ) );
    pPan->setToolTipDuration ( 15000 );

    // C4+C5 M+S（同一条）
    pcbMute->setToolTip ( strC45 );
    pcbMute->setWhatsThis ( "" );
    pcbMute->setAccessibleName ( tr ( "Mute button" ) );
    pcbSolo->setToolTip ( strC45 );
    pcbSolo->setWhatsThis ( "" );
    pcbSolo->setAccessibleName ( tr ( "Solo button" ) );
    pcbMute->setToolTipDuration ( 15000 );
    pcbSolo->setToolTipDuration ( 15000 );

    // C6 Grp
    pcbGroup->setToolTip ( strC6 );
    pcbGroup->setWhatsThis ( "" );
    pcbGroup->setAccessibleName ( tr ( "Group button" ) );
    pcbGroup->setToolTipDuration ( 15000 );

    // 用户名框：移除 WhatsThis(Fader Tag)
    plblInstrument->setWhatsThis ( "" );
    plblInstrument->setAccessibleName ( tr ( "Mixer channel instrument picture" ) );
    plblLabel->setWhatsThis ( "" );
    plblLabel->setAccessibleName ( tr ( "Mixer channel label (fader tag)" ) );
    plblCountryFlag->setWhatsThis ( "" );
    plblCountryFlag->setAccessibleName ( tr ( "Mixer channel country/region flag" ) );

    // Connections -------------------------------------------------------------
    QObject::connect ( pFader, &QSlider::valueChanged, this, &CChannelFader::OnLevelValueChanged );

    QObject::connect ( pPan, &JamonyPanBar::valueChanged, this, &CChannelFader::OnPanValueChanged );

    QObject::connect ( pcbMute, &QPushButton::toggled, this, [this](bool c){ OnMuteStateChanged(c?Qt::Checked:0); } );

    QObject::connect ( pcbSolo, &QPushButton::toggled, this, [this](bool c){ soloStateChanged(c?Qt::Checked:0); } );

    QObject::connect ( pcbGroup, &QPushButton::toggled, this, [this](bool c){ OnGroupStateChanged(c?Qt::Checked:0); } );
}

void CChannelFader::SetGUIDesign ( const EGUIDesign eNewDesign )
{
    eDesign = eNewDesign;

    switch ( eNewDesign )
    {
    case GD_ORIGINAL:
        // jamony v0: 推子由 JamonyChannelSlider::paintEvent 自绘（不用 QSS/PNG skin）
        pFader->setStyleSheet ( "" );

        pLabelGrid->addWidget ( plblLabel, 0, Qt::AlignVCenter ); // label next to icons
        pLabelInstBox->setFixedHeight ( 35 );                      // jamony 08-10: 用户名框高 40→35 (下沿→684=root726 对齐A区滚动区)
        pPanLabel->setText ( "Pan" ); // jamony: 英文(原 tr("PAN") 大写漏改, 翻译成"声像")
        pcbMute->setText ( tr ( "M" ) );
        pcbSolo->setText ( tr ( "S" ) );
        strGroupBaseText  = "Grp"; // jamony: 英文(原 tr("分组")), 显示 Grp / Grp1-8
        // jamony v0: pcbGroup 全宽（pMainGrid 全宽行），不设 setFixedWidth
        // jamony: MS按钮 删LED + 试听混音器颜色(Mute红#FF3366/Solo绿#BBEE00/Group紫#9933FF)
        pcbMute->setStyleSheet (
            "QPushButton { font: bold 13px; color: #999999; background: #262626;"
            "            border-radius: 3px; padding: 1px 4px; text-align: center; }"
            "QPushButton::indicator { width: 0px; height: 0px; }"
            "QPushButton:checked { background: #FF33AA; color: #ffffff; }" );
        pcbSolo->setStyleSheet (
            "QPushButton { font: bold 13px; color: #999999; background: #262626;"
            "            border-radius: 3px; padding: 1px 4px; text-align: center; }"
            "QPushButton::indicator { width: 0px; height: 0px; }"
            "QPushButton:checked { background: #BBEE00; color: #121212; }" );
        pcbGroup->setStyleSheet (
            "QPushButton { font: bold 13px; color: #999999; background: #262626;"
            "            border-radius: 3px; padding: 1px 4px; text-align: center; }"
            "QPushButton::indicator { width: 0px; height: 0px; }"
            "QPushButton:checked { background: #9933FF; color: #ffffff; }" );
        iInstrPicMaxWidth = INVALID_INDEX; // no instrument picture scaling
        break;

    case GD_SLIMFADER:
        pLabelPictGrid->addWidget ( plblLabel, 0, Qt::AlignHCenter ); // label below icons
        pLabelInstBox->setMinimumHeight ( 130 );                      // maximum height of the instrument+flag+label
        pFader->setTickPosition ( QSlider::NoTicks );
        pFader->setStyleSheet ( "" );
        pPanLabel->setText ( "Pan" ); // jamony: 英文
        pcbMute->setText ( tr ( "M" ) );
        pcbSolo->setText ( tr ( "S" ) );
        strGroupBaseText  = "Grp"; // jamony: 英文(原 tr("分组")), 显示 Grp / Grp1-8
        // jamony v0: pcbGroup 全宽（pMainGrid 全宽行），不设 setFixedWidth
        // jamony: MS按钮 删LED + 试听混音器颜色(Mute红#FF3366/Solo绿#BBEE00/Group紫#9933FF)
        pcbMute->setStyleSheet (
            "QPushButton { font: bold 13px; color: #999999; background: #262626;"
            "            border-radius: 3px; padding: 1px 4px; text-align: center; }"
            "QPushButton::indicator { width: 0px; height: 0px; }"
            "QPushButton:checked { background: #FF33AA; color: #ffffff; }" );
        pcbSolo->setStyleSheet (
            "QPushButton { font: bold 13px; color: #999999; background: #262626;"
            "            border-radius: 3px; padding: 1px 4px; text-align: center; }"
            "QPushButton::indicator { width: 0px; height: 0px; }"
            "QPushButton:checked { background: #BBEE00; color: #121212; }" );
        pcbGroup->setStyleSheet (
            "QPushButton { font: bold 13px; color: #999999; background: #262626;"
            "            border-radius: 3px; padding: 1px 4px; text-align: center; }"
            "QPushButton::indicator { width: 0px; height: 0px; }"
            "QPushButton:checked { background: #9933FF; color: #ffffff; }" );
        iInstrPicMaxWidth = 18; // scale instrument picture to avoid enlarging the width by the picture
        break;

    default:
        // reset style sheet and set original parameters
        pFader->setTickPosition ( QSlider::TicksBothSides );
        pFader->setStyleSheet ( "" );
        pLabelGrid->addWidget ( plblLabel, 0, Qt::AlignVCenter ); // label next to icons
        pLabelInstBox->setFixedHeight ( 35 );                      // jamony 08-10: 用户名框高 40→35 (下沿→684=root726 对齐A区滚动区)
        pPanLabel->setText ( "Pan" ); // jamony: 英文
        pcbMute->setText ( tr ( "Mute" ) );
        pcbSolo->setText ( tr ( "Solo" ) );
        strGroupBaseText  = "Grp"; // jamony: 英文(去 tr 避免翻译成"分组")
        iInstrPicMaxWidth = INVALID_INDEX; // no instrument picture scaling
        break;
    }

    // we need to update since we changed the checkbox text
    UpdateGroupIDDependencies();

    // the instrument picture might need scaling after a style change
    SetChannelInfos ( cReceivedChanInfo );
}

void CChannelFader::SetMeterStyle ( const EMeterStyle eNewMeterStyle )
{
    eMeterStyle = eNewMeterStyle;

    switch ( eNewMeterStyle )
    {
    case MT_BAR_NARROW:
        plbrChannelLevel->SetLevelMeterType ( CLevelMeter::MT_BAR_NARROW );
        // Fader height controls the distribution of the LEDs, if the value is too small the fader might not be movable
        pFader->setMinimumHeight ( 85 );
        break;

    case MT_BAR_WIDE:
        plbrChannelLevel->SetLevelMeterType ( CLevelMeter::MT_BAR_WIDE );
        // Fader height controls the distribution of the LEDs, if the value is too small the fader might not be movable
        pFader->setMinimumHeight ( 120 );
        break;

    case MT_LED_ROUND_SMALL:
        plbrChannelLevel->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_SMALL );
        // Fader height controls the distribution of the LEDs, if the value is too small the fader might not be movable
        pFader->setMinimumHeight ( 85 );
        break;

    case MT_LED_ROUND_BIG:
        plbrChannelLevel->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_BIG );
        // Fader height controls the distribution of the LEDs, if the value is too small the fader might not be movable
        pFader->setMinimumHeight ( 162 );
        break;

    default:
        // reset style sheet and set original parameters
        plbrChannelLevel->SetLevelMeterType ( CLevelMeter::MT_LED_STRIPE );
        // Fader height controls the distribution of the LEDs, if the value is too small the fader might not be movable
        pFader->setMinimumHeight ( 120 );
        break;
    }
}

void CChannelFader::SetDisplayChannelLevel ( const bool eNDCL ) { plbrChannelLevel->setHidden ( !eNDCL ); }

bool CChannelFader::GetDisplayChannelLevel() { return !plbrChannelLevel->isHidden(); }

void CChannelFader::SetDisplayPans ( const bool eNDP )
{
    // jamony v0: pPanLabel(Pan文案) 始终隐藏，只控制 pPan(PanBar) 显示
    pPan->setHidden ( !eNDP );
}

void CChannelFader::SetupFaderTag ( const ESkillLevel eSkillLevel )
{
    // Should never happen here
    if ( iGroupID >= MAX_NUM_FADER_GROUPS )
    {
        SetGroupID ( INVALID_INDEX );
    }

    // the group ID defines the border color and style
    QString strBorderColor = "black";
    QString strBorderStyle = "solid";

    if ( iGroupID != INVALID_INDEX )
    {
        switch ( iGroupID % 4 )
        {
        case 0:
            strBorderColor = "#9933FF"; // jamony purple
            break;

        case 1:
            strBorderColor = "#00AAFF"; // jamony blue
            break;

        case 2:
            strBorderColor = "#BBEE00"; // jamony green
            break;

        case 3:
            strBorderColor = "#FF33AA"; // jamony pink
            break;

        default:
            break;
        }

        switch ( iGroupID / 4 )
        {
        case 0:
            strBorderStyle = "solid";
            break;

        case 1:
            strBorderStyle = "dashed";
            break;

        case 2:
            strBorderStyle = "dotted";
            break;

        case 3:
            strBorderStyle = "double";
            break;

        default:
            break;
        }
    }

    // setup group box for label/instrument picture: jamony style
    // jamony: 未分组 border none（背景全宽等宽 Pan/M+S）；分组 border 2px 彩色（视觉补偿）
    QString strStile;
    if ( iGroupID != INVALID_INDEX )
    {
        strStile = "QGroupBox { border:        2px " + strBorderStyle + " " + strBorderColor +
                   "; border-radius: 3px; padding: 3px; background: #0d0d0d; }";
    }
    else
    {
        strStile = "QGroupBox { border: none; border-radius: 3px; padding: 3px; background: #0d0d0d; }";
    }

    pLabelInstBox->setStyleSheet ( strStile );

    // jamony: 分组按钮边框同步用户名边框(同色+同样式), 启用时文案白色
    if ( iGroupID != INVALID_INDEX )
    {
        pcbGroup->setStyleSheet (
            "QPushButton { font: bold 13px; color: #ffffff; background: #262626;"
            "              border: 2px " + strBorderStyle + " " + strBorderColor + "; border-radius: 3px; padding: 1px 4px; }" );
    }
    else
    {
        pcbGroup->setStyleSheet (
            "QPushButton { font: bold 13px; color: #999999; background: #262626;"
            "              border: none; border-radius: 3px; padding: 1px 4px; }" ); // jamony: 未分组 border none（背景全宽等宽 Pan/M+S）
    }
}

void CChannelFader::Reset()
{
    // it is important to reset the group index first (#611)
    iGroupID = INVALID_INDEX;

    // general initializations
    SetRemoteFaderIsMute ( false );

    // init gain and pan value -> maximum value as definition according to server
    pFader->setValue ( AUD_MIX_FADER_MAX );
    dPreviousFaderLevel = AUD_MIX_FADER_MAX;
    pPan->setValue ( AUD_MIX_PAN_MAX / 2 );

    // reset mute/solo/group check boxes and level meter
    pcbMute->setChecked ( false );
    pcbSolo->setChecked ( false );
    plbrChannelLevel->SetValue ( 0 );
    plbrChannelLevel->ClipReset();

    // clear instrument picture, country flag, tool tips and label text
    plblLabel->setText ( "" );
    plblLabel->setToolTip ( "" );
    plblInstrument->setVisible ( false );
    plblInstrument->setToolTip ( "" );
    plblCountryFlag->setVisible ( false );
    plblCountryFlag->setToolTip ( "" );
    cReceivedChanInfo = CChannelInfo();
    SetupFaderTag ( SL_NOT_SET );

    // set a defined tool tip time out
    const int iToolTipDurMs = 30000;
    plblLabel->setToolTipDuration ( iToolTipDurMs );
    plblInstrument->setToolTipDuration ( iToolTipDurMs );
    plblCountryFlag->setToolTipDuration ( iToolTipDurMs );

    bOtherChannelIsSolo  = false;
    bIsMyOwnFader        = false;
    bIsMutedAtServer     = false;
    iRunningNewClientCnt = 0;

    UpdateGroupIDDependencies();
}

void CChannelFader::SetFaderLevel ( const double dLevel, const bool bIsGroupUpdate )
{
    // first make a range check
    if ( dLevel >= 0 )
    {
        // we set the new fader level in the GUI (slider control) and also tell the
        // server about the change (block the signal of the fader since we want to
        // call SendFaderLevelToServer with a special additional parameter)
        pFader->blockSignals ( true );
        pFader->setValue ( std::min ( AUD_MIX_FADER_MAX, MathUtils::round ( dLevel ) ) );
        pFader->blockSignals ( false );

        SendFaderLevelToServer ( std::min ( static_cast<double> ( AUD_MIX_FADER_MAX ), dLevel ), bIsGroupUpdate );

        if ( dLevel > AUD_MIX_FADER_MAX )
        {
            // If the level is above the maximum, we have to store it for the purpose
            // of group fader movement. If you move a fader which has lower volume than
            // this one and this clips at max, we want to retain the ratio between this
            // fader and the others in the group.
            dPreviousFaderLevel = dLevel;
        }
    }
}

void CChannelFader::SetPanValue ( const int iPan )
{
    // first make a range check
    if ( ( iPan >= 0 ) && ( iPan <= AUD_MIX_PAN_MAX ) )
    {
        // we set the new fader level in the GUI (slider control) which then
        // emits to signal to tell the server about the change (implicitly)
        pPan->setValue ( iPan );
        pPan->setAccessibleName ( QString::number ( iPan ) );
    }
}

void CChannelFader::SetFaderIsSolo ( const bool bIsSolo )
{
    // changing the state automatically emits the signal, too
    pcbSolo->setChecked ( bIsSolo );
}

void CChannelFader::SetFaderIsMute ( const bool bIsMute )
{
    // changing the state automatically emits the signal, too
    pcbMute->setChecked ( bIsMute );
}

void CChannelFader::SetRemoteFaderIsMute ( const bool bIsMute )
{
    if ( bIsMute )
    {
        // show muted icon orange
        pInfoLabel->setPixmap ( BitmapMutedIcon );
    }
    else
    {
        pInfoLabel->setPixmap ( QPixmap() );
    }
}

void CChannelFader::SendFaderLevelToServer ( const double dLevel, const bool bIsGroupUpdate )
{
    // if mute flag is set or other channel is on solo, do not apply the new
    // fader value to the server (exception: we are on solo, in that case we
    // ignore the "other channel is on solo" flag)
    const bool bSuppressServerUpdate = !( ( !pcbMute->isChecked() ) && ( !bOtherChannelIsSolo || IsSolo() ) );

    // emit signal for new fader gain value
    emit gainValueChanged ( MathUtils::CalcFaderGain ( static_cast<float> ( dLevel ) ),
                            bIsMyOwnFader,
                            bIsGroupUpdate,
                            bSuppressServerUpdate,
                            dLevel / dPreviousFaderLevel );

    // update previous fader level since the level has changed, avoid to use
    // the zero value not to have division by zero and also to retain the ratio
    // after the fader is moved up again from the zero position
    if ( dLevel > 0 )
    {
        dPreviousFaderLevel = dLevel;
    }
}

void CChannelFader::SendPanValueToServer ( const int iPan ) { emit panValueChanged ( static_cast<float> ( iPan ) / AUD_MIX_PAN_MAX ); }

void CChannelFader::OnPanValueChanged ( int value )
{
    // on shift-click the pan shall reset to 0 L/R (#707)
    if ( QGuiApplication::keyboardModifiers() == Qt::ShiftModifier )
    {
        // correct the value to the center position
        value = AUD_MIX_PAN_MAX / 2;

        // set the GUI control in the center position while deactivating
        // the signals to avoid an infinite loop
        pPan->blockSignals ( true );
        pPan->setValue ( value );
        pPan->blockSignals ( false );
    }

    pPan->setAccessibleName ( QString::number ( value ) );
    SendPanValueToServer ( value );
}

void CChannelFader::OnMuteStateChanged ( int value )
{
    // call muting function
    SetMute ( static_cast<Qt::CheckState> ( value ) == Qt::Checked );
}

void CChannelFader::SetGroupID ( const int iNGroupID )
{
    iGroupID = iNGroupID;
    UpdateGroupIDDependencies();
}

void CChannelFader::UpdateGroupIDDependencies()
{
    // update the group checkbox according the current group ID setting
    pcbGroup->blockSignals ( true ); // make sure no signals are fired
    if ( iGroupID == INVALID_INDEX )
    {
        pcbGroup->setChecked ( false );
    }
    else
    {
        pcbGroup->setChecked ( true );
    }
    pcbGroup->blockSignals ( false );

    // update group checkbox text
    if ( iGroupID != INVALID_INDEX )
    {
        pcbGroup->setText ( strGroupBaseText + QString::number ( iGroupID + 1 ) );
    }
    else
    {
        pcbGroup->setText ( strGroupBaseText );
    }

    // if the group is disable for this fader, reset the previous fader level
    if ( iGroupID == INVALID_INDEX )
    {
        // for the special case that the fader is all the way down, use a small
        // value instead
        if ( GetFaderLevel() > 0 )
        {
            dPreviousFaderLevel = GetFaderLevel();
        }
        else
        {
            dPreviousFaderLevel = 1; // small value
        }
    }

    // the fader tag border color is set according to the selected group
    SetupFaderTag ( cReceivedChanInfo.eSkillLevel );
}

void CChannelFader::OnGroupStateChanged ( int )
{
    // we want a popup menu shown if the user presses the group checkbox but
    // we want to make sure that the checkbox state represents the current group
    // setting and not the current click state since the user might not click
    // on the menu but at one other place and then the popup menu disappears but
    // the checkobx state would be on an invalid state
    UpdateGroupIDDependencies();
    pGroupPopupMenu->popup ( QCursor::pos() );
}

void CChannelFader::SetMute ( const bool bState )
{
    if ( bState )
    {
        if ( !bIsMutedAtServer )
        {
            // mute channel -> send gain of 0
            emit gainValueChanged ( 0, bIsMyOwnFader, false, false, -1 ); // set level ratio to in invalid value
            bIsMutedAtServer = true;
        }
    }
    else
    {
        // only unmute if we are not solot but an other channel is on solo
        if ( ( !bOtherChannelIsSolo || IsSolo() ) && bIsMutedAtServer )
        {
            // mute was unchecked, get current fader value and apply
            emit gainValueChanged ( MathUtils::CalcFaderGain ( GetFaderLevel() ),
                                    bIsMyOwnFader,
                                    false,
                                    false,
                                    -1 ); // set level ratio to in invalid value
            bIsMutedAtServer = false;
        }
    }
}

void CChannelFader::UpdateSoloState ( const bool bNewOtherSoloState )
{
    // store state (must be done before the SetMute() call!)
    bOtherChannelIsSolo = bNewOtherSoloState;

    // mute overwrites solo -> if mute is active, do not change anything
    if ( !pcbMute->isChecked() )
    {
        // mute channel if we are not solo but another channel is solo
        SetMute ( bOtherChannelIsSolo && !IsSolo() );
    }
}

void CChannelFader::SetChannelLevel ( const uint16_t iLevel ) { plbrChannelLevel->SetValue ( iLevel ); }

void CChannelFader::SetChannelInfos ( const CChannelInfo& cChanInfo )
{
    // store received channel info
    cReceivedChanInfo = cChanInfo;

    // init properties for the tool tip
    int              iTTInstrument = CInstPictures::GetNotUsedInstrument();
    QLocale::Country eTTCountry    = QLocale::AnyCountry;

    // Label text --------------------------------------------------------------

    QString strModText = cChanInfo.strName;

    // show channel numbers if --ctrlmidich is used (#241, #95)
    if ( bMIDICtrlUsed )
    {
        strModText.prepend ( QString().setNum ( cChanInfo.iChanID ) + ":" );
    }

    QTextBoundaryFinder tbfName ( QTextBoundaryFinder::Grapheme, cChanInfo.strName );
    int                 iBreakPos;

    // apply break position and font size depending on the selected design
    if ( eDesign == GD_SLIMFADER )
    {
        // in slim mode use a non-bold font (smaller width font)
        plblLabel->setStyleSheet ( "QLabel { color: white; }" );

        // break at every 4th character
        iBreakPos = 4;
    }
    else
    {
        // in normal mode use bold font
        plblLabel->setStyleSheet ( "QLabel { color: white; font: bold 13px; }" );

        // break text at predefined position
        iBreakPos = MAX_LEN_FADER_TAG / 2;
    }

    int iInsPos     = iBreakPos;
    int iCount      = 0;
    int iLineNumber = 0;
    while ( tbfName.toNextBoundary() != -1 )
    {
        ++iCount;
        if ( iCount == iInsPos && tbfName.position() + iLineNumber < strModText.length() )
        {
            strModText.insert ( tbfName.position() + iLineNumber, QString ( "\n" ) );
            iLineNumber++;
            iInsPos += iBreakPos;
        }
    }

    plblLabel->setText ( strModText );

    // Instrument picture ------------------------------------------------------
    // get the resource reference string for this instrument
    const QString strCurResourceRef = CInstPictures::GetResourceReference ( cChanInfo.iInstrument );

    // first check if instrument picture is used or not and if it is valid
    if ( CInstPictures::IsNotUsedInstrument ( cChanInfo.iInstrument ) || strCurResourceRef.isEmpty() )
    {
        // disable instrument picture
        plblInstrument->setVisible ( false );
    }
    else
    {
        // set correct picture
        QPixmap pixInstr ( strCurResourceRef );

        if ( ( iInstrPicMaxWidth != INVALID_INDEX ) && ( pixInstr.width() > iInstrPicMaxWidth ) )
        {
            // scale instrument picture on request (scale to the width with correct aspect ratio)
            plblInstrument->setPixmap ( pixInstr.scaledToWidth ( iInstrPicMaxWidth, Qt::SmoothTransformation ) );
        }
        else
        {
            plblInstrument->setPixmap ( pixInstr );
        }
        iTTInstrument = cChanInfo.iInstrument;

        // enable instrument picture
        plblInstrument->setVisible ( true );
    }

    // Country flag icon -------------------------------------------------------
    if ( cChanInfo.eCountry != QLocale::AnyCountry )
    {
        // try to load the country flag icon
        QPixmap CountryFlagPixmap ( CLocale::GetCountryFlagIconsResourceReference ( cChanInfo.eCountry ) );

        // first check if resource reference was valid
        if ( CountryFlagPixmap.isNull() )
        {
            // disable country flag
            plblCountryFlag->setVisible ( false );
        }
        else
        {
            // set correct picture
            plblCountryFlag->setPixmap ( CountryFlagPixmap );
            eTTCountry = cChanInfo.eCountry;

            // jamony: 隐藏国旗 icon（country 字段保留，只不显示）
            plblCountryFlag->setVisible ( false );
        }
    }
    else
    {
        // disable country flag
        plblCountryFlag->setVisible ( false );
    }

    // Skill level background color --------------------------------------------
    SetupFaderTag ( cChanInfo.eSkillLevel );

    // Tool tip ----------------------------------------------------------------
    // complete musician profile in the tool tip
    QString strToolTip              = "";
    QString strAliasAccessible      = "";
    QString strInstrumentAccessible = "";
    QString strLocationAccessible   = "";

    // alias/name
    if ( !cChanInfo.strName.isEmpty() )
    {
        strToolTip += "<h4>" + tr ( "Alias/Name" ) + "</h4>" + cChanInfo.strName;
        strAliasAccessible += cChanInfo.strName;
    }

    // instrument
    if ( !CInstPictures::IsNotUsedInstrument ( iTTInstrument ) )
    {
        strToolTip += "<h4>" + tr ( "Instrument" ) + "</h4>" + CInstPictures::GetName ( iTTInstrument );

        strInstrumentAccessible += CInstPictures::GetName ( iTTInstrument );
    }

    // location
    if ( ( eTTCountry != QLocale::AnyCountry ) || ( !cChanInfo.strCity.isEmpty() ) )
    {
        strToolTip += "<h4>" + tr ( "Location" ) + "</h4>";

        if ( !cChanInfo.strCity.isEmpty() )
        {
            strToolTip += cChanInfo.strCity;
            strLocationAccessible += cChanInfo.strCity;

            if ( eTTCountry != QLocale::AnyCountry )
            {
                strToolTip += ", ";
                strLocationAccessible += ", ";
            }
        }

        if ( eTTCountry != QLocale::AnyCountry )
        {
            strToolTip += QLocale::countryToString ( eTTCountry );
            strLocationAccessible += QLocale::countryToString ( eTTCountry );
        }
    }

    // skill level
    QString strSkillLevel;

    switch ( cChanInfo.eSkillLevel )
    {
    case SL_BEGINNER:
        strSkillLevel = tr ( "Beginner" );
        strToolTip += "<h4>" + tr ( "Skill Level" ) + "</h4>" + strSkillLevel;
        strInstrumentAccessible += ", " + strSkillLevel;
        break;

    case SL_INTERMEDIATE:
        strSkillLevel = tr ( "Intermediate" );
        strToolTip += "<h4>" + tr ( "Skill Level" ) + "</h4>" + strSkillLevel;
        strInstrumentAccessible += ", " + strSkillLevel;
        break;

    case SL_PROFESSIONAL:
        strSkillLevel = tr ( "Expert" );
        strToolTip += "<h4>" + tr ( "Skill Level" ) + "</h4>" + strSkillLevel;
        strInstrumentAccessible += ", " + strSkillLevel;
        break;

    case SL_NOT_SET:
        // skill level not set, do not add this entry
        break;
    }

    // if no information is given, leave the tool tip empty, otherwise add header
    if ( !strToolTip.isEmpty() )
    {
        strToolTip.prepend ( "<h3>" + tr ( "Musician Profile" ) + "</h3>" );
    }

    // jamony 08-14: 用户名框移除 toolTip（Musician Profile），定稿见 jamsoul说明文本.txt 配置项
    plblCountryFlag->setAccessibleDescription ( strLocationAccessible );
    plblInstrument->setAccessibleDescription ( strInstrumentAccessible );
    plblLabel->setAccessibleName ( strAliasAccessible );
    plblLabel->setAccessibleDescription ( tr ( "Alias" ) );
    pcbMute->setAccessibleName ( "Mute " + strAliasAccessible + ", " + strInstrumentAccessible );
    pcbSolo->setAccessibleName ( "Solo " + strAliasAccessible + ", " + strInstrumentAccessible );
    pcbGroup->setAccessibleName ( "Group " + strAliasAccessible + ", " + strInstrumentAccessible );
    dynamic_cast<QWidget*> ( plblLabel->parent() )
        ->setAccessibleName ( strAliasAccessible + ", " + strInstrumentAccessible + ", " + strLocationAccessible );
}

/******************************************************************************\
* CAudioMixerBoard                                                             *
\******************************************************************************/
CAudioMixerBoard::CAudioMixerBoard ( QWidget* parent ) :
    QGroupBox ( parent ),
    pSettings ( nullptr ),
    bDisplayPans ( false ),
    bIsPanSupported ( false ),
    bNoFaderVisible ( true ),
    iMyChannelID ( INVALID_INDEX ),
    iRunningNewClientCnt ( 0 ),
    iNumMixerPanelRows ( 1 ), // pSettings->iNumMixerPanelRows is not yet available
    strServerName ( "" ),
    eRecorderState ( RS_UNDEFINED ),
    eChSortType ( ST_NO_SORT )
{
    // add group box and hboxlayout
    QHBoxLayout* pGroupBoxLayout = new QHBoxLayout ( this );
    QWidget*     pMixerWidget    = new QWidget(); // will be added to the scroll area which is then the parent
    pScrollArea                  = new CMixerBoardScrollArea ( this );
    pMainLayout                  = new QGridLayout ( pMixerWidget );

    setAccessibleName ( "Personal Mix at the Server groupbox" );
    // jamony 08-14: 移除 MainMixerBoard「位于服务器的个人混音室」whatsThis（定稿见 jamsoul说明文本.txt 配置项）
    setWhatsThis ( "" );

    // set title text (default: no server given)
    SetServerName ( "" );

    // create all mixer controls and make them invisible
    vecpChanFader.Init ( MAX_NUM_CHANNELS );

    vecAvgLevels.Init ( MAX_NUM_CHANNELS, 0.0f );

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        vecpChanFader[i] = new CChannelFader ( this );
        vecpChanFader[i]->Hide();
    }

    // insert horizontal spacer (at position MAX_NUM_CHANNELS+1 which is index MAX_NUM_CHANNELS)
    pMainLayout->addItem ( new QSpacerItem ( 0, 0, QSizePolicy::Expanding ), 0, MAX_NUM_CHANNELS );

    // set margins of the layout to zero to get maximum space for the controls
    pGroupBoxLayout->setContentsMargins ( 0, 0, 0, 1 ); // note: to avoid problems at the bottom, use a small margin for that
    pMainLayout->setContentsMargins ( 0, 4, 12, 12 ); // jamony 08-12: 回退 right 0→12 (c5ec5295 状态)

    // add the group box to the scroll area
    pScrollArea->setMinimumWidth ( 0 ); // jamony: 板宽由 MainMixerBoard setMinimumWidth 控制(2轨最小), 不强制200
    pScrollArea->setWidget ( pMixerWidget );
    pScrollArea->setWidgetResizable ( true ); // make sure it fills the entire scroll area
    pScrollArea->setFrameShape ( QFrame::NoFrame );
    pScrollArea->setHorizontalScrollBarPolicy ( Qt::ScrollBarAsNeeded ); // jamony 08-17: AsNeeded —— 装不下自动出现, 装下自动消失(与 ChangeFaderOrder 一致)
    // jamony: 水平滚动条 styled(深色配 jamsoul)
    pScrollArea->horizontalScrollBar()->setStyleSheet(
        "QScrollBar:horizontal { background: transparent; height: 10px; border: none; margin: 0; }"
        "QScrollBar::handle:horizontal { background: #555; min-width: 28px; border-radius: 3px; margin: 1px; }"
        "QScrollBar::handle:horizontal:hover { background: #888; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; height: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }" );
    pGroupBoxLayout->addWidget ( pScrollArea );

    // Connections -------------------------------------------------------------
    connectFaderSignalsToMixerBoardSlots<MAX_NUM_CHANNELS>();
}

CAudioMixerBoard::~CAudioMixerBoard()
{
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        delete vecpChanFader[i];
    }
}

template<unsigned int slotId>
inline void CAudioMixerBoard::connectFaderSignalsToMixerBoardSlots()
{
    size_t iCurChanID = slotId - 1;

    void ( CAudioMixerBoard::*pGainValueChanged ) ( float, bool, bool, bool, double ) = &CAudioMixerBoardSlots<slotId>::OnChGainValueChanged;

    void ( CAudioMixerBoard::*pPanValueChanged ) ( float ) = &CAudioMixerBoardSlots<slotId>::OnChPanValueChanged;

    QObject::connect ( vecpChanFader[iCurChanID], &CChannelFader::soloStateChanged, this, &CAudioMixerBoard::UpdateSoloStates );

    QObject::connect ( vecpChanFader[iCurChanID], &CChannelFader::gainValueChanged, this, pGainValueChanged );

    QObject::connect ( vecpChanFader[iCurChanID], &CChannelFader::panValueChanged, this, pPanValueChanged );

    connectFaderSignalsToMixerBoardSlots<slotId - 1>();
}

template<>
inline void CAudioMixerBoard::connectFaderSignalsToMixerBoardSlots<0>()
{}

void CAudioMixerBoard::SetServerName ( const QString& strNewServerName )
{
    // store the current server name
    strServerName = strNewServerName;

    if ( strServerName.isEmpty() )
    {
        // no connection or connection was reset: show default title
        setTitle ( "" ); // jamony: 隐藏 title 展示(原 tr("Server")), 逻辑保留
    }
    else
    {
        // Do not set the server name directly but first show a label which indicates
        // that we are trying to connect the server. First if a connected client
        // list was received, the connection was successful and the title is updated
        // with the correct server name. Make sure to choose a "try to connect" title
        // which is most striking (we use filled blocks and upper case letters).
        setTitle ( "" ); // jamony: \u9690\u85cf title \u5c55\u793a(\u539f TRYING TO CONNECT), \u903b\u8f91\u4fdd\u7559
    }
}

void CAudioMixerBoard::SetGUIDesign ( const EGUIDesign eNewDesign )
{
    // move the channels tighter together in slim fader mode
    if ( eNewDesign == GD_SLIMFADER )
    {
        pMainLayout->setSpacing ( 2 );
    }
    else
    {
        pMainLayout->setSpacing ( 5 ); // jamony: 分轨间距 6→5(欢哥定; 与 iMixerWidth ×5 匹配)
    }

    // apply GUI design to child GUI controls
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        vecpChanFader[i]->SetGUIDesign ( eNewDesign );
    }
}

void CAudioMixerBoard::SetMeterStyle ( const EMeterStyle eNewMeterStyle )
{
    // apply GUI design to child GUI controls
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        vecpChanFader[i]->SetMeterStyle ( eNewMeterStyle );
    }
}

void CAudioMixerBoard::SetDisplayPans ( const bool eNDP )
{
    bDisplayPans = eNDP;

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        vecpChanFader[i]->SetDisplayPans ( eNDP && bIsPanSupported );
    }
}

void CAudioMixerBoard::SetPanIsSupported()
{
    bIsPanSupported = true;
    SetDisplayPans ( bDisplayPans );
}

void CAudioMixerBoard::HideAll()
{
    // before hiding the faders, store their settings
    StoreAllFaderSettings();

    // make all controls invisible
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        vecpChanFader[i]->SetChannelLevel ( 0 );
        vecpChanFader[i]->SetDisplayChannelLevel ( false );
        vecpChanFader[i]->SetDisplayPans ( false );
        vecpChanFader[i]->Hide();
    }

    // initialize flags and other parameters
    bIsPanSupported      = false;
    bNoFaderVisible      = true;
    eRecorderState       = RS_UNDEFINED;
    iMyChannelID         = INVALID_INDEX;
    iRunningNewClientCnt = 0; // reset running counter on new server connection

    // use original order of channel (by server ID)
    ChangeFaderOrder ( ST_NO_SORT );

    // Reset recording indication styleSheet
    setStyleSheet ( "" );

    // emit status of connected clients
    emit NumClientsChanged ( 0 ); // -> no clients connected
}

void CAudioMixerBoard::SetNumMixerPanelRows ( const int iNNumMixerPanelRows )
{
    // store new value and immediately initiate the sorting
    iNumMixerPanelRows = iNNumMixerPanelRows;
    ChangeFaderOrder ( eChSortType );
}

void CAudioMixerBoard::SetFaderSorting ( const EChSortType eNChSortType )
{
    // store new sort type and update the fader order
    eChSortType = eNChSortType;
    ChangeFaderOrder ( eNChSortType );
}

void CAudioMixerBoard::ChangeFaderOrder ( const EChSortType eChSortType )
{
    QMutexLocker locker ( &Mutex );

    // create a pair list of lower strings and fader ID for each channel
    QList<QPair<QString, size_t>> PairList;
    int                           iNumVisibleFaders = 0;
    int                           iMyFader          = -1;

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( vecpChanFader[i]->GetIsMyOwnFader() )
        {
            iMyFader = static_cast<int> ( i );
        }

        switch ( eChSortType )
        {
        case ST_BY_NAME:
            PairList << QPair<QString, size_t> ( vecpChanFader[i]->GetReceivedName().toLower(), i );
            break;
        case ST_BY_CITY:
            // sort first "by city" and second "by name" by adding the name after the city
            PairList << QPair<QString, size_t> ( vecpChanFader[i]->GetReceivedCity().toLower() + vecpChanFader[i]->GetReceivedName().toLower(), i );
            break;
        case ST_BY_INSTRUMENT:
            // sort first "by instrument" and second "by name" by adding the name after the instrument
            PairList << QPair<QString, size_t> ( CInstPictures::GetName ( vecpChanFader[i]->GetReceivedInstrument() ) +
                                                     vecpChanFader[i]->GetReceivedName().toLower(),
                                                 i );
            break;
        case ST_BY_GROUPID:
            // sort first "by group" and second "by name" by adding the name after the group
            if ( vecpChanFader[i]->GetGroupID() == INVALID_INDEX )
            {
                // put channels without a group at the end
                PairList << QPair<QString, size_t> ( "999" + vecpChanFader[i]->GetReceivedName().toLower(),
                                                     i ); // worst case is one group per channel (current max is 8)
            }
            else
            {
                PairList << QPair<QString, size_t> ( QString ( "%1" ).arg ( vecpChanFader[i]->GetGroupID(), 3, 10, QLatin1Char ( '0' ) ) +
                                                         vecpChanFader[i]->GetReceivedName().toLower(),
                                                     i );
            }
            break;
        case ST_BY_SERVER_CHANNEL:
            PairList << QPair<QString, size_t> ( QString ( "%1" ).arg ( vecpChanFader[i]->GetReceivedChID(), 3, 10, QLatin1Char ( '0' ) ) +
                                                     vecpChanFader[i]->GetReceivedName().toLower(),
                                                 i );
            break;
        default: // ST_NO_SORT
            // per definition for no sort: faders are sorted in the order they appeared (note that we
            // pad to a total of 11 characters with zeros to make sure the sorting is done correctly)
            PairList << QPair<QString, size_t> ( QString ( "%1" ).arg ( vecpChanFader[i]->GetRunningNewClientCnt(), 11, 10, QLatin1Char ( '0' ) ),
                                                 i );
            break;
        }

        // count the number of visible faders
        if ( vecpChanFader[i]->IsVisible() )
        {
            iNumVisibleFaders++;
        }
    }

    // sort the channels according to the first of the pair
    std::stable_sort ( PairList.begin(), PairList.end() );

    // move my fader to first position
    if ( pSettings->bOwnFaderFirst )
    {
        for ( int i = 0; i < MAX_NUM_CHANNELS; i++ )
        {
            if ( iMyFader == static_cast<int> ( PairList[i].second ) )
            {
                PairList.move ( i, 0 );
                break;
            }
        }
    }

    // jamony: jamony-looper 幽灵乐手排最后
    for ( int i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( vecpChanFader[PairList[i].second]->GetReceivedName() == "jamony-looper" )
        {
            PairList.move ( i, MAX_NUM_CHANNELS - 1 );
            break;
        }
    }

    // we want to distribute iNumVisibleFaders across the first row, then the next, etc
    // up to iNumMixerPanelRows.  So row wants to start at 0 until we get to some number,
    // then increase, where "some number" means we get no more than iNumMixerPanelRows.
    const int iNumFadersFirstRow = ( iNumVisibleFaders + iNumMixerPanelRows - 1 ) / iNumMixerPanelRows;

    // add channels to the layout in the new order, note that it is not required to remove
    // the widget from the layout first but it is moved to the new position automatically
    int iVisibleFaderCnt = 0;

    for ( int i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        const size_t iCurFaderID = PairList[i].second;

        if ( vecpChanFader[iCurFaderID]->IsVisible() )
        {
            // channels are added row-first, up to iNumFadersFirstRow, then onto
            // the next row.
            pMainLayout->addWidget ( vecpChanFader[iCurFaderID]->GetMainWidget(),
                                     iVisibleFaderCnt / iNumFadersFirstRow,
                                     iVisibleFaderCnt % iNumFadersFirstRow );

            iVisibleFaderCnt++;
        }
    }

    // jamony 08-17: 拖拽逻辑照抄 jamulus 原版 —— A/B 栏写死, 拖宽只喂 C 区。
    // MainMixerBoard 只设最小宽(=2轨: 自己+jamony-looper 保证完整显示), 不再 setFixedWidth 锁死、
    // 不再随分轨数自动加宽窗口; 多余宽度全部被板内网格尾部 Expanding spacer 吸收(分轨永远64宽, 空隙堆最右轨右侧)。
    // 窗口最小宽 = 左359 + 2轨最小板宽(151), 用户不可拖窄; 程序启动后不再自动改窗口宽度(宽度归用户拖拽管)。
    const int iMixerMinWidth = 2 * 64 + 5 + 18; // 2轨128 + 间距5 + 余量18 (与 c5ec5295 的 2 轨板宽 151 一致)
    setMinimumWidth ( iMixerMinWidth );
    // jamony 08-17: 水平滚动条 AsNeeded —— 视口装不下分轨自动出现, 刚好装下自动消失(拖宽看全部分轨场景)
    pScrollArea->setHorizontalScrollBarPolicy ( Qt::ScrollBarAsNeeded );
    // jamony 08-17: 默认宽=最小宽(2轨), 首次调用一次性设定(启动场景); 之后永不 resize(窗口宽度归用户拖拽管, 人进人出不碰)
    if ( QWidget* pw = window() )
    {
        const int iMinWinWidth = 359 + iMixerMinWidth; // 359 = 左12 + A栏272 + 间距5 + B栏38 + 间距5 + BC线 + vl5右12 + hbox右3 (c5ec5295 常数, 2轨窗口510)
        pw->setMinimumWidth ( iMinWinWidth );          // 锁死: 不可向左拖窄
        if ( !bDefaultWidthApplied )
        {
            bDefaultWidthApplied = true;
            pw->resize ( iMinWinWidth, pw->height() ); // .ui 初始 511 → 拉到最小宽 510, 保证默认态=最小态=2轨完整
        }
    }
}

void CAudioMixerBoard::UpdateTitle()
{
    QString strTitlePrefix = "";

    if ( eRecorderState == RS_RECORDING )
    {
        strTitlePrefix = QString ( "[%1] " ).arg ( tr ( "RECORDING ACTIVE" ) );
    }

    // replace & signs with && (See Qt documentation for QLabel)
    // if strServerName includes an "&" sign, this is interpreted as keyboard shortcut (#1886)
    // it might be possible to find a more elegant solution here?

    QString strEscServerName = strServerName;
    strEscServerName.replace ( "&", "&&" );

    // jamony: 隐藏 title 展示(原 strTitlePrefix+音频已连接, 不暴露服务器地址), 逻辑保留
    setTitle ( "" );
    setAccessibleName ( title() );
}

void CAudioMixerBoard::SetRecorderState ( const ERecorderState newRecorderState )
{
    // store the new recorder state and update the title
    eRecorderState = newRecorderState;
    UpdateTitle();
}

void CAudioMixerBoard::ApplyNewConClientList ( CVector<CChannelInfo>& vecChanInfo )
{
    // get number of connected clients
    const size_t iNumConnectedClients = vecChanInfo.size();

    Mutex.lock();
    {
        // we want to set the server name only if the very first faders appear
        // in the audio mixer board to show a "try to connect" before
        if ( bNoFaderVisible )
        {
            UpdateTitle();
        }

        // search for channels which are already present and preserve their gain
        // setting, for all other channels reset gain

        // get all channels which are in use/not in use.
        // We use the array index of vecChanInfo if the fader is in use,
        // else INVALID_INDEX to specify it is not in use
        // so must use "int" for the array type.
        int iFaderNumber[MAX_NUM_CHANNELS];

        for ( size_t iChanID = 0; iChanID < MAX_NUM_CHANNELS; iChanID++ )
        {
            iFaderNumber[iChanID] = INVALID_INDEX;
        }

        for ( size_t iFader = 0; iFader < iNumConnectedClients; iFader++ )
        {
            // ideally "iChanID" in CChannelInfo would be size_t if it can never be INVALID_INDEX
            // as assumed here
            iFaderNumber[vecChanInfo[iFader].iChanID] = static_cast<int> ( iFader );
        }

        // Hide all unused faders and initialize used ones
        for ( size_t iChanID = 0; iChanID < MAX_NUM_CHANNELS; iChanID++ )
        {
            if ( iFaderNumber[iChanID] == INVALID_INDEX )
            {
                // current fader is not used
                StoreFaderSettings ( vecpChanFader[iChanID] );

                vecpChanFader[iChanID]->Hide();
                continue;
            }
            size_t idxVecpChan = static_cast<size_t> ( iFaderNumber[iChanID] );

            // current fader is used
            if ( !vecpChanFader[iChanID]->IsVisible() )
            {
                // the fader was not in use,
                // reset everything for new client
                vecpChanFader[iChanID]->Reset();
                vecAvgLevels[iChanID] = 0.0f;

                if ( static_cast<int> ( iChanID ) == iMyChannelID )
                {
                    // this is my own fader --> set fader property
                    vecpChanFader[iChanID]->SetIsMyOwnFader();
                }

                // keep track of each new client
                // for "no sorting" channel sort order new clients are added
                // to the right-hand side of the mixer (#673)
                vecpChanFader[iChanID]->SetRunningNewClientCnt ( iRunningNewClientCnt++ );

                // show fader
                vecpChanFader[iChanID]->Show();

                // Set the default initial fader level. Check first that
                // this is not the initialization (i.e. previously there
                // were no faders visible) to avoid that our own level is
                // adjusted. If we have received our own channel ID, then
                // we can adjust the level even if no fader was visible.
                // The fader level of 100 % is the default in the
                // server, in that case we do not have to do anything here.
                if ( ( !bNoFaderVisible || ( ( iMyChannelID != INVALID_INDEX ) && ( iMyChannelID != static_cast<int> ( iChanID ) ) ) ) &&
                     ( pSettings->iNewClientFaderLevel != 100 ) )
                {
                    // the value is in percent -> convert range
                    vecpChanFader[iChanID]->SetFaderLevel ( pSettings->iNewClientFaderLevel / 100.0 * AUD_MIX_FADER_MAX );
                }
            }

            if ( vecpChanFader[iChanID]->GetReceivedName().compare ( vecChanInfo[idxVecpChan].strName ) )
            {
                // the text has actually changed, search in the list of
                // stored settings if we have a matching entry
                int  iStoredFaderLevel;
                int  iStoredPanValue;
                bool bStoredFaderIsSolo;
                bool bStoredFaderIsMute;
                int  iGroupID;

                if ( GetStoredFaderSettings ( vecChanInfo[idxVecpChan].strName,
                                              iStoredFaderLevel,
                                              iStoredPanValue,
                                              bStoredFaderIsSolo,
                                              bStoredFaderIsMute,
                                              iGroupID ) )
                {
                    vecpChanFader[iChanID]->SetFaderLevel ( iStoredFaderLevel, true ); // suppress group update
                    vecpChanFader[iChanID]->SetPanValue ( iStoredPanValue );
                    vecpChanFader[iChanID]->SetFaderIsSolo ( bStoredFaderIsSolo );
                    vecpChanFader[iChanID]->SetFaderIsMute ( bStoredFaderIsMute );
                    vecpChanFader[iChanID]->SetGroupID ( iGroupID ); // Must be the last to be set in the fader!
                }
            }

            // set the channel infos
            vecpChanFader[iChanID]->SetChannelInfos ( vecChanInfo[idxVecpChan] );
        }

        // update the solo states since if any channel was on solo and a new client
        // has just connected, the new channel must be muted
        UpdateSoloStates();

        // update flag for "all faders are invisible"
        bNoFaderVisible = ( iNumConnectedClients == 0 );
    }
    Mutex.unlock(); // release mutex

    // Ensure MIDI state is applied to faders during the connection process
    SetMIDICtrlUsed ( pSettings->bUseMIDIController );

    // sort the channels according to the selected sorting type
    ChangeFaderOrder ( eChSortType );

    // emit status of connected clients
    emit NumClientsChanged ( static_cast<int> ( iNumConnectedClients ) );
}

void CAudioMixerBoard::SetFaderLevel ( const int iChannelIdx, const int iValue )
{
    // only apply new fader level if channel index is valid and the fader is visible
    if ( ( iChannelIdx >= 0 ) && ( iChannelIdx < MAX_NUM_CHANNELS ) )
    {
        if ( vecpChanFader[static_cast<size_t> ( iChannelIdx )]->IsVisible() )
        {
            // Check for MIDI pickup mode
            if ( pSettings && pSettings->bMIDIPickupMode && g_midiPickupInitialized[iChannelIdx] )
            {
                auto& midiState = g_midiPickupStates[iChannelIdx];
                midiPickupInactivityCheck ( iChannelIdx, midiState.lastMidiTimeFader, midiState.recentFader, g_midiPickupWaitingForPickup );
            }

            if ( pSettings && pSettings->bMIDIPickupMode )
            {
                // Initialize pickup state on first use
                if ( !g_midiPickupInitialized[iChannelIdx] )
                {
                    g_midiPickupInitialized[iChannelIdx]      = true;
                    g_midiPickupWaitingForPickup[iChannelIdx] = true;
                    g_midiPickupStates[iChannelIdx].recentFader.clear();
                }

                auto& pickup                              = g_midiPickupStates[iChannelIdx].recentFader;
                int   current                             = vecpChanFader[static_cast<size_t> ( iChannelIdx )]->GetFaderLevel();
                bool  waiting                             = g_midiPickupWaitingForPickup[iChannelIdx];
                waiting                                   = midiPickupTryApply ( iValue, current, MIDI_PICKUP_TOLERANCE, pickup, waiting );
                g_midiPickupWaitingForPickup[iChannelIdx] = waiting;

                if ( waiting )
                    return; // Don't apply the value yet, still waiting for pickup
            }

            vecpChanFader[static_cast<size_t> ( iChannelIdx )]->SetFaderLevel ( iValue );
        }
    }
}

void CAudioMixerBoard::SetPanValue ( const int iChannelIdx, const int iValue )
{
    // only apply new pan value if channel index is valid and the panner is visible
    if ( ( iChannelIdx >= 0 ) && ( iChannelIdx < MAX_NUM_CHANNELS ) && bDisplayPans )
    {
        if ( vecpChanFader[static_cast<size_t> ( iChannelIdx )]->IsVisible() )
        {
            // Check for MIDI pickup mode
            if ( pSettings && pSettings->bMIDIPickupMode && g_midiPickupInitialized[iChannelIdx] )
            {
                auto& midiState = g_midiPickupStates[iChannelIdx];
                midiPickupInactivityCheck ( iChannelIdx, midiState.lastMidiTimePan, midiState.recentPan, g_midiPickupWaitingForPickup );
            }

            if ( pSettings && pSettings->bMIDIPickupMode )
            {
                // Initialize pickup state on first use
                if ( !g_midiPickupInitialized[iChannelIdx] )
                {
                    g_midiPickupInitialized[iChannelIdx]      = true;
                    g_midiPickupWaitingForPickup[iChannelIdx] = true;
                    g_midiPickupStates[iChannelIdx].recentPan.clear();
                }

                auto& pickup                              = g_midiPickupStates[iChannelIdx].recentPan;
                int   current                             = vecpChanFader[static_cast<size_t> ( iChannelIdx )]->GetPanValue();
                bool  waiting                             = g_midiPickupWaitingForPickup[iChannelIdx];
                waiting                                   = midiPickupTryApply ( iValue, current, MIDI_PICKUP_TOLERANCE, pickup, waiting );
                g_midiPickupWaitingForPickup[iChannelIdx] = waiting;

                if ( waiting )
                    return; // Don't apply the value yet, still waiting for pickup
            }

            vecpChanFader[static_cast<size_t> ( iChannelIdx )]->SetPanValue ( iValue );
        }
    }
}

void CAudioMixerBoard::SetFaderIsSolo ( const int iChannelIdx, const bool bIsSolo )
{
    // only apply solo if channel index is valid and the fader is visible
    if ( ( iChannelIdx >= 0 ) && ( iChannelIdx < MAX_NUM_CHANNELS ) )

    {
        if ( vecpChanFader[static_cast<size_t> ( iChannelIdx )]->IsVisible() )
        {
            vecpChanFader[static_cast<size_t> ( iChannelIdx )]->SetFaderIsSolo ( bIsSolo );
        }
    }
}

void CAudioMixerBoard::SetFaderIsMute ( const int iChannelIdx, const bool bIsMute )
{
    // only apply mute if channel index is valid and the fader is visible
    if ( ( iChannelIdx >= 0 ) && ( iChannelIdx < MAX_NUM_CHANNELS ) )
    {
        if ( vecpChanFader[static_cast<size_t> ( iChannelIdx )]->IsVisible() )
        {
            vecpChanFader[static_cast<size_t> ( iChannelIdx )]->SetFaderIsMute ( bIsMute );
        }
    }
}

void CAudioMixerBoard::SetAllFaderLevelsToNewClientLevel()
{
    QMutexLocker locker ( &Mutex );

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        // only apply to visible faders and not to my own channel fader
        if ( vecpChanFader[i]->IsVisible() && ( static_cast<int> ( i ) != iMyChannelID ) )
        {
            // the value is in percent -> convert range, also use the group
            // update flag to make sure the group values are all set to the
            // same fader level now
            vecpChanFader[i]->SetFaderLevel ( pSettings->iNewClientFaderLevel / 100.0 * AUD_MIX_FADER_MAX, true );

            // Reset MIDI pickup state so pickup mode works with the new value
            if ( g_midiPickupInitialized[i] )
            {
                g_midiPickupWaitingForPickup[i] = true;
                g_midiPickupStates[i].recentFader.clear();
            }
        }
    }
}

void CAudioMixerBoard::AutoAdjustAllFaderLevels()
{
    QMutexLocker locker ( &Mutex );

    // initialize variables used for statistics
    float vecMaxLevel[MAX_NUM_FADER_GROUPS + 1];
    int   vecChannelsPerGroup[MAX_NUM_FADER_GROUPS + 1];
    for ( int i = 0; i < MAX_NUM_FADER_GROUPS + 1; ++i )
    {
        vecMaxLevel[i]         = LOW_BOUND_SIG_METER;
        vecChannelsPerGroup[i] = 0;
    }
    CVector<CVector<float>> levels;
    levels.resize ( MAX_NUM_FADER_GROUPS + 1 );

    // compute min/max level per group and number of channels per group
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; ++i )
    {
        // only apply to visible faders (and not to my own channel fader)
        if ( vecpChanFader[i]->IsVisible() && ( static_cast<int> ( i ) != iMyChannelID ) )
        {
            // map averaged meter output level to decibels
            // (invert CStereoSignalLevelMeter::CalcLogResultForMeter)
            float leveldB = vecAvgLevels[i] * ( UPPER_BOUND_SIG_METER - LOW_BOUND_SIG_METER ) / NUM_STEPS_LED_BAR + LOW_BOUND_SIG_METER;

            size_t group = MAX_NUM_FADER_GROUPS;
            if ( vecpChanFader[i]->GetGroupID() != INVALID_INDEX )
            {
                group = static_cast<size_t> ( vecpChanFader[i]->GetGroupID() );
            }

            if ( leveldB >= AUTO_FADER_NOISE_THRESHOLD_DB )
            {
                vecMaxLevel[group] = fmax ( vecMaxLevel[group], leveldB );
                levels[group].Add ( leveldB );
            }
            ++vecChannelsPerGroup[group];
        }
    }

    // sort levels for later median computation
    for ( size_t i = 0; i < MAX_NUM_FADER_GROUPS + 1; ++i )
    {
        std::sort ( levels[i].begin(), levels[i].end() );
    }

    // compute the number of active groups (at least one channel)
    int cntActiveGroups = 0;
    for ( int i = 0; i < MAX_NUM_FADER_GROUPS; ++i )
    {
        cntActiveGroups += vecChannelsPerGroup[i] > 0;
    }

    // only my channel is active, nothing to do
    if ( cntActiveGroups == 0 && vecChannelsPerGroup[MAX_NUM_FADER_GROUPS] == 0 )
    {
        return;
    }

    // compute target level for each group
    // (prevent clipping when each group contributes at maximum level)
    float targetLevelPerGroup = -20.0f * log10 ( std::max ( cntActiveGroups, 1 ) );

    // compute target levels for the channels of each group individually
    float vecTargetChannelLevel[MAX_NUM_FADER_GROUPS + 1];
    float levelOffset = 0.0f;
    float minFader    = 0.0f;
    for ( size_t i = 0; i < MAX_NUM_FADER_GROUPS + 1; ++i )
    {
        // compute the target level for each channel in the current group
        // (prevent clipping when each channel in this group contributes at
        // the maximum level)
        vecTargetChannelLevel[i] = vecChannelsPerGroup[i] > 0 ? targetLevelPerGroup - 20.0f * log10 ( vecChannelsPerGroup[i] ) : 0.0f;

        // get median level
        size_t cntChannels = levels[i].size();
        if ( cntChannels == 0 )
        {
            continue;
        }
        float refLevel = levels[i][cntChannels / 2];

        // since we can only attenuate channels but not amplify, we have to
        // check that the reference channel can be brought to the target
        // level
        if ( refLevel < vecTargetChannelLevel[i] )
        {
            // otherwise, we adjust the level offset in such a way that
            // the level can be reached
            levelOffset = fmin ( levelOffset, refLevel - vecTargetChannelLevel[i] );

            // compute the minimum necessary fader setting
            minFader = fmin ( minFader, -vecMaxLevel[i] + vecTargetChannelLevel[i] + levelOffset );
        }
    }

    // take minimum fader value into account
    // very weak channels would actually require strong channels to be
    // attenuated to a large amount; however, the attenuation is limited by
    // the faders
    if ( minFader < -AUD_MIX_FADER_RANGE_DB )
    {
        levelOffset += -AUD_MIX_FADER_RANGE_DB - minFader;
    }

    // adjust all levels
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; ++i )
    {
        // only apply to visible faders (and not to my own channel fader)
        if ( vecpChanFader[i]->IsVisible() && ( static_cast<int> ( i ) != iMyChannelID ) )
        {
            // map averaged meter output level to decibels
            // (invert CStereoSignalLevelMeter::CalcLogResultForMeter)
            float leveldB = vecAvgLevels[i] * ( UPPER_BOUND_SIG_METER - LOW_BOUND_SIG_METER ) / NUM_STEPS_LED_BAR + LOW_BOUND_SIG_METER;

            int group = vecpChanFader[i]->GetGroupID();
            if ( group == INVALID_INDEX )
            {
                if ( cntActiveGroups > 0 )
                {
                    // do not adjust the channels without group in group mode
                    continue;
                }
                else
                {
                    group = MAX_NUM_FADER_GROUPS;
                }
            }

            // do not adjust channels with almost zero level to full level since
            // the channel might simply be silent at the moment
            if ( leveldB >= AUTO_FADER_NOISE_THRESHOLD_DB )
            {
                // compute new level
                float newdBLevel = -leveldB + vecTargetChannelLevel[group] + levelOffset;

                // map range from decibels to fader level
                // (this inverts MathUtils::CalcFaderGain)
                float newFaderLevel = ( newdBLevel / AUD_MIX_FADER_RANGE_DB + 1.0f ) * AUD_MIX_FADER_MAX;

                // limit fader
                newFaderLevel = fmin ( fmax ( newFaderLevel, 0.0f ), float ( AUD_MIX_FADER_MAX ) );

                // set fader level
                vecpChanFader[i]->SetFaderLevel ( newFaderLevel, true );

                // Reset MIDI pickup state so pickup mode works with the auto-adjusted value
                if ( g_midiPickupInitialized[i] )
                {
                    g_midiPickupWaitingForPickup[i] = true;
                    g_midiPickupStates[i].recentFader.clear();
                }
            }
        }
    }
}

void CAudioMixerBoard::SetMIDICtrlUsed ( const bool bMIDICtrlUsed )
{
    QMutexLocker locker ( &Mutex );

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        vecpChanFader[i]->SetMIDICtrlUsed ( bMIDICtrlUsed );
    }

    // Reset MIDI pickup state when toggling MIDI control to
    // ensure pickup mode works correctly when re-enabling MIDI
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        g_midiPickupInitialized[i]      = false;
        g_midiPickupWaitingForPickup[i] = false;
        g_midiPickupStates[i].recentFader.clear();
        g_midiPickupStates[i].recentPan.clear();
    }
}

void CAudioMixerBoard::StoreAllFaderSettings()
{
    QMutexLocker locker ( &Mutex );

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        StoreFaderSettings ( vecpChanFader[i] );
    }
}

void CAudioMixerBoard::LoadAllFaderSettings()
{
    QMutexLocker locker ( &Mutex );

    int  iStoredFaderLevel;
    int  iStoredPanValue;
    bool bStoredFaderIsSolo;
    bool bStoredFaderIsMute;
    int  iGroupID;

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( GetStoredFaderSettings ( vecpChanFader[i]->GetReceivedName(),
                                      iStoredFaderLevel,
                                      iStoredPanValue,
                                      bStoredFaderIsSolo,
                                      bStoredFaderIsMute,
                                      iGroupID ) )
        {
            vecpChanFader[i]->SetFaderLevel ( iStoredFaderLevel, true ); // suppress group update
            vecpChanFader[i]->SetPanValue ( iStoredPanValue );
            vecpChanFader[i]->SetFaderIsSolo ( bStoredFaderIsSolo );
            vecpChanFader[i]->SetFaderIsMute ( bStoredFaderIsMute );
            vecpChanFader[i]->SetGroupID ( iGroupID ); // Must be the last to be set in the fader!

            // Reset MIDI pickup state for this channel so pickup mode works with restored values
            // Without this, if MIDI values arrived before settings were loaded, pickup might have
            // already occurred at the wrong value, causing jumps when the controller is moved
            if ( g_midiPickupInitialized[i] )
            {
                g_midiPickupWaitingForPickup[i] = true;
                g_midiPickupStates[i].recentFader.clear();
                g_midiPickupStates[i].recentPan.clear();
            }
        }
    }
}

void CAudioMixerBoard::SetRemoteFaderIsMute ( const int iChannelIdx, const bool bIsMute )
{
    // only apply remote mute state if channel index is valid and the fader is visible
    if ( ( iChannelIdx >= 0 ) && ( iChannelIdx < MAX_NUM_CHANNELS ) )
    {
        if ( vecpChanFader[static_cast<size_t> ( iChannelIdx )]->IsVisible() )
        {
            vecpChanFader[static_cast<size_t> ( iChannelIdx )]->SetRemoteFaderIsMute ( bIsMute );
        }
    }
}

void CAudioMixerBoard::UpdateSoloStates()
{
    // first check if any channel has a solo state active
    bool bAnyChannelIsSolo = false;

    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        // check if fader is in use and has solo state active
        if ( vecpChanFader[i]->IsVisible() && vecpChanFader[i]->IsSolo() )
        {
            bAnyChannelIsSolo = true;
            continue;
        }
    }

    // now update the solo state of all active faders
    for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( vecpChanFader[i]->IsVisible() )
        {
            vecpChanFader[i]->UpdateSoloState ( bAnyChannelIsSolo );
        }
    }
}

void CAudioMixerBoard::UpdateGainValue ( const int    iChannelIdx,
                                         const float  fValue,
                                         const bool   bIsMyOwnFader,
                                         const bool   bIsGroupUpdate,
                                         const bool   bSuppressServerUpdate,
                                         const double dLevelRatio )
{
    size_t stChannelIdx = static_cast<size_t> ( iChannelIdx );
    // update current gain
    if ( !bSuppressServerUpdate )
    {
        emit ChangeChanGain ( iChannelIdx, fValue, bIsMyOwnFader );
    }

    // if this fader is selected, all other in the group must be updated as
    // well (note that we do not have to update if this is already a group update
    // to avoid an infinite loop)
    if ( ( vecpChanFader[stChannelIdx]->GetGroupID() != INVALID_INDEX ) && !bIsGroupUpdate )
    {
        for ( size_t i = 0; i < MAX_NUM_CHANNELS; i++ )
        {
            // update rest of faders selected
            if ( vecpChanFader[i]->IsVisible() && ( vecpChanFader[i]->GetGroupID() == vecpChanFader[stChannelIdx]->GetGroupID() ) &&
                 ( i != stChannelIdx ) && ( dLevelRatio >= 0 ) )
            {
                // synchronize faders with moving fader level (it is important
                // to set the group flag to avoid infinite looping)
                vecpChanFader[i]->SetFaderLevel ( vecpChanFader[i]->GetPreviousFaderLevel() * dLevelRatio, true );
            }
        }
    }
}

void CAudioMixerBoard::UpdatePanValue ( const int iChannelIdx, const float fValue ) { emit ChangeChanPan ( iChannelIdx, fValue ); }

void CAudioMixerBoard::StoreFaderSettings ( CChannelFader* pChanFader )
{
    // if the fader was visible and the name is not empty, we store the old gain
    if ( pChanFader->IsVisible() && !pChanFader->GetReceivedName().isEmpty() )
    {
        CVector<int> viOldStoredFaderLevels ( pSettings->vecStoredFaderLevels );
        CVector<int> viOldStoredPanValues ( pSettings->vecStoredPanValues );
        CVector<int> vbOldStoredFaderIsSolo ( pSettings->vecStoredFaderIsSolo );
        CVector<int> vbOldStoredFaderIsMute ( pSettings->vecStoredFaderIsMute );
        CVector<int> vbOldStoredFaderGroupID ( pSettings->vecStoredFaderGroupID );

        // put new value on the top of the list
        const int iOldIdx = pSettings->vecStoredFaderTags.StringFiFoWithCompare ( pChanFader->GetReceivedName() );

        // current fader level and solo state is at the top of the list
        pSettings->vecStoredFaderLevels[0]  = pChanFader->GetFaderLevel();
        pSettings->vecStoredPanValues[0]    = pChanFader->GetPanValue();
        pSettings->vecStoredFaderIsSolo[0]  = pChanFader->IsSolo();
        pSettings->vecStoredFaderIsMute[0]  = pChanFader->IsMute();
        pSettings->vecStoredFaderGroupID[0] = pChanFader->GetGroupID();
        size_t iTempListCnt                 = 1; // current fader is on top, other faders index start at 1

        for ( size_t iIdx = 0; iIdx < MAX_NUM_STORED_FADER_SETTINGS; iIdx++ )
        {
            // first check if we still have space in our data storage
            if ( iTempListCnt < MAX_NUM_STORED_FADER_SETTINGS )
            {
                // check for the old index of the current entry (this has to be
                // skipped), note that per definition: the old index is an illegal
                // index in case the entry was not present in the vector before
                if ( static_cast<int> ( iIdx ) != iOldIdx )
                {
                    pSettings->vecStoredFaderLevels[iTempListCnt]  = viOldStoredFaderLevels[iIdx];
                    pSettings->vecStoredPanValues[iTempListCnt]    = viOldStoredPanValues[iIdx];
                    pSettings->vecStoredFaderIsSolo[iTempListCnt]  = vbOldStoredFaderIsSolo[iIdx];
                    pSettings->vecStoredFaderIsMute[iTempListCnt]  = vbOldStoredFaderIsMute[iIdx];
                    pSettings->vecStoredFaderGroupID[iTempListCnt] = vbOldStoredFaderGroupID[iIdx];

                    iTempListCnt++;
                }
            }
        }
    }
}

bool CAudioMixerBoard::GetStoredFaderSettings ( const QString& strName,
                                                int&           iStoredFaderLevel,
                                                int&           iStoredPanValue,
                                                bool&          bStoredFaderIsSolo,
                                                bool&          bStoredFaderIsMute,
                                                int&           iGroupID )
{
    // only do the check if the name string is not empty
    if ( !strName.isEmpty() )
    {
        for ( size_t iIdx = 0; iIdx < MAX_NUM_STORED_FADER_SETTINGS; iIdx++ )
        {
            // check if fader text is already known in the list
            if ( !pSettings->vecStoredFaderTags[iIdx].compare ( strName ) )
            {
                // copy stored settings values
                iStoredFaderLevel  = pSettings->vecStoredFaderLevels[iIdx];
                iStoredPanValue    = pSettings->vecStoredPanValues[iIdx];
                bStoredFaderIsSolo = pSettings->vecStoredFaderIsSolo[iIdx] != 0;
                bStoredFaderIsMute = pSettings->vecStoredFaderIsMute[iIdx] != 0;
                iGroupID           = pSettings->vecStoredFaderGroupID[iIdx];

                // values found and copied, return OK
                return true;
            }
        }
    }

    // return "not OK" since we did not find matching fader settings
    return false;
}

void CAudioMixerBoard::SetChannelLevels ( const CVector<uint16_t>& vecChannelLevel )
{
    const size_t iNumChannelLevels = vecChannelLevel.size();
    size_t       i                 = 0;

    for ( size_t iChId = 0; iChId < MAX_NUM_CHANNELS; iChId++ )
    {
        if ( vecpChanFader[iChId]->IsVisible() && ( i < iNumChannelLevels ) )
        {
            // compute exponential moving average
            vecAvgLevels[iChId] = ( 1.0f - AUTO_FADER_ADJUST_ALPHA ) * vecAvgLevels[iChId] + AUTO_FADER_ADJUST_ALPHA * vecChannelLevel[i];

            vecpChanFader[iChId]->SetChannelLevel ( vecChannelLevel[i++] );

            // show level only if we successfully received levels from the
            // server (if server does not support levels, do not show levels)
            if ( !vecpChanFader[iChId]->GetDisplayChannelLevel() )
            {
                vecpChanFader[iChId]->SetDisplayChannelLevel ( true );
            }
        }
    }
}

void CAudioMixerBoard::MuteMyChannel() { SetFaderIsMute ( iMyChannelID, true ); }
