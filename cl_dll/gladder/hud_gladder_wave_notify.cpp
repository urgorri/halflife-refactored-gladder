/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Wave Transition Centered Overlay Implementation (SPEC §12, GitHub Issue #25)
 *
 ****/

#include "hud_gladder_wave_notify.h"
#include "gladder_cl_usermsg.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

static int MsgFunc_GladNotify_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderWaveNotify.MsgFunc_GladderWaveNotify( pszName, iSize, pbuf );
}
#endif

CHudGladderWaveNotify g_HudGladderWaveNotify;

int CHudGladderWaveNotify::Init( void )
{
#ifdef CLIENT_DLL
	gEngfuncs.pfnHookUserMsg( "GladNotify", MsgFunc_GladNotify_Dispatcher );
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderWaveNotify::VidInit( void )
{
	Reset();
	return 1;
}

void CHudGladderWaveNotify::TriggerNotification( int iType, int iWaveNumber, int iMutatorType, float flTime, int iHostileQuota )
{
	m_iType         = iType;
	m_iWaveNumber   = iWaveNumber;
	m_iMutatorType  = iMutatorType;
	m_iHostileQuota = iHostileQuota;
	m_flStartTime   = flTime;

	m_szHostileLine[0] = '\0';

	if ( m_iType == 0 ) // WAVE STARTED
	{
		snprintf( m_szTitle, sizeof( m_szTitle ), "WAVE %d STARTED", m_iWaveNumber );

		switch ( m_iMutatorType )
		{
			case 1:
				snprintf( m_szSubtitle, sizeof( m_szSubtitle ), "BLACKOUT MUTATOR ACTIVE" );
				break;
			case 2:
				snprintf( m_szSubtitle, sizeof( m_szSubtitle ), "LOW GRAVITY MUTATOR ACTIVE" );
				break;
			case 3:
				snprintf( m_szSubtitle, sizeof( m_szSubtitle ), "SWARM MUTATOR ACTIVE" );
				break;
			case 4:
				snprintf( m_szSubtitle, sizeof( m_szSubtitle ), "ELITE SURGE MUTATOR ACTIVE" );
				break;
			case 5:
				snprintf( m_szSubtitle, sizeof( m_szSubtitle ), "BIG MOMMA MUTATOR ACTIVE" );
				break;
			default:
				m_szSubtitle[0] = '\0';
				break;
		}

		// Hostile quota to eliminate this wave (SPEC §12.1); hidden when not applicable
		if ( m_iHostileQuota > 0 )
		{
			if ( m_iHostileQuota == 1 )
				snprintf( m_szHostileLine, sizeof( m_szHostileLine ), "ELIMINATE 1 HOSTILE" );
			else
				snprintf( m_szHostileLine, sizeof( m_szHostileLine ), "ELIMINATE %d HOSTILES", m_iHostileQuota );
		}
	}
	else // WAVE COMPLETED
	{
		snprintf( m_szTitle, sizeof( m_szTitle ), "WAVE %d COMPLETED", m_iWaveNumber );
		m_szSubtitle[0] = '\0';
	}
}

int CHudGladderWaveNotify::MsgFunc_GladderWaveNotify( const char *pszName, int iSize, void *pbuf )
{
	GladderNotifyMsg nmsg;
	if ( Gladder_ParseNotifyMsg( pbuf, iSize, nmsg ) )
	{
		float curTime = 0.0f;
#ifdef CLIENT_DLL
		curTime = gHUD.m_flTime;
#endif
		TriggerNotification( nmsg.iType, nmsg.iWaveNumber, nmsg.iMutatorType, curTime, nmsg.iHostileQuota );
	}

	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderWaveNotify::Draw( float flTime )
{
#ifdef CLIENT_DLL
	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	float flElapsed = flTime - m_flStartTime;
	if ( flElapsed < 0.0f || flElapsed >= DISPLAY_TOTAL_TIME )
	{
		return 1; // Hidden outside 4.0s display duration
	}

	// Alpha fade calculation (3.0s solid hold, 1.0s fade-out)
	float flAlpha = 1.0f;
	if ( flElapsed > DISPLAY_HOLD_TIME )
	{
		flAlpha = 1.0f - ( ( flElapsed - DISPLAY_HOLD_TIME ) / DISPLAY_FADE_TIME );
		flAlpha = (std::max)( 0.0f, (std::min)( 1.0f, flAlpha ) );
	}

	// Centered in horizontal axis, upper-middle third (ScreenHeight / 3)
	int cx = ScreenWidth / 2;
	int cy = ScreenHeight / 3;

	// Title: classic HL green (0, 0.9, 0.2) or bright amber for completion
	float tr = ( m_iType == 1 ) ? 1.0f : 0.0f;
	float tg = ( m_iType == 1 ) ? 0.85f : 0.9f;
	float tb = ( m_iType == 1 ) ? 0.1f : 0.2f;

	gEngfuncs.pfnDrawSetTextColor( tr * flAlpha, tg * flAlpha, tb * flAlpha );

	int titleWidth = static_cast<int>( strlen( m_szTitle ) ) * 9;
	int drawX = cx - ( titleWidth / 2 );
	DrawConsoleString( drawX, cy, m_szTitle );

	// Hostile quota line, drawn in the title's colour family (SPEC §12.1)
	if ( m_szHostileLine[0] != '\0' )
	{
		int hostileWidth = static_cast<int>( strlen( m_szHostileLine ) ) * 9;
		int hostileX = cx - ( hostileWidth / 2 );
		DrawConsoleString( hostileX, cy + 18, m_szHostileLine );
	}

	// Subtitle for active mutator: warning amber/red
	if ( m_szSubtitle[0] != '\0' )
	{
		gEngfuncs.pfnDrawSetTextColor( 1.0f * flAlpha, 0.35f * flAlpha, 0.1f * flAlpha );
		int subWidth = static_cast<int>( strlen( m_szSubtitle ) ) * 9;
		int subX = cx - ( subWidth / 2 );
		DrawConsoleString( subX, cy + 36, m_szSubtitle );
	}
#endif
	return 1;
}

REGISTER_HUD_ELEMENT_NAMED( g_HudGladderWaveNotify, 2700, "GladderWaveNotify" );
