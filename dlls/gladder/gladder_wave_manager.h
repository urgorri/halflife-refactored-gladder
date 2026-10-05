/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Wave Lifecycle State Machine & Pacing Telemetry
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

	// Session configuration
	void SetSessionTimeLimit( float flSeconds );
	float GetSessionTimeLimit( void ) const { return m_flSessionDuration; }

	// Lifecycle events
	void InitializeMatch( float flStartTime );
	void StartWave( float flCurrentTime );
	void CompleteWave( float flCurrentTime );
	void EndMatch( float flCurrentTime, bool bSurvived );
	void Tick( float flCurrentTime );

	// State queries
	GladderWaveState GetState( void ) const { return m_state; }
	int GetWaveNumber( void ) const { return m_iWaveNumber; }
	int GetCompletedWavesCount( void ) const { return (int)m_lapTimes.size(); }
	bool IsMatchOver( void ) const { return m_state == GLADDER_STATE_MATCH_OVER; }
	bool IsWaveActive( void ) const { return m_state == GLADDER_STATE_WAVE_ACTIVE; }

	// Timers & telemetry
	float GetSessionTimeRemaining( float flCurrentTime ) const;
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
	float m_flSessionDuration;
	float m_flSessionStartTime;
	float m_flSessionEndTime;
	float m_flWaveStartTime;
	float m_flCurrentLapElapsed;
	float m_flIdleEntryTime;    // Engine time when we last entered a non-active state
	bool m_bSurvived;

	std::vector<float> m_lapTimes;
};

#endif // GLADDER_WAVE_MANAGER_H
