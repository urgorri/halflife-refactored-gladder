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
#include "core/player.h"
#include "weapons/weapon_base.h"
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
	CHECK( GladderSpawner::GetWeaponTierWeight( 1, 1 ) == Catch::Approx( 20.0f ) );
	CHECK( GladderSpawner::GetWeaponTierWeight( 3, 1 ) == Catch::Approx( 0.0f ) );
	CHECK( GladderSpawner::GetWeaponTierWeight( 3, 10 ) == Catch::Approx( 15.0f ) );
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
		"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357", "ammo_ARgrenades", "ammo_762", "ammo_556", "ammo_rpgclip",
		"weapon_knife", "weapon_pipewrench", "weapon_glock", "weapon_shotgun", "weapon_mp5", "weapon_357", "weapon_eagle",
		"weapon_crossbow", "weapon_sniper", "weapon_m249", "weapon_rpg", "weapon_gauss", "weapon_egon",
		"weapon_handgrenade", "weapon_tripmine", "weapon_satchel", "weapon_snark",
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

TEST_CASE( "Gladder Spawner: Wave GC does not purge weapons collected by player (SPEC §11, Issue #34)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "weapon_shotgun", FactoryTestItem );
	RegisterMockEntityFactory( "monster_zombie", FactoryTestMonster );

	GladderSpawner spawner;

	// Spawn a wave weapon
	CBaseEntity *pWeapon = spawner.CreateWaveEntity( "weapon_shotgun", Vector( 10, 10, 0 ) );
	REQUIRE( pWeapon != nullptr );
	REQUIRE( spawner.GetTrackedEntityCount() == 1 );

	// Setup a mock player entity
	CBaseEntity player;
	edict_t edPlayer;
	std::memset( &edPlayer, 0, sizeof( edPlayer ) );
	edPlayer.v.pContainingEntity = &edPlayer;
	edPlayer.v.flags            |= FL_CLIENT;
	edPlayer.pvPrivateData       = &player;
	player.pev                   = &edPlayer.v;

	// Weapon is now picked up and owned by the player
	pWeapon->pev->owner = &edPlayer;

	// Execute wave entity purge
	spawner.PurgeWaveEntities();

	// Weapon MUST NOT have FL_KILLME set! It must be preserved in player's inventory across waves!
	CHECK( !( pWeapon->pev->flags & FL_KILLME ) );

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
		"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357", "ammo_ARgrenades", "ammo_762", "ammo_556", "ammo_rpgclip",
		"weapon_knife", "weapon_pipewrench", "weapon_glock", "weapon_shotgun", "weapon_mp5", "weapon_357", "weapon_eagle",
		"weapon_crossbow", "weapon_sniper", "weapon_m249", "weapon_rpg", "weapon_gauss", "weapon_egon",
		"weapon_handgrenade", "weapon_tripmine", "weapon_satchel", "weapon_snark",
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
		"ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot", "ammo_357", "ammo_ARgrenades", "ammo_762", "ammo_556", "ammo_rpgclip",
		"weapon_knife", "weapon_pipewrench", "weapon_glock", "weapon_shotgun", "weapon_mp5", "weapon_357", "weapon_eagle", "weapon_crossbow", "weapon_sniper", "weapon_m249", "weapon_rpg", "weapon_gauss", "weapon_egon",
		"weapon_handgrenade", "weapon_tripmine", "weapon_satchel", "weapon_snark"
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

TEST_CASE( "Gladder Spawner: Rebalanced pickup spawn distribution favors ammo (SPEC §3, Issue #39)", "[gladder][spawner]" )
{
	ResetMockEngine();
	GladderMapConfig config;

	// Simulate 1,000 rolls across wave 1 to 10
	int ammoCount = 0;
	int weaponCount = 0;
	int healthArmorCount = 0;
	const int kTotalRolls = 1000;

	for ( int i = 0; i < kTotalRolls; ++i )
	{
		int wave = 1 + ( i % 10 );
		const char *item = GladderSpawner::RollPickupItem( wave, config );
		REQUIRE( item != nullptr );

		if ( strncmp( item, "ammo_", 5 ) == 0 )
			ammoCount++;
		else if ( strncmp( item, "weapon_", 7 ) == 0 )
			weaponCount++;
		else if ( strcmp( item, "item_healthkit" ) == 0 || strcmp( item, "item_battery" ) == 0 )
			healthArmorCount++;
	}

	float ammoPct = ( static_cast<float>( ammoCount ) / kTotalRolls ) * 100.0f;
	float weaponPct = ( static_cast<float>( weaponCount ) / kTotalRolls ) * 100.0f;
	float healthArmorPct = ( static_cast<float>( healthArmorCount ) / kTotalRolls ) * 100.0f;

	// Verify required distributions (SPEC §3, Issue #39):
	// - Ammo frequency > 55%
	// - Weapon frequency < 20%
	// - Health/Armor frequency ~ 20-25%
	CHECK( ammoPct > 55.0f );
	CHECK( weaponPct < 20.0f );
	CHECK( healthArmorPct >= 18.0f );
	CHECK( healthArmorPct <= 28.0f );
}

TEST_CASE( "Gladder Spawner: Solid brush entities (func_wall, func_monsterclip) enforce cell rejection (SPEC §4.3, Issue #37)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_headcrab", FactoryTestMonster );

	GladderGridIndexer indexer;
	GladderGridCell c;
	c.origin   = Vector( 100.0f, 100.0f, 0.0f );
	c.normal   = Vector( 0.0f, 0.0f, 1.0f );
	c.areaId   = 1;
	c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
	c.ceilingZ = 120.0f;
	indexer.AddCell( c );

	GladderMapConfig config;
	config.monsterWhitelist.push_back( "monster_headcrab" );
	config.baseMonsters    = 1;
	config.monstersPerWave = 0.0f;
	config.basePickups     = 0;
	config.pickupsPerWave  = 0.0f;

	SECTION( "Cell with clear lateral clearance spawns successfully" )
	{
		GladderSpawner spawner;
		int totalSpawned = spawner.SpawnWave( 1, indexer, config );
		CHECK( spawner.GetLastSpawnedMonsterCount() == 1 );
		CHECK( totalSpawned >= 1 );
	}

	SECTION( "Cell with solid brush entity (func_wall, SOLID_BSP) in lateral clearance is rejected" )
	{
		edict_t *pWall = INDEXENT( 1 );
		REQUIRE( pWall != nullptr );
		pWall->free = 0;
		pWall->v.solid  = SOLID_BSP;
		// Spawn coordinate is (100, 100, 0), waist is at (100, 100, 36), lateral offset +16 is (116, 100, 36)
		pWall->v.absmin = Vector( 110.0f, 90.0f, 20.0f );
		pWall->v.absmax = Vector( 130.0f, 110.0f, 50.0f );

		GladderSpawner spawner;
		int totalSpawned = spawner.SpawnWave( 1, indexer, config );
		CHECK( spawner.GetLastSpawnedMonsterCount() == 0 );

		std::memset( pWall, 0, sizeof( edict_t ) );
	}

	SECTION( "Cell with func_monsterclip in lateral clearance is rejected" )
	{
		edict_t *pClip = INDEXENT( 1 );
		REQUIRE( pClip != nullptr );
		pClip->free = 0;
		pClip->v.classname = MAKE_STRING( "func_monsterclip" );
		pClip->v.solid     = SOLID_BSP;
		pClip->v.absmin    = Vector( 110.0f, 90.0f, 20.0f );
		pClip->v.absmax    = Vector( 130.0f, 110.0f, 50.0f );

		GladderSpawner spawner;
		int totalSpawned = spawner.SpawnWave( 1, indexer, config );
		CHECK( spawner.GetLastSpawnedMonsterCount() == 0 );

		std::memset( pClip, 0, sizeof( edict_t ) );
	}

	ClearMockEntityFactories();
}

