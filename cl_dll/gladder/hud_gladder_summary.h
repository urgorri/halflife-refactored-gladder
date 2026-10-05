/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD: Post-Match Summary Screen & Grade Presentation
 *	(SPEC §6, §9.2, GitHub Issue #16)
 *
 ****/

#pragma once

#ifndef HUD_GLADDER_SUMMARY_H
#define HUD_GLADDER_SUMMARY_H

#include "hud/hud_base.h"
#include "hud/hud_registry.h"
#include "gladder_cl_usermsg.h"
#include "gladder_leaderboard.h"
#include <vector>
#include <string>

class CHudGladderSummary : public CHudBase
{
  public:
	static constexpr float TALLY_DURATION = 1.5f; // Seconds for arcade tally roll-up

	CHudGladderSummary()
	    : m_bActive( false ),
	      m_bShowLeaderboard( false ),
	      m_flActivationTime( 0.0f ),
	      m_iQualifiedRank( -1 ),
	      m_bIsNewHighScore( false )
	{
		m_iFlags = HUD_ACTIVE;
	}

	int Init( void ) override;
	int VidInit( void ) override;
	int Draw( float flTime ) override;
	void Reset( void ) override
	{
		m_bActive          = false;
		m_bShowLeaderboard = false;
		m_flActivationTime = 0.0f;
		m_iQualifiedRank   = -1;
		m_bIsNewHighScore  = false;
		m_summaryData      = GladderSummaryMsg();
		m_leaderboard.clear();
	}

	// User message dispatcher
	int MsgFunc_GladderSummary( const char *pszName, int iSize, void *pbuf );

	// Interaction controls
	void ToggleDisplay( void ) { m_bActive = !m_bActive; }
	void ToggleLeaderboardTab( void ) { m_bShowLeaderboard = !m_bShowLeaderboard; }
	void Dismiss( void ) { m_bActive = false; }

	// Test/Simulation setters
	void SetSummaryData( const GladderSummaryMsg &data, float flTime = 0.0f );
	void SetLeaderboard( const std::vector<GladderLeaderboardEntry> &entries, int rank = -1, bool isNewHigh = false );

	// State queries
	bool IsActive( void ) const { return m_bActive; }
	bool IsShowingLeaderboard( void ) const { return m_bShowLeaderboard; }
	const GladderSummaryMsg &GetSummaryData( void ) const { return m_summaryData; }
	const std::vector<GladderLeaderboardEntry> &GetLeaderboard( void ) const { return m_leaderboard; }

	// Animated tally calculation
	int GetTallyScore( float flTime ) const;
	float GetTallyFraction( float flTime ) const;

  private:
	bool m_bActive;
	bool m_bShowLeaderboard;
	float m_flActivationTime;
	int m_iQualifiedRank;
	bool m_bIsNewHighScore;
	GladderSummaryMsg m_summaryData;
	std::vector<GladderLeaderboardEntry> m_leaderboard;

	void DrawSummaryPage( float flTime, float tallyFrac );
	void DrawLeaderboardPage( void );
};

extern CHudGladderSummary g_HudGladderSummary;

#endif // HUD_GLADDER_SUMMARY_H
