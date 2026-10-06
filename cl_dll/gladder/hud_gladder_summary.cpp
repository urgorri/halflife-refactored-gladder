/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Post-Match Summary Screen Implementation
 *	(SPEC §6, §9.2, GitHub Issue #16)
 *
 ****/

#include "hud_gladder_summary.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "in_buttons.h"
#include "kbutton.h"
#include "screenfade.h"
#include "shake.h"

extern kbutton_t in_score;

void COM_FileBase( const char *in, char *out );

static int MsgFunc_GladSumm_Dispatcher( const char *pszName, int iSize, void *pbuf )
{
	return g_HudGladderSummary.MsgFunc_GladderSummary( pszName, iSize, pbuf );
}

static void Cmd_ToggleSummary( void )
{
	g_HudGladderSummary.ToggleDisplay();
}

static void Cmd_ToggleLeaderboard( void )
{
	g_HudGladderSummary.ToggleLeaderboardTab();
}
#elif defined( HL_TESTS )
#include "cl_dll/input/kbutton.h"
extern kbutton_t in_score;
#endif

CHudGladderSummary g_HudGladderSummary;

int CHudGladderSummary::Init( void )
{
#ifdef CLIENT_DLL
	gEngfuncs.pfnHookUserMsg( "GladSumm", MsgFunc_GladSumm_Dispatcher );
	gEngfuncs.pfnAddCommand( "gladder_summary_toggle", Cmd_ToggleSummary );
	gEngfuncs.pfnAddCommand( "gladder_leaderboard_toggle", Cmd_ToggleLeaderboard );
	gEngfuncs.pfnAddCommand( "gladder_scores", Cmd_ToggleLeaderboard );
#endif
	m_iFlags |= ( HUD_ACTIVE | HUD_INTERMISSION );
	return 1;
}

int CHudGladderSummary::VidInit( void )
{
	Reset();
	return 1;
}

std::string CHudGladderSummary::GetCurrentMapName( void )
{
	std::string mapname = "default";
#ifdef CLIENT_DLL
	const char *level = gEngfuncs.pfnGetLevelName();
	if ( level && level[0] )
	{
		char baseName[64];
		COM_FileBase( level, baseName );
		baseName[sizeof( baseName ) - 1] = '\0';
		if ( baseName[0] )
		{
			mapname = baseName;
		}
	}
#endif
	return mapname;
}

void CHudGladderSummary::ReloadLeaderboard( void )
{
	std::string mapname = GetCurrentMapName();
	GladderLeaderboard::Load( mapname, m_leaderboard );
}

void CHudGladderSummary::ToggleDisplay( void )
{
	m_bActive = !m_bActive;
	if ( m_bActive )
	{
		m_bShowLeaderboard = false;
		m_bHoldTabMode     = false;
	}
	else
	{
		m_bShowLeaderboard = false;
		m_bHoldTabMode     = false;
	}
	UpdateScreenFade();
}

void CHudGladderSummary::ToggleLeaderboardTab( void )
{
	if ( !m_bActive )
	{
		m_bActive          = true;
		m_bShowLeaderboard = true;
		m_bHoldTabMode     = false;
		ReloadLeaderboard();
		UpdateScreenFade();
		return;
	}

	m_bShowLeaderboard = !m_bShowLeaderboard;
	if ( m_bShowLeaderboard )
	{
		ReloadLeaderboard();
	}
}

void CHudGladderSummary::ShowLeaderboardOnly( void )
{
	m_bActive          = true;
	m_bShowLeaderboard = true;
	m_bHoldTabMode     = true;
	ReloadLeaderboard();
	UpdateScreenFade();
}

void CHudGladderSummary::Dismiss( void )
{
	m_bActive          = false;
	m_bShowLeaderboard = false;
	m_bHoldTabMode     = false;
	UpdateScreenFade();
}

void CHudGladderSummary::UpdateScreenFade( void )
{
#ifdef CLIENT_DLL
	if ( !gEngfuncs.pfnSetScreenFade )
		return;

	screenfade_t sf;
	memset( &sf, 0, sizeof( sf ) );

	if ( m_bActive )
	{
		// Authentically dim/darken the 3D viewport behind the summary/leaderboard (SPEC §9.2, §10.2)
		sf.fader     = 0;
		sf.fadeg     = 0;
		sf.fadeb     = 0;
		sf.fadealpha = 205; // ~80% opacity dark modal overlay
		sf.fadeFlags = FFADE_STAYOUT;
		sf.fadeReset = 99999.0f;
	}
	else
	{
		// Restore normal brightness
		sf.fadeFlags = FFADE_IN;
		sf.fadealpha = 0;
	}

	gEngfuncs.pfnSetScreenFade( &sf );
#endif
}

void CHudGladderSummary::Think( void )
{
#if defined( CLIENT_DLL ) || defined( HL_TESTS )
	// Query physical button states directly (in_score) instead of gHUD.m_iKeyBits.
	// In the engine, CL_ButtonBits forcibly sets IN_SCORE on every frame whenever the player
	// is dead (CL_IsDead()) or during intermission (gHUD.m_iIntermission).
	// Querying in_score.state ensures we react only to the actual physical TAB key presses,
	// allowing clean toggling between summary and leaderboard pages during post-match flow.
	bool isScoreDown = ( in_score.state & 1 ) != 0;

	// Check Tab key transition
	if ( isScoreDown && !m_bOldScoreDown )
	{
		if ( !m_bActive )
		{
			// In gameplay: holding TAB displays the leaderboard overlay
			ShowLeaderboardOnly();
		}
		else
		{
			// Post-match summary active: TAB toggles between summary and leaderboard pages
			m_bShowLeaderboard = !m_bShowLeaderboard;
			if ( m_bShowLeaderboard )
			{
				ReloadLeaderboard();
			}
		}
	}
	else if ( !isScoreDown && m_bOldScoreDown )
	{
		if ( m_bActive && m_bHoldTabMode )
		{
			// If we opened the leaderboard by holding TAB in-game, release hides it
			Dismiss();
		}
	}

	m_bOldScoreDown = isScoreDown;

	if ( m_bActive )
	{
		UpdateScreenFade();
	}
#endif
}

void CHudGladderSummary::SetSummaryData( const GladderSummaryMsg &data, float flTime )
{
	m_summaryData      = data;
	m_flActivationTime = flTime;
	m_bActive          = true;
	m_bShowLeaderboard = false;
	m_bHoldTabMode     = false;

	std::string mapname = GetCurrentMapName();
	GladderLeaderboard::Load( mapname, m_leaderboard );

	m_iQualifiedRank  = -1;
	m_bIsNewHighScore = false;
	for ( size_t i = 0; i < m_leaderboard.size(); ++i )
	{
		if ( m_leaderboard[i].score == data.iFinalScore &&
		     m_leaderboard[i].waves == data.iWavesCompleted &&
		     m_leaderboard[i].frags == data.iTotalFrags )
		{
			m_iQualifiedRank = static_cast<int>( i + 1 );
			if ( i == 0 )
			{
				m_bIsNewHighScore = true;
			}
			break;
		}
	}

	UpdateScreenFade();
}

void CHudGladderSummary::SetLeaderboard( const std::vector<GladderLeaderboardEntry> &entries, int rank, bool isNewHigh )
{
	m_leaderboard     = entries;
	m_iQualifiedRank  = rank;
	m_bIsNewHighScore = isNewHigh;
}

float CHudGladderSummary::GetTallyFraction( float flTime ) const
{
	float elapsed = flTime - m_flActivationTime;
	if ( elapsed <= 0.0f )
		return 0.0f;
	if ( elapsed >= TALLY_DURATION )
		return 1.0f;
	return elapsed / TALLY_DURATION;
}

int CHudGladderSummary::GetTallyScore( float flTime ) const
{
	return static_cast<int>( m_summaryData.iFinalScore * GetTallyFraction( flTime ) );
}

int CHudGladderSummary::MsgFunc_GladderSummary( const char *pszName, int iSize, void *pbuf )
{
	GladderSummaryMsg msg;
	if ( Gladder_ParseSummaryMsg( pbuf, iSize, msg ) )
	{
		float curTime = 0.0f;
#ifdef CLIENT_DLL
		curTime = gHUD.m_flTime;
#endif
		SetSummaryData( msg, curTime );
	}

	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderSummary::Draw( float flTime )
{
#ifdef CLIENT_DLL
	if ( !m_bActive )
		return 1;

	// Dark semi-transparent modal backdrop over viewport (preserves 3D map visibility behind).
	// In GoldSrc, screen dimming is handled natively and statically by engine ScreenFade
	// (applied in UpdateScreenFade via gEngfuncs.pfnSetScreenFade with FFADE_STAYOUT).
	// If pfnSetScreenFade is unavailable, FillRGBA serves as a safe fallback.
	if ( !gEngfuncs.pfnSetScreenFade )
	{
		FillRGBA( 0, 0, ScreenWidth, ScreenHeight, 0, 0, 0, 210 );
	}

	float tallyFrac = GetTallyFraction( flTime );

	if ( m_bShowLeaderboard )
	{
		DrawLeaderboardPage();
	}
	else
	{
		DrawSummaryPage( flTime, tallyFrac );
	}
#endif
	return 1;
}

void CHudGladderSummary::DrawSummaryPage( float flTime, float tallyFrac )
{
#ifdef CLIENT_DLL
	int cx = ScreenWidth / 2;
	int y  = 40;
	char szBuf[128];

	// 1. Outcome Status Header
	if ( m_summaryData.bSurvived )
	{
		gEngfuncs.pfnDrawSetTextColor( 0.2f, 1.0f, 0.3f ); // Bright Green
		snprintf( szBuf, sizeof( szBuf ), "=== STATUS: SURVIVED - TIME EXPIRED ===" );
	}
	else
	{
		gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.3f, 0.2f ); // Bright Red
		snprintf( szBuf, sizeof( szBuf ), "=== STATUS: KIA - FALLEN IN COMBAT ===" );
	}
	int len = static_cast<int>( strlen( szBuf ) ) * 9;
	DrawConsoleString( cx - ( len / 2 ), y, szBuf );
	y += 24;

	// 2. Large Performance Grade Stamp
	gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.85f, 0.1f ); // Gold
	snprintf( szBuf, sizeof( szBuf ), "[ PERFORMANCE RANK GRADE: %c ]", m_summaryData.cRankGrade );
	len = static_cast<int>( strlen( szBuf ) ) * 9;
	DrawConsoleString( cx - ( len / 2 ), y, szBuf );
	y += 24;

	// 3. Prominent Final Composite Score (Rolling Tally)
	int displayedScore = static_cast<int>( m_summaryData.iFinalScore * tallyFrac );
	gEngfuncs.pfnDrawSetTextColor( 1.0f, 1.0f, 0.4f );
	snprintf( szBuf, sizeof( szBuf ), "FINAL COMPOSITE SCORE: %d", displayedScore );
	len = static_cast<int>( strlen( szBuf ) ) * 9;
	DrawConsoleString( cx - ( len / 2 ), y, szBuf );
	y += 36;

	// Multi-column presentation
	int colWidth = ScreenWidth / 3;
	int col1X = 32;
	int col2X = col1X + colWidth;
	int col3X = col2X + colWidth;

	// Column 1: Granular Score Breakdown
	int colY = y;
	gEngfuncs.pfnDrawSetTextColor( 0.4f, 0.9f, 1.0f ); // Cyan Header
	DrawConsoleString( col1X, colY, "-- SCORE BREAKDOWN --" );
	colY += 18;

	gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.9f, 0.2f );
	snprintf( szBuf, sizeof( szBuf ), "Base Combat: +%d", static_cast<int>( m_summaryData.iBaseCombatPoints * tallyFrac ) );
	DrawConsoleString( col1X, colY, szBuf ); colY += 16;

	snprintf( szBuf, sizeof( szBuf ), "Combos (x%d peak): +%d", m_summaryData.iMaxComboStreak, static_cast<int>( m_summaryData.iComboBonusPoints * tallyFrac ) );
	DrawConsoleString( col1X, colY, szBuf ); colY += 16;

	snprintf( szBuf, sizeof( szBuf ), "Waves (%d cleared): +%d", m_summaryData.iWavesCompleted, static_cast<int>( m_summaryData.iWaveBonusPoints * tallyFrac ) );
	DrawConsoleString( col1X, colY, szBuf ); colY += 16;

	snprintf( szBuf, sizeof( szBuf ), "Lambdas (%d found): +%d", m_summaryData.iCollectibles, static_cast<int>( m_summaryData.iLambdaBonusPoints * tallyFrac ) );
	DrawConsoleString( col1X, colY, szBuf ); colY += 16;

	if ( m_summaryData.iSurvivalBonusPoints > 0 )
	{
		snprintf( szBuf, sizeof( szBuf ), "Survival Milestone: +%d", static_cast<int>( m_summaryData.iSurvivalBonusPoints * tallyFrac ) );
		DrawConsoleString( col1X, colY, szBuf ); colY += 16;
	}

	if ( m_summaryData.iDamagePenalty > 0 )
	{
		gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.4f, 0.3f );
		snprintf( szBuf, sizeof( szBuf ), "Damage Penalty: -%d", static_cast<int>( m_summaryData.iDamagePenalty * tallyFrac ) );
		DrawConsoleString( col1X, colY, szBuf ); colY += 16;
	}

	// Column 2: Pacing & Lap Telemetry
	colY = y;
	gEngfuncs.pfnDrawSetTextColor( 0.4f, 0.9f, 1.0f );
	DrawConsoleString( col2X, colY, "-- PACING TELEMETRY --" );
	colY += 18;

	gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.9f, 0.2f );
	int totM = static_cast<int>( m_summaryData.flTotalMatchTime ) / 60;
	int totS = static_cast<int>( m_summaryData.flTotalMatchTime ) % 60;
	snprintf( szBuf, sizeof( szBuf ), "Total Time: %02d:%02d", totM, totS );
	DrawConsoleString( col2X, colY, szBuf ); colY += 16;

	int avgM = static_cast<int>( m_summaryData.flAverageLapTime ) / 60;
	int avgS = static_cast<int>( m_summaryData.flAverageLapTime ) % 60;
	snprintf( szBuf, sizeof( szBuf ), "Average Lap: %02d:%02d", avgM, avgS );
	DrawConsoleString( col2X, colY, szBuf ); colY += 16;

	int fstM = static_cast<int>( m_summaryData.flFastestLapTime ) / 60;
	int fstS = static_cast<int>( m_summaryData.flFastestLapTime ) % 60;
	snprintf( szBuf, sizeof( szBuf ), "Fastest Lap: %02d:%02d", fstM, fstS );
	DrawConsoleString( col2X, colY, szBuf ); colY += 16;

	int slwM = static_cast<int>( m_summaryData.flSlowestLapTime ) / 60;
	int slwS = static_cast<int>( m_summaryData.flSlowestLapTime ) % 60;
	snprintf( szBuf, sizeof( szBuf ), "Slowest Lap: %02d:%02d", slwM, slwS );
	DrawConsoleString( col2X, colY, szBuf ); colY += 16;

	snprintf( szBuf, sizeof( szBuf ), "Damage Taken: %.0f HP", m_summaryData.flTotalDamage );
	DrawConsoleString( col2X, colY, szBuf ); colY += 16;

	snprintf( szBuf, sizeof( szBuf ), "Total Monsters Slain: %d", m_summaryData.iTotalFrags );
	DrawConsoleString( col2X, colY, szBuf ); colY += 16;

	// Column 3: Itemized Species Kill Log
	colY = y;
	gEngfuncs.pfnDrawSetTextColor( 0.4f, 0.9f, 1.0f );
	DrawConsoleString( col3X, colY, "-- SPECIES KILL LOG --" );
	colY += 18;

	gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.9f, 0.2f );
	if ( m_summaryData.speciesLog.empty() )
	{
		DrawConsoleString( col3X, colY, "None encountered" );
	}
	else
	{
		int maxItems = (std::min)( 10, static_cast<int>( m_summaryData.speciesLog.size() ) );
		for ( int i = 0; i < maxItems; ++i )
		{
			snprintf( szBuf, sizeof( szBuf ), "%s: %d", m_summaryData.speciesLog[i].speciesName.c_str(), m_summaryData.speciesLog[i].kills );
			DrawConsoleString( col3X, colY, szBuf );
			colY += 16;
		}
	}

	// Footer Prompt & High Score Notification
	int footerY = ScreenHeight - 48;
	if ( m_bIsNewHighScore )
	{
		gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.9f, 0.1f );
		snprintf( szBuf, sizeof( szBuf ), "*** NEW HIGH SCORE! TOP RANK ACHIEVED! ***" );
		len = static_cast<int>( strlen( szBuf ) ) * 9;
		DrawConsoleString( cx - ( len / 2 ), footerY - 20, szBuf );
	}

	gEngfuncs.pfnDrawSetTextColor( 0.7f, 0.7f, 0.7f );
	snprintf( szBuf, sizeof( szBuf ), "[TAB] View High Scores" );
	len = static_cast<int>( strlen( szBuf ) ) * 9;
	DrawConsoleString( cx - ( len / 2 ), footerY, szBuf );
