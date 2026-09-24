/***
 *
 *	Behavioral Equivalence Verification - AI Navigation & Graph Node Routing Unit Tests
 *	Verifies CGraph, FindNearestNode, HashSearch, FindShortestPath, and Monster Navigation Safety
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"

#include <cstring>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "tests/mock_engine.h"
#include "dlls/ai/nodes.h"
#include "dlls/ai/basemonster.h"
#include "dlls/world/rotating.h"
#include "dlls/monsters/apache.h"
#include "pm_shared/pm_shared.h"

extern CGraph WorldGraph;

TEST_CASE( "CGraph: uninitialized and empty graph queries return safe defaults", "[ai][navigation][cgraph]" )
{
	WorldGraph.InitGraph();

	SECTION( "FindNearestNode on uninitialized graph" )
	{
		CHECK( WorldGraph.FindNearestNode( Vector( 0, 0, 0 ), bits_NODE_LAND ) == -1 );
		CHECK( WorldGraph.FindNearestNode( Vector( 100, -200, 50 ), (CBaseEntity *)NULL ) == -1 );
	}

	SECTION( "FindShortestPath on uninitialized graph" )
	{
		int path[36];
		CHECK( WorldGraph.FindShortestPath( path, 0, 1, NODE_HUMAN_HULL, 0 ) == 0 );
		CHECK( WorldGraph.FindShortestPath( path, -1, 5, NODE_HUMAN_HULL, 0 ) == 0 );
		CHECK( WorldGraph.FindShortestPath( path, 0, 100, NODE_HUMAN_HULL, 0 ) == 0 );
	}

	SECTION( "HashSearch and HashInsert with zero/null table" )
	{
		int key = -999;
		WorldGraph.HashSearch( 0, 1, key );
		CHECK( key == ENTRY_STATE_EMPTY );

		// HashInsert should gracefully do nothing without dividing by zero
		WorldGraph.HashInsert( 0, 1, 5 );
	}

	SECTION( "NextNodeInRoute and PathLength with uninitialized graph" )
	{
		CHECK( WorldGraph.NextNodeInRoute( 0, 1, NODE_HUMAN_HULL, 0 ) == 0 );
		CHECK( WorldGraph.NextNodeInRoute( -1, 10, NODE_HUMAN_HULL, 0 ) == -1 );
		CHECK( WorldGraph.PathLength( 0, 1, NODE_HUMAN_HULL, 0 ) == 0.0f );
	}
}

TEST_CASE( "CGraph: HullIndex and NodeType null safety", "[ai][navigation][cgraph]" )
{
	CHECK( WorldGraph.HullIndex( NULL ) == NODE_HUMAN_HULL );
	CHECK( WorldGraph.NodeType( NULL ) == bits_NODE_LAND );

	edict_t ent;
	memset( &ent, 0, sizeof( ent ) );

	CBaseEntity dummy;
	dummy.pev = &ent.v;

	SECTION( "Fly monster detection" )
	{
		dummy.pev->movetype = MOVETYPE_FLY;
		dummy.pev->waterlevel = 0;
		CHECK( WorldGraph.HullIndex( &dummy ) == NODE_FLY_HULL );
		CHECK( WorldGraph.NodeType( &dummy ) == bits_NODE_AIR );

		dummy.pev->waterlevel = 1;
		CHECK( WorldGraph.NodeType( &dummy ) == bits_NODE_WATER );
	}

	SECTION( "Hull sizes detection" )
	{
		dummy.pev->movetype = MOVETYPE_WALK;
		dummy.pev->mins = Vector( -12, -12, 0 );
		CHECK( WorldGraph.HullIndex( &dummy ) == NODE_SMALL_HULL );

		dummy.pev->mins = VEC_HUMAN_HULL_MIN;
		CHECK( WorldGraph.HullIndex( &dummy ) == NODE_HUMAN_HULL );

		dummy.pev->mins = Vector( -32, -32, 0 );
		CHECK( WorldGraph.HullIndex( &dummy ) == NODE_LARGE_HULL );
	}
}

TEST_CASE( "CGraph: HashInsert and HashSearch operations", "[ai][navigation][hash]" )
{
	WorldGraph.InitGraph();

	WorldGraph.m_nHashLinks = 37;
	WorldGraph.m_pHashLinks = (short *)calloc( sizeof( short ), WorldGraph.m_nHashLinks );
	for ( int i = 0; i < WorldGraph.m_nHashLinks; ++i )
	{
		WorldGraph.m_pHashLinks[i] = ENTRY_STATE_EMPTY;
	}

	for ( int i = 0; i < 16; ++i )
	{
		WorldGraph.m_HashPrimes[i] = 1;
	}

	WorldGraph.m_cLinks = 4;
	WorldGraph.m_pLinkPool = (CLink *)calloc( sizeof( CLink ), WorldGraph.m_cLinks );
	WorldGraph.m_pLinkPool[0].m_iSrcNode = 0;
	WorldGraph.m_pLinkPool[0].m_iDestNode = 1;
	WorldGraph.m_pLinkPool[1].m_iSrcNode = 1;
	WorldGraph.m_pLinkPool[1].m_iDestNode = 2;
	WorldGraph.m_pLinkPool[2].m_iSrcNode = 2;
	WorldGraph.m_pLinkPool[2].m_iDestNode = 3;
	WorldGraph.m_pLinkPool[3].m_iSrcNode = 1;
	WorldGraph.m_pLinkPool[3].m_iDestNode = 0;

	WorldGraph.HashInsert( 0, 1, 0 );
	WorldGraph.HashInsert( 1, 2, 1 );
	WorldGraph.HashInsert( 2, 3, 2 );
	WorldGraph.HashInsert( 1, 0, 3 );

	SECTION( "Search existing entries" )
	{
		int key = ENTRY_STATE_EMPTY;
		WorldGraph.HashSearch( 0, 1, key );
		CHECK( key == 0 );

		WorldGraph.HashSearch( 1, 2, key );
		CHECK( key == 1 );

		WorldGraph.HashSearch( 2, 3, key );
		CHECK( key == 2 );

		WorldGraph.HashSearch( 1, 0, key );
		CHECK( key == 3 );
	}

	SECTION( "Search non-existent entries" )
	{
		int key = -999;
		WorldGraph.HashSearch( 0, 3, key );
		CHECK( key == ENTRY_STATE_EMPTY );

		WorldGraph.HashSearch( 3, 0, key );
		CHECK( key == ENTRY_STATE_EMPTY );
	}

	WorldGraph.InitGraph();
}

TEST_CASE( "CGraph: BuildRegionTables and FindNearestNode with > 255 nodes", "[ai][navigation][spatial]" )
{
	WorldGraph.InitGraph();

	const int kTestNodes = 300;
	WorldGraph.m_cNodes = kTestNodes;
	WorldGraph.m_pNodes = (CNode *)calloc( sizeof( CNode ), kTestNodes );

	for ( int i = 0; i < kTestNodes; ++i )
	{
		WorldGraph.m_pNodes[i].m_vecOrigin = Vector( (float)( i * 10 ), (float)( ( i % 10 ) * 20 ), 0.0f );
		WorldGraph.m_pNodes[i].m_vecOriginPeek = WorldGraph.m_pNodes[i].m_vecOrigin;
		WorldGraph.m_pNodes[i].m_afNodeInfo = bits_NODE_LAND;
	}

	WorldGraph.BuildRegionTables();
	WorldGraph.m_fGraphPresent = TRUE;
	WorldGraph.m_fGraphPointersSet = TRUE;

	SECTION( "Range start and range end sentinels are valid" )
	{
		for ( int axis = 0; axis < 3; ++axis )
		{
			for ( int r = 0; r < NUM_RANGES; ++r )
			{
				if ( WorldGraph.m_RangeStart[axis][r] != MAX_NODES + 1 )
				{
					CHECK( WorldGraph.m_RangeStart[axis][r] >= 0 );
					CHECK( WorldGraph.m_RangeStart[axis][r] < kTestNodes );
					CHECK( WorldGraph.m_RangeEnd[axis][r] >= WorldGraph.m_RangeStart[axis][r] );
					CHECK( WorldGraph.m_RangeEnd[axis][r] < kTestNodes );
				}
			}
		}
	}

	SECTION( "Find nearest node correctly resolves nearest coordinate" )
	{
		// Searching near node 0
		int nearest0 = WorldGraph.FindNearestNode( Vector( 2.0f, 1.0f, 0.0f ), bits_NODE_LAND );
		CHECK( nearest0 >= 0 );
		CHECK( nearest0 < kTestNodes );

		// Searching near node 280 (exceeding legacy 255 boundary)
		Vector target280 = WorldGraph.m_pNodes[280].m_vecOrigin;
		int nearest280 = WorldGraph.FindNearestNode( target280 + Vector( 1.0f, 1.0f, 0.0f ), bits_NODE_LAND );
		CHECK( nearest280 == 280 );
	}

	WorldGraph.InitGraph();
}

TEST_CASE( "CGraph: FindShortestPath bounds validation and unreachable targets", "[ai][navigation][path]" )
{
	WorldGraph.InitGraph();

	const int kNumNodes = 5;
	WorldGraph.m_cNodes = kNumNodes;
	WorldGraph.m_pNodes = (CNode *)calloc( sizeof( CNode ), kNumNodes );
	for ( int i = 0; i < kNumNodes; ++i )
	{
		WorldGraph.m_pNodes[i].m_vecOrigin = Vector( (float)( i * 100 ), 0, 0 );
		WorldGraph.m_pNodes[i].m_afNodeInfo = bits_NODE_LAND;
	}

	WorldGraph.m_fGraphPresent = TRUE;
	WorldGraph.m_fGraphPointersSet = TRUE;
	WorldGraph.m_fRoutingComplete = FALSE; // Use Dijkstra mode

	int path[36];
	memset( path, 0, sizeof( path ) );

	SECTION( "Out of range start and dest" )
	{
		CHECK( WorldGraph.FindShortestPath( path, -1, 2, NODE_HUMAN_HULL, 0 ) == 0 );
		CHECK( WorldGraph.FindShortestPath( path, 0, 10, NODE_HUMAN_HULL, 0 ) == 0 );
		CHECK( WorldGraph.FindShortestPath( path, 5, 0, NODE_HUMAN_HULL, 0 ) == 0 );
	}

	SECTION( "Same start and dest" )
	{
		int res = WorldGraph.FindShortestPath( path, 2, 2, NODE_HUMAN_HULL, 0 );
		CHECK( res == 2 );
		CHECK( path[0] == 2 );
		CHECK( path[1] == 2 );
	}

	SECTION( "Unreachable destination (disconnected graph)" )
	{
		int res = WorldGraph.FindShortestPath( path, 0, 3, NODE_HUMAN_HULL, 0 );
		CHECK( res == 0 );
	}

	WorldGraph.InitGraph();
}

TEST_CASE( "CGraph: FindShortestPath never overflows MAX_PATH_SIZE in routing mode", "[ai][navigation][path]" )
{
	WorldGraph.InitGraph();

	const int kNumNodes = 25;
	WorldGraph.m_cNodes = kNumNodes;
	WorldGraph.m_pNodes = (CNode *)calloc( sizeof( CNode ), kNumNodes );
	for ( int i = 0; i < kNumNodes; ++i )
	{
		WorldGraph.m_pNodes[i].m_vecOrigin = Vector( (float)( i * 50 ), 0, 0 );
		WorldGraph.m_pNodes[i].m_afNodeInfo = bits_NODE_LAND;
		// Each node points to offset 0 in route info
		for ( int h = 0; h < MAX_NODE_HULLS; ++h )
		{
			WorldGraph.m_pNodes[i].m_pNextBestNode[h][0] = 0;
			WorldGraph.m_pNodes[i].m_pNextBestNode[h][1] = 0;
		}
	}

	// Route info that says: for all destinations, next node is iCurrentNode + 1
	// Repeat phrase: count = 126 (byte 125), delta = +1
	char routeData[4] = { 125, 1, 0, 0 };
	WorldGraph.m_nRouteInfo = sizeof( routeData );
	WorldGraph.m_pRouteInfo = (char *)malloc( sizeof( routeData ) );
	memcpy( WorldGraph.m_pRouteInfo, routeData, sizeof( routeData ) );

	WorldGraph.m_fGraphPresent = TRUE;
	WorldGraph.m_fGraphPointersSet = TRUE;
	WorldGraph.m_fRoutingComplete = TRUE;

	// Stack buffer matching FGetNodeRoute with boundary canaries
	struct StackGuard
	{
		int canaryBefore = 0xDEADBEEF;
		int pathBuffer[MAX_PATH_SIZE];
		int canaryAfter = 0xCAFEBABE;
	} guard;

	for ( int i = 0; i < MAX_PATH_SIZE; ++i )
		guard.pathBuffer[i] = -1;

	// Find path from 0 to 24 (24 hops, which far exceeds MAX_PATH_SIZE = 10)
	int result = WorldGraph.FindShortestPath( guard.pathBuffer, 0, 24, NODE_HUMAN_HULL, 0 );

	// Must be capped at MAX_PATH_SIZE (10)
	CHECK( result == MAX_PATH_SIZE );
	// Stack canaries MUST NOT be corrupted
	CHECK( guard.canaryBefore == 0xDEADBEEF );
	CHECK( guard.canaryAfter == 0xCAFEBABE );

	WorldGraph.InitGraph();
}

TEST_CASE( "CGraph: FindShortestPath never overflows MAX_PATH_SIZE in dynamic Dijkstra mode (!m_fRoutingComplete)", "[ai][navigation][path]" )
{
	WorldGraph.InitGraph();

	const int kNumNodes = 25;
	WorldGraph.m_cNodes = kNumNodes;
	WorldGraph.m_pNodes = (CNode *)calloc( sizeof( CNode ), kNumNodes );
	for ( int i = 0; i < kNumNodes; ++i )
	{
		WorldGraph.m_pNodes[i].m_vecOrigin = Vector( (float)( i * 50 ), 0, 0 );
		WorldGraph.m_pNodes[i].m_afNodeInfo = bits_NODE_LAND;
		WorldGraph.m_pNodes[i].m_cNumLinks  = ( i < kNumNodes - 1 ) ? 1 : 0;
		WorldGraph.m_pNodes[i].m_iFirstLink = i;
	}

	WorldGraph.m_cLinks    = kNumNodes - 1;
	WorldGraph.m_pLinkPool = (CLink *)calloc( sizeof( CLink ), WorldGraph.m_cLinks );
	for ( int i = 0; i < WorldGraph.m_cLinks; ++i )
	{
		WorldGraph.m_pLinkPool[i].m_iSrcNode   = i;
		WorldGraph.m_pLinkPool[i].m_iDestNode  = i + 1;
		WorldGraph.m_pLinkPool[i].m_afLinkInfo = bits_LINK_HUMAN_HULL | bits_LINK_SMALL_HULL | bits_LINK_LARGE_HULL;
		WorldGraph.m_pLinkPool[i].m_flWeight   = 50.0f;
		WorldGraph.m_pLinkPool[i].m_pLinkEnt   = NULL;
	}

	WorldGraph.m_fGraphPresent     = TRUE;
	WorldGraph.m_fGraphPointersSet = TRUE;
	WorldGraph.m_fRoutingComplete  = FALSE; // Dynamic Dijkstra mode

	struct StackGuard
	{
		int canaryBefore = 0xDEADBEEF;
		int pathBuffer[MAX_PATH_SIZE];
		int canaryAfter = 0xCAFEBABE;
	} guard;

	for ( int i = 0; i < MAX_PATH_SIZE; ++i )
		guard.pathBuffer[i] = -1;

	// Path from 0 to 24 (24 hops) must be capped to MAX_PATH_SIZE = 10
	int result = WorldGraph.FindShortestPath( guard.pathBuffer, 0, 24, NODE_HUMAN_HULL, 0 );

	CHECK( result == MAX_PATH_SIZE );
	CHECK( guard.canaryBefore == 0xDEADBEEF );
	CHECK( guard.canaryAfter == 0xCAFEBABE );

	for ( int i = 0; i < MAX_PATH_SIZE; ++i )
	{
		CHECK( guard.pathBuffer[i] == i );
	}

	WorldGraph.InitGraph();
}

TEST_CASE( "Spawnflags and spectator constants match canonical GoldSrc values", "[ai][spawnflags][constants]" )
{
	SECTION( "Rotating and pendulum spawnflags" )
	{
		CHECK( SF_BRUSH_ROTATE_INSTANT == 1 );
		CHECK( SF_BRUSH_ROTATE_START_ON == 1 );
		CHECK( SF_PENDULUM_PASSABLE == 32 );
	}

	SECTION( "Apache helicopter spawnflags" )
	{
		CHECK( SF_WAITFORTRIGGER == ( 0x04 | 0x40 ) );
		CHECK( SF_NOWRECKAGE == 0x08 );
	}

	SECTION( "Spectator modes" )
	{
		CHECK( OBS_ROAMING == 3 );
	}
}
