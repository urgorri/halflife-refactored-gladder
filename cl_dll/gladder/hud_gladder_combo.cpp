/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Dynamic Kill Streak Combo Gauge Implementation (SPEC §9.1, GitHub Issue #15)
 *
 ****/

#include "hud_gladder_combo.h"
#include "gladder_cl_usermsg.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

static int MsgFunc_GladCombo_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderCombo.MsgFunc_GladderCombo( pszName, iSize, pbuf );
}
#endif

CHudGladderCombo g_HudGladderCombo;

int CHudGladderCombo::Init( void )
{
#ifdef CLIENT_DLL
	gEngfuncs.pfnHookUserMsg( "GladCombo", MsgFunc_GladCombo_Dispatcher );
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderCombo::VidInit( void )
{
	Reset();
	return 1;
}

int CHudGladderCombo::MsgFunc_GladderCombo( const char *pszName, int iSize, void *pbuf )
{
	GladderComboMsg cmsg;
	if ( Gladder_ParseComboMsg( pbuf, iSize, cmsg ) )
	{
		float curTime = 0.0f;
#ifdef CLIENT_DLL
		curTime = gHUD.m_flTime;
#endif
		if ( cmsg.iComboMultiplier > m_iComboMultiplier ||
		     ( cmsg.iComboMultiplier > 1 && cmsg.flDecayFraction > m_flDecayFraction ) )
		{
			m_flLastKillTime = curTime;
		}

		m_iComboMultiplier = cmsg.iComboMultiplier;
		m_flDecayFraction  = cmsg.flDecayFraction;
	}

	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderCombo::Draw( float flTime )
{
#ifdef CLIENT_DLL
	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	// Only render when streak multiplier is active (> 1) and gauge has time remaining
	if ( m_iComboMultiplier <= 1 || m_flDecayFraction <= 0.0f )
	{
		return 1;
	}

	// Dynamic text color scaling by multiplier tier
	float r = 0.8f, g = 1.0f, b = 0.2f;
	if ( m_iComboMultiplier == 3 )
	{
		r = 1.0f; g = 0.75f; b = 0.1f; // Amber / Gold
	}
	else if ( m_iComboMultiplier >= 4 )
	{
		r = 1.0f; g = 0.25f; b = 0.1f; // Blazing Orange / Red
	}

	// Pulse animation for 0.2s following a fresh kill
	float flPulseAge = flTime - m_flLastKillTime;
	int yOffset = 0;
	if ( flPulseAge >= 0.0f && flPulseAge < 0.2f )
	{
		yOffset = -2;
	}

	// Render centered above crosshairs (slightly above center of screen)
	int cx = ScreenWidth / 2;
	int cy = ScreenHeight / 2 - 45 + yOffset;

	char szBuf[64];
	if ( m_iComboMultiplier >= 4 )
	{
		snprintf( szBuf, sizeof( szBuf ), "COMBO: x%d MAX!", m_iComboMultiplier );
	}
	else
	{
		snprintf( szBuf, sizeof( szBuf ), "COMBO: x%d", m_iComboMultiplier );
	}

	int textWidth = static_cast<int>( strlen( szBuf ) ) * 9;
	int drawX = cx - ( textWidth / 2 );

	gEngfuncs.pfnDrawSetTextColor( r, g, b );
	DrawConsoleString( drawX, cy, szBuf );

	// Animated meter bar indicating remaining window decay
	int barWidth = 84;
	int barHeight = 4;
	int barX = cx - ( barWidth / 2 );
	int barY = cy + 16;

	// Background track (dark transparent)
	FillRGBA( barX - 1, barY - 1, barWidth + 2, barHeight + 2, 0, 0, 0, 160 );

	// Foreground fill proportional to normalized gauge
	int fillWidth = static_cast<int>( barWidth * (std::max)( 0.0f, (std::min)( 1.0f, m_flDecayFraction ) ) );
	if ( fillWidth > 0 )
	{
		int ir = static_cast<int>( r * 255.0f );
		int ig = static_cast<int>( g * 255.0f );
		int ib = static_cast<int>( b * 255.0f );
		FillRGBA( barX, barY, fillWidth, barHeight, ir, ig, ib, 220 );
	}
#endif
	return 1;
}

REGISTER_HUD_ELEMENT_NAMED( g_HudGladderCombo, 2600, "GladderCombo" );
