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
#endif

CHudGladderSummary g_HudGladderSummary;

int CHudGladderSummary::Init( void )
{
#ifdef CLIENT_DLL
	gEngfuncs.pfnHookUserMsg( "GladSumm", MsgFunc_GladSumm_Dispatcher );
	gEngfuncs.pfnAddCommand( "gladder_summary_toggle", Cmd_ToggleSummary );
	gEngfuncs.pfnAddCommand( "gladder_leaderboard_toggle", Cmd_ToggleLeaderboard );
#endif
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudGladderSummary::VidInit( void )
{
	Reset();
	return 1;
}

void CHudGladderSummary::SetSummaryData( const GladderSummaryMsg &data, float flTime )
{
	m_summaryData      = data;
	m_flActivationTime = flTime;
	m_bActive          = true;
	m_bShowLeaderboard = false;

	// Populate leaderboard entry and load high scores
	GladderLeaderboardEntry entry;
	entry.score     = data.iFinalScore;
	entry.waves     = data.iWavesCompleted;
	entry.frags     = data.iTotalFrags;
	entry.totalTime = data.flTotalMatchTime;
	entry.grade     = data.cRankGrade;
	entry.survived  = data.bSurvived;

	std::string mapname = "default";
#ifdef CLIENT_DLL
	// Query current level name from engine if available
#endif
	GladderLeaderboard::Load( mapname, m_leaderboard );
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

	// Dark semi-transparent modal backdrop over viewport (preserves 3D map visibility behind)
	FillRGBA( 0, 0, ScreenWidth, ScreenHeight, 10, 12, 16, 210 );

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
	snprintf( szBuf, sizeof( szBuf ), "[TAB] View High Scores   [USE / ENTER] Dismiss" );
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
	snprintf( szBuf, sizeof( szBuf ), "[TAB] Return to Summary   [USE / ENTER] Dismiss" );
	len = static_cast<int>( strlen( szBuf ) ) * 9;
	DrawConsoleString( cx - ( len / 2 ), footerY, szBuf );
#endif
}

REGISTER_HUD_ELEMENT_NAMED( g_HudGladderSummary, 3000, "GladderSummary" );
