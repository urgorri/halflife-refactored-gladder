/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD Overlay Implementation
 *
 ****/

#include "hud_gladder_overlay.h"
#include <cstdio>
#include <cstring>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"

static int MsgFunc_GladWave_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderOverlay.MsgFunc_GladderWave( pszName, iSize, pbuf );
}

static int MsgFunc_GladTelem_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderOverlay.MsgFunc_GladderTelemetry( pszName, iSize, pbuf );
}

static void Cmd_ClearDecals( void )
{
	GladderPurgeCombatDecals();
}
#endif

CHudGladderOverlay g_HudGladderOverlay;

void GladderPurgeCombatDecals( void )
{
#ifdef CLIENT_DLL
	if ( !gEngfuncs.pEfxAPI || !gEngfuncs.pEfxAPI->R_DecalRemoveAll )
		return;

	// 1. Purge all standard combat decals by engine index (0..64)
	for ( int i = 0; i <= 64; ++i )
	{
		gEngfuncs.pEfxAPI->R_DecalRemoveAll( i );
	}

	// 2. Also resolve combat decals by name in decals.wad and remove all instances
	static const char *s_combatDecals[] = {
		"{shot1", "{shot2", "{shot3", "{shot4", "{shot5",
		"{scorch1", "{scorch2",
		"{blood1", "{blood2", "{blood3", "{blood4", "{blood5", "{blood6",
		"{yblood1", "{yblood2", "{yblood3", "{yblood4", "{yblood5", "{yblood6",
		"{break1", "{break2", "{break3",
		"{bigshot1", "{bigshot2", "{bigshot3", "{bigshot4", "{bigshot5",
		"{spit1", "{spit2",
		"{bproof1", "{gargstomp",
		"{smscorch1", "{smscorch2", "{smscorch3",
		"{mommablob",
		"{lambda01", "{lambda02", "{lambda03", "{lambda04", "{lambda05", "{lambda06"
	};

	if ( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName )
	{
		for ( const char *name : s_combatDecals )
		{
			int idx = gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( (char *)name );
			if ( idx >= 0 )
			{
				gEngfuncs.pEfxAPI->R_DecalRemoveAll( idx );
				if ( gEngfuncs.pEfxAPI->Draw_DecalIndex )
				{
					int mapped = gEngfuncs.pEfxAPI->Draw_DecalIndex( idx );
					if ( mapped >= 0 && mapped != idx )
					{
						gEngfuncs.pEfxAPI->R_DecalRemoveAll( mapped );
					}
				}
			}
		}
	}
#endif
}

