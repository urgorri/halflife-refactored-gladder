/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Wave Lifecycle State Machine & Pacing Telemetry Implementation
 *	(SPEC §3, §9.1, §9.2, GitHub Issue #134)
 *
 ****/

#include "gladder_wave_manager.h"
#include <numeric>

CGladderWaveManager::CGladderWaveManager()
    : m_state( GLADDER_STATE_WAITING_FOR_START ),
      m_iWaveNumber( 1 ),
      m_flSessionStartTime( 0.0f ),
      m_flMatchEndTime( 0.0f ),
      m_flWaveStartTime( 0.0f ),
      m_flCurrentLapElapsed( 0.0f ),
      m_bSurvived( false )
{
}

void CGladderWaveManager::InitializeMatch( float flStartTime )
{
	m_state              = GLADDER_STATE_WAITING_FOR_START;
	m_iWaveNumber        = 1;
	m_flSessionStartTime = flStartTime;
	m_flMatchEndTime     = 0.0f;
	m_flWaveStartTime    = 0.0f;
	m_flCurrentLapElapsed = 0.0f;
	m_bSurvived          = false;
	m_lapTimes.clear();
}

void CGladderWaveManager::StartWave( float flCurrentTime )
{
	if ( m_state == GLADDER_STATE_MATCH_OVER )
		return;

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

	// In Endless Roguelite mode (SPEC §3, Issue #134), waves never expire by session timer.
	// Transition to STANDBY for the next wave's Sky-Drop deployment.
	m_state = GLADDER_STATE_WAITING_FOR_START;
}

void CGladderWaveManager::EndMatch( float flCurrentTime, bool bSurvived )
{
	m_state          = GLADDER_STATE_MATCH_OVER;
	m_flMatchEndTime = flCurrentTime;
	m_bSurvived      = bSurvived;
}

void CGladderWaveManager::Tick( float flCurrentTime )
{
	if ( m_state == GLADDER_STATE_WAVE_ACTIVE )
	{
		m_flCurrentLapElapsed = flCurrentTime - m_flWaveStartTime;
	}
}

float CGladderWaveManager::GetSessionElapsedTime( float flCurrentTime ) const
{
	if ( m_state == GLADDER_STATE_MATCH_OVER && m_flMatchEndTime > 0.0f )
	{
		float elapsed = m_flMatchEndTime - m_flSessionStartTime;
		return ( elapsed > 0.0f ) ? elapsed : 0.0f;
	}

	float elapsed = flCurrentTime - m_flSessionStartTime;
	return ( elapsed > 0.0f ) ? elapsed : 0.0f;
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
	m_flMatchEndTime     = 0.0f;
	m_flWaveStartTime    = 0.0f;
	m_flCurrentLapElapsed = 0.0f;
	m_bSurvived          = false;
	m_lapTimes.clear();
}
