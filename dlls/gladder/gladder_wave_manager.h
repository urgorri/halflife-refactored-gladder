/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Wave Lifecycle State Machine & Pacing Telemetry
 *	(SPEC §3, §9.1, §9.2, GitHub Issue #134)
 *
 ****/

#pragma once

#ifndef GLADDER_WAVE_MANAGER_H
#define GLADDER_WAVE_MANAGER_H

#include <vector>
#include <algorithm>

#ifndef GLADDER_WAVE_STATE_DEFINED
#define GLADDER_WAVE_STATE_DEFINED
enum GladderWaveState
{
	GLADDER_STATE_WAITING_FOR_START = 0,
	GLADDER_STATE_WAVE_ACTIVE       = 1,
	GLADDER_STATE_WAVE_COMPLETED    = 2,
	GLADDER_STATE_MATCH_OVER        = 3
};
#endif

class CGladderWaveManager
{
  public:
	CGladderWaveManager();

	// Lifecycle events
	void InitializeMatch( float flStartTime );
	void StartWave( float flCurrentTime );
	void CompleteWave( float flCurrentTime );
	void EndMatch( float flCurrentTime, bool bSurvived = false );
	void Tick( float flCurrentTime );

	// State queries
	GladderWaveState GetState( void ) const { return m_state; }
	int GetWaveNumber( void ) const { return m_iWaveNumber; }
	int GetCompletedWavesCount( void ) const { return (int)m_lapTimes.size(); }
	bool IsMatchOver( void ) const { return m_state == GLADDER_STATE_MATCH_OVER; }
	bool IsWaveActive( void ) const { return m_state == GLADDER_STATE_WAVE_ACTIVE; }
	bool HasSurvived( void ) const { return m_bSurvived; }

	// Timers & telemetry (SPEC §9.1: elapsed run time count-up, endless roguelite)
	float GetSessionElapsedTime( float flCurrentTime ) const;
	float GetActiveLapTime( float flCurrentTime ) const;
	float GetFastestLapTime( void ) const;
	float GetSlowestLapTime( void ) const;
	float GetAverageLapTime( void ) const;
	const std::vector<float> &GetLapTimes( void ) const { return m_lapTimes; }

	// Reset
	void Reset( void );

  private:
	GladderWaveState m_state;
	int m_iWaveNumber;
	float m_flSessionStartTime;
	float m_flMatchEndTime;
	float m_flWaveStartTime;
	float m_flCurrentLapElapsed;
	bool m_bSurvived;

	std::vector<float> m_lapTimes;
};

#endif // GLADDER_WAVE_MANAGER_H
