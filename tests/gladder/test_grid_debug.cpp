/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests for In-Game Spatial Grid Real-Time Debug Overlay - Server (SPEC §4.5, GitHub Issue #105)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "dlls/gladder/gladder_grid_debug.h"
#include "dlls/gladder/gladder_grid_indexer.h"
#include <vector>

TEST_CASE( "Spatial Grid Debug - Server State & Radius Controls", "[gladder][grid_debug]" )
{
	GladderGridDebug_ServerReset();
	REQUIRE_FALSE( GladderGridDebug_IsServerActive() );
	REQUIRE( GladderGridDebug_GetRadius() == Catch::Approx( 512.0f ) );

	SECTION( "Toggle and explicit activation" )
	{
		REQUIRE( GladderGridDebug_ToggleServer() == true );
		REQUIRE( GladderGridDebug_IsServerActive() );

		REQUIRE( GladderGridDebug_ToggleServer() == false );
		REQUIRE_FALSE( GladderGridDebug_IsServerActive() );

		GladderGridDebug_SetServerActive( true );
		REQUIRE( GladderGridDebug_IsServerActive() );

		GladderGridDebug_SetServerActive( false );
		REQUIRE_FALSE( GladderGridDebug_IsServerActive() );
	}

	SECTION( "Radius clamp guarantees safe bounds" )
	{
		GladderGridDebug_SetRadius( 64.0f ); // below min (128)
		REQUIRE( GladderGridDebug_GetRadius() == Catch::Approx( 128.0f ) );

		GladderGridDebug_SetRadius( 2048.0f ); // above max (1024)
		REQUIRE( GladderGridDebug_GetRadius() == Catch::Approx( 1024.0f ) );

		GladderGridDebug_SetRadius( 768.0f );
		REQUIRE( GladderGridDebug_GetRadius() == Catch::Approx( 768.0f ) );
	}

	GladderGridDebug_ServerReset();
}

TEST_CASE( "Spatial Grid Debug - Cell Filtering", "[gladder][grid_debug]" )
{
	GladderGridIndexer indexer;
	GladderGridCell c1;
	c1.origin = Vector( 0.0f, 0.0f, 0.0f );
	c1.areaId = 1;
	c1.flags  = GLADDER_CELL_CLEARANCE_OK;

	GladderGridCell c2;
	c2.origin = Vector( 100.0f, 0.0f, 0.0f );
	c2.areaId = 2;
	c2.flags  = GLADDER_CELL_CLEARANCE_OK;

	GladderGridCell c3;
	c3.origin = Vector( 1000.0f, 0.0f, 0.0f ); // Outside 500 radius
	c3.areaId = 3;
	c3.flags  = GLADDER_CELL_CEILING_VALID;

	indexer.AddCell( c1 );
	indexer.AddCell( c2 );
	indexer.AddCell( c3 );

	std::vector<bool> occupied( 3, false );
	occupied[1] = true; // c2 is occupied

	Vector playerPos( 0.0f, 0.0f, 0.0f );
	float radius = 500.0f;

	std::vector<GladderDebugCell> filtered = GladderGridDebug_FilterCells( indexer, playerPos, radius, &occupied );

	REQUIRE( filtered.size() == 2 );
	REQUIRE( filtered[0].areaId == 1 );
	REQUIRE_FALSE( filtered[0].isOccupied );

	REQUIRE( filtered[1].areaId == 2 );
	REQUIRE( filtered[1].isOccupied );
}