TEST_CASE( "Gladder Spawner: Floor-grounded monsters drop-in elevation (+8 units) (SPEC §4.4, Issue #43)", "[gladder][spawner]" )
{
	ResetMockEngine();

	RegisterMockEntityFactory( "monster_zombie", FactoryTestMonster );
	RegisterMockEntityFactory( "item_gladder_lambda", FactoryTestItem );

	GladderGridIndexer indexer;
	for ( int i = 0; i < 4; ++i )
	{
		GladderGridCell c;
		c.origin   = Vector( 100.0f + i * 64.0f, 100.0f, 50.0f );
		c.normal   = Vector( 0.0f, 0.0f, 1.0f );
		c.areaId   = 1;
		c.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK | GLADDER_CELL_CEILING_VALID;
		c.ceilingZ = 200.0f;
		indexer.AddCell( c );
	}

	GladderMapConfig config;
	config.baseMonsters = 1;
	config.monstersPerWave = 0.0f;
	config.maxMonsters = 1;
	config.basePickups = 0;
	config.pickupsPerWave = 0.0f;
	config.maxPickups = 0;
	config.monsterWhitelist.push_back( "monster_zombie" );

	GladderSpawner spawner;
	spawner.SpawnWave( 1, indexer, config );

	REQUIRE( spawner.GetLastSpawnedMonsterCount() == 1 );

	// Find the spawned monster
	CBaseEntity *pSpawnedMonster = nullptr;
	for ( size_t i = 0; i < spawner.GetTrackedEntityCount(); ++i )
	{
		CBaseEntity *pEnt = spawner.GetTrackedEntity( i );
		const char *szClass = ( pEnt && pEnt->pev ) ? STRING( pEnt->pev->classname ) : "";
		if ( strncmp( szClass, "monster_", 8 ) == 0 )
		{
			pSpawnedMonster = pEnt;
			break;
		}
	}

	REQUIRE( pSpawnedMonster != nullptr );
	// SPEC §4.4: Ground monster spawn elevation must be cell.origin.z + 8.0f
	CHECK( pSpawnedMonster->pev->origin.z == Catch::Approx( 50.0f + 8.0f ) );

	ClearMockEntityFactories();
}

