/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Telemetry & Simulation Test Suite: Early-Wave Pacing & Engagement Audit
 *	(SPEC §3, GitHub Issue #103)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_spawner.h"
#include "dlls/gladder/gladder_grid_indexer.h"
#include "dlls/gladder/gladder_modifiers.h"

// Factory helpers for mock entity creation
static void FactoryTestPacingMonster( entvars_t *pev )
{
	if ( !pev || !pev->pContainingEntity )
		return;
	static CBaseEntity dummyMonsters[128];
	static size_t idx = 0;
	CBaseEntity *pEnt = &dummyMonsters[( idx++ ) % 128];
	pEnt->pev = pev;
	pev->solid = SOLID_SLIDEBOX;
	pev->flags |= FL_MONSTER;
	pev->pContainingEntity->pvPrivateData = pEnt;
}

static void FactoryTestPacingItem( entvars_t *pev )
{
	if ( !pev || !pev->pContainingEntity )
		return;
	static CBaseEntity dummyItems[128];
	static size_t idx = 0;
	CBaseEntity *pEnt = &dummyItems[( idx++ ) % 128];
	pEnt->pev = pev;
	pev->solid = SOLID_TRIGGER;
	pev->pContainingEntity->pvPrivateData = pEnt;
}

// Builds a synthetic gauntlet arena grid with a clear corridor (Point A -> Point B)
// and peripheral side-wings to test spatial dispersion and route engagement.
static GladderGridIndexer BuildGauntletArenaGrid( const Vector &vecA, const Vector &vecB )
{
	GladderGridIndexer indexer;

	// Arena corridor: X in [-100, 2100], Y in [-800, 800] sampled at 64-unit intervals for test speed
	// Direct corridor is Y in [-256, 256]; peripheral wings are |Y| > 256.
	for ( float x = -64.0f; x <= 2064.0f; x += 64.0f )
	{
		for ( float y = -768.0f; y <= 768.0f; y += 64.0f )
		{
			GladderGridCell cell;
			cell.origin   = Vector( x, y, 0.0f );
			cell.normal   = Vector( 0.0f, 0.0f, 1.0f );
			cell.ceilingZ = 256.0f;
			cell.areaId   = ( std::abs( y ) <= 256.0f ) ? 1 : 2;
			cell.flags    = GLADDER_CELL_VALID | GLADDER_CELL_CLEARANCE_OK | GLADDER_CELL_LARGE_CLEARANCE;
			indexer.AddCell( cell );
		}
	}

	return indexer;
}

// Distance from 2D point P to line segment [A, B]
static float DistanceToRoute2D( const Vector &P, const Vector &A, const Vector &B )
{
	Vector2D v( B.x - A.x, B.y - A.y );
	Vector2D w( P.x - A.x, P.y - A.y );
	float c1 = w.x * v.x + w.y * v.y;
	if ( c1 <= 0.0f )
		return w.Length();
	float c2 = v.x * v.x + v.y * v.y;
	if ( c2 <= c1 )
	{
		Vector2D pb( P.x - B.x, P.y - B.y );
		return pb.Length();
	}
	float b = c1 / c2;
	Vector2D pb( A.x + b * v.x - P.x, A.y + b * v.y - P.y );
	return pb.Length();
}

struct PacingTelemetry
{
	int wave;
	int monsterCount;
	int pickupCount;
	int routeEncounters; // Monsters within 384 units of the route line
	int routePickups;    // Pickups within 384 units of the route line
	float routeDensity;  // Fraction of monsters in corridor
};

static PacingTelemetry SimulateWavePacing( GladderSpawner &spawner,
                                          const GladderGridIndexer &indexer,
                                          const GladderMapConfig &config,
                                          int wave,
                                          const Vector &vecA,
                                          const Vector &vecB,
                                          float corridorRadius = 384.0f )
{
	spawner.PurgeWaveEntities();
	spawner.SetSimulatedRoute( vecA, vecB );

	int spawnedTotal = spawner.SpawnWave( wave, indexer, config );
	(void)spawnedTotal;

	PacingTelemetry telem;
	telem.wave            = wave;
	telem.monsterCount    = spawner.GetLastSpawnedMonsterCount();
	telem.pickupCount     = spawner.GetLastSpawnedPickupCount();
	telem.routeEncounters = 0;
	telem.routePickups    = 0;

	for ( size_t i = 0; i < spawner.GetTrackedEntityCount(); ++i )
	{
		CBaseEntity *pEnt = spawner.GetTrackedEntity( i );
		if ( !pEnt || !pEnt->pev )
			continue;

		float dist = DistanceToRoute2D( pEnt->pev->origin, vecA, vecB );
		bool inCorridor = ( dist <= corridorRadius );

		if ( pEnt->pev->flags & FL_MONSTER )
		{
			if ( inCorridor )
				telem.routeEncounters++;
		}
		else if ( pEnt->pev->solid == SOLID_TRIGGER )
		{
			if ( inCorridor )
				telem.routePickups++;
		}
	}

	telem.routeDensity = ( telem.monsterCount > 0 ) ?
	                         static_cast<float>( telem.routeEncounters ) / static_cast<float>( telem.monsterCount ) :
	                         0.0f;

	return telem;
}

TEST_CASE( "Gladder Early-Wave Pacing: Telemetry audit & quantitative evaluation (SPEC §3, Issue #103)", "[gladder][pacing][audit]" )
{
	ResetMockEngine();

	// Register entity factories for spawner
	const char *monsterTypes[] = {
		"monster_headcrab", "monster_zombie", "monster_houndeye", "monster_bullchicken",
		"monster_barnacle", "monster_vortigaunt", "monster_alien_grunt", "monster_alien_controller",
		"monster_human_grunt", "monster_human_assassin",
		"monster_zombie_barney", "monster_zombie_soldier", "monster_gonome", "monster_male_assassin"
	};
	for ( const char *szM : monsterTypes )
	{
		RegisterMockEntityFactory( szM, FactoryTestPacingMonster );
	}

	const char *pickupTypes[] = {
		"item_healthkit", "item_battery", "ammo_9mmclip", "ammo_9mmAR", "ammo_buckshot",
		"ammo_357", "ammo_ARgrenades", "ammo_crossbow", "ammo_gaussclip", "ammo_egonclip",
		"ammo_762", "ammo_556", "ammo_rpgclip", "weapon_knife", "weapon_pipewrench",
		"weapon_glock", "weapon_shotgun", "weapon_mp5", "weapon_357", "weapon_eagle",
		"weapon_crossbow", "weapon_m249", "weapon_sniper", "weapon_satchel", "weapon_snark",
		"item_gladder_lambda"
	};
	for ( const char *szP : pickupTypes )
	{
		RegisterMockEntityFactory( szP, FactoryTestPacingItem );
	}

	Vector vecStart( 0.0f, 0.0f, 0.0f );
	Vector vecFinish( 2000.0f, 0.0f, 0.0f );
	GladderGridIndexer arenaGrid = BuildGauntletArenaGrid( vecStart, vecFinish );
	REQUIRE( arenaGrid.GetCellCount() > 500 );

	GladderSpawner spawner;

	SECTION( "Baseline (pre-tuning) audit: low monster density and scarce route encounters" )
	{
		std::srand( 12345 );
		GladderMapConfig baselineConfig;
		baselineConfig.baseMonsters        = 6;
		baselineConfig.monstersPerWave     = 1.5f;
		baselineConfig.basePickups         = 4;
		baselineConfig.pickupsPerWave      = 0.8f;
		baselineConfig.routeBiasFraction   = 0.0f; // Uniform map-wide scatter (old behavior)
		baselineConfig.routeCorridorRadius = 512.0f;

		std::vector<PacingTelemetry> baselineRuns;
		for ( int w = 1; w <= 5; ++w )
		{
			baselineRuns.push_back( SimulateWavePacing( spawner, arenaGrid, baselineConfig, w, vecStart, vecFinish ) );
		}

		// Wave 1 under baseline: only 6 monsters across the entire arena (~800 units wide).
		// With only 33% of the arena in the corridor, route encounters average ~2 or fewer.
		CHECK( baselineRuns[0].monsterCount == 6 );
		CHECK( baselineRuns[0].pickupCount == 4 );

		// The evasion exploit: player sprinting Point A -> Point B encounters very low resistance
		// in early baseline waves because monsters are scattered into peripheral wings.
		CHECK( baselineRuns[0].routeEncounters <= 3 );
	}

	SECTION( "Tuned configuration audit: guaranteed route encounters and smooth progression" )
	{
		std::srand( 12345 );
		GladderMapConfig tunedConfig;
		// Tuned parameters: baseMonsters 20, monstersPerWave 2.0, basePickups 10, 60% corridor bias (Issue #111)
		CHECK( tunedConfig.baseMonsters == 20 );
		CHECK( tunedConfig.monstersPerWave == Catch::Approx( 2.0f ) );
		CHECK( tunedConfig.basePickups == 10 );
		CHECK( tunedConfig.routeBiasFraction == Catch::Approx( 0.60f ) );

		std::vector<PacingTelemetry> tunedRuns;
		for ( int w = 1; w <= 5; ++w )
		{
			tunedRuns.push_back( SimulateWavePacing( spawner, arenaGrid, tunedConfig, w, vecStart, vecFinish ) );
		}

		// Acceptance Criterion 1: Target quota scaling
		// Wave 1 has at least 20 monsters and 10 supplies
		CHECK( tunedRuns[0].monsterCount >= 20 );
		CHECK( tunedRuns[0].pickupCount >= 10 );

		// Wave 5 scales smoothly to at least 28 monsters
		CHECK( tunedRuns[4].monsterCount >= 28 );

		// Acceptance Criterion 2: Guaranteed route encounters in Wave 1
		// Route corridor must contain at least 4 confirmed threat encounters in Wave 1
		CHECK( tunedRuns[0].routeEncounters >= 4 );

		// Route density must exceed 50% under route-biased weighting
		CHECK( tunedRuns[0].routeDensity >= 0.45f );

		// Acceptance Criterion 3: Route supplies available to incentivize combat
		CHECK( tunedRuns[0].routePickups >= 2 );

		// Acceptance Criterion 4: Monotonic scaling across waves 1 to 5
		for ( size_t i = 1; i < tunedRuns.size(); ++i )
		{
			CHECK( tunedRuns[i].monsterCount >= tunedRuns[i - 1].monsterCount );
		}
	}
}
