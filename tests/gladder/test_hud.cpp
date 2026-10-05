/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: HUD Elements & Dynamic Combo Gauge (SPEC §9.1, GitHub Issue #15)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "cl_dll/gladder/hud_gladder_combo.h"
#include "cl_dll/gladder/hud_gladder_overlay.h"

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