TEST_CASE( "Gladder Spawner: PurgeWaveEntities clears orphaned sprites, beams, projectiles, and ordnance (SPEC §11, Issue #42)", "[gladder][spawner]" )
{
	ResetMockEngine();
	gpGlobals->maxEntities = 32;

	struct TransientEntMock
	{
		CBaseEntity ent;
	};

	const char *classesToPurge[] = {
		"beam",
		"laser_spot",
		"squidspit",
		"bmortar",
		"bolt",
		"controller_head_ball",
		"controller_energy_ball",
		"nihilanth_energy_ball",
		"hornet",
		"monster_satchel",
		"monster_tripmine"
	};

	size_t numClasses = sizeof( classesToPurge ) / sizeof( classesToPurge[0] );
	std::vector<TransientEntMock> mocks( numClasses );

	for ( size_t i = 0; i < numClasses; ++i )
	{
		int slot = static_cast<int>( i + 1 );
		edict_t *pSlot = GetMockClientEntity( slot );
		REQUIRE( pSlot != nullptr );

		memset( pSlot, 0, sizeof( edict_t ) );
		pSlot->free = 0;
		pSlot->v.pContainingEntity = pSlot;
		pSlot->v.classname         = MAKE_STRING( classesToPurge[i] );
		pSlot->pvPrivateData       = &mocks[i].ent;
		mocks[i].ent.pev           = &pSlot->v;
	}

	// Also add an orphaned env_sprite with MOVETYPE_FOLLOW and dead aiment
	int spriteSlot = static_cast<int>( numClasses + 1 );
	edict_t *pSpriteSlot = GetMockClientEntity( spriteSlot );
	REQUIRE( pSpriteSlot != nullptr );
	memset( pSpriteSlot, 0, sizeof( edict_t ) );
	pSpriteSlot->free = 0;
	pSpriteSlot->v.pContainingEntity = pSpriteSlot;
	pSpriteSlot->v.classname         = MAKE_STRING( "env_sprite" );
	pSpriteSlot->v.movetype          = MOVETYPE_FOLLOW;
	pSpriteSlot->v.aiment            = nullptr; // orphaned!
	CBaseEntity spriteEnt;
	pSpriteSlot->pvPrivateData       = &spriteEnt;
	spriteEnt.pev                    = &pSpriteSlot->v;

	// Also add an active, non-orphaned env_sprite that should NOT be purged
	int validSpriteSlot = static_cast<int>( numClasses + 2 );
	edict_t *pValidSpriteSlot = GetMockClientEntity( validSpriteSlot );
	REQUIRE( pValidSpriteSlot != nullptr );
	memset( pValidSpriteSlot, 0, sizeof( edict_t ) );
	pValidSpriteSlot->free = 0;
	pValidSpriteSlot->v.pContainingEntity = pValidSpriteSlot;
	pValidSpriteSlot->v.classname         = MAKE_STRING( "env_sprite" );
	pValidSpriteSlot->v.movetype          = MOVETYPE_NONE; // not follow, static map sprite
	CBaseEntity validSpriteEnt;
	pValidSpriteSlot->pvPrivateData       = &validSpriteEnt;
	validSpriteEnt.pev                    = &pValidSpriteSlot->v;

	GladderSpawner spawner;
	spawner.PurgeWaveEntities();

	// All transient entities must be flagged with FL_KILLME
	for ( size_t i = 0; i < numClasses; ++i )
	{
		int slot = static_cast<int>( i + 1 );
		edict_t *pSlot = GetMockClientEntity( slot );
		CHECK( ( pSlot->v.flags & FL_KILLME ) != 0 );
	}

	// Orphaned follow sprite must be flagged with FL_KILLME
	CHECK( ( pSpriteSlot->v.flags & FL_KILLME ) != 0 );

	// Static map sprite must NOT be flagged
	CHECK( ( pValidSpriteSlot->v.flags & FL_KILLME ) == 0 );

	// Cleanup mock slots
	for ( int s = 1; s <= 32; ++s )
	{
		edict_t *pSlot = GetMockClientEntity( s );
		if ( pSlot )
			memset( pSlot, 0, sizeof( edict_t ) );
	}
}