#endif
}

void CHudGladderSummary::DrawLeaderboardPage( void )
{
#ifdef CLIENT_DLL
	int cx = ScreenWidth / 2;
	int y  = 40;
	char szBuf[128];

	gEngfuncs.pfnDrawSetTextColor( 1.0f, 0.85f, 0.1f );
	snprintf( szBuf, sizeof( szBuf ), "=== LOCAL MAP LEADERBOARDS (TOP 10) ===" );
	int len = static_cast<int>( strlen( szBuf ) ) * 9;
	DrawConsoleString( cx - ( len / 2 ), y, szBuf );
	y += 32;

	// Table Header
	int tableX = cx - 220;
	gEngfuncs.pfnDrawSetTextColor( 0.4f, 0.9f, 1.0f );
	snprintf( szBuf, sizeof( szBuf ), "%-4s %-10s %-7s %-7s %-8s %-5s %-8s", "RANK", "SCORE", "WAVES", "FRAGS", "TIME", "GRADE", "STATUS" );
	DrawConsoleString( tableX, y, szBuf );
	y += 20;

	gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.9f, 0.2f );
	if ( m_leaderboard.empty() )
	{
		DrawConsoleString( tableX, y, "No local scores recorded yet." );
	}
	else
	{
		for ( size_t i = 0; i < m_leaderboard.size(); ++i )
		{
			const auto &e = m_leaderboard[i];
			int m = static_cast<int>( e.totalTime ) / 60;
			int s = static_cast<int>( e.totalTime ) % 60;
			char szTime[16];
			snprintf( szTime, sizeof( szTime ), "%02d:%02d", m, s );

			snprintf( szBuf, sizeof( szBuf ), "#%-3d %-10d %-7d %-7d %-8s %-5c %-8s",
			          static_cast<int>( i + 1 ),
			          e.score,
			          e.waves,
			          e.frags,
			          szTime,
			          e.grade,
			          e.survived ? "SURVIVED" : "KIA" );

			if ( static_cast<int>( i + 1 ) == m_iQualifiedRank )
			{
				gEngfuncs.pfnDrawSetTextColor( 1.0f, 1.0f, 0.2f ); // Highlight current run
			}
			else
			{
				gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.9f, 0.2f );
			}

			DrawConsoleString( tableX, y, szBuf );
			y += 18;
		}
	}

	int footerY = ScreenHeight - 48;
	gEngfuncs.pfnDrawSetTextColor( 0.7f, 0.7f, 0.7f );
	if ( m_bHoldTabMode )
	{
		snprintf( szBuf, sizeof( szBuf ), "[TAB] Release to Dismiss Leaderboard" );
	}
	else
	{
		snprintf( szBuf, sizeof( szBuf ), "[TAB] Return to Summary" );
	}
	len = static_cast<int>( strlen( szBuf ) ) * 9;
	DrawConsoleString( cx - ( len / 2 ), footerY, szBuf );
#endif
}

REGISTER_HUD_ELEMENT_NAMED( g_HudGladderSummary, 3000, "GladderSummary" );
