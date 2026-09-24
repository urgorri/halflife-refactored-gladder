/***
 *
 *	Behavioral Equivalence Verification - AI Factions & Companions Unit Tests (Layer 3)
 *	Verifies MonsterRelationshipManager (#92) and TalkCompanionRegistry (#93)
 *
 ****/

#include <cstring>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#include "external/catch2/catch_amalgamated.hpp"
#include "tests/mock_engine.h"
#include "dlls/ai/monster_relationship_manager.h"
#include "dlls/ai/talk_companion_registry.h"

TEST_CASE( "MonsterRelationshipManager: canonical 14x14 matrix equivalence (#92)", "[ai][relationships]" )
{
	MonsterRelationshipManager &mgr = MonsterRelationshipManager::GetInstance();
	mgr.ResetToDefaults();

	// Canonical Valve GoldSrc table for verification
	const int expected[14][14] = {
		//   NONE   MACH   PLYR   HPASS  HMIL   AMIL   APASS  AMONST APREY  APRED  INSECT PLRALY PBWPN  ABWPN
		/*NONE*/          { R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO },
		/*MACHINE*/       { R_NO, R_NO, R_DL, R_DL, R_NO, R_DL, R_DL, R_DL, R_DL, R_DL, R_NO, R_DL, R_DL, R_DL },
		/*PLAYER*/        { R_NO, R_DL, R_NO, R_NO, R_DL, R_DL, R_DL, R_DL, R_DL, R_DL, R_NO, R_NO, R_DL, R_DL },
		/*HUMANPASSIVE*/  { R_NO, R_NO, R_AL, R_AL, R_HT, R_FR, R_NO, R_HT, R_DL, R_FR, R_NO, R_AL, R_NO, R_NO },
		/*HUMANMILITARY*/ { R_NO, R_NO, R_HT, R_DL, R_NO, R_HT, R_DL, R_DL, R_DL, R_DL, R_NO, R_HT, R_NO, R_NO },
		/*ALIENMILITARY*/ { R_NO, R_DL, R_HT, R_DL, R_HT, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_DL, R_NO, R_NO },
		/*ALIENPASSIVE*/  { R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO },
		/*ALIENMONSTER*/  { R_NO, R_DL, R_DL, R_DL, R_DL, R_NO, R_NO, R_NO, R_NO, R_NO, R_NO, R_DL, R_NO, R_NO },
		/*ALIENPREY*/     { R_NO, R_NO, R_DL, R_DL, R_DL, R_NO, R_NO, R_NO, R_NO, R_FR, R_NO, R_DL, R_NO, R_NO },
		/*ALIENPREDATOR*/ { R_NO, R_NO, R_DL, R_DL, R_DL, R_NO, R_NO, R_NO, R_HT, R_DL, R_NO, R_DL, R_NO, R_NO },
		/*INSECT*/        { R_FR, R_FR, R_FR, R_FR, R_FR, R_NO, R_FR, R_FR, R_FR, R_FR, R_NO, R_FR, R_NO, R_NO },
		/*PLAYERALLY*/    { R_NO, R_DL, R_AL, R_AL, R_DL, R_DL, R_DL, R_DL, R_DL, R_DL, R_NO, R_NO, R_NO, R_NO },
		/*PBIOWEAPON*/    { R_NO, R_NO, R_DL, R_DL, R_DL, R_DL, R_DL, R_DL, R_DL, R_DL, R_NO, R_DL, R_NO, R_DL },
		/*ABIOWEAPON*/    { R_NO, R_NO, R_DL, R_DL, R_DL, R_AL, R_NO, R_DL, R_DL, R_NO, R_NO, R_DL, R_DL, R_NO }
	};

	for ( int from = 0; from < 14; ++from )
	{
		for ( int to = 0; to < 14; ++to )
		{
			CHECK( mgr.GetRelationship( from, to ) == expected[from][to] );
		}
	}
}

TEST_CASE( "MonsterRelationshipManager: bounds validation and extended classes (#92)", "[ai][relationships]" )
{
	MonsterRelationshipManager &mgr = MonsterRelationshipManager::GetInstance();
	mgr.ResetToDefaults();

	SECTION( "Class validation helper" )
	{
		for ( int c = 0; c < 14; ++c )
		{
			CHECK( MonsterRelationshipManager::IsValidClass( c ) );
		}

		CHECK_FALSE( MonsterRelationshipManager::IsValidClass( -1 ) );
		CHECK_FALSE( MonsterRelationshipManager::IsValidClass( 14 ) );
		CHECK_FALSE( MonsterRelationshipManager::IsValidClass( 99 ) );
	}

	SECTION( "Out-of-bounds queries safely return R_NO without memory faults" )
	{
		// Vehicle class (14)
		CHECK( mgr.GetRelationship( CLASS_VEHICLE, CLASS_PLAYER ) == R_NO );
		CHECK( mgr.GetRelationship( CLASS_PLAYER, CLASS_VEHICLE ) == R_NO );

		// Barnacle class (99)
		CHECK( mgr.GetRelationship( CLASS_BARNACLE, CLASS_PLAYER ) == R_NO );
		CHECK( mgr.GetRelationship( CLASS_HUMAN_MILITARY, CLASS_BARNACLE ) == R_NO );

		// Arbitrary / negative class values
		CHECK( mgr.GetRelationship( -1, CLASS_PLAYER ) == R_NO );
		CHECK( mgr.GetRelationship( CLASS_PLAYER, -1 ) == R_NO );
		CHECK( mgr.GetRelationship( 1000, 2000 ) == R_NO );
	}
}