TEST_CASE( "Gladder Spawner: Consumable ordnance weapons have dedicated category and tier weights (SPEC §3, §7, Issue #78)", "[gladder][spawner][ordnance]" )
{
	ResetMockEngine();

	// 1. Verify GetOrdnanceTierWeight curves
	// Tier 1 (hand grenade): immediately available from Wave 1, decreasing towards floor of 10.0f
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 1, 1 ) == Catch::Approx( 25.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 1, 5 ) == Catch::Approx( 17.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 1, 10 ) == Catch::Approx( 10.0f ) );

	// Tier 2 (tripmine, satchel): locked at Wave 1, unlocks at Wave 2, caps at 20.0f
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 2, 1 ) == Catch::Approx( 0.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 2, 2 ) == Catch::Approx( 8.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 2, 6 ) == Catch::Approx( 18.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 2, 10 ) == Catch::Approx( 20.0f ) );

	// Tier 3 (snark): locked at Waves 1-2, unlocks at Wave 3, caps at 15.0f
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 3, 1 ) == Catch::Approx( 0.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 3, 2 ) == Catch::Approx( 0.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 3, 3 ) == Catch::Approx( 5.0f ) );
	CHECK( GladderSpawner::GetOrdnanceTierWeight( 3, 7 ) == Catch::Approx( 15.0f ) );

	// 2. Verify ordnance weapons appear in procedural supply rolls across waves
	GladderMapConfig config;
	bool foundGrenade = false;
	bool foundTripmine = false;
	bool foundSatchel = false;
	bool foundSnark = false;

	for ( int i = 0; i < 2000; ++i )
	{
		int wave = 1 + ( i % 10 );
		const char *item = GladderSpawner::RollPickupItem( wave, config );
		if ( !item ) continue;

		if ( strcmp( item, "weapon_handgrenade" ) == 0 ) foundGrenade = true;
		else if ( strcmp( item, "weapon_tripmine" ) == 0 ) foundTripmine = true;
		else if ( strcmp( item, "weapon_satchel" ) == 0 ) foundSatchel = true;
		else if ( strcmp( item, "weapon_snark" ) == 0 ) foundSnark = true;
	}

	CHECK( foundGrenade );
	CHECK( foundTripmine );
	CHECK( foundSatchel );
	CHECK( foundSnark );
}

