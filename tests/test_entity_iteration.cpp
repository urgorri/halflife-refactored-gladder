#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <vector>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "tests/mock_engine.h"

// Concrete dummy entity for testing iteration
class CTestIterEntity : public CBaseEntity
{
  public:
	int m_testId = 0;
};

static void CollectEntityCallback( CBaseEntity *pEntity, void *pUserData )
{
	auto *pList = reinterpret_cast<std::vector<int> *>( pUserData );
	auto *pIter = static_cast<CTestIterEntity *>( pEntity );
	pList->push_back( pIter->m_testId );
}

TEST_CASE( "Util: UTIL_ForEachEntity safely iterates live server entities (#164)", "[util][iteration]" )
{
	ResetMockEngine();

	CTestIterEntity entityA;
	CTestIterEntity entityB;
	CTestIterEntity entityWorld;

	entityWorld.m_testId = 999;
	entityA.m_testId     = 101;
	entityB.m_testId     = 102;

	// Edict 0 (world entity): should be ignored by UTIL_ForEachEntity
	edict_t *pWorld = GetMockClientEntity( 0 );
	if ( pWorld )
	{
		pWorld->free          = 0;
		pWorld->pvPrivateData = &entityWorld;
		entityWorld.pev       = &pWorld->v;
	}

	// Edict 1: Valid live entity
	edict_t *pEd1 = GetMockClientEntity( 1 );
	pEd1->free          = 0;
	pEd1->pvPrivateData = &entityA;
	entityA.pev         = &pEd1->v;

	// Edict 2: Free / deleted entity (must be skipped)
	edict_t *pEd2 = GetMockClientEntity( 2 );
	pEd2->free          = 1;
	pEd2->pvPrivateData = nullptr;

	// Edict 3: Valid live entity
	edict_t *pEd3 = GetMockClientEntity( 3 );
	pEd3->free          = 0;
	pEd3->pvPrivateData = &entityB;
	entityB.pev         = &pEd3->v;

	// Edict 4: Inactive slot
	edict_t *pEd4 = GetMockClientEntity( 4 );
	pEd4->free          = 0;
	pEd4->pvPrivateData = nullptr;

	gpGlobals->maxEntities = 5;

	SECTION( "Function pointer callback visits only active entities in order" )
	{
		std::vector<int> visitedIds;
		UTIL_ForEachEntity( CollectEntityCallback, &visitedIds );

		// Red phase check: if UTIL_ForEachEntity is a no-op stub, visitedIds is empty
		REQUIRE( visitedIds.size() == 2 );
		CHECK( visitedIds[0] == 101 );
		CHECK( visitedIds[1] == 102 );
	}

	SECTION( "C++ template lambda callback correctly accumulates entities" )
	{
		std::vector<int> visitedIds;
		UTIL_ForEachEntity( [&]( CBaseEntity *pEnt ) {
			auto *pIter = static_cast<CTestIterEntity *>( pEnt );
			visitedIds.push_back( pIter->m_testId );
		} );

		REQUIRE( visitedIds.size() == 2 );
		CHECK( visitedIds[0] == 101 );
		CHECK( visitedIds[1] == 102 );
	}

	SECTION( "Null callback or invalid gpGlobals handles safely" )
	{
		// Calling with null callback should not crash
		UTIL_ForEachEntity( nullptr, nullptr );

		globalvars_t *pSaveGlobals = gpGlobals;
		gpGlobals                  = nullptr;
		UTIL_ForEachEntity( CollectEntityCallback, nullptr );
		gpGlobals = pSaveGlobals;
	}
}
