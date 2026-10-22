/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Wave Transition Overlay (SPEC §12, GitHub Issue #25)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "cl_dll/gladder/hud_gladder_wave_notify.h"
#include <cstring>

TEST_CASE( "GladderWaveNotify: Title and Mutator Subtitle Formatting (#25)", "[gladder][hud][notify]" )
{
	CHudGladderWaveNotify notify;

	// 1. Wave 1 Start, No Mutator
	notify.TriggerNotification( 0, 1, 0, 10.0f );
	CHECK( notify.GetType() == 0 );
	CHECK( notify.GetWaveNumber() == 1 );
	CHECK( std::string( notify.GetTitle() ) == "WAVE 1 STARTED" );
	CHECK( std::string( notify.GetSubtitle() ) == "" );

	// 2. Wave 5 Start, Low Gravity Mutator
	notify.TriggerNotification( 0, 5, 2, 20.0f );
	CHECK( notify.GetWaveNumber() == 5 );
	CHECK( std::string( notify.GetTitle() ) == "WAVE 5 STARTED" );
	CHECK( std::string( notify.GetSubtitle() ) == "LOW GRAVITY MUTATOR ACTIVE" );

	// 3. Wave 10 Start, Blackout Mutator
	notify.TriggerNotification( 0, 10, 1, 30.0f );
	CHECK( std::string( notify.GetSubtitle() ) == "BLACKOUT MUTATOR ACTIVE" );

	// 4. Wave 15 Start, Swarm Mutator
	notify.TriggerNotification( 0, 15, 3, 40.0f );
	CHECK( std::string( notify.GetSubtitle() ) == "SWARM MUTATOR ACTIVE" );

	// 5. Wave 20 Start, Elite Surge Mutator
	notify.TriggerNotification( 0, 20, 4, 50.0f );
	CHECK( std::string( notify.GetSubtitle() ) == "ELITE SURGE MUTATOR ACTIVE" );

	// 6. Wave 5 Complete
	notify.TriggerNotification( 1, 5, 0, 60.0f );
	CHECK( notify.GetType() == 1 );
	CHECK( std::string( notify.GetTitle() ) == "WAVE 5 COMPLETED" );
	CHECK( std::string( notify.GetSubtitle() ) == "" );
}

TEST_CASE( "GladderWaveNotify: Display Timing and Visibility Window (#25)", "[gladder][hud][notify]" )
{
	CHudGladderWaveNotify notify;
	notify.TriggerNotification( 0, 3, 0, 100.0f );

	// Before start
	CHECK( notify.IsDisplaying( 99.9f ) == false );

	// Active hold window (3.0s)
	CHECK( notify.IsDisplaying( 100.0f ) == true );
	CHECK( notify.IsDisplaying( 101.0f ) == true );
	CHECK( notify.IsDisplaying( 102.5f ) == true );
	CHECK( notify.IsDisplaying( 103.0f ) == true );

	// Fade-out window (1.0s)
	CHECK( notify.IsDisplaying( 103.5f ) == true );
	CHECK( notify.IsDisplaying( 103.9f ) == true );

	// Past 4.0s total display time
	CHECK( notify.IsDisplaying( 104.0f ) == false );
	CHECK( notify.IsDisplaying( 105.0f ) == false );
}

TEST_CASE( "GladderWaveNotify: Hostile quota line (SPEC §12.1, #127)", "[gladder][hud][notify]" )
{
	CHudGladderWaveNotify notify;

	// Plurals: 24 hostiles
	notify.TriggerNotification( 0, 3, 0, 100.0f, 24 );
	CHECK( std::string( notify.GetTitle() ) == "WAVE 3 STARTED" );
	CHECK( std::string( notify.GetHostileLine() ) == "ELIMINATE 24 HOSTILES" );
	CHECK( std::string( notify.GetSubtitle() ) == "" );

	// Singular: exactly 1 hostile
	notify.TriggerNotification( 0, 4, 0, 110.0f, 1 );
	CHECK( std::string( notify.GetHostileLine() ) == "ELIMINATE 1 HOSTILE" );

	// Mutator wave keeps both the hostile line and the mutator subtitle
	notify.TriggerNotification( 0, 5, 2, 120.0f, 24 );
	CHECK( std::string( notify.GetHostileLine() ) == "ELIMINATE 24 HOSTILES" );
	CHECK( std::string( notify.GetSubtitle() ) == "LOW GRAVITY MUTATOR ACTIVE" );

	// Completion notification hides the hostile line
	notify.TriggerNotification( 1, 3, 0, 130.0f, 0 );
	CHECK( std::string( notify.GetHostileLine() ) == "" );
	CHECK( std::string( notify.GetTitle() ) == "WAVE 3 COMPLETED" );

	// Quota 0 on a start notification also hides the line
	notify.TriggerNotification( 0, 6, 0, 140.0f, 0 );
	CHECK( std::string( notify.GetHostileLine() ) == "" );
}