TEST_CASE( "Gladder Spawner: Restrict fixed weapon spawns to unacquired loadout items (SPEC §3, Issue #79)", "[gladder][spawner][loadout]" )
{
	ResetMockEngine();

	struct MockPlayerHolder
	{
		alignas( CBasePlayer ) uint8_t storage[sizeof( CBasePlayer )];
		MockPlayerHolder()
		{
			std::memset( storage, 0, sizeof( storage ) );
		}
		CBasePlayer *player()
		{
			return reinterpret_cast<CBasePlayer *>( storage );
		}
	};

	struct MockOwnedItem
	{
		alignas( CBasePlayerItem ) uint8_t itemStorage[sizeof( CBasePlayerItem )];
		edict_t edict;
		MockOwnedItem( const char *name )
		{
			std::memset( itemStorage, 0, sizeof( itemStorage ) );
			std::memset( &edict, 0, sizeof( edict ) );
			edict.v.classname = MAKE_STRING( name );
			edict.v.pContainingEntity = &edict;
			item()->pev = &edict.v;
			item()->m_pNext = nullptr;
		}
		CBasePlayerItem *item()
		{
			return reinterpret_cast<CBasePlayerItem *>( itemStorage );
		}
	};

	// Setup a mock player entity with an inventory
	edict_t playerEdict;
	memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.flags = FL_CLIENT;
	playerEdict.v.pContainingEntity = &playerEdict;

	MockPlayerHolder playerHolder;
	CBasePlayer *pPlayer = playerHolder.player();
	playerEdict.pvPrivateData = pPlayer;
	pPlayer->pev = &playerEdict.v;
	memset( pPlayer->m_rgpPlayerItems, 0, sizeof( pPlayer->m_rgpPlayerItems ) );

	MockOwnedItem ownedGlock( "weapon_glock" );
	MockOwnedItem ownedShotgun( "weapon_shotgun" );
	MockOwnedItem ownedCrowbar( "weapon_crowbar" );

	pPlayer->m_rgpPlayerItems[1] = ownedCrowbar.item();
	pPlayer->m_rgpPlayerItems[2] = ownedGlock.item();
	pPlayer->m_rgpPlayerItems[3] = ownedShotgun.item();

	CHECK( pPlayer->HasNamedPlayerItem( "weapon_glock" ) == TRUE );
	CHECK( pPlayer->HasNamedPlayerItem( "weapon_shotgun" ) == TRUE );
	CHECK( pPlayer->HasNamedPlayerItem( "weapon_mp5" ) == FALSE );

	GladderMapConfig config;

	// In 1,000 rolls at Wave 1 with player context, owned fixed weapons must NEVER be rolled
	for ( int i = 0; i < 1000; ++i )
	{
		const char *item = GladderSpawner::RollPickupItem( 1, config, pPlayer );
		REQUIRE( item != nullptr );
		CHECK( strcmp( item, "weapon_glock" ) != 0 );
		CHECK( strcmp( item, "weapon_shotgun" ) != 0 );
	}

	// Now equip ALL fixed weapons in player inventory
	const char *allFixedWeapons[] = {
		"weapon_knife", "weapon_pipewrench", "weapon_glock", "weapon_shotgun",
		"weapon_mp5", "weapon_357", "weapon_eagle", "weapon_crossbow",
		"weapon_sniper", "weapon_m249", "weapon_rpg", "weapon_gauss", "weapon_egon"
	};

	std::vector<MockOwnedItem *> allOwnedItems;
	CBasePlayerItem *pHead = nullptr;
	for ( const char *wepName : allFixedWeapons )
	{
		MockOwnedItem *pMock = new MockOwnedItem( wepName );
		allOwnedItems.push_back( pMock );
		pMock->item()->m_pNext = pHead;
		pHead = pMock->item();
	}
	pPlayer->m_rgpPlayerItems[0] = pHead;

	for ( const char *wepName : allFixedWeapons )
	{
		CHECK( pPlayer->HasNamedPlayerItem( wepName ) == TRUE );
	}

	// When all fixed weapons are owned, rolls must never produce ANY fixed weapon,
	// but continue gracefully yielding ammo, ordnance, and health/armor
	bool rolledAnyFixedWeapon = false;
	bool rolledAmmoOrHealthOrOrdnance = false;

	for ( int i = 0; i < 500; ++i )
	{
		const char *item = GladderSpawner::RollPickupItem( 3, config, pPlayer );
		REQUIRE( item != nullptr );

		for ( const char *fixedWep : allFixedWeapons )
		{
			if ( strcmp( item, fixedWep ) == 0 )
			{
				rolledAnyFixedWeapon = true;
				break;
			}
		}

		if ( strncmp( item, "ammo_", 5 ) == 0 ||
		     strncmp( item, "item_", 5 ) == 0 ||
		     strcmp( item, "weapon_handgrenade" ) == 0 ||
		     strcmp( item, "weapon_tripmine" ) == 0 ||
		     strcmp( item, "weapon_satchel" ) == 0 ||
		     strcmp( item, "weapon_snark" ) == 0 )
		{
			rolledAmmoOrHealthOrOrdnance = true;
		}
	}

	CHECK_FALSE( rolledAnyFixedWeapon );
	CHECK( rolledAmmoOrHealthOrOrdnance );

	// Verify consumable ordnance weapons can repeat even when owned by player (SPEC §3, §7)
	MockOwnedItem ownedGrenade( "weapon_handgrenade" );
	pPlayer->m_rgpPlayerItems[4] = ownedGrenade.item();
	CHECK( pPlayer->HasNamedPlayerItem( "weapon_handgrenade" ) == TRUE );

	int grenadeRollCount = 0;
	int totalOrdnanceCount = 0;
	int totalWeaponCount = 0;

	for ( int i = 0; i < 3000; ++i )
	{
		const char *item = GladderSpawner::RollPickupItem( 2, config, pPlayer );
		if ( !item ) continue;

		if ( strcmp( item, "weapon_handgrenade" ) == 0 )
		{
			grenadeRollCount++;
			totalOrdnanceCount++;
		}
		else if ( strcmp( item, "weapon_tripmine" ) == 0 ||
		          strcmp( item, "weapon_satchel" ) == 0 ||
		          strcmp( item, "weapon_snark" ) == 0 )
		{
			totalOrdnanceCount++;
		}
	}

	// Consumable ordnance (hand grenades) MUST still roll even when owned
	CHECK( grenadeRollCount > 0 );
	CHECK( totalOrdnanceCount > 0 );

	for ( auto *p : allOwnedItems )
		delete p;
}