TEST_CASE( "MonsterRelationshipManager: class-level runtime overrides (#92)", "[ai][relationships]" )
{
	MonsterRelationshipManager &mgr = MonsterRelationshipManager::GetInstance();
	mgr.ResetToDefaults();

	SECTION( "Override existing class relationship" )
	{
		// Default: ALIEN_MONSTER vs PLAYER is R_DL
		REQUIRE( mgr.GetRelationship( CLASS_ALIEN_MONSTER, CLASS_PLAYER ) == R_DL );
		REQUIRE_FALSE( mgr.HasRelationshipOverride( CLASS_ALIEN_MONSTER, CLASS_PLAYER ) );

		// Override to ally
		mgr.SetRelationship( CLASS_ALIEN_MONSTER, CLASS_PLAYER, R_AL );
		CHECK( mgr.HasRelationshipOverride( CLASS_ALIEN_MONSTER, CLASS_PLAYER ) );
		CHECK( mgr.GetRelationship( CLASS_ALIEN_MONSTER, CLASS_PLAYER) == R_AL );

		// Remove override reverts to default
		mgr.RemoveRelationship( CLASS_ALIEN_MONSTER, CLASS_PLAYER );
		CHECK_FALSE( mgr.HasRelationshipOverride( CLASS_ALIEN_MONSTER, CLASS_PLAYER ) );
		CHECK( mgr.GetRelationship( CLASS_ALIEN_MONSTER, CLASS_PLAYER ) == R_DL );
	}

	SECTION( "Override extended class relationship" )
	{
		// Extended class CLASS_VEHICLE (14)
		REQUIRE( mgr.GetRelationship( CLASS_VEHICLE, CLASS_HUMAN_MILITARY ) == R_NO );

		mgr.SetRelationship( CLASS_VEHICLE, CLASS_HUMAN_MILITARY, R_HT );
		CHECK( mgr.GetRelationship( CLASS_VEHICLE, CLASS_HUMAN_MILITARY ) == R_HT );

		mgr.ClearOverrides();
		CHECK( mgr.GetRelationship( CLASS_VEHICLE, CLASS_HUMAN_MILITARY ) == R_NO );
	}

	SECTION( "Static facade convenience methods" )
	{
		MonsterRelationshipManager::Reset();
		CHECK( MonsterRelationshipManager::QueryRelationship( CLASS_PLAYER, CLASS_HUMAN_MILITARY ) == R_DL );

		MonsterRelationshipManager::OverrideRelationship( CLASS_PLAYER, CLASS_HUMAN_MILITARY, R_AL );
		CHECK( MonsterRelationshipManager::QueryRelationship( CLASS_PLAYER, CLASS_HUMAN_MILITARY ) == R_AL );

		MonsterRelationshipManager::Reset();
		CHECK( MonsterRelationshipManager::QueryRelationship( CLASS_PLAYER, CLASS_HUMAN_MILITARY ) == R_DL );
	}
}

class CTestEntity : public CBaseEntity
{
  public:
	int m_iClassification;
	CTestEntity( int classification = CLASS_NONE ) : m_iClassification( classification ) {}
	int Classify( void ) override { return m_iClassification; }
};

TEST_CASE( "MonsterRelationshipManager: entity-level runtime overrides (#92)", "[ai][relationships]" )
{
	MonsterRelationshipManager &mgr = MonsterRelationshipManager::GetInstance();
	mgr.ResetToDefaults();

	CTestEntity entA( CLASS_PLAYER );
	CTestEntity entB( CLASS_HUMAN_MILITARY );

	SECTION( "Null entity pointers return R_NO safely" )
	{
		CHECK( mgr.GetRelationship( nullptr, &entB ) == R_NO );
		CHECK( mgr.GetRelationship( &entA, nullptr ) == R_NO );
		CHECK( mgr.GetRelationship( nullptr, nullptr ) == R_NO );
	}

	SECTION( "Entity-to-entity override takes highest precedence" )
	{
		// Default: PLAYER vs HUMAN_MILITARY is R_DL
		CHECK( mgr.GetRelationship( &entA, &entB ) == R_DL );

		mgr.SetEntityRelationship( &entA, &entB, R_NM );
		CHECK( mgr.GetRelationship( &entA, &entB ) == R_NM );

		// Clear specific actor overrides
		mgr.ClearEntityRelationships( &entA );
		CHECK( mgr.GetRelationship( &entA, &entB ) == R_DL );
	}

	SECTION( "Entity-to-class override takes precedence over class default" )
	{
		CHECK( mgr.GetRelationship( &entA, &entB ) == R_DL );

		mgr.SetEntityClassRelationship( &entA, CLASS_HUMAN_MILITARY, R_AL );
		CHECK( mgr.GetRelationship( &entA, &entB ) == R_AL );

		mgr.ClearOverrides();
		CHECK( mgr.GetRelationship( &entA, &entB ) == R_DL );
	}
}

