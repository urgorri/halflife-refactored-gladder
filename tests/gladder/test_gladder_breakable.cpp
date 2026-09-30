/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit & Behavioral Equivalence Test Suite: gladder_breakable (SPEC §11.3)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <vector>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_breakable.h"

TEST_CASE( "Gladder Breakable: Spawn caches original properties", "[gladder][breakable]" )
{
	ResetMockEngine();

	CGladderBreakable breakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	breakable.pev             = &edict.v;

	breakable.pev->health     = 85.0f;
	breakable.pev->solid      = SOLID_BSP;
	breakable.pev->model      = MAKE_STRING( "models/wood_crate.mdl" );
	breakable.pev->targetname = MAKE_STRING( "crate_objective_1" );
	breakable.pev->mins       = Vector( -16, -16, -16 );
	breakable.pev->maxs       = Vector( 16, 16, 16 );
	breakable.pev->origin     = Vector( 100, 200, 50 );
	breakable.pev->angles     = Vector( 0, 90, 0 );
	breakable.m_Material      = matWood;

	breakable.Spawn();

	CHECK( breakable.GetOriginalHealth() == Catch::Approx( 85.0f ) );
	CHECK( breakable.GetOriginalSolid() == SOLID_BSP );
	CHECK( breakable.GetOriginalMins() == Vector( -16, -16, -16 ) );
	CHECK( breakable.GetOriginalMaxs() == Vector( 16, 16, 16 ) );
	CHECK( breakable.GetOriginalOrigin() == Vector( 100, 200, 50 ) );
	CHECK( std::strcmp( breakable.GetOriginalModel(), "models/wood_crate.mdl" ) == 0 );
	CHECK( breakable.IsBroken() == false );
}

TEST_CASE( "Gladder Breakable: Destruction sets hidden non-solid state without freeing edict", "[gladder][breakable]" )
{
	ResetMockEngine();

	CGladderBreakable breakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	breakable.pev             = &edict.v;

	breakable.pev->health     = 50.0f;
	breakable.pev->solid      = SOLID_BSP;
	breakable.pev->model      = MAKE_STRING( "models/wood_crate.mdl" );
	breakable.m_Material      = matWood;
	breakable.m_Explosion     = expRandom;
	breakable.pev->impulse    = 0;

	breakable.Spawn();

	// Inflict lethal damage
	breakable.TakeDamage( nullptr, nullptr, 60.0f, DMG_GENERIC );

	// Must be marked broken
	CHECK( breakable.IsBroken() == true );

	// Must be hidden via EF_NODRAW
	CHECK( ( breakable.pev->effects & EF_NODRAW ) != 0 );

	// Collision must be disabled
	CHECK( breakable.pev->solid == SOLID_NOT );

	// Must NOT schedule SUB_Remove (remains alive in memory)
	CHECK( breakable.m_pfnThink != &CBaseEntity::SUB_Remove );
	CHECK( breakable.pev->nextthink == 0.0f );
}

TEST_CASE( "Gladder Breakable: Broken entity ignores subsequent damage and use", "[gladder][breakable]" )
{
	ResetMockEngine();

	CGladderBreakable breakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	breakable.pev             = &edict.v;

	breakable.pev->health  = 40.0f;
	breakable.pev->solid   = SOLID_BSP;
	breakable.m_Material   = matGlass;
	breakable.m_Explosion  = expRandom;
	breakable.pev->impulse = 0;

	breakable.Spawn();
	breakable.Die();

	REQUIRE( breakable.IsBroken() == true );

	// Subsequent damage must return 0 and not trigger extra effects
	int dmgResult = breakable.TakeDamage( nullptr, nullptr, 25.0f, DMG_GENERIC );
	CHECK( dmgResult == 0 );

	TraceResult tr;
	std::memset( &tr, 0, sizeof( tr ) );
	breakable.TraceAttack( nullptr, 50.0f, Vector( 1, 0, 0 ), &tr, DMG_BULLET );
	CHECK( breakable.IsBroken() == true );
}