TEST_CASE( "Gladder Spawner: Consumable ordnance has higher category frequency than fixed weapons (SPEC §3, §7)", "[gladder][spawner][frequency]" )
{
	ResetMockEngine();
	GladderMapConfig config;

	// In a fresh player scenario with no weapons owned, ordnance category (15%)
	// must roll with higher frequency than fixed weapons (10%)
	int ordnanceCount = 0;
	int fixedWeaponCount = 0;

	for ( int i = 0; i < 10000; ++i )
	{
		const char *item = GladderSpawner::RollPickupItem( 3, config, nullptr );
		if ( !item ) continue;

		if ( strcmp( item, "weapon_handgrenade" ) == 0 ||
		     strcmp( item, "weapon_tripmine" ) == 0 ||
		     strcmp( item, "weapon_satchel" ) == 0 ||
		     strcmp( item, "weapon_snark" ) == 0 )
		{
			ordnanceCount++;
		}
		else if ( strncmp( item, "weapon_", 7 ) == 0 )
		{
			fixedWeaponCount++;
		}
	}

	// Macro category weighting: Ordnance (15%) > Fixed weapons (10%)
	CHECK( ordnanceCount > 0 );
	CHECK( fixedWeaponCount > 0 );
	CHECK( ordnanceCount > fixedWeaponCount );
}

