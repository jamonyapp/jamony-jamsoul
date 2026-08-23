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
 * Description:
 *  Implements a multi color LED
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

#include "multicolorled.h"

/* Implementation *************************************************************/
CMultiColorLED::CMultiColorLED ( QWidget* parent ) :
    QLabel ( parent ),
    BitmCubeDisabled ( QString::fromUtf8 ( ":/png/LEDs/res/CLEDDisabled.png" ) ),
    BitmCubeGrey ( QString::fromUtf8 ( ":/png/LEDs/res/CLEDGrey.png" ) ),
    BitmCubeGreen ( QString::fromUtf8 ( ":/png/LEDs/res/CLEDGreenBig.png" ) ),
    BitmCubeYellow ( QString::fromUtf8 ( ":/png/LEDs/res/IndicatorYellowFancy.png" ) ),
    BitmCubeRed ( QString::fromUtf8 ( ":/png/LEDs/res/IndicatorRedFancy.png" ) ),
    BitmIndicatorGreen ( QString::fromUtf8 ( ":/png/LEDs/res/IndicatorGreen.png" ) ),
    BitmIndicatorYellow ( QString::fromUtf8 ( ":/png/LEDs/res/IndicatorYellow.png" ) ),
    BitmIndicatorRed ( QString::fromUtf8 ( ":/png/LEDs/res/IndicatorRed.png" ) )
{
    // set init bitmap
    setPixmap ( BitmCubeGrey );
    eColorFlag = RL_GREY;

    // set default type and reset
    eType = MT_LED;
    Reset();
}

void CMultiColorLED::SetType ( const EType eNType )
{
    eType = eNType;
    Reset();
}

void CMultiColorLED::changeEvent ( QEvent* curEvent )
{
    // act on enabled changed state
    if ( curEvent->type() == QEvent::EnabledChange )
    {
        if ( isEnabled() )
        {
            setPixmap ( BitmCubeGrey );
            eColorFlag = RL_GREY;
        }
        else
        {
            setPixmap ( BitmCubeDisabled );
            eColorFlag = RL_DISABLED;
        }
    }
}

void CMultiColorLED::SetColor ( const ELightColor eNewColorFlag )
{
    setPixmap ( QPixmap() ); // jamony: 清pixmap, 改用stylesheet(jamony品牌色+圆+无外框)
    // jamony 08-24: 规则加 .CMultiColorLED 类选择器 —— 无选择器的 stylesheet 会级联到
    // 子部件, QToolTip 按悬停目标的样式表渲染, 导致灯上悬停弹窗背景跟随灯色跳动
    switch ( eNewColorFlag )
    {
    case RL_RED:
        if ( eColorFlag != RL_RED )
        {
            setStyleSheet ( ".CMultiColorLED { background-color: #FF3366; border-radius: 5px; border: none; }" );
            setAccessibleDescription ( tr ( "Red" ) );
            eColorFlag = RL_RED;
        }
        break;

    case RL_YELLOW:
        if ( eColorFlag != RL_YELLOW )
        {
            setStyleSheet ( ".CMultiColorLED { background-color: #FFCC00; border-radius: 5px; border: none; }" );
            setAccessibleDescription ( tr ( "Yellow" ) );
            eColorFlag = RL_YELLOW;
        }
        break;

    case RL_GREEN:
        if ( eColorFlag != RL_GREEN )
        {
            setStyleSheet ( ".CMultiColorLED { background-color: #BBEE00; border-radius: 5px; border: none; }" );
            setAccessibleDescription ( tr ( "Green" ) );
            eColorFlag = RL_GREEN;
        }
        break;

    default:
        if ( eColorFlag != RL_GREY )
        {
            setStyleSheet ( ".CMultiColorLED { background-color: #333333; border-radius: 5px; border: none; }" );
            eColorFlag = RL_GREY;
        }
        break;
    }
}

void CMultiColorLED::Reset()
{
    if ( isEnabled() )
    {
        // set color flag to disable to make sure the pixmap gets updated
        eColorFlag = RL_DISABLED;
        SetColor ( RL_GREY );
    }
}

void CMultiColorLED::SetLight ( const ELightColor eNewStatus )
{
    if ( isEnabled() )
    {
        SetColor ( eNewStatus );
    }
}
