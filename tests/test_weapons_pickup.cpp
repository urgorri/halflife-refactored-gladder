/***
 *
 *	Behavioral Equivalence Verification - Weapon Pickup HUD & Dispatch Unit Tests (#195)
 *	Verifies WeapPickup user message contract, HUD history recording,
 *	and duplicate dispatch suppression across frame/acquisition boundaries.
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_client_engine.h"
#include "cl_dll/hud/hud_ammo.h"
#include "weapons/weapon_defs.h"

extern int ScreenWidth;
extern int ScreenHeight;

static void ResetPickupTestState( void )
{
	InitMockClientEngine();
	ScreenWidth  = 640;
	ScreenHeight = 480;
	g_mockClientEngineFuncs.pfnRegisterVariable( "hud_drawhistory_time", "4.0", 0 );
	gHUD.m_flTime = 10.0f;
	gHR.Reset();
	gHR.iCurrentHistorySlot = 0;
}

TEST_CASE( "Weapon Pickup: Standard Weapon Constants Contract (#195)", "[weapons][pickup][constants]" )
{
	// All weapon IDs must be distinct, positive identifiers greater than WEAPON_NONE
	CHECK( WEAPON_NONE == 0 );
	CHECK( WEAPON_CROWBAR > WEAPON_NONE );
	CHECK( WEAPON_GLOCK > WEAPON_NONE );
	CHECK( WEAPON_PYTHON > WEAPON_NONE );
	CHECK( WEAPON_MP5 > WEAPON_NONE );
	CHECK( WEAPON_CHAINGUN > WEAPON_NONE );
	CHECK( WEAPON_CROSSBOW > WEAPON_NONE );
	CHECK( WEAPON_SHOTGUN > WEAPON_NONE );
	CHECK( WEAPON_RPG > WEAPON_NONE );
	CHECK( WEAPON_GAUSS > WEAPON_NONE );
	CHECK( WEAPON_EGON > WEAPON_NONE );
	CHECK( WEAPON_HORNETGUN > WEAPON_NONE );
	CHECK( WEAPON_HANDGRENADE > WEAPON_NONE );
	CHECK( WEAPON_TRIPMINE > WEAPON_NONE );
	CHECK( WEAPON_SATCHEL > WEAPON_NONE );
	CHECK( WEAPON_SNARK > WEAPON_NONE );

	// Verify values match vanilla GoldSrc protocol
	CHECK( WEAPON_CROWBAR == 1 );
	CHECK( WEAPON_GLOCK == 2 );
	CHECK( WEAPON_PYTHON == 3 );
	CHECK( WEAPON_MP5 == 4 );
	CHECK( WEAPON_CROSSBOW == 6 );
	CHECK( WEAPON_SHOTGUN == 7 );
	CHECK( WEAPON_RPG == 8 );
	CHECK( WEAPON_GAUSS == 9 );
	CHECK( WEAPON_EGON == 10 );
	CHECK( WEAPON_HORNETGUN == 11 );
	CHECK( WEAPON_HANDGRENADE == 12 );
	CHECK( WEAPON_TRIPMINE == 13 );
	CHECK( WEAPON_SATCHEL == 14 );
	CHECK( WEAPON_SNARK == 15 );
}

TEST_CASE( "HistoryResource: Weapon pickup adds entries and suppresses duplicates (#195)", "[hud][history][pickup]" )
{
	ResetPickupTestState();

	SECTION( "Initial pickup records weapon ID into first history slot" )
	{
		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_GLOCK );

		REQUIRE( gHR.iCurrentHistorySlot == 1 );
		CHECK( gHR.GetHistorySlotType( 0 ) == HISTSLOT_WEAP );
		CHECK( gHR.GetHistorySlotId( 0 ) == WEAPON_GLOCK );
		CHECK( gHR.GetHistorySlotDisplayTime( 0 ) == Catch::Approx( gHUD.m_flTime + 4.0f ) );
	}

	SECTION( "Duplicate pickup in same acquisition window is suppressed" )
	{
		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_GLOCK );
		REQUIRE( gHR.iCurrentHistorySlot == 1 );

		// Simulated redundant dispatch from legacy derived weapon class override
		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_GLOCK );
		CHECK( gHR.iCurrentHistorySlot == 1 );
		CHECK( gHR.GetHistorySlotId( 0 ) == WEAPON_GLOCK );
	}

	SECTION( "Distinct weapons acquired in sequence are recorded into successive slots" )
	{
		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_CROWBAR );
		REQUIRE( gHR.iCurrentHistorySlot == 1 );
		CHECK( gHR.GetHistorySlotId( 0 ) == WEAPON_CROWBAR );

		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_GLOCK );
		REQUIRE( gHR.iCurrentHistorySlot == 2 );
		CHECK( gHR.GetHistorySlotId( 1 ) == WEAPON_GLOCK );

		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_SHOTGUN );
		REQUIRE( gHR.iCurrentHistorySlot == 3 );
		CHECK( gHR.GetHistorySlotId( 2 ) == WEAPON_SHOTGUN );

		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_MP5 );
		REQUIRE( gHR.iCurrentHistorySlot == 4 );
		CHECK( gHR.GetHistorySlotId( 3 ) == WEAPON_MP5 );

		// Immediate re-dispatch of MP5 should be suppressed
		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_MP5 );
		CHECK( gHR.iCurrentHistorySlot == 4 );
	}

	SECTION( "Re-acquiring the same weapon after display expiry is permitted" )
	{
		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_PYTHON );
		REQUIRE( gHR.iCurrentHistorySlot == 1 );

		// Advance simulation time beyond HUD draw history duration (4.0s)
		gHUD.m_flTime += 5.0f;

		// Weapon picked up again in subsequent gameplay (e.g. after drop and re-acquire)
		gHR.AddToHistory( HISTSLOT_WEAP, WEAPON_PYTHON );
		REQUIRE( gHR.iCurrentHistorySlot == 2 );
		CHECK( gHR.GetHistorySlotId( 1 ) == WEAPON_PYTHON );
	}

	SECTION( "All previously omitted standard weapons register cleanly into HUD history" )
	{
		const int omittedWeapons[] = {
			WEAPON_CROWBAR,
			WEAPON_GLOCK,
			WEAPON_HANDGRENADE,
			WEAPON_TRIPMINE,
			WEAPON_SATCHEL,
			WEAPON_SNARK
		};

		int expectedSlot = 1;
		for ( int weaponId : omittedWeapons )
		{
			gHR.AddToHistory( HISTSLOT_WEAP, weaponId );

			REQUIRE( gHR.iCurrentHistorySlot == expectedSlot );
			CHECK( gHR.GetHistorySlotType( expectedSlot - 1 ) == HISTSLOT_WEAP );
			CHECK( gHR.GetHistorySlotId( expectedSlot - 1 ) == weaponId );
			expectedSlot++;
		}
	}
}
