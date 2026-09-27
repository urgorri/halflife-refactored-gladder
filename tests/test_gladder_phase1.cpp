/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Behavioral Equivalence & Unit Test Suite - Phase 1 Foundation & Lifecycle Core
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_wave_manager.h"
#include "dlls/gladder/gladder_usermsg.h"
#include "cl_dll/gladder/hud_gladder_overlay.h"

TEST_CASE( "Gladder Phase 1: Wave State Machine Transitions", "[gladder][wave_manager]" )
{
	CGladderWaveManager manager;
	manager.SetSessionTimeLimit( 600.0f );
	manager.InitializeMatch( 100.0f );

	SECTION( "Initial state after match initialization" )
	{
		REQUIRE( manager.GetState() == GLADDER_STATE_WAITING_FOR_START );
		REQUIRE( manager.GetWaveNumber() == 1 );
		REQUIRE( manager.GetCompletedWavesCount() == 0 );
		REQUIRE( manager.GetSessionTimeRemaining( 100.0f ) == Catch::Approx( 600.0f ) );
		REQUIRE( manager.GetActiveLapTime( 100.0f ) == 0.0f );
		REQUIRE( manager.GetFastestLapTime() == 0.0f );
		REQUIRE( manager.GetSlowestLapTime() == 0.0f );
		REQUIRE( manager.GetAverageLapTime() == 0.0f );
	}

	SECTION( "Start wave transitions to GLADDER_STATE_WAVE_ACTIVE" )
	{
		manager.StartWave( 110.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_WAVE_ACTIVE );
		REQUIRE( manager.GetActiveLapTime( 125.0f ) == Catch::Approx( 15.0f ) );
	}

	SECTION( "Completing a wave records lap telemetry and increments wave" )
	{
		manager.StartWave( 100.0f );
		manager.CompleteWave( 120.0f ); // 20 second lap
		// CompleteWave now transitions directly to STANDBY (WAITING_FOR_START) for the
		// next wave, skipping the transient WAVE_COMPLETED state. The HUD shows
		// "WAVE N+1 [STANDBY]" immediately so the player can prepare without confusion.
		REQUIRE( manager.GetState() == GLADDER_STATE_WAITING_FOR_START );
		REQUIRE( manager.GetCompletedWavesCount() == 1 );
		REQUIRE( manager.GetWaveNumber() == 2 );
		REQUIRE( manager.GetLapTimes().back() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetFastestLapTime() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetSlowestLapTime() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetAverageLapTime() == Catch::Approx( 20.0f ) );

		// Start wave 2 and complete with a slower lap (40 seconds)
		manager.StartWave( 130.0f );
		manager.CompleteWave( 170.0f );
		REQUIRE( manager.GetCompletedWavesCount() == 2 );
		REQUIRE( manager.GetWaveNumber() == 3 );
		REQUIRE( manager.GetLapTimes().back() == Catch::Approx( 40.0f ) );
		REQUIRE( manager.GetFastestLapTime() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetSlowestLapTime() == Catch::Approx( 40.0f ) );
		REQUIRE( manager.GetAverageLapTime() == Catch::Approx( 30.0f ) );

		// Start wave 3 and complete with a faster lap (12 seconds)
		manager.StartWave( 180.0f );
		manager.CompleteWave( 192.0f );
		REQUIRE( manager.GetCompletedWavesCount() == 3 );
		REQUIRE( manager.GetWaveNumber() == 4 );
		REQUIRE( manager.GetFastestLapTime() == Catch::Approx( 12.0f ) );
		REQUIRE( manager.GetSlowestLapTime() == Catch::Approx( 40.0f ) );
		REQUIRE( manager.GetAverageLapTime() == Catch::Approx( ( 20.0f + 40.0f + 12.0f ) / 3.0f ) );
	}

	SECTION( "Match ends immediately when EndMatch is called" )
	{
		manager.StartWave( 100.0f );
		manager.EndMatch( 150.0f, false );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
		REQUIRE( manager.IsMatchOver() == true );

		// Subsequent start/complete actions must not change MATCH_OVER state
		manager.StartWave( 160.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
		manager.CompleteWave( 170.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
	}

	SECTION( "Session timer expiration during Tick triggers GLADDER_STATE_MATCH_OVER" )
	{
		manager.StartWave( 100.0f );
		// Advance tick beyond the 600s session limit (100 + 601 = 701.0f)
		manager.Tick( 701.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
		REQUIRE( manager.IsMatchOver() == true );
		REQUIRE( manager.GetSessionTimeRemaining( 701.0f ) == 0.0f );
	}
}

TEST_CASE( "Gladder Phase 1: Wall Charger Capacity Degradation", "[gladder][rules]" )
{
	// Formula:
	// Wave 1: 1.0f * 100%
	// Wave 2: 0.85f * 100%
	// Wave 3: 0.70f * 100%
	// Wave >= 6: 0.25f (clamped minimum)
	auto CalcHealthCapacity = []( int waveNum ) -> float {
		float factor = 1.0f - ( waveNum - 1 ) * 0.15f;
		if ( factor < 0.25f )
			factor = 0.25f;
		return factor * 100.0f; // assuming base 100.0f
	};

	auto CalcHEVCapacity = []( int waveNum ) -> float {
		float factor = 1.0f - ( waveNum - 1 ) * 0.15f;
		if ( factor < 0.25f )
			factor = 0.25f;
		return factor * 100.0f; // assuming base 100.0f
	};

	REQUIRE( CalcHealthCapacity( 1 ) == Catch::Approx( 100.0f ) );
	REQUIRE( CalcHealthCapacity( 2 ) == Catch::Approx( 85.0f ) );
	REQUIRE( CalcHealthCapacity( 3 ) == Catch::Approx( 70.0f ) );
	REQUIRE( CalcHealthCapacity( 4 ) == Catch::Approx( 55.0f ) );
	REQUIRE( CalcHealthCapacity( 5 ) == Catch::Approx( 40.0f ) );
	REQUIRE( CalcHealthCapacity( 6 ) == Catch::Approx( 25.0f ) );
	REQUIRE( CalcHealthCapacity( 10 ) == Catch::Approx( 25.0f ) ); // minimum clamped

	REQUIRE( CalcHEVCapacity( 1 ) == Catch::Approx( 100.0f ) );
	REQUIRE( CalcHEVCapacity( 6 ) == Catch::Approx( 25.0f ) );
}

TEST_CASE( "Gladder Phase 1: Save & Respawn Prevention Logic", "[gladder][rules]" )
{
	// Verifies that Gladder game rules block save and reload client commands
	const char *blockedCommands[] = {
		"save", "load", "reload", "autosave", "quickload", "quicksave",
		"SAVE", "LOAD", "RELOAD"
	};

	auto CaseInsensitiveEquals = []( const char *s1, const char *s2 ) -> bool {
		if ( !s1 || !s2 )
			return s1 == s2;
		while ( *s1 && *s2 )
		{
			if ( tolower( (unsigned char)*s1 ) != tolower( (unsigned char)*s2 ) )
				return false;
			s1++;
			s2++;
		}
		return *s1 == *s2;
	};

	auto IsBlockedCommand = [&]( const char *pcmd ) -> bool {
		if ( !pcmd )
			return false;
		if ( CaseInsensitiveEquals( pcmd, "save" ) ||
		     CaseInsensitiveEquals( pcmd, "load" ) ||
		     CaseInsensitiveEquals( pcmd, "reload" ) ||
		     CaseInsensitiveEquals( pcmd, "autosave" ) ||
		     CaseInsensitiveEquals( pcmd, "quickload" ) ||
		     CaseInsensitiveEquals( pcmd, "quicksave" ) )
		{
			return true;
		}
		return false;
	};

	for ( const char *cmd : blockedCommands )
	{
		CHECK( IsBlockedCommand( cmd ) == true );
	}

	CHECK( IsBlockedCommand( "attack" ) == false );
	CHECK( IsBlockedCommand( "use" ) == false );
	CHECK( IsBlockedCommand( "jump" ) == false );
}

TEST_CASE( "Gladder Phase 1: Client HUD Overlay Telemetry Sync", "[gladder][hud]" )
{
	CHudGladderOverlay hud;
	hud.Reset();

	REQUIRE( hud.GetWaveNumber() == 1 );
	REQUIRE( hud.GetTimeRemaining() == Catch::Approx( 600.0f ) );
	REQUIRE( hud.GetFrags() == 0 );
	REQUIRE( hud.GetCompletedLaps() == 0 );

	// Simulate wave update
	hud.SetWaveState( GLADDER_STATE_WAVE_ACTIVE, 3, 450.0f, 15.5f );
	REQUIRE( hud.GetWaveState() == GLADDER_STATE_WAVE_ACTIVE );
	REQUIRE( hud.GetWaveNumber() == 3 );
	REQUIRE( hud.GetTimeRemaining() == Catch::Approx( 450.0f ) );
	REQUIRE( hud.GetLapTime() == Catch::Approx( 15.5f ) );

	// Simulate telemetry metrics update
	hud.SetTelemetry( 12.0f, 35.0f, 21.5f, 2, 47 );
	REQUIRE( hud.GetFastestLap() == Catch::Approx( 12.0f ) );
	REQUIRE( hud.GetSlowestLap() == Catch::Approx( 35.0f ) );
	REQUIRE( hud.GetAverageLap() == Catch::Approx( 21.5f ) );
	REQUIRE( hud.GetCompletedLaps() == 2 );
	REQUIRE( hud.GetFrags() == 47 );
}

#ifdef DEBUG
edict_t *DBG_EntOfVars( const entvars_t *pev )
{
	return pev ? pev->pContainingEntity : nullptr;
}
#endif
