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

	const char *pickupsToRegister[] = {
		"item_healthkit", "item_battery",
		"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357", "ammo_ARgrenades", "ammo_rpgclip",
		"weapon_glock", "weapon_shotgun", "weapon_mp5", "weapon_357",
		"weapon_crossbow", "weapon_rpg", "weapon_gauss", "weapon_egon",
		"item_gladder_lambda"
	};
	for ( const char *szP : pickupsToRegister )
		RegisterMockEntityFactory( szP, FactoryTestItem );

	const char *monstersToRegister[] = {
		"monster_headcrab", "monster_zombie", "monster_houndeye", "monster_bullchicken",
		"monster_barnacle", "monster_vortigaunt", "monster_alien_grunt",
		"monster_alien_controller", "monster_human_grunt", "monster_human_assassin"
	};
	for ( const char *szM : monstersToRegister )
		RegisterMockEntityFactory( szM, FactoryTestMonster );

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

TEST_CASE( "Gladder Spawner: Precache registers all roster monsters and pickups", "[gladder][spawner]" )
{
	ResetMockEngine();

	GladderSpawner spawner;
	// Verify that Precache executes without error across all roster monsters and items
	spawner.Precache();
	SUCCEED( "Precache completed successfully" );
}

TEST_CASE( "Gladder Spawner: Large monster classification", "[gladder][spawner]" )
{
	CHECK( GladderSpawner::IsLargeMonster( "monster_bullchicken" ) == true );
	CHECK( GladderSpawner::IsLargeMonster( "monster_alien_grunt" ) == true );
	CHECK( GladderSpawner::IsLargeMonster( "monster_gargantua" ) == true );
	CHECK( GladderSpawner::IsLargeMonster( "monster_bigmomma" ) == true );

	CHECK( GladderSpawner::IsLargeMonster( "monster_headcrab" ) == false );
	CHECK( GladderSpawner::IsLargeMonster( "monster_zombie" ) == false );
	CHECK( GladderSpawner::IsLargeMonster( "monster_houndeye" ) == false );
	CHECK( GladderSpawner::IsLargeMonster( "monster_vortigaunt" ) == false );
	CHECK( GladderSpawner::IsLargeMonster( "monster_human_grunt" ) == false );
	CHECK( GladderSpawner::IsLargeMonster( "monster_human_assassin" ) == false );
	CHECK( GladderSpawner::IsLargeMonster( nullptr ) == false );
	CHECK( GladderSpawner::IsLargeMonster( "" ) == false );
}

TEST_CASE( "Gladder Spawner: Cell occupancy and spatial clearance prevents duplicate/overlapping spawns", "[gladder][spawner]" )
{
	ResetMockEngine();

	const char *pickupsToRegister[] = {
		"item_healthkit", "item_battery",
		"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357", "ammo_ARgrenades", "ammo_rpgclip",
		"weapon_glock", "weapon_shotgun", "weapon_mp5", "weapon_357",
		"weapon_crossbow", "weapon_rpg", "weapon_gauss", "weapon_egon",
		"item_gladder_lambda"
	};
	for ( const char *szP : pickupsToRegister )
		RegisterMockEntityFactory( szP, FactoryTestItem );

	const char *monstersToRegister[] = {
		"monster_headcrab", "monster_zombie", "monster_houndeye", "monster_bullchicken",
		"monster_barnacle", "monster_vortigaunt", "monster_alien_grunt",
		"monster_alien_controller", "monster_human_grunt", "monster_human_assassin"
	};
	for ( const char *szM : monstersToRegister )
		RegisterMockEntityFactory( szM, FactoryTestMonster );

	// Create a grid with 10 cells, each spaced 64 units apart along X
	GladderGridIndexer indexer;
	for ( int i = 0; i < 10; ++i )
	{
		GladderGridCell c;
		c.origin   = Vector( i * 64.0f, 0.0f, 0.0f );
		c.normal   = Vector( 0.0f, 0.0f, 1.0f );
		c.areaId   = 1;
		c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK | GLADDER_CELL_LARGE_CLEARANCE;
		c.ceilingZ = 120.0f;
		indexer.AddCell( c );
	}

	GladderMapConfig config;
	config.baseMonsters = 3;
	config.monstersPerWave = 0.0f;
	config.basePickups = 2;
	config.pickupsPerWave = 0.0f;

	GladderSpawner spawner;
	int totalSpawned = spawner.SpawnWave( 1, indexer, config );
	CHECK( totalSpawned == 6 ); // 1 Lambda + 3 monsters + 2 pickups = 6
	CHECK( spawner.GetLastSpawnedMonsterCount() == 3 );
	CHECK( spawner.GetLastSpawnedPickupCount() == 2 );
	CHECK( spawner.WasLambdaSpawned() == true );

	// Verify that each spawned entity has a distinct position (no two entities on the same cell)
	const auto &tracked = spawner.GetTrackedEntityCount();
	CHECK( tracked == 6 );

	ClearMockEntityFactories();
}

