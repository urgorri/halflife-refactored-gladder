/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Kill Streak Combo Tracker & Itemized Species Kill Log
 *	(SPEC §3, §9.1, §9.2, GitHub Issue #8)
 *
 ****/

#pragma once

#ifndef GLADDER_COMBO_TRACKER_H
#define GLADDER_COMBO_TRACKER_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/basemonster.h"
#include <string>
#include <unordered_map>
#include <vector>

// Itemized kill stats and combo milestones achieved during a match
struct GladderKillStats
{
	std::unordered_map<std::string, int> speciesKills;
	int totalFrags        = 0;
	int headshotKills     = 0;
	int x2Combos          = 0;
	int x3Combos          = 0;
	int x4PlusCombos      = 0;
	int maxComboStreak    = 0;

	void Reset()
	{
		speciesKills.clear();
		totalFrags     = 0;
		headshotKills  = 0;
		x2Combos       = 0;
		x3Combos       = 0;
		x4PlusCombos   = 0;
		maxComboStreak = 0;
	}
};

class GladderComboTracker
{
  public:
	static constexpr float DEFAULT_COMBO_WINDOW   = 3.5f; // Seconds before combo decays
	static constexpr float HEADSHOT_WINDOW_BONUS = 1.0f; // Bonus seconds added on headshot

	GladderComboTracker();
	~GladderComboTracker();

	// Initialize or reset tracking for a new match
	void Reset( void );

	// Frame update: ticks decay timer and computes normalized gauge
	void Update( float flCurrentTime );

	// Invoked when any monster is slain
	void OnMonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor, float flCurrentTime );

	// Active streak & gauge queries (for HUD overlays and networking)
	int GetCurrentStreak( void ) const { return m_iCurrentStreak; }
	int GetCurrentMultiplier( void ) const;
	float GetNormalizedGauge( void ) const { return m_flNormalizedGauge; }
	float GetRemainingWindow( void ) const;
	bool IsComboActive( void ) const { return m_iCurrentStreak > 1 && m_flNormalizedGauge > 0.0f; }

	// Match aggregated telemetry
	const GladderKillStats &GetStats( void ) const { return m_stats; }
	int GetTotalFrags( void ) const { return m_stats.totalFrags; }
	int GetMaxStreak( void ) const { return m_stats.maxComboStreak; }
	int GetSpeciesKills( const std::string &species ) const;

	// Friendly species display name translation (e.g. monster_bullchicken -> "Bullsquids")
	static std::string GetFriendlySpeciesName( const std::string &classname );

	// Configuration
	void SetComboWindow( float flSeconds ) { m_flComboWindowDuration = ( flSeconds > 0.5f ) ? flSeconds : 3.5f; }
	float GetComboWindow( void ) const { return m_flComboWindowDuration; }

  private:
	GladderKillStats m_stats;

	int m_iCurrentStreak;
	float m_flComboWindowDuration;
	float m_flComboExpireTime;
	float m_flLastKillTime;
	float m_flNormalizedGauge;
};

#endif // GLADDER_COMBO_TRACKER_H
