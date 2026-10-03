/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client HUD Overlay: Wave, Session Timer & Lap Pacing
 *
 ****/

#pragma once

#ifndef HUD_GLADDER_OVERLAY_H
#define HUD_GLADDER_OVERLAY_H

#include "hud/hud_base.h"
#include "hud/hud_registry.h"

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

// Purges all transient combat decals (bullet holes, blood splatters, burns)
// while strictly preserving mapper-placed permanent infodecals (SPEC §11.3, Issue #49)
void GladderPurgeCombatDecals( void );

class CHudGladderOverlay : public CHudBase
{
  public:
	CHudGladderOverlay()
	    : m_iWaveState( 0 ),
	      m_iWaveNumber( 1 ),
	      m_flTimeRemaining( 600.0f ),
	      m_flLapTime( 0.0f ),
	      m_flFastestLap( 0.0f ),
	      m_flSlowestLap( 0.0f ),
	      m_flAverageLap( 0.0f ),
	      m_iCompletedLaps( 0 ),
	      m_iFrags( 0 ),
	      m_iDecalPurgeCount( 0 )
	{
		m_iFlags = HUD_ACTIVE;
	}

	int Init( void ) override;
	int VidInit( void ) override;
	int Draw( float flTime ) override;
	void Reset( void ) override
	{
		m_iWaveState       = 0;
		m_iWaveNumber      = 1;
		m_flTimeRemaining  = 600.0f;
		m_flLapTime        = 0.0f;
		m_flFastestLap     = 0.0f;
		m_flSlowestLap     = 0.0f;
		m_flAverageLap     = 0.0f;
		m_iCompletedLaps   = 0;
		m_iFrags           = 0;
		m_iDecalPurgeCount = 0;
	}

	// User message handlers
	int MsgFunc_GladderWave( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_GladderTelemetry( const char *pszName, int iSize, void *pbuf );

	// State getters
	int GetWaveState( void ) const { return m_iWaveState; }
	int GetWaveNumber( void ) const { return m_iWaveNumber; }
	float GetTimeRemaining( void ) const { return m_flTimeRemaining; }
	float GetLapTime( void ) const { return m_flLapTime; }
	int GetCompletedLaps( void ) const { return m_iCompletedLaps; }
	int GetFrags( void ) const { return m_iFrags; }
	float GetFastestLap( void ) const { return m_flFastestLap; }
	float GetSlowestLap( void ) const { return m_flSlowestLap; }
	float GetAverageLap( void ) const { return m_flAverageLap; }
	int GetDecalPurgeCount( void ) const { return m_iDecalPurgeCount; }

	// Test/Simulation setters
	void SetWaveState( int state, int waveNum, float timeRemaining, float lapTime )
	{
		m_iWaveState      = state;
		m_iWaveNumber     = waveNum;
		m_flTimeRemaining = timeRemaining;
		m_flLapTime       = lapTime;
	}

	void SetTelemetry( float fastest, float slowest, float avg, int completedLaps, int frags )
	{
		m_flFastestLap   = fastest;
		m_flSlowestLap   = slowest;
		m_flAverageLap   = avg;
		m_iCompletedLaps = completedLaps;
		m_iFrags         = frags;
	}

  private:
	int m_iWaveState;
	int m_iWaveNumber;
	float m_flTimeRemaining;
	float m_flLapTime;

	float m_flFastestLap;
	float m_flSlowestLap;
	float m_flAverageLap;
	int m_iCompletedLaps;
	int m_iFrags;
	int m_iDecalPurgeCount;
};

extern CHudGladderOverlay g_HudGladderOverlay;

#endif // HUD_GLADDER_OVERLAY_H