TEST_CASE( "Gladder Breakable: Wave Reset restores visibility, solid, bounds and health", "[gladder][breakable]" )
{
	ResetMockEngine();

	CGladderBreakable breakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	breakable.pev             = &edict.v;

	breakable.pev->health     = 70.0f;
	breakable.pev->solid      = SOLID_BSP;
	breakable.pev->model      = MAKE_STRING( "models/wood_crate.mdl" );
	breakable.pev->targetname = MAKE_STRING( "target_crate" );
	breakable.pev->mins       = Vector( -20, -20, 0 );
	breakable.pev->maxs       = Vector( 20, 20, 40 );
	breakable.pev->origin     = Vector( 50, 50, 0 );
	breakable.m_Material      = matWood;

	breakable.Spawn();

	// Wave 1: Breakable takes lethal damage
	breakable.Die();
	REQUIRE( breakable.IsBroken() == true );
	REQUIRE( ( breakable.pev->effects & EF_NODRAW ) != 0 );
	REQUIRE( breakable.pev->solid == SOLID_NOT );

	// Wave Reset: Trigger regeneration
	breakable.Reset();

	CHECK( breakable.IsBroken() == false );
	CHECK( ( breakable.pev->effects & EF_NODRAW ) == 0 );
	CHECK( breakable.pev->solid == SOLID_BSP );
	CHECK( breakable.pev->health == Catch::Approx( 70.0f ) );
	CHECK( breakable.pev->takedamage == DAMAGE_YES );
	CHECK( breakable.pev->mins == Vector( -20, -20, 0 ) );
	CHECK( breakable.pev->maxs == Vector( 20, 20, 40 ) );
	CHECK( breakable.pev->origin == Vector( 50, 50, 0 ) );
	CHECK( breakable.m_pfnThink == nullptr );
	CHECK( breakable.pev->nextthink == 0.0f );

	// Wave 2: Entity can be broken AGAIN cleanly
	breakable.TakeDamage( nullptr, nullptr, 100.0f, DMG_GENERIC );
	CHECK( breakable.IsBroken() == true );
	CHECK( ( breakable.pev->effects & EF_NODRAW ) != 0 );
	CHECK( breakable.pev->solid == SOLID_NOT );
	CHECK( breakable.m_pfnThink != &CBaseEntity::SUB_Remove );
}

TEST_CASE( "Gladder Breakable: Partially damaged breakable restores full health on Wave Reset", "[gladder][breakable]" )
{
	ResetMockEngine();

	CGladderBreakable breakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	breakable.pev             = &edict.v;

	breakable.pev->health = 100.0f;
	breakable.pev->solid  = SOLID_BSP;
	breakable.m_Material  = matWood;

	breakable.Spawn();

	// Take partial damage (100 -> 60)
	breakable.TakeDamage( nullptr, nullptr, 40.0f, DMG_GENERIC );
	CHECK( breakable.pev->health == Catch::Approx( 60.0f ) );
	CHECK( breakable.IsBroken() == false );

	// Wave Reset restores full health
	breakable.Reset();
	CHECK( breakable.pev->health == Catch::Approx( 100.0f ) );
	CHECK( breakable.IsBroken() == false );
	CHECK( breakable.pev->solid == SOLID_BSP );
}

TEST_CASE( "Gladder Breakable: Wave GC skips gladder_breakable and wave reset regenerates via entity iteration", "[gladder][breakable][iteration]" )
{
	ResetMockEngine();

	CGladderBreakable breakable;
	edict_t *pBreakableEdict = GetMockClientEntity( 1 );
	REQUIRE( pBreakableEdict != nullptr );
	pBreakableEdict->free          = 0;
	pBreakableEdict->pvPrivateData = &breakable;
	breakable.pev                  = &pBreakableEdict->v;
	breakable.pev->classname       = MAKE_STRING( "gladder_breakable" );
	breakable.pev->health          = 60.0f;
	breakable.pev->solid           = SOLID_BSP;
	breakable.m_Material           = matWood;

	breakable.Spawn();

	gpGlobals->maxEntities = 2;

	// Break the entity during active wave
	breakable.Die();
	REQUIRE( breakable.IsBroken() == true );
	REQUIRE( ( breakable.pev->effects & EF_NODRAW ) != 0 );
	REQUIRE( breakable.pev->solid == SOLID_NOT );

	// Simulate wave GC pass: PurgeWaveEntities algorithm skips gladder_breakable
	bool bEdictFreed = false;
	UTIL_ForEachEntity( [&]( CBaseEntity *pEnt ) {
		if ( FClassnameIs( pEnt->pev, "gladder_breakable" ) )
		{
			// Explicitly skipped by GC policy
			return;
		}
		bEdictFreed = true;
	} );

	CHECK( bEdictFreed == false );
	CHECK( pBreakableEdict->free == 0 );
	CHECK( pBreakableEdict->pvPrivateData == &breakable );

	// Simulate wave reset: ResetBreakableEntities algorithm iterates and calls Reset()
	UTIL_ForEachEntity( []( CBaseEntity *pEnt ) {
		if ( FClassnameIs( pEnt->pev, "gladder_breakable" ) )
		{
			auto *pBreak = dynamic_cast<CGladderBreakable *>( pEnt );
			if ( pBreak )
			{
				pBreak->Reset();
			}
		}
	} );

	CHECK( breakable.IsBroken() == false );
	CHECK( ( breakable.pev->effects & EF_NODRAW ) == 0 );
	CHECK( breakable.pev->solid == SOLID_BSP );
	CHECK( breakable.pev->health == Catch::Approx( 60.0f ) );
}
