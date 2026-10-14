/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Lambda Collectible Center-Screen Notification (SPEC §7.3, GitHub Issue #92)
 *
 ****/

#include "hud_gladder_lambda.h"
#include "gladder_cl_usermsg.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"

static int MsgFunc_GladLambda_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderLambda.MsgFunc_GladderLambda( pszName, iSize, pbuf );
}
#endif

CHudGladderLambda g_HudGladderLambda;

int CHudGladderLambda::Init( void )
{
#ifdef CLIENT_DLL
	gEngfuncs.pfnHookUserMsg( "GladLambda", MsgFunc_GladLambda_Dispatcher );
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderLambda::VidInit( void )
{
	Reset();
	return 1;
}

void CHudGladderLambda::TriggerNotification( int iScoreBonus, int iCollectibleCount, float flTime )
{
	m_iScoreBonus       = iScoreBonus;
	m_iCollectibleCount = iCollectibleCount;
	m_flStartTime       = flTime;

	if ( m_iCollectibleCount > 0 )
	{
		snprintf( m_szText, sizeof( m_szText ), "LAMBDA COLLECTED +%d (TOTAL: %d)", m_iScoreBonus, m_iCollectibleCount );
	}
	else
	{
		snprintf( m_szText, sizeof( m_szText ), "LAMBDA COLLECTED +%d", m_iScoreBonus );
	}
}

int CHudGladderLambda::MsgFunc_GladderLambda( const char *pszName, int iSize, void *pbuf )
{
	(void)pszName;
	GladderLambdaMsg lmsg;
	if ( Gladder_ParseLambdaMsg( pbuf, iSize, lmsg ) )
	{
		float curTime = 0.0f;
#ifdef CLIENT_DLL
		curTime = gHUD.m_flTime;
#endif
		TriggerNotification( lmsg.iScoreBonus, lmsg.iCollectibleCount, curTime );
	}

	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderLambda::Draw( float flTime )
{
#ifdef CLIENT_DLL
	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	float flElapsed = flTime - m_flStartTime;
	if ( flElapsed < 0.0f || flElapsed >= DISPLAY_TOTAL_TIME )
	{
		return 1; // Hidden outside display window
	}

	// Alpha fade calculation (1.2s solid hold, 0.5s fade-out, ~1.7s total)
	float flAlpha = 1.0f;
	if ( flElapsed > DISPLAY_HOLD_TIME )
	{
		flAlpha = 1.0f - ( ( flElapsed - DISPLAY_HOLD_TIME ) / DISPLAY_FADE_TIME );
		flAlpha = (std::max)( 0.0f, (std::min)( 1.0f, flAlpha ) );
	}

	// Centered horizontally, middle-lower vertical axis (ScreenHeight * 5 / 9)
	// Non-overlapping with wave transition notification at ScreenHeight / 3
	int cx = ScreenWidth / 2;
	int cy = ( ScreenHeight * 5 ) / 9;

	// Vibrant golden-orange Lambda presentation (SPEC §7.3)
	float tr = 1.0f;
	float tg = 0.65f;
	float tb = 0.1f;

	gEngfuncs.pfnDrawSetTextColor( tr * flAlpha, tg * flAlpha, tb * flAlpha );

	int textWidth = static_cast<int>( strlen( m_szText ) ) * 9;
	int drawX     = cx - ( textWidth / 2 );
	DrawConsoleString( drawX, cy, m_szText );
#endif
	return 1;
}

REGISTER_HUD_ELEMENT_NAMED( g_HudGladderLambda, 2750, "GladderLambda" );