int CHudGladderOverlay::Init( void )
{
#ifdef CLIENT_DLL
	gEngfuncs.pfnHookUserMsg( "GladWave", MsgFunc_GladWave_Dispatcher );
	gEngfuncs.pfnHookUserMsg( "GladTelem", MsgFunc_GladTelem_Dispatcher );
	gEngfuncs.pfnAddCommand( "r_cleardecals", Cmd_ClearDecals );
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderOverlay::VidInit( void )
{
	return 1;
}

int CHudGladderOverlay::MsgFunc_GladderWave( const char *pszName, int iSize, void *pbuf )
{
#ifdef CLIENT_DLL
	BEGIN_READ( pbuf, iSize );
	int iNewWaveState = READ_BYTE();
	m_iWaveNumber     = READ_SHORT();
	m_flTimeRemaining = READ_COORD();
	m_flLapTime       = READ_COORD();

	// Purge dynamic combat decals on wave completion transition (fade-to-black) (SPEC §11.3, Issue #49)
	// GoldSrc's R_DecalRemoveAll strips all dynamic combat decals while strictly preserving permanent infodecals
	if ( ( iNewWaveState == GLADDER_STATE_WAVE_COMPLETED || iNewWaveState == GLADDER_STATE_MATCH_OVER ) &&
	     m_iWaveState != iNewWaveState )
	{
		m_iDecalPurgeCount++;
		GladderPurgeCombatDecals();
	}

	m_iWaveState = iNewWaveState;
#else
	if ( pbuf && iSize >= 1 )
	{
		const unsigned char *pData = static_cast<const unsigned char *>( pbuf );
		int iNewWaveState = pData[0];
		if ( ( iNewWaveState == GLADDER_STATE_WAVE_COMPLETED || iNewWaveState == GLADDER_STATE_MATCH_OVER ) &&
		     m_iWaveState != iNewWaveState )
		{
			m_iDecalPurgeCount++;
		}
		m_iWaveState = iNewWaveState;
	}
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderOverlay::MsgFunc_GladderTelemetry( const char *pszName, int iSize, void *pbuf )
{
#ifdef CLIENT_DLL
	BEGIN_READ( pbuf, iSize );
	m_flFastestLap   = READ_COORD();
	m_flSlowestLap   = READ_COORD();
	m_flAverageLap   = READ_COORD();
	m_iCompletedLaps = READ_SHORT();
	m_iFrags         = READ_SHORT();
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderOverlay::Draw( float flTime )
{
#ifdef CLIENT_DLL
	// Do not draw if HUD is hidden
	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	// Greenish / Amber color styling
	gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.9f, 0.2f );

	int x = 16;
	int y = 16;
	char szBuf[128];

	// 1. Current Wave Counter & Status
	if ( m_iWaveState == 0 ) // GLADDER_STATE_WAITING_FOR_START
	{
		sprintf( szBuf, "WAVE: %d [STANDBY]", m_iWaveNumber );
	}
	else if ( m_iWaveState == 3 ) // GLADDER_STATE_MATCH_OVER
	{
		sprintf( szBuf, "WAVE: %d [MATCH OVER]", m_iWaveNumber );
	}
	else // GLADDER_STATE_WAVE_ACTIVE (1)
	{
		sprintf( szBuf, "WAVE: %d", m_iWaveNumber );
	}
	DrawConsoleString( x, y, szBuf );
	y += 16;

	// 2. Session Time Remaining
	int minutes = static_cast<int>( m_flTimeRemaining ) / 60;
	int seconds = static_cast<int>( m_flTimeRemaining ) % 60;
	if ( minutes < 0 )
		minutes = 0;
	if ( seconds < 0 )
		seconds = 0;

	sprintf( szBuf, "TIME LEFT: %02d:%02d", minutes, seconds );
	DrawConsoleString( x, y, szBuf );
	y += 16;

	// 3. Current Lap Timer
	if ( m_iWaveState == 1 ) // GLADDER_STATE_WAVE_ACTIVE
	{
		int lapMin = static_cast<int>( m_flLapTime ) / 60;
		int lapSec = static_cast<int>( m_flLapTime ) % 60;
		sprintf( szBuf, "WAVE TIME: %02d:%02d", lapMin, lapSec );
	}
	else
	{
		sprintf( szBuf, "WAVE TIME: --:--" );
	}
	DrawConsoleString( x, y, szBuf );
	y += 16;

	// 4. Frags
	sprintf( szBuf, "FRAGS: %d", m_iFrags );
	DrawConsoleString( x, y, szBuf );
	y += 16;

	// 5. Average Wave Time
	if ( m_iCompletedLaps > 0 )
	{
		int avgMin = static_cast<int>( m_flAverageLap ) / 60;
		int avgSec = static_cast<int>( m_flAverageLap ) % 60;
		sprintf( szBuf, "AVG WAVE: %02d:%02d", avgMin, avgSec );
		DrawConsoleString( x, y, szBuf );
	}
#endif
	return 1;
}

// Register HUD element in draw order (draw order 2500, late in the pass)
REGISTER_HUD_ELEMENT_NAMED( g_HudGladderOverlay, 2500, "GladderOverlay" );
