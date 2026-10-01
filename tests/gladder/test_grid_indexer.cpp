/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit & Behavioral Equivalence Test Suite: GladderGridIndexer (SPEC §4, GitHub Issue #6)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_grid_indexer.h"
#include "dlls/gladder/gladder_entities.h"

TEST_CASE( "Gladder Grid Indexer: Default state & path formatting", "[gladder][grid]" )
{
	GladderGridIndexer indexer;

	CHECK( indexer.GetCellCount() == 0 );
	CHECK( indexer.IsLoaded() == false );
	CHECK( indexer.GetCells().empty() );

	SECTION( "Path formatting with default mock engine game directory" )
	{
		CHECK( GladderGridIndexer::GetGridFilePath( "c1a0" ) == "valve/maps/grid/c1a0.grid" );
		CHECK( GladderGridIndexer::GetGridFilePath( "maps/c1a0.bsp" ) == "valve/maps/grid/c1a0.grid" );
		CHECK( GladderGridIndexer::GetGridFilePath( "custom\\boot_camp.bsp" ) == "valve/maps/grid/boot_camp.grid" );
		CHECK( GladderGridIndexer::GetGridFilePath( "" ) == "maps/grid/unknown.grid" );
		CHECK( GladderGridIndexer::GetGridFilePath( nullptr ) == "maps/grid/unknown.grid" );
	}

	SECTION( "Path formatting without engine game directory" )
	{
		auto originalGetGameDir = g_engfuncs.pfnGetGameDir;
		g_engfuncs.pfnGetGameDir = nullptr;

		CHECK( GladderGridIndexer::GetGridFilePath( "c1a0" ) == "maps/grid/c1a0.grid" );
		CHECK( GladderGridIndexer::GetGridFilePath( "maps/c1a0.bsp" ) == "maps/grid/c1a0.grid" );
		CHECK( GladderGridIndexer::GetGridFilePath( "custom\\boot_camp.bsp" ) == "maps/grid/boot_camp.grid" );

		g_engfuncs.pfnGetGameDir = originalGetGameDir;
	}

	SECTION( "Path formatting with custom engine game directory" )
	{
		auto originalGetGameDir = g_engfuncs.pfnGetGameDir;
		g_engfuncs.pfnGetGameDir = []( char *szGetGameDir ) {
			strcpy( szGetGameDir, "E:/Half-Life/gladder" );
		};

		CHECK( GladderGridIndexer::GetGridFilePath( "c1a0" ) == "E:/Half-Life/gladder/maps/grid/c1a0.grid" );
		CHECK( GladderGridIndexer::GetGridFilePath( "gl_01" ) == "E:/Half-Life/gladder/maps/grid/gl_01.grid" );

		g_engfuncs.pfnGetGameDir = originalGetGameDir;
	}
}

TEST_CASE( "Gladder Grid Indexer: Binary serialization & cache round-trip", "[gladder][grid]" )
{
	const char *testFile = "test_grid_cache.grid";
	std::remove( testFile );

	GladderGridIndexer writer;
	GladderGridCell cell1;
	cell1.origin   = Vector( 128.0f, 256.0f, 32.0f );
	cell1.normal   = Vector( 0.0f, 0.0f, 1.0f );
	cell1.areaId   = 1;
	cell1.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK | GLADDER_CELL_CEILING_VALID;
	cell1.ceilingZ = 160.0f;
	writer.AddCell( cell1 );

	GladderGridCell cell2;
	cell2.origin   = Vector( 160.0f, 256.0f, 48.0f );
	cell2.normal   = Vector( 0.1f, 0.0f, 0.99f );
	cell2.areaId   = 2;
	cell2.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
	cell2.ceilingZ = 200.0f;
	writer.AddCell( cell2 );

	REQUIRE( writer.GetCellCount() == 2 );
	REQUIRE( writer.SaveToFile( testFile ) == true );

	// Read back via a fresh indexer
	GladderGridIndexer reader;
	REQUIRE( reader.LoadFromFile( testFile ) == true );
	REQUIRE( reader.GetCellCount() == 2 );
	REQUIRE( reader.IsLoaded() == true );

	const auto &rc1 = reader.GetCell( 0 );
	CHECK( rc1.origin.x == Catch::Approx( 128.0f ) );
	CHECK( rc1.origin.y == Catch::Approx( 256.0f ) );
	CHECK( rc1.origin.z == Catch::Approx( 32.0f ) );
	CHECK( rc1.normal.z == Catch::Approx( 1.0f ) );
	CHECK( rc1.areaId == 1 );
	CHECK( ( rc1.flags & GLADDER_CELL_VALID ) != 0 );
	CHECK( ( rc1.flags & GLADDER_CELL_CLEARANCE_OK ) != 0 );
	CHECK( ( rc1.flags & GLADDER_CELL_CEILING_VALID ) != 0 );
	CHECK( rc1.ceilingZ == Catch::Approx( 160.0f ) );

	const auto &rc2 = reader.GetCell( 1 );
	CHECK( rc2.origin.x == Catch::Approx( 160.0f ) );
	CHECK( rc2.origin.y == Catch::Approx( 256.0f ) );
	CHECK( rc2.origin.z == Catch::Approx( 48.0f ) );
	CHECK( rc2.areaId == 2 );
	CHECK( ( rc2.flags & GLADDER_CELL_CEILING_VALID ) == 0 );
	CHECK( rc2.ceilingZ == Catch::Approx( 200.0f ) );

	// Clean up temporary test file
	std::remove( testFile );
}

TEST_CASE( "Gladder Grid Indexer: Corrupted or invalid cache rejection", "[gladder][grid]" )
{
	const char *badFile = "test_bad_cache.grid";

	// 1. Non-existent file
	GladderGridIndexer indexer;
	CHECK( indexer.LoadFromFile( "does_not_exist_12345.grid" ) == false );

	// 2. Corrupt magic header
	FILE *f = fopen( badFile, "wb" );
	REQUIRE( f != nullptr );
	const char badMagic[8] = { 'B', 'A', 'D', 'M', 'A', 'G', 'I', 'C' };
	fwrite( badMagic, 1, 8, f );
	uint32_t version = 1;
	fwrite( &version, sizeof( version ), 1, f );
	fclose( f );

	CHECK( indexer.LoadFromFile( badFile ) == false );

	// 3. Truncated file (claims 10 cells, contains 0)
	f = fopen( badFile, "wb" );
	REQUIRE( f != nullptr );
	GladderGridHeader hdr;
	std::memset( &hdr, 0, sizeof( hdr ) );
	std::memcpy( hdr.magic, "GLADGRID", 8 );
	hdr.version   = 1;
	hdr.cellCount = 10;
	fwrite( &hdr, sizeof( hdr ), 1, f );
	fclose( f );

	CHECK( indexer.LoadFromFile( badFile ) == false );
	CHECK( indexer.GetCellCount() == 0 );

	std::remove( badFile );
}

TEST_CASE( "Gladder Grid Indexer: Spatial queries & area filtering", "[gladder][grid]" )
{
	GladderGridIndexer indexer;

	GladderGridCell c1;
	c1.origin = Vector( 100.0f, 100.0f, 0.0f );
	c1.areaId = 10;
	c1.flags  = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
	indexer.AddCell( c1 );

	GladderGridCell c2;
	c2.origin = Vector( 500.0f, 500.0f, 0.0f );
	c2.areaId = 20;
	c2.flags  = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
	indexer.AddCell( c2 );

	GladderGridCell c3;
	c3.origin = Vector( 120.0f, 100.0f, 0.0f );
	c3.areaId = 10;
	c3.flags  = GLADDER_CELL_VALID; // Missing clearance flag
	indexer.AddCell( c3 );

	// Test GetCellsForArea
	auto area10Cells = indexer.GetCellsForArea( 10 );
	CHECK( area10Cells.size() == 2 );

	auto area20Cells = indexer.GetCellsForArea( 20 );
	CHECK( area20Cells.size() == 1 );

	auto area99Cells = indexer.GetCellsForArea( 99 );
	CHECK( area99Cells.empty() );

	// Test FindNearestCell
	const auto *pNearest = indexer.FindNearestCell( Vector( 105.0f, 102.0f, 0.0f ), 50.0f );
	REQUIRE( pNearest != nullptr );
	CHECK( pNearest->origin.x == Catch::Approx( 100.0f ) );

	// Too far search returns nullptr
	CHECK( indexer.FindNearestCell( Vector( 1000.0f, 1000.0f, 0.0f ), 50.0f ) == nullptr );

	// Test GetRandomCell with required clearance flag
	const auto *pRandomClear = indexer.GetRandomCell( 10, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK );
	REQUIRE( pRandomClear != nullptr );
	CHECK( pRandomClear->origin.x == Catch::Approx( 100.0f ) ); // Only c1 matches in area 10
}

// Custom trace function mocks for testing raycasting logic
static TraceResult s_mockDownwardTrace;
static TraceResult s_mockCeilingTrace;
static TraceResult s_mockHullTrace;
static int s_traceLineCallCount = 0;

static void CustomTraceLine( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	s_traceLineCallCount++;
	if ( ptr )
	{
		// Downward trace (v1.z > v2.z) vs Upward ceiling trace (v2.z > v1.z)
		if ( v1[2] > v2[2] )
		{
			*ptr = s_mockDownwardTrace;
			ptr->vecEndPos[0] = v2[0];
			ptr->vecEndPos[1] = v2[1];
			ptr->vecEndPos[2] = s_mockDownwardTrace.vecEndPos[2];
		}
		else
		{
			*ptr = s_mockCeilingTrace;
			ptr->vecEndPos[0] = v2[0];
			ptr->vecEndPos[1] = v2[1];
			ptr->vecEndPos[2] = s_mockCeilingTrace.vecEndPos[2];
		}
	}
}

static void CustomTraceHull( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr )
{
	if ( ptr )
	{
		*ptr = s_mockHullTrace;
	}
}

TEST_CASE( "Gladder Grid Indexer: TraceCellCandidate raycasting verification", "[gladder][grid]" )
{
	ResetMockEngine();

	// Hook custom engine trace functions
	auto originalTraceLine = g_engfuncs.pfnTraceLine;
	auto originalTraceHull = g_engfuncs.pfnTraceHull;
	g_engfuncs.pfnTraceLine = CustomTraceLine;
	g_engfuncs.pfnTraceHull = CustomTraceHull;

	GladderGridIndexer indexer;
	GladderGridCell outCell;

	SECTION( "Valid flat surface with adequate clearance produces valid cell" )
	{
		s_traceLineCallCount = 0;

		std::memset( &s_mockDownwardTrace, 0, sizeof( s_mockDownwardTrace ) );
		s_mockDownwardTrace.flFraction      = 0.5f;
		s_mockDownwardTrace.vecPlaneNormal  = Vector( 0.0f, 0.0f, 1.0f );
		s_mockDownwardTrace.vecEndPos       = Vector( 100.0f, 200.0f, 50.0f );
		s_mockDownwardTrace.fStartSolid     = 0;
		s_mockDownwardTrace.fAllSolid       = 0;

		std::memset( &s_mockCeilingTrace, 0, sizeof( s_mockCeilingTrace ) );
		s_mockCeilingTrace.flFraction       = 0.3f;
		s_mockCeilingTrace.vecEndPos        = Vector( 100.0f, 200.0f, 200.0f ); // Clearance: 150 > 72

		std::memset( &s_mockHullTrace, 0, sizeof( s_mockHullTrace ) );
		s_mockHullTrace.flFraction          = 1.0f;
		s_mockHullTrace.fStartSolid         = 0;
		s_mockHullTrace.fAllSolid           = 0;

		bool ok = indexer.TraceCellCandidate( 100.0f, 200.0f, 500.0f, -100.0f, 42, outCell );
		REQUIRE( ok == true );
		CHECK( outCell.origin.x == Catch::Approx( 100.0f ) );
		CHECK( outCell.origin.y == Catch::Approx( 200.0f ) );
		CHECK( outCell.origin.z == Catch::Approx( 51.0f ) ); // surface + 1.0f offset
		CHECK( outCell.areaId == 42 );
		CHECK( ( outCell.flags & GLADDER_CELL_VALID ) != 0 );
		CHECK( ( outCell.flags & GLADDER_CELL_CLEARANCE_OK ) != 0 );
		CHECK( ( outCell.flags & GLADDER_CELL_CEILING_VALID ) != 0 );
		CHECK( outCell.ceilingZ == Catch::Approx( 200.0f ) );
	}

	SECTION( "Steep slope surface is rejected (slope normal.z < 0.7)" )
	{
		std::memset( &s_mockDownwardTrace, 0, sizeof( s_mockDownwardTrace ) );
		s_mockDownwardTrace.flFraction     = 0.5f;
		s_mockDownwardTrace.vecPlaneNormal = Vector( 0.0f, 0.8f, 0.6f ); // 0.6 < 0.7
		s_mockDownwardTrace.vecEndPos      = Vector( 100.0f, 200.0f, 50.0f );

		bool ok = indexer.TraceCellCandidate( 100.0f, 200.0f, 500.0f, -100.0f, 0, outCell );
		CHECK( ok == false );
	}

	SECTION( "Insufficient vertical clearance is rejected (clearance < 72)" )
	{
		std::memset( &s_mockDownwardTrace, 0, sizeof( s_mockDownwardTrace ) );
		s_mockDownwardTrace.flFraction     = 0.5f;
		s_mockDownwardTrace.vecPlaneNormal = Vector( 0.0f, 0.0f, 1.0f );
		s_mockDownwardTrace.vecEndPos      = Vector( 100.0f, 200.0f, 50.0f );

		// Ceiling only 40 units above floor (50 + 40 = 90)
		std::memset( &s_mockCeilingTrace, 0, sizeof( s_mockCeilingTrace ) );
		s_mockCeilingTrace.flFraction      = 0.1f;
		s_mockCeilingTrace.vecEndPos       = Vector( 100.0f, 200.0f, 90.0f );

		bool ok = indexer.TraceCellCandidate( 100.0f, 200.0f, 500.0f, -100.0f, 0, outCell );
		CHECK( ok == false );
	}

	SECTION( "Standing hull collision obstruction is rejected" )
	{
		std::memset( &s_mockDownwardTrace, 0, sizeof( s_mockDownwardTrace ) );
		s_mockDownwardTrace.flFraction     = 0.5f;
		s_mockDownwardTrace.vecPlaneNormal = Vector( 0.0f, 0.0f, 1.0f );
		s_mockDownwardTrace.vecEndPos      = Vector( 100.0f, 200.0f, 50.0f );

		std::memset( &s_mockCeilingTrace, 0, sizeof( s_mockCeilingTrace ) );
		s_mockCeilingTrace.flFraction      = 0.5f;
		s_mockCeilingTrace.vecEndPos       = Vector( 100.0f, 200.0f, 300.0f );

		// Standing hull stuck in solid brush
		std::memset( &s_mockHullTrace, 0, sizeof( s_mockHullTrace ) );
		s_mockHullTrace.fStartSolid        = 1;
		s_mockHullTrace.flFraction         = 0.0f;

		bool ok = indexer.TraceCellCandidate( 100.0f, 200.0f, 500.0f, -100.0f, 0, outCell );
		CHECK( ok == false );
	}

	SECTION( "Start point inside solid ceiling steps downward into open air" )
	{
		auto originalPointContents = g_engfuncs.pfnPointContents;
		g_engfuncs.pfnPointContents = []( const float *p ) -> int {
			// Simulate solid ceiling above Z = 450
			if ( p[2] >= 450.0f )
				return -1; // CONTENTS_SOLID
			return 0;      // CONTENTS_EMPTY
		};

		std::memset( &s_mockDownwardTrace, 0, sizeof( s_mockDownwardTrace ) );
		s_mockDownwardTrace.flFraction      = 0.5f;
		s_mockDownwardTrace.vecPlaneNormal  = Vector( 0.0f, 0.0f, 1.0f );
		s_mockDownwardTrace.vecEndPos       = Vector( 100.0f, 200.0f, 50.0f );
		s_mockDownwardTrace.fStartSolid     = 0;
		s_mockDownwardTrace.fAllSolid       = 0;

		std::memset( &s_mockCeilingTrace, 0, sizeof( s_mockCeilingTrace ) );
		s_mockCeilingTrace.flFraction       = 0.3f;
		s_mockCeilingTrace.vecEndPos        = Vector( 100.0f, 200.0f, 200.0f );

		std::memset( &s_mockHullTrace, 0, sizeof( s_mockHullTrace ) );
		s_mockHullTrace.flFraction          = 1.0f;
		s_mockHullTrace.fStartSolid         = 0;
		s_mockHullTrace.fAllSolid           = 0;

		// zTop = 500 (inside ceiling >= 450), steps down below 450 and finds floor
		bool ok = indexer.TraceCellCandidate( 100.0f, 200.0f, 500.0f, -100.0f, 1, outCell );
		CHECK( ok == true );
		CHECK( outCell.origin.z == Catch::Approx( 51.0f ) );

		g_engfuncs.pfnPointContents = originalPointContents;
	}

	// Restore original engine callbacks
	g_engfuncs.pfnTraceLine = originalTraceLine;
	g_engfuncs.pfnTraceHull = originalTraceHull;
}

TEST_CASE( "Gladder Grid Indexer: trigger_gladder_area volumetric bounds & netname", "[gladder][grid]" )
{
	ResetMockEngine();

	CTriggerGladderArea area;
	entvars_t localPev;
	std::memset( &localPev, 0, sizeof( localPev ) );
	area.pev = &localPev;

	// Test netname KeyValue handling
	KeyValueData kvdNetname;
	std::memset( &kvdNetname, 0, sizeof( kvdNetname ) );
	kvdNetname.szKeyName = const_cast<char *>( "netname" );
	kvdNetname.szValue   = const_cast<char *>( "Sector 1 - Pasillo Central" );
	area.KeyValue( &kvdNetname );
	CHECK( kvdNetname.fHandled == TRUE );
	CHECK( std::string( area.GetAreaName() ) == "Sector 1 - Pasillo Central" );

	// Test areaid KeyValue handling
	KeyValueData kvdId;
	std::memset( &kvdId, 0, sizeof( kvdId ) );
	kvdId.szKeyName = const_cast<char *>( "areaid" );
	kvdId.szValue   = const_cast<char *>( "42" );
	area.KeyValue( &kvdId );
	CHECK( kvdId.fHandled == TRUE );
	CHECK( area.GetAreaId() == 42 );

	// Test volumetric bounds calculation (origin: 576, -260, -12; size: 904, 406, 142)
	area.pev->origin = Vector( 576.0f, -260.0f, -12.0f );
	area.pev->mins   = Vector( -452.0f, -203.0f, -71.0f );
	area.pev->maxs   = Vector( 452.0f, 203.0f, 71.0f );

	Vector mins = area.GetMins();
	Vector maxs = area.GetMaxs();

	CHECK( mins.x == Catch::Approx( 124.0f ) );
	CHECK( mins.y == Catch::Approx( -463.0f ) );
	CHECK( mins.z == Catch::Approx( -83.0f ) );

	CHECK( maxs.x == Catch::Approx( 1028.0f ) );
	CHECK( maxs.y == Catch::Approx( -57.0f ) );
	CHECK( maxs.z == Catch::Approx( 59.0f ) );
}

TEST_CASE( "Gladder Grid Indexer: GetRandomCellIndex occupancy filtering", "[gladder][grid]" )
{
	GladderGridIndexer indexer;
	for ( int i = 0; i < 5; ++i )
	{
		GladderGridCell c;
		c.origin   = Vector( i * 32.0f, 0.0f, 0.0f );
		c.normal   = Vector( 0.0f, 0.0f, 1.0f );
		c.areaId   = 1;
		c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
		c.ceilingZ = 120.0f;
		indexer.AddCell( c );
	}

	REQUIRE( indexer.GetCellCount() == 5 );

	// Unoccupied query returns valid index in [0, 4]
	int idx = indexer.GetRandomCellIndex( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK, nullptr );
	CHECK( idx >= 0 );
	CHECK( idx < 5 );

	// Mark all except index 3 as occupied
	std::vector<bool> occupied( 5, true );
	occupied[3] = false;

	int chosen = indexer.GetRandomCellIndex( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK, &occupied );
	CHECK( chosen == 3 ); // Exactly the only free cell remaining!

	// Mark index 3 as occupied too -> all cells occupied!
	occupied[3] = true;
	int noneLeft = indexer.GetRandomCellIndex( -1, GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK, &occupied );
	CHECK( noneLeft == -1 ); // Gracefully returns -1
}

TEST_CASE( "Gladder Grid Indexer: TraceCellCandidate rejects breakable crates as walking surface", "[gladder][grid]" )
{
	ResetMockEngine();

	// Hook custom engine trace functions
	auto originalTraceLine = g_engfuncs.pfnTraceLine;
	auto originalTraceHull = g_engfuncs.pfnTraceHull;
	g_engfuncs.pfnTraceLine = CustomTraceLine;
	g_engfuncs.pfnTraceHull = CustomTraceHull;

	GladderGridIndexer indexer;
	GladderGridCell outCell;

	// Mock breakable crate hit
	edict_t edBreakable;
	std::memset( &edBreakable, 0, sizeof( edBreakable ) );
	edBreakable.v.classname = MAKE_STRING( "func_breakable" );

	std::memset( &s_mockDownwardTrace, 0, sizeof( s_mockDownwardTrace ) );
	s_mockDownwardTrace.flFraction      = 0.5f;
	s_mockDownwardTrace.vecPlaneNormal  = Vector( 0.0f, 0.0f, 1.0f );
	s_mockDownwardTrace.vecEndPos       = Vector( 100.0f, 200.0f, 50.0f );
	s_mockDownwardTrace.pHit            = &edBreakable; // Ray hit top of a breakable box!

	std::memset( &s_mockCeilingTrace, 0, sizeof( s_mockCeilingTrace ) );
	s_mockCeilingTrace.flFraction       = 0.3f;
	s_mockCeilingTrace.vecEndPos        = Vector( 100.0f, 200.0f, 200.0f );

	std::memset( &s_mockHullTrace, 0, sizeof( s_mockHullTrace ) );
	s_mockHullTrace.flFraction          = 1.0f;
	s_mockHullTrace.fStartSolid         = 0;

	// Candidate on top of a breakable box must be rejected!
	bool ok = indexer.TraceCellCandidate( 100.0f, 200.0f, 500.0f, -100.0f, 1, outCell );
	CHECK( ok == false );

	// Restore original engine callbacks
	g_engfuncs.pfnTraceLine = originalTraceLine;
	g_engfuncs.pfnTraceHull = originalTraceHull;
}

// Test mock stubs for hl_tests linkage
void UTIL_TraceHull( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr )
{
	if ( ptr )
	{
		if ( g_engfuncs.pfnTraceHull )
			g_engfuncs.pfnTraceHull( vecStart, vecEnd, igmon, hullNumber, pentIgnore, ptr );
		else
			*ptr = g_mockTraceResult;
	}
}

CBaseEntity *UTIL_FindEntityByClassname( CBaseEntity *pStartEntity, const char *szName )
{
	if ( !szName || !gpGlobals )
		return nullptr;

	int startIndex = 1;
	if ( pStartEntity && pStartEntity->edict() )
	{
		startIndex = ENTINDEX( pStartEntity->edict() ) + 1;
	}

	for ( int i = startIndex; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !pEdict || pEdict->free )
			continue;
		if ( FClassnameIs( &pEdict->v, szName ) )
		{
			return CBaseEntity::Instance( pEdict );
		}
	}
	return nullptr;
}
