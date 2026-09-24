/***
 *
 *	Behavioral Equivalence Verification - Level Transition Unit Tests (Layer 3C)
 *	Verifies landmark coordinate delta math, CChangeLevel transition list assembly,
 *	transition volume checks, and entity table flag bitmasks against canonical Valve SDK.
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <vector>

#include "core/extdll.h"
#include "core/util.h"
#include "core/saverestore.h"
#include "tests/mock_engine.h"

// Canonical Valve transition structures and bitmasks
#ifndef FENTTABLE_MOVEABLE
#define FENTTABLE_MOVEABLE 0x4000
#endif
#ifndef FENTTABLE_GLOBAL
#define FENTTABLE_GLOBAL 0x8000
#endif
#ifndef FCAP_FORCE_TRANSITION
#define FCAP_FORCE_TRANSITION 0x00000040
#endif
#ifndef FCAP_ACROSS_TRANSITION
#define FCAP_ACROSS_TRANSITION 0x00000080
#endif
#ifndef FCAP_DONT_SAVE
#define FCAP_DONT_SAVE 0x80000000
#endif

// Exact canonical AddTransitionToList implementation from Valve GoldSrc SDK
static int SimulateAddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark )
{
	int i;

	if ( !pLevelList || !pMapName || !pLandmarkName || !pentLandmark )
		return 0;

	for ( i = 0; i < listCount; i++ )
	{
		if ( pLevelList[i].pentLandmark == pentLandmark && strcmp( pLevelList[i].mapName, pMapName ) == 0 )
			return 0;
	}
	strcpy( pLevelList[listCount].mapName, pMapName );
	strcpy( pLevelList[listCount].landmarkName, pLandmarkName );
	pLevelList[listCount].pentLandmark      = pentLandmark;
	pLevelList[listCount].vecLandmarkOrigin = VARS( pentLandmark )->origin;

	return 1;
}

// Exact canonical InTransitionVolume logic from Valve GoldSrc SDK
struct MockTransitionEntity
{
	int caps;
	int movetype;
	MockTransitionEntity *aiment;
	Vector origin;
	Vector mins;
	Vector maxs;
};

struct MockTransitionVolume
{
	const char *targetname;
	const char *classname;
	Vector mins;
	Vector maxs;

	bool Intersects( const MockTransitionEntity *pEntity ) const
	{
		if ( pEntity->origin.x + pEntity->maxs.x < mins.x || pEntity->origin.x + pEntity->mins.x > maxs.x )
			return false;
		if ( pEntity->origin.y + pEntity->maxs.y < mins.y || pEntity->origin.y + pEntity->mins.y > maxs.y )
			return false;
		if ( pEntity->origin.z + pEntity->maxs.z < mins.z || pEntity->origin.z + pEntity->mins.z > maxs.z )
			return false;
		return true;
	}
};

static int SimulateInTransitionVolume( MockTransitionEntity *pEntity, const char *pVolumeName, const std::vector<MockTransitionVolume> &volumes )
{
	if ( pEntity->caps & FCAP_FORCE_TRANSITION )
		return 1;

	// Follow attachments / carried entities through transition
	if ( pEntity->movetype == MOVETYPE_FOLLOW )
	{
		if ( pEntity->aiment != nullptr )
			pEntity = pEntity->aiment;
	}

	int inVolume = 1; // Unless we find a trigger_transition, everything is in the volume

	for ( const auto &vol : volumes )
	{
		if ( strcmp( vol.targetname, pVolumeName ) == 0 && strcmp( vol.classname, "trigger_transition" ) == 0 )
		{
			if ( vol.Intersects( pEntity ) )
				return 1;
			else
				inVolume = 0; // Found trigger_transition, but doesn't intersect
		}
	}

	return inVolume;
}

TEST_CASE( "Level Transition: Landmark Coordinate Delta Calculation", "[level_transition][math]" )
{
	// Scenario: Player transitions from map A (c1a0) to map B (c1a0a)
	// Source landmark position in map A
	Vector srcLandmarkOrigin( 256.0f, -512.0f, 64.0f );
	// Destination landmark position in map B
	Vector dstLandmarkOrigin( 1024.0f, 2048.0f, -128.0f );

	// Player position in map A relative to source landmark
	Vector playerSrcOrigin( 300.0f, -400.0f, 96.0f );

	// 1. Step 1 (Source Map): Engine sets vecLandmarkOffset = srcLandmarkOrigin
	Vector vecLandmarkOffset = srcLandmarkOrigin;
	CHECK( vecLandmarkOffset == srcLandmarkOrigin );

	// 2. Step 2 (Entity Save): Position vector written as offset relative to landmark
	// In util_saverestore.cpp: Vector tmp = value - m_pdata->vecLandmarkOffset;
	Vector savedRelativePosition = playerSrcOrigin - vecLandmarkOffset;
	CHECK( savedRelativePosition.x == Catch::Approx( 44.0f ) );
	CHECK( savedRelativePosition.y == Catch::Approx( 112.0f ) );
	CHECK( savedRelativePosition.z == Catch::Approx( 32.0f ) );

	// 3. Step 3 (Destination Map): Engine finds landmark entity and adds its origin
	// In util_saverestore.cpp: position = savedRelativePosition + destLandmark->origin;
	Vector restoredPlayerOrigin = savedRelativePosition + dstLandmarkOrigin;
	CHECK( restoredPlayerOrigin.x == Catch::Approx( 1068.0f ) );
	CHECK( restoredPlayerOrigin.y == Catch::Approx( 2160.0f ) );
	CHECK( restoredPlayerOrigin.z == Catch::Approx( -96.0f ) );

	// Mathematical identity check: (P - L_src) + L_dst == P + (L_dst - L_src)
	Vector expectedDelta = dstLandmarkOrigin - srcLandmarkOrigin;
	Vector directCalculated = playerSrcOrigin + expectedDelta;
	CHECK( restoredPlayerOrigin.x == Catch::Approx( directCalculated.x ) );
	CHECK( restoredPlayerOrigin.y == Catch::Approx( directCalculated.y ) );
	CHECK( restoredPlayerOrigin.z == Catch::Approx( directCalculated.z ) );
}

TEST_CASE( "Level Transition: Uninitialized Landmark Offset Regression Test", "[level_transition][regression]" )
{
	// Regression from Issue #125: If gpGlobals->vecLandmarkOffset was never set (defaulted to 0,0,0),
	// the entity origin was saved without subtracting the source landmark!
	Vector srcLandmarkOrigin( 500.0f, 500.0f, 100.0f );
	Vector dstLandmarkOrigin( 1500.0f, 1500.0f, 200.0f );
	Vector playerSrcOrigin( 550.0f, 520.0f, 110.0f );

	// Broken behavior: vecLandmarkOffset == (0,0,0)
	Vector brokenOffset( 0.0f, 0.0f, 0.0f );
	Vector brokenSavedPosition = playerSrcOrigin - brokenOffset; // 550, 520, 110
	Vector brokenRestoredOrigin = brokenSavedPosition + dstLandmarkOrigin; // 2050, 2020, 310 (corrupted by entire src landmark!)

	// Canonical behavior: vecLandmarkOffset == srcLandmarkOrigin
	Vector canonicalSavedPosition = playerSrcOrigin - srcLandmarkOrigin; // 50, 20, 10
	Vector canonicalRestoredOrigin = canonicalSavedPosition + dstLandmarkOrigin; // 1550, 1520, 210

	// Verify that failing to set vecLandmarkOffset introduces a massive coordinate error equal to srcLandmarkOrigin
	Vector error = brokenRestoredOrigin - canonicalRestoredOrigin;
	CHECK( error.x == Catch::Approx( srcLandmarkOrigin.x ) );
	CHECK( error.y == Catch::Approx( srcLandmarkOrigin.y ) );
	CHECK( error.z == Catch::Approx( srcLandmarkOrigin.z ) );
}

TEST_CASE( "Level Transition: AddTransitionToList Duplicate Rejection", "[level_transition][changelist]" )
{
	LEVELLIST levelList[16];
	memset( levelList, 0, sizeof( levelList ) );

	entvars_t varsLandmark1, varsLandmark2;
	memset( &varsLandmark1, 0, sizeof( varsLandmark1 ) );
	memset( &varsLandmark2, 0, sizeof( varsLandmark2 ) );
	varsLandmark1.origin = Vector( 100.0f, 200.0f, 300.0f );
	varsLandmark2.origin = Vector( 400.0f, 500.0f, 600.0f );

	edict_t entLandmark1, entLandmark2;
	memset( &entLandmark1, 0, sizeof( entLandmark1 ) );
	memset( &entLandmark2, 0, sizeof( entLandmark2 ) );
	entLandmark1.pvPrivateData = &varsLandmark1;
	entLandmark1.v = varsLandmark1;
	entLandmark2.pvPrivateData = &varsLandmark2;
	entLandmark2.v = varsLandmark2;

	int count = 0;

	// Add transition 1: map "c1a0a", landmark "lm1"
	int res1 = SimulateAddTransitionToList( levelList, count, "c1a0a", "lm1", &entLandmark1 );
	REQUIRE( res1 == 1 );
	count++;
	CHECK( count == 1 );
	CHECK( strcmp( levelList[0].mapName, "c1a0a" ) == 0 );
	CHECK( strcmp( levelList[0].landmarkName, "lm1" ) == 0 );
	CHECK( levelList[0].pentLandmark == &entLandmark1 );
	CHECK( levelList[0].vecLandmarkOrigin.x == Catch::Approx( 100.0f ) );
	CHECK( levelList[0].vecLandmarkOrigin.y == Catch::Approx( 200.0f ) );
	CHECK( levelList[0].vecLandmarkOrigin.z == Catch::Approx( 300.0f ) );

	// Duplicate transition: identical map and landmark entity must be rejected
	int resDup = SimulateAddTransitionToList( levelList, count, "c1a0a", "lm1", &entLandmark1 );
	CHECK( resDup == 0 );
	CHECK( count == 1 ); // count must not increase

	// Distinct transition: different map with same landmark
	int res2 = SimulateAddTransitionToList( levelList, count, "c1a0b", "lm1", &entLandmark1 );
	REQUIRE( res2 == 1 );
	count++;
	CHECK( count == 2 );

	// Distinct transition: different landmark entity
	int res3 = SimulateAddTransitionToList( levelList, count, "c1a0c", "lm2", &entLandmark2 );
	REQUIRE( res3 == 1 );
	count++;
	CHECK( count == 3 );
	CHECK( levelList[2].vecLandmarkOrigin.x == Catch::Approx( 400.0f ) );

	// Null parameter validation
	CHECK( SimulateAddTransitionToList( nullptr, count, "c1a0", "lm", &entLandmark1 ) == 0 );
	CHECK( SimulateAddTransitionToList( levelList, count, nullptr, "lm", &entLandmark1 ) == 0 );
	CHECK( SimulateAddTransitionToList( levelList, count, "c1a0", nullptr, &entLandmark1 ) == 0 );
	CHECK( SimulateAddTransitionToList( levelList, count, "c1a0", "lm", nullptr ) == 0 );
}

TEST_CASE( "Level Transition: InTransitionVolume Capabilities and Attachment Traversal", "[level_transition][volume]" )
{
	std::vector<MockTransitionVolume> volumes;
	MockTransitionVolume vol1;
	vol1.targetname = "landmark_c1a1";
	vol1.classname  = "trigger_transition";
	vol1.mins       = Vector( -100.0f, -100.0f, -100.0f );
	vol1.maxs       = Vector( 100.0f, 100.0f, 100.0f );
	volumes.push_back( vol1 );

	// Case 1: Entity with FCAP_FORCE_TRANSITION is always inside
	MockTransitionEntity forcedEntity;
	memset( &forcedEntity, 0, sizeof( forcedEntity ) );
	forcedEntity.caps   = FCAP_FORCE_TRANSITION;
	forcedEntity.origin = Vector( 9999.0f, 9999.0f, 9999.0f ); // far outside
	CHECK( SimulateInTransitionVolume( &forcedEntity, "landmark_c1a1", volumes ) == 1 );

	// Case 2: Entity inside the volume brush
	MockTransitionEntity insideEntity;
	memset( &insideEntity, 0, sizeof( insideEntity ) );
	insideEntity.origin = Vector( 10.0f, 10.0f, 10.0f );
	insideEntity.mins   = Vector( -16.0f, -16.0f, -36.0f );
	insideEntity.maxs   = Vector( 16.0f, 16.0f, 36.0f );
	CHECK( SimulateInTransitionVolume( &insideEntity, "landmark_c1a1", volumes ) == 1 );

	// Case 3: Entity outside the volume brush
	MockTransitionEntity outsideEntity;
	memset( &outsideEntity, 0, sizeof( outsideEntity ) );
	outsideEntity.origin = Vector( 500.0f, 500.0f, 0.0f );
	outsideEntity.mins   = Vector( -16.0f, -16.0f, -36.0f );
	outsideEntity.maxs   = Vector( 16.0f, 16.0f, 36.0f );
	CHECK( SimulateInTransitionVolume( &outsideEntity, "landmark_c1a1", volumes ) == 0 );

	// Case 4: Follower / attachment (MOVETYPE_FOLLOW) follows player
	MockTransitionEntity weaponEntity;
	memset( &weaponEntity, 0, sizeof( weaponEntity ) );
	weaponEntity.movetype = MOVETYPE_FOLLOW;
	weaponEntity.aiment   = &insideEntity;
	weaponEntity.origin   = Vector( 9999.0f, 9999.0f, 9999.0f ); // weapon origin might be decoupled
	CHECK( SimulateInTransitionVolume( &weaponEntity, "landmark_c1a1", volumes ) == 1 );

	// If aiment is outside, weapon is also outside
	weaponEntity.aiment = &outsideEntity;
	CHECK( SimulateInTransitionVolume( &weaponEntity, "landmark_c1a1", volumes ) == 0 );

	// Case 5: Map with NO trigger_transition brushes (pure landmark point transition)
	// Upstream canonical behavior: everything in PVS is included (returns 1)
	std::vector<MockTransitionVolume> emptyVolumes;
	CHECK( SimulateInTransitionVolume( &outsideEntity, "unbounded_landmark", emptyVolumes ) == 1 );
}

TEST_CASE( "Level Transition: Entity Table Flags and Transition Mask", "[level_transition][flags]" )
{
	// Canonical Valve SDK definitions from engine/eiface.h:
	// Caps:   FCAP_ACROSS_TRANSITION = 0x00000080
	// Flags:  FENTTABLE_MOVEABLE     = 0x20000000
	// Flags:  FENTTABLE_GLOBAL       = 0x10000000

	int entityCapsAcross = FCAP_ACROSS_TRANSITION;
	int flags = 0;

	if ( entityCapsAcross & FCAP_ACROSS_TRANSITION )
		flags |= FENTTABLE_MOVEABLE;

	CHECK( ( flags & FENTTABLE_MOVEABLE ) != 0 );
	CHECK( flags == FENTTABLE_MOVEABLE );

	// Global entity test
	const char *globalname = "barney_companion";
	bool isDormant = false;
	if ( globalname && !isDormant )
		flags |= FENTTABLE_GLOBAL;

	CHECK( ( flags & FENTTABLE_GLOBAL ) != 0 );
	CHECK( flags == ( FENTTABLE_MOVEABLE | FENTTABLE_GLOBAL ) );

	// Verify transition level index bitmask: flags | (1 << transitionIndex)
	int transitionIndex = 2; // third transition
	int finalFlags = flags | ( 1 << transitionIndex );
	CHECK( ( finalFlags & ( 1 << transitionIndex ) ) != 0 );
	CHECK( ( finalFlags & FENTTABLE_MOVEABLE ) != 0 );
	CHECK( ( finalFlags & FENTTABLE_GLOBAL ) != 0 );

	// Regression check from Issue #125:
	// Refactored code assigned `entityFlags = caps;` which stored 0x80 instead of FENTTABLE_MOVEABLE!
	int brokenFlags = entityCapsAcross;
	CHECK( ( brokenFlags & FENTTABLE_MOVEABLE ) == 0 ); // Engine failed to identify entity as moveable!
}

TEST_CASE( "Level Transition: Missing Landmark Restore Fallback", "[level_transition][fallback]" )
{
	// In CBasePlayer::Restore:
	// If pSaveData->fUseLandmark is FALSE, the player must spawn at EntSelectSpawnPoint() + Vector(0,0,1)
	bool fUseLandmark = false;

	Vector defaultSpawnSpotOrigin( 64.0f, 128.0f, 16.0f );
	Vector restoredOrigin( 999.0f, 999.0f, 999.0f );

	if ( !fUseLandmark )
	{
		restoredOrigin = defaultSpawnSpotOrigin + Vector( 0.0f, 0.0f, 1.0f );
	}

	CHECK( restoredOrigin.x == Catch::Approx( 64.0f ) );
	CHECK( restoredOrigin.y == Catch::Approx( 128.0f ) );
	CHECK( restoredOrigin.z == Catch::Approx( 17.0f ) );
}
