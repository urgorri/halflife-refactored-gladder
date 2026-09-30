/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit & Behavioral Equivalence Test Suite: GladderSpawner & GladderMapConfig (SPEC §2, §3, §5, §7.3, §11, Issue #7)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <fstream>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_spawner.h"
#include "dlls/gladder/gladder_breakable.h"

static void FactoryTestMonster( entvars_t *pev )
{
	if ( !pev || !pev->pContainingEntity )
		return;
	static CBaseEntity dummyMonsters[64];
	static size_t monsterIdx = 0;
	CBaseEntity *pEnt = &dummyMonsters[( monsterIdx++ ) % 64];
	pEnt->pev = pev;
	pev->solid = SOLID_SLIDEBOX;
	pev->pContainingEntity->pvPrivateData = pEnt;
}

static void FactoryTestItem( entvars_t *pev )
{
	if ( !pev || !pev->pContainingEntity )
		return;
	static CBaseEntity dummyItems[64];
	static size_t itemIdx = 0;
	CBaseEntity *pEnt = &dummyItems[( itemIdx++ ) % 64];
	pEnt->pev = pev;
	pev->solid = SOLID_TRIGGER;
	pev->pContainingEntity->pvPrivateData = pEnt;
}

TEST_CASE( "Gladder Spawner: Dynamic monster tier probability curves (SPEC §3)", "[gladder][spawner]" )
{
	// Wave 1: Heavily weighted basic tier 1, Tier 4 completely absent
	CHECK( GladderSpawner::GetMonsterTierWeight( 1, 1 ) == Catch::Approx( 75.0f ) );
	CHECK( GladderSpawner::GetMonsterTierWeight( 2, 1 ) == Catch::Approx( 20.0f ) );
	CHECK( GladderSpawner::GetMonsterTierWeight( 3, 1 ) == Catch::Approx( 5.0f ) );
	CHECK( GladderSpawner::GetMonsterTierWeight( 4, 1 ) == Catch::Approx( 0.0f ) );

	// Wave 3: Tier 4 still absent (< wave 4)
	CHECK( GladderSpawner::GetMonsterTierWeight( 4, 3 ) == Catch::Approx( 0.0f ) );

	// Wave 5: Mid-game progression: Tier 1 decreases, Tier 2-4 active
	CHECK( GladderSpawner::GetMonsterTierWeight( 1, 5 ) == Catch::Approx( 55.0f ) );
	CHECK( GladderSpawner::GetMonsterTierWeight( 3, 5 ) == Catch::Approx( 17.0f ) );
	CHECK( GladderSpawner::GetMonsterTierWeight( 4, 5 ) == Catch::Approx( 8.0f ) );

	// Wave 15: Late-game progression: Tier 1 remains as ambient fodder (>= 15), Tier 3 & 4 peak
	CHECK( GladderSpawner::GetMonsterTierWeight( 1, 15 ) == Catch::Approx( 15.0f ) );
	CHECK( GladderSpawner::GetMonsterTierWeight( 3, 15 ) == Catch::Approx( 35.0f ) );
	CHECK( GladderSpawner::GetMonsterTierWeight( 4, 15 ) == Catch::Approx( 35.0f ) );

	// Check Weapon Tier Weight progression
	CHECK( GladderSpawner::GetWeaponTierWeight( 1, 1 ) == Catch::Approx( 80.0f ) );
	CHECK( GladderSpawner::GetWeaponTierWeight( 3, 1 ) == Catch::Approx( 0.0f ) );
	CHECK( GladderSpawner::GetWeaponTierWeight( 3, 10 ) == Catch::Approx( 40.0f ) );
}

TEST_CASE( "Gladder Map Config: Whitelist, blacklist & file parsing (SPEC §5)", "[gladder][spawner]" )
{
	const char *testCfgPath = "test_map_profile.cfg";

	std::ofstream ofs( testCfgPath );
	REQUIRE( ofs.is_open() );
	ofs << "# Map Configuration Profile Test\n";
	ofs << "whitelist_monster monster_headcrab\n";
	ofs << "whitelist_monster monster_houndeye\n";
	ofs << "blacklist_monster monster_gargantua\n";
	ofs << "base_monsters 10\n";
	ofs << "monsters_per_wave 2.5\n";
	ofs << "max_monsters 50\n";
	ofs << "allow_barnacles 0\n";
	ofs.close();

	GladderMapConfig config;
	REQUIRE( config.LoadFromFile( testCfgPath ) == true );

	CHECK( config.baseMonsters == 10 );
	CHECK( config.monstersPerWave == Catch::Approx( 2.5f ) );
	CHECK( config.maxMonsters == 50 );
	CHECK( config.allowBarnacles == false );
	CHECK( config.allowFlying == true ); // Default preserved

	// Whitelist validation: only whitelisted entries are permitted
	CHECK( config.IsMonsterAllowed( "monster_headcrab" ) == true );
	CHECK( config.IsMonsterAllowed( "monster_houndeye" ) == true );
	CHECK( config.IsMonsterAllowed( "monster_zombie" ) == false ); // Not on whitelist
	CHECK( config.IsMonsterAllowed( "monster_gargantua" ) == false ); // Blacklisted

	std::remove( testCfgPath );
}

TEST_CASE( "Gladder Spawner: Edict budget safety protection (SPEC §11)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_headcrab", FactoryTestMonster );

	GladderSpawner spawner;

	// In mock engine, simulate low free edict budget by setting maxEntities low
	if ( gpGlobals )
	{
		gpGlobals->maxEntities = 70; // 70 total edicts; threshold is 64
	}

	// First entities succeed while free edicts > 64
	// If 70 total and 10 active, free = 60 <= 64 -> Throttling triggers!
	// Let's activate edicts 1 to 10
	for ( int i = 1; i <= 10; ++i )
	{
		edict_t *ed = GetMockClientEntity( i );
		if ( ed )
		{
			ed->free = 0;
			ed->v.pContainingEntity = ed;
		}
	}

	// Free edicts = 70 - 10 = 60 <= 64 -> CreateWaveEntity must throttle
	CBaseEntity *pEnt = spawner.CreateWaveEntity( "monster_headcrab", Vector( 0, 0, 0 ) );
	CHECK( pEnt == nullptr );

	// Restore normal budget
	if ( gpGlobals )
	{
		gpGlobals->maxEntities = 512;
	}

	ClearMockEntityFactories();
}