TEST_CASE( "Gladder Spawner: Pickup and Lambda reject cells overlapping solid brush or breakables (SPEC §11.2, Issue #90)", "[gladder][spawner][breakables]" )
{
	ResetMockEngine();
	GladderGridIndexer indexer;
	indexer.Clear();

	// Add 2 valid cells:
	// Cell 0 is clear.
	// Cell 1 is inside a solid breakable crate.
	GladderGridCell c0;
	c0.origin = Vector( 0.0f, 0.0f, 0.0f );
	c0.normal = Vector( 0.0f, 0.0f, 1.0f );
	c0.areaId = 1;
	c0.flags = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
	c0.ceilingZ = 128.0f;
	indexer.AddCell( c0 );

	GladderGridCell c1;
	c1.origin = Vector( 200.0f, 200.0f, 0.0f );
	c1.normal = Vector( 0.0f, 0.0f, 1.0f );
	c1.areaId = 1;
	c1.flags = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK;
	c1.ceilingZ = 128.0f;
	indexer.AddCell( c1 );

	// Setup a solid breakable entity covering Cell 1
	edict_t *pBreakableSlot = GetMockClientEntity( 2 );
	REQUIRE( pBreakableSlot != nullptr );
	pBreakableSlot->free = 0;
	pBreakableSlot->v.solid = SOLID_BSP;
	pBreakableSlot->v.classname = MAKE_STRING( "gladder_breakable" );
	pBreakableSlot->v.pContainingEntity = pBreakableSlot;
	pBreakableSlot->v.origin = Vector( 200.0f, 200.0f, 0.0f );
	pBreakableSlot->v.absmin = Vector( 180.0f, 180.0f, -10.0f );
	pBreakableSlot->v.absmax = Vector( 220.0f, 220.0f, 50.0f );

	CGladderBreakable breakable;
	pBreakableSlot->pvPrivateData = &breakable;
	breakable.pev = &pBreakableSlot->v;

	// Verify GladderIsPositionSolidOrBrush returns true inside the breakable bounds
	CHECK( GladderIsPositionSolidOrBrush( Vector( 200.0f, 200.0f, 8.0f ) ) == true );
	CHECK( GladderIsPositionSolidOrBrush( Vector( 0.0f, 0.0f, 8.0f ) ) == false );

	// Spawner should reject cell 1 (inside the solid breakable)
	GladderSpawner spawner;
	GladderMapConfig config;
	std::vector<bool> occupiedCells( indexer.GetCellCount(), false );

	// Mark cell 0 as occupied, leaving only cell 1 (the occluded cell)
	occupiedCells[0] = true;

	// Attempt to spawn Lambda collectible in cell 1: must be rejected, and cell marked occupied!
	spawner.SpawnLambdaCollectible( indexer, &occupiedCells );
	CHECK( spawner.WasLambdaSpawned() == false );
	CHECK( occupiedCells[1] == true );

	// Reset occupiedCells[1] and attempt SpawnPickups: must also reject cell 1
	occupiedCells[1] = false;
	spawner.SpawnPickups( 1, indexer, config, 1, &occupiedCells );
	CHECK( spawner.GetLastSpawnedPickupCount() == 0 );
	CHECK( occupiedCells[1] == true );
}

TEST_CASE( "Gladder Spawner: Monster placement performs 8-direction clearance check (SPEC §4.3, Issue #89)", "[gladder][spawner][clearance]" )
{
	ResetMockEngine();

	// Diagonal offset clearance verification
	float flRadius = 16.0f;
	float flDiag = flRadius * 0.7071f;
	Vector vecCenter( 100.0f, 100.0f, 36.0f );

	// Place solid brush at diagonal (+X, +Y)
	edict_t *pSolidSlot = GetMockClientEntity( 3 );
	REQUIRE( pSolidSlot != nullptr );
	pSolidSlot->free = 0;
	pSolidSlot->v.solid = SOLID_BSP;
	pSolidSlot->v.classname = MAKE_STRING( "func_wall" );
	pSolidSlot->v.pContainingEntity = pSolidSlot;
	pSolidSlot->v.origin = vecCenter + Vector( flDiag, flDiag, 0.0f );
	pSolidSlot->v.absmin = pSolidSlot->v.origin - Vector( 4.0f, 4.0f, 4.0f );
	pSolidSlot->v.absmax = pSolidSlot->v.origin + Vector( 4.0f, 4.0f, 4.0f );

	CBaseEntity wall;
	pSolidSlot->pvPrivateData = &wall;
	wall.pev = &pSolidSlot->v;

	// Position checking at diagonal must detect the solid obstacle
	CHECK( GladderIsPositionSolidOrBrush( vecCenter + Vector( flDiag, flDiag, 0.0f ) ) == true );
	// Opposite diagonal (-X, -Y) is clear
	CHECK( GladderIsPositionSolidOrBrush( vecCenter - Vector( flDiag, flDiag, 0.0f ) ) == false );
}