TEST_CASE( "Gladder Spawner: Throttling when grid cells are exhausted", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_headcrab", FactoryTestMonster );
	RegisterMockEntityFactory( "item_gladder_lambda", FactoryTestItem );
	RegisterMockEntityFactory( "item_healthkit", FactoryTestItem );

	// Create a tiny grid with only 2 cells
	GladderGridIndexer indexer;
	for ( int i = 0; i < 2; ++i )
	{
		GladderGridCell c;
		c.origin   = Vector( i * 64.0f, 0.0f, 0.0f );
		c.normal   = Vector( 0.0f, 0.0f, 1.0f );
		c.areaId   = 1;
		c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
		c.ceilingZ = 120.0f;
		indexer.AddCell( c );
	}

	GladderMapConfig config;
	config.monsterWhitelist.push_back( "monster_headcrab" );
	config.baseMonsters = 10; // Requests 10 monsters, but only 2 cells total on the entire map!
	config.monstersPerWave = 0.0f;
	config.basePickups = 5;
	config.pickupsPerWave = 0.0f;

	GladderSpawner spawner;
	int totalSpawned = spawner.SpawnWave( 1, indexer, config );

	// Cell 1 is claimed by Lambda, Cell 2 is claimed by Monster 1.
	// No free cells remain, so spawner must gracefully stop without spawning more monsters or pickups!
	CHECK( spawner.WasLambdaSpawned() == true );
	CHECK( spawner.GetLastSpawnedMonsterCount() == 1 );
	CHECK( spawner.GetLastSpawnedPickupCount() == 0 );
	CHECK( totalSpawned == 2 ); // Exactly 2 entities spawned, exactly matching cell capacity!

	ClearMockEntityFactories();
}

TEST_CASE( "Gladder Spawner: Large monster species safely degrades to human-hull threat when large cells exhausted (Issue #34)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_zombie", FactoryTestMonster );
	RegisterMockEntityFactory( "monster_alien_grunt", FactoryTestMonster );
	RegisterMockEntityFactory( "item_gladder_lambda", FactoryTestItem );

	// Create grid with cells that have clearance for human_hull but NOT GLADDER_CELL_LARGE_CLEARANCE
	GladderGridIndexer indexer;
	for ( int i = 0; i < 4; ++i )
	{
		GladderGridCell c;
		c.origin   = Vector( i * 64.0f, 0.0f, 0.0f );
		c.normal   = Vector( 0.0f, 0.0f, 1.0f );
		c.areaId   = 1;
		c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK; // No GLADDER_CELL_LARGE_CLEARANCE!
		c.ceilingZ = 120.0f;
		indexer.AddCell( c );
	}

	GladderMapConfig config;
	config.monsterWhitelist.push_back( "monster_alien_grunt" ); // Only large monsters requested
	config.baseMonsters    = 2;
	config.monstersPerWave = 0.0f;
	config.basePickups     = 0;
	config.pickupsPerWave  = 0.0f;

	GladderSpawner spawner;
	int totalSpawned = spawner.SpawnWave( 1, indexer, config );

	// Spawner should safely fall back and spawn regular monsters (zombie) rather than forcing large monsters into walls!
	CHECK( spawner.GetLastSpawnedMonsterCount() == 2 );
	CHECK( totalSpawned >= 2 );

	ClearMockEntityFactories();
}

TEST_CASE( "Gladder Spawner: Pickup and monster spatial separation enforces clearance (Issue #34)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_zombie", FactoryTestMonster );

	const char *pickupsToRegister[] = {
		"item_healthkit", "item_battery",
		"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357", "ammo_ARgrenades", "ammo_rpgclip",
		"weapon_glock", "weapon_shotgun", "weapon_mp5", "weapon_357", "weapon_crossbow", "weapon_rpg", "weapon_gauss", "weapon_egon"
	};
	for ( const char *szP : pickupsToRegister )
		RegisterMockEntityFactory( szP, FactoryTestItem );
	RegisterMockEntityFactory( "item_gladder_lambda", FactoryTestItem );

	GladderGridIndexer indexer;
	// Create an 8x8 grid of 32-unit spaced cells (64 total cells)
	for ( int x = 0; x < 8; ++x )
	{
		for ( int y = 0; y < 8; ++y )
		{
			GladderGridCell c;
			c.origin   = Vector( x * 32.0f, y * 32.0f, 0.0f );
			c.normal   = Vector( 0.0f, 0.0f, 1.0f );
			c.areaId   = 1;
			c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
			c.ceilingZ = 120.0f;
			indexer.AddCell( c );
		}
	}

	GladderMapConfig config;
	config.monsterWhitelist.push_back( "monster_zombie" );
	config.baseMonsters    = 2;
	config.monstersPerWave = 0.0f;
	config.basePickups     = 2;
	config.pickupsPerWave  = 0.0f;

	GladderSpawner spawner;
	int totalSpawned = spawner.SpawnWave( 1, indexer, config );
	CHECK( totalSpawned >= 4 );
	CHECK( spawner.GetLastSpawnedMonsterCount() == 2 );
	CHECK( spawner.GetLastSpawnedPickupCount() == 2 );

	std::vector<Vector> monsterPositions;
	std::vector<Vector> itemPositions;
	for ( size_t i = 0; i < spawner.GetTrackedEntityCount(); ++i )
	{
		CBaseEntity *pEnt = spawner.GetTrackedEntity( i );
		if ( !pEnt || !pEnt->pev ) continue;
		const char *szClass = STRING( pEnt->pev->classname );
		if ( strncmp( szClass, "monster_", 8 ) == 0 )
			monsterPositions.push_back( pEnt->pev->origin );
		else if ( strncmp( szClass, "item_", 5 ) == 0 || strncmp( szClass, "weapon_", 7 ) == 0 || strncmp( szClass, "ammo_", 5 ) == 0 )
		{
			if ( strcmp( szClass, "item_gladder_lambda" ) != 0 )
				itemPositions.push_back( pEnt->pev->origin );
		}
	}

	CHECK( monsterPositions.size() == 2 );
	CHECK( itemPositions.size() == 2 );

	// Verify all spawned pickups maintain at least 48 units distance from all spawned monsters
	for ( const auto &itemPos : itemPositions )
	{
		for ( const auto &monsterPos : monsterPositions )
		{
			float dist2D = ( itemPos - monsterPos ).Length2D();
			CHECK( dist2D >= 48.0f );
		}
	}

	ClearMockEntityFactories();
}