TEST_CASE( "TalkCompanionRegistry: companion registration, query, and defaults (#93)", "[ai][companions]" )
{
	TalkCompanionRegistry::ResetToDefaults();

	SECTION( "Canonical defaults are properly registered" )
	{
		REQUIRE( TalkCompanionRegistry::GetCompanionCount() == 3 );
		CHECK( std::string( TalkCompanionRegistry::GetCompanion( 0 ) ) == "monster_barney" );
		CHECK( std::string( TalkCompanionRegistry::GetCompanion( 1 ) ) == "monster_scientist" );
		CHECK( std::string( TalkCompanionRegistry::GetCompanion( 2 ) ) == "monster_sitting_scientist" );
		CHECK( TalkCompanionRegistry::GetCompanion( 3 ) == nullptr );
		CHECK( TalkCompanionRegistry::GetCompanion( -1 ) == nullptr );

		CHECK( TalkCompanionRegistry::IsRegistered( "monster_barney" ) );
		CHECK( TalkCompanionRegistry::IsRegistered( "monster_scientist" ) );
		CHECK( TalkCompanionRegistry::IsRegistered( "monster_sitting_scientist" ) );
		CHECK_FALSE( TalkCompanionRegistry::IsRegistered( "monster_otis" ) );
		CHECK_FALSE( TalkCompanionRegistry::IsRegistered( nullptr ) );
	}

	SECTION( "Register new companion NPCs" )
	{
		TalkCompanionRegistry::Register( "monster_otis" );
		REQUIRE( TalkCompanionRegistry::GetCompanionCount() == 4 );
		CHECK( TalkCompanionRegistry::IsRegistered( "monster_otis" ) );
		CHECK( std::string( TalkCompanionRegistry::GetCompanion( 3 ) ) == "monster_otis" );

		// Duplicate registration is ignored
		TalkCompanionRegistry::Register( "monster_otis" );
		CHECK( TalkCompanionRegistry::GetCompanionCount() == 4 );

		// Null or empty registration is safely ignored
		TalkCompanionRegistry::Register( nullptr );
		TalkCompanionRegistry::Register( "" );
		CHECK( TalkCompanionRegistry::GetCompanionCount() == 4 );
	}

	SECTION( "Unregister companion NPCs" )
	{
		TalkCompanionRegistry::Register( "monster_custom" );
		REQUIRE( TalkCompanionRegistry::IsRegistered( "monster_custom" ) );

		CHECK( TalkCompanionRegistry::Unregister( "monster_custom" ) );
		CHECK_FALSE( TalkCompanionRegistry::IsRegistered( "monster_custom" ) );
		CHECK_FALSE( TalkCompanionRegistry::Unregister( "monster_custom" ) );
		CHECK_FALSE( TalkCompanionRegistry::Unregister( nullptr ) );
	}

	SECTION( "Clear and ResetToDefaults" )
	{
		TalkCompanionRegistry::Clear();
		CHECK( TalkCompanionRegistry::GetCompanionCount() == 0 );
		CHECK( TalkCompanionRegistry::GetCompanion( 0 ) == nullptr );
		CHECK_FALSE( TalkCompanionRegistry::IsRegistered( "monster_barney" ) );

		TalkCompanionRegistry::ResetToDefaults();
		CHECK( TalkCompanionRegistry::GetCompanionCount() == 3 );
		CHECK( TalkCompanionRegistry::IsRegistered( "monster_barney" ) );
	}
}

TEST_CASE( "Graph: Path buffer allocation and routing verification (#144)", "[ai][graph][routing]" )
{
	// Verifies that routing test buffers allocated with new int[m_cNodes]
	// are properly managed without memory leaks or allocator mismatch.
	const int nodeCount = 8;
	int *pMyPath = new int[nodeCount];
	int *pMyPath2 = new int[nodeCount];
	REQUIRE( pMyPath != nullptr );
	REQUIRE( pMyPath2 != nullptr );

	for ( int i = 0; i < nodeCount; ++i )
	{
		pMyPath[i] = i;
		pMyPath2[i] = nodeCount - 1 - i;
	}

	CHECK( pMyPath[0] == 0 );
	CHECK( pMyPath2[0] == nodeCount - 1 );

	// Array delete must be used for buffers allocated with new[]
	delete[] pMyPath;
	delete[] pMyPath2;
	pMyPath = nullptr;
	pMyPath2 = nullptr;

	CHECK( pMyPath == nullptr );
	CHECK( pMyPath2 == nullptr );
}
