/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Wave Lifecycle State Machine & Pacing Telemetry Implementation
 *
 ****/

#include "gladder_wave_manager.h"
#include <numeric>

CGladderWaveManager::CGladderWaveManager()
    : m_state( GLADDER_STATE_WAITING_FOR_START ),
      m_iWaveNumber( 1 ),
      m_flSessionDuration( 600.0f ), // Default 10 minutes session
      m_flSessionStartTime( 0.0f ),
      m_flSessionEndTime( 0.0f ),
      m_flWaveStartTime( 0.0f ),
      m_flCurrentLapElapsed( 0.0f ),
      m_flIdleEntryTime( 0.0f ),
      m_bSurvived( false )
{
}

void CGladderWaveManager::SetSessionTimeLimit( float flSeconds )
{
	m_flSessionDuration = ( flSeconds > 0.0f ) ? flSeconds : 600.0f;
}

void CGladderWaveManager::InitializeMatch( float flStartTime )
{
	m_state              = GLADDER_STATE_WAITING_FOR_START;
	m_iWaveNumber        = 1;
	m_flSessionStartTime = flStartTime;
	m_flSessionEndTime   = flStartTime + m_flSessionDuration;
	m_flWaveStartTime    = 0.0f;
	m_flCurrentLapElapsed = 0.0f;
	// Timer starts paused: the countdown only runs while a wave is ACTIVE.
	// m_flIdleEntryTime marks when idle began so Tick() can defer the end time.
	m_flIdleEntryTime    = flStartTime;
	m_bSurvived          = false;
	m_lapTimes.clear();
}

void CGladderWaveManager::StartWave( float flCurrentTime )
{
	if ( m_state == GLADDER_STATE_MATCH_OVER )
		return;

	// Compensate the session end time by the idle period that just elapsed.
	// This ensures the countdown only runs while a wave is ACTIVE.
	if ( m_flIdleEntryTime > 0.0f )
	{
		float flIdleDuration = flCurrentTime - m_flIdleEntryTime;
		if ( flIdleDuration > 0.0f )
			m_flSessionEndTime += flIdleDuration;
		m_flIdleEntryTime = 0.0f;
	}

	m_state             = GLADDER_STATE_WAVE_ACTIVE;
	m_flWaveStartTime   = flCurrentTime;
	m_flCurrentLapElapsed = 0.0f;
}

void CGladderWaveManager::CompleteWave( float flCurrentTime )
{
	if ( m_state != GLADDER_STATE_WAVE_ACTIVE )
		return;

	float flLapDuration = flCurrentTime - m_flWaveStartTime;
	if ( flLapDuration < 0.0f )
		flLapDuration = 0.0f;

	m_lapTimes.push_back( flLapDuration );
	m_iWaveNumber++;
	m_flCurrentLapElapsed = 0.0f;

	// Check if session timer has expired at completion
	if ( flCurrentTime >= m_flSessionEndTime )
	{
		EndMatch( flCurrentTime, true );
		return;
	}

	// Transition directly to STANDBY for the next wave (skip COMPLETED state).
	// The HUD will show "WAVE N+1 [STANDBY]" — the player already knows wave N was cleared.
	m_state           = GLADDER_STATE_WAITING_FOR_START;
	m_flIdleEntryTime = flCurrentTime; // start tracking idle again
}

void CGladderWaveManager::EndMatch( float flCurrentTime, bool bSurvived )
{
	m_state     = GLADDER_STATE_MATCH_OVER;
	m_bSurvived = bSurvived;
}

void CGladderWaveManager::Tick( float flCurrentTime )
{
	if ( m_state == GLADDER_STATE_WAVE_ACTIVE )
	{
		m_flCurrentLapElapsed = flCurrentTime - m_flWaveStartTime;
		if ( flCurrentTime >= m_flSessionEndTime )
		{
			// Time limit expired during active wave
			EndMatch( flCurrentTime, true );
		}
	}
	else if ( m_state == GLADDER_STATE_WAITING_FOR_START )
	{
		// The session countdown is paused while the player is in STANDBY.
		// Slide the end time forward so GetSessionTimeRemaining stays constant.
		if ( m_flIdleEntryTime > 0.0f )
		{
			float flIdleDuration = flCurrentTime - m_flIdleEntryTime;
			if ( flIdleDuration > 0.0f )
				m_flSessionEndTime += flIdleDuration;
			m_flIdleEntryTime = flCurrentTime;
		}
	}
}

float CGladderWaveManager::GetSessionTimeRemaining( float flCurrentTime ) const
{
	if ( m_state == GLADDER_STATE_MATCH_OVER )
		return 0.0f;

	float flRemaining = m_flSessionEndTime - flCurrentTime;
	return ( flRemaining > 0.0f ) ? flRemaining : 0.0f;
}

float CGladderWaveManager::GetActiveLapTime( float flCurrentTime ) const
{
	if ( m_state == GLADDER_STATE_WAVE_ACTIVE )
	{
		float elapsed = flCurrentTime - m_flWaveStartTime;
		return ( elapsed > 0.0f ) ? elapsed : 0.0f;
	}
	return m_flCurrentLapElapsed;
}

float CGladderWaveManager::GetFastestLapTime( void ) const
{
	if ( m_lapTimes.empty() )
		return 0.0f;

	return *std::min_element( m_lapTimes.begin(), m_lapTimes.end() );
}

float CGladderWaveManager::GetSlowestLapTime( void ) const
{
	if ( m_lapTimes.empty() )
		return 0.0f;

	return *std::max_element( m_lapTimes.begin(), m_lapTimes.end() );
}

float CGladderWaveManager::GetAverageLapTime( void ) const
{
	if ( m_lapTimes.empty() )
		return 0.0f;

	float sum = std::accumulate( m_lapTimes.begin(), m_lapTimes.end(), 0.0f );
	return sum / static_cast<float>( m_lapTimes.size() );
}

void CGladderWaveManager::Reset( void )
{
	m_state              = GLADDER_STATE_WAITING_FOR_START;
	m_iWaveNumber        = 1;
	m_flSessionStartTime = 0.0f;
	m_flSessionEndTime   = 0.0f;
	m_flWaveStartTime    = 0.0f;
	m_flCurrentLapElapsed = 0.0f;
	m_flIdleEntryTime    = 0.0f;
	m_bSurvived          = false;
	m_lapTimes.clear();
}