TEST_CASE( "Gladder Spawner: Wave generation across grid indexer & Lambda item (SPEC §7.3)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_headcrab", FactoryTestMonster );
	RegisterMockEntityFactory( "monster_zombie", FactoryTestMonster );
	RegisterMockEntityFactory( "item_healthkit", FactoryTestItem );
	RegisterMockEntityFactory( "item_battery", FactoryTestItem );
	RegisterMockEntityFactory( "item_gladder_lambda", FactoryTestItem );
	RegisterMockEntityFactory( "ammo_9mmclip", FactoryTestItem );
	RegisterMockEntityFactory( "ammo_9mmAR", FactoryTestItem );
	RegisterMockEntityFactory( "ammo_buckshot", FactoryTestItem );
	RegisterMockEntityFactory( "weapon_glock", FactoryTestItem );
	RegisterMockEntityFactory( "weapon_shotgun", FactoryTestItem );

	GladderGridIndexer indexer;
	for ( int i = 0; i < 20; ++i )
	{
		GladderGridCell c;
		c.origin   = Vector( i * 32.0f, 0.0f, 0.0f );
		c.normal   = Vector( 0.0f, 0.0f, 1.0f );
		c.areaId   = 1;
		c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK | GLADDER_CELL_CEILING_VALID;
		c.ceilingZ = 120.0f;
		indexer.AddCell( c );
	}

	GladderMapConfig config;
	config.baseMonsters = 4;
	config.monstersPerWave = 1.0f;
	config.basePickups = 2;
	config.pickupsPerWave = 0.5f;

	GladderSpawner spawner;

	// Spawn Wave 1
	int totalSpawned = spawner.SpawnWave( 1, indexer, config );
	CHECK( totalSpawned > 0 );
	CHECK( spawner.GetLastSpawnedMonsterCount() == 4 );
	CHECK( spawner.GetLastSpawnedPickupCount() == 2 );
	CHECK( spawner.WasLambdaSpawned() == true ); // Exactly 1 Lambda item per wave!

	ClearMockEntityFactories();
}

TEST_CASE( "Gladder Spawner: Comprehensive wave garbage collection (SPEC §11)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_headcrab", FactoryTestMonster );
	RegisterMockEntityFactory( "item_healthkit", FactoryTestItem );

	GladderSpawner spawner;

	// Spawn a tracked wave monster
	CBaseEntity *pMonster = spawner.CreateWaveEntity( "monster_headcrab", Vector( 10, 10, 0 ) );
	REQUIRE( pMonster != nullptr );
	REQUIRE( spawner.GetTrackedEntityCount() == 1 );

	// Setup a persistent breakable entity that MUST be protected
	CGladderBreakable breakable;
	edict_t edBreakable;
	std::memset( &edBreakable, 0, sizeof( edBreakable ) );
	edBreakable.v.pContainingEntity = &edBreakable;
	edBreakable.v.classname         = MAKE_STRING( "gladder_breakable" );
	edBreakable.pvPrivateData       = &breakable;
	breakable.pev                   = &edBreakable.v;

	// Setup a player entity that MUST be protected
	CBaseEntity player;
	edict_t edPlayer;
	std::memset( &edPlayer, 0, sizeof( edPlayer ) );
	edPlayer.v.pContainingEntity = &edPlayer;
	edPlayer.v.flags            |= FL_CLIENT;
	edPlayer.pvPrivateData       = &player;
	player.pev                   = &edPlayer.v;

	// Place entities in mock client array so UTIL_ForEachEntity sees them
	edict_t *pSlot1 = GetMockClientEntity( 1 );
	if ( pSlot1 )
	{
		pSlot1->free          = 0;
		pSlot1->pvPrivateData = &player;
		player.pev            = &pSlot1->v;
		pSlot1->v.flags      |= FL_CLIENT;
	}

	edict_t *pSlot2 = GetMockClientEntity( 2 );
	if ( pSlot2 )
	{
		pSlot2->free          = 0;
		pSlot2->pvPrivateData = &breakable;
		breakable.pev         = &pSlot2->v;
		pSlot2->v.classname   = MAKE_STRING( "gladder_breakable" );
	}

	// Execute wave entity purge
	spawner.PurgeWaveEntities();

	// Tracked entities list must be cleared
	CHECK( spawner.GetTrackedEntityCount() == 0 );
	CHECK( spawner.WasLambdaSpawned() == false );

	// Player and breakable must NOT be removed or nulled out
	CHECK( pSlot1->free == 0 );
	CHECK( pSlot2->free == 0 );

	ClearMockEntityFactories();
}
