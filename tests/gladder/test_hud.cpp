/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: HUD Elements & Dynamic Combo Gauge (SPEC §9.1, GitHub Issue #15)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "cl_dll/gladder/hud_gladder_combo.h"
#include "cl_dll/gladder/hud_gladder_overlay.h"
#include "cl_dll/gladder/hud_gladder_lambda.h"


TEST_CASE( "CHudGladderCombo: Multiplier and Decay Meter Tracking (#15)", "[gladder][hud][combo]" )
{
	CHudGladderCombo combo;
	CHECK( combo.IsActive() == false );
	CHECK( combo.GetComboMultiplier() == 1 );
	CHECK( combo.GetDecayFraction() == 0.0f );

	// Multiplier x2 with full window (1.0)
	combo.SetComboState( 2, 1.0f, 10.0f );
	CHECK( combo.IsActive() == true );
	CHECK( combo.GetComboMultiplier() == 2 );
	CHECK( combo.GetDecayFraction() == 1.0f );

	// Decaying window
	combo.SetComboState( 2, 0.5f, 11.5f );
	CHECK( combo.IsActive() == true );
	CHECK( combo.GetDecayFraction() == 0.5f );

	// Multiplier x3 fresh kill
	combo.SetComboState( 3, 1.0f, 12.0f );
	CHECK( combo.IsActive() == true );
	CHECK( combo.GetComboMultiplier() == 3 );

	// Window fully decayed to 0
	combo.SetComboState( 1, 0.0f, 15.5f );
	CHECK( combo.IsActive() == false );
	CHECK( combo.GetComboMultiplier() == 1 );
}

TEST_CASE( "CHudGladderOverlay: Telemetry and Collectibles Field Tracking (#15)", "[gladder][hud][overlay]" )
{
	CHudGladderOverlay overlay;
	CHECK( overlay.GetCollectibles() == 0 );

	overlay.SetTelemetry( 25.0f, 50.0f, 35.0f, 5, 80, 8 );
	CHECK( overlay.GetCollectibles() == 8 );
	CHECK( overlay.GetFrags() == 80 );
	CHECK( overlay.GetCompletedLaps() == 5 );
	CHECK( overlay.GetAverageLap() == 35.0f );

	overlay.Reset();
	CHECK( overlay.GetCollectibles() == 0 );
	CHECK( overlay.GetFrags() == 0 );
}

TEST_CASE( "CHudGladderLambda: Center-screen notification lifecycle & replacement (SPEC §7.3, Issue #92)", "[gladder][hud][lambda]" )
{
	CHudGladderLambda lambda;
	CHECK( lambda.GetScoreBonus() == 0 );
	CHECK( lambda.GetCollectibleCount() == 0 );
	CHECK( lambda.IsDisplaying( 0.0f ) == false );

	SECTION( "Trigger sets score, collectible count, formatted text, and active display window" )
	{
		lambda.TriggerNotification( 1000, 3, 10.0f );
		CHECK( lambda.GetScoreBonus() == 1000 );
		CHECK( lambda.GetCollectibleCount() == 3 );
		CHECK( std::string( lambda.GetText() ).find( "LAMBDA COLLECTED +1000" ) != std::string::npos );
		CHECK( std::string( lambda.GetText() ).find( "TOTAL: 3" ) != std::string::npos );

		// Inside display window
		CHECK( lambda.IsDisplaying( 9.9f ) == false );
		CHECK( lambda.IsDisplaying( 10.0f ) == true );
		CHECK( lambda.IsDisplaying( 11.2f ) == true ); // hold time boundary
		CHECK( lambda.IsDisplaying( 11.6f ) == true ); // fade time
		CHECK( lambda.IsDisplaying( 11.75f ) == false ); // expired
	}

	SECTION( "Subsequent notification replaces active one without stacking" )
	{
		lambda.TriggerNotification( 1000, 1, 10.0f );
		CHECK( lambda.GetCollectibleCount() == 1 );
		CHECK( lambda.IsDisplaying( 11.0f ) == true );

		// New pickup occurs at 11.0s
		lambda.TriggerNotification( 1000, 2, 11.0f );
		CHECK( lambda.GetCollectibleCount() == 2 );
		CHECK( lambda.GetStartTime() == 11.0f );
		// New window expires at 11.0 + 1.7 = 12.7s
		CHECK( lambda.IsDisplaying( 12.0f ) == true );
		CHECK( lambda.IsDisplaying( 12.6f ) == true );
		CHECK( lambda.IsDisplaying( 12.8f ) == false );
	}

	SECTION( "Reset clears all notification state" )
	{
		lambda.TriggerNotification( 1000, 5, 20.0f );
		REQUIRE( lambda.IsDisplaying( 20.5f ) == true );

		lambda.Reset();
		CHECK( lambda.GetScoreBonus() == 0 );
		CHECK( lambda.GetCollectibleCount() == 0 );
		CHECK( lambda.GetText()[0] == '\0' );
		CHECK( lambda.IsDisplaying( 20.5f ) == false );
	}
}

