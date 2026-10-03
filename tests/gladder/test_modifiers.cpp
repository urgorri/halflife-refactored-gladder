/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: GladderModifiers (Skill, Champions, Mutators, Chargers) (SPEC §3, Issue #10)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/basemonster.h"
#include "core/skill.h"
#include "core/skill_manager.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_modifiers.h"

// Test dummy monster
class CTestModifierMonster : public CBaseMonster
{
  public:
	CTestModifierMonster( float initialHealth = 100.0f )
	{
		pev = new entvars_t();
		memset( pev, 0, sizeof( entvars_t ) );
		pev->health     = initialHealth;
		pev->max_health = initialHealth;
		pev->framerate  = 1.0f;
	}

	~CTestModifierMonster()
	{
		if ( pev )
		{
			delete pev;
			pev = nullptr;
		}
	}
};

TEST_CASE( "GladderModifiers: Dynamic engine skill progression milestones", "[gladder][modifiers][skill]" )
{
	// Wave 1-4: SKILL_EASY
	CHECK( GladderModifiers::CalculateSkillTier( 1 ) == SKILL_EASY );
	CHECK( GladderModifiers::CalculateSkillTier( 2 ) == SKILL_EASY );
	CHECK( GladderModifiers::CalculateSkillTier( 4 ) == SKILL_EASY );

	// Wave 5-9: SKILL_MEDIUM
	CHECK( GladderModifiers::CalculateSkillTier( 5 ) == SKILL_MEDIUM );
	CHECK( GladderModifiers::CalculateSkillTier( 7 ) == SKILL_MEDIUM );
	CHECK( GladderModifiers::CalculateSkillTier( 9 ) == SKILL_MEDIUM );

	// Wave 10+: SKILL_HARD
	CHECK( GladderModifiers::CalculateSkillTier( 10 ) == SKILL_HARD );
	CHECK( GladderModifiers::CalculateSkillTier( 15 ) == SKILL_HARD );
	CHECK( GladderModifiers::CalculateSkillTier( 50 ) == SKILL_HARD );
}

TEST_CASE( "GladderModifiers: Elite Champion monster tagging and stat scaling", "[gladder][modifiers][champion]" )
{
	ResetMockEngine();

	// Champion spawn chance scaling
	CHECK( GladderModifiers::GetChampionSpawnChance( 1 ) == Catch::Approx( 0.0f ) );
	CHECK( GladderModifiers::GetChampionSpawnChance( 2 ) == Catch::Approx( 0.0f ) );
	CHECK( GladderModifiers::GetChampionSpawnChance( 3 ) == Catch::Approx( 0.10f ) );
	CHECK( GladderModifiers::GetChampionSpawnChance( 6 ) == Catch::Approx( 0.20f ) );
	CHECK( GladderModifiers::GetChampionSpawnChance( 10 ) == Catch::Approx( 0.30f ) );
	CHECK( GladderModifiers::GetChampionSpawnChance( 20 ) == Catch::Approx( 0.35f ) );

	CTestModifierMonster monster( 120.0f );
	CHECK_FALSE( GladderModifiers::IsEliteChampion( &monster ) );

	// Tag monster as Elite Champion
	GladderModifiers::MakeEliteChampion( &monster );

	CHECK( GladderModifiers::IsEliteChampion( &monster ) );
	CHECK( monster.pev->renderfx == kRenderFxGlowShell );
	CHECK( monster.pev->rendercolor.x >= 200.0f ); // Vibrant red
	CHECK( monster.pev->rendercolor.y <= 64.0f );
	CHECK( monster.pev->renderamt == Catch::Approx( 16.0f ) );
	CHECK( monster.pev->health == Catch::Approx( 120.0f * 2.5f ) ); // 300 HP
	CHECK( monster.pev->max_health == Catch::Approx( 300.0f ) );
	CHECK( monster.pev->framerate == Catch::Approx( 1.25f ) );
}

TEST_CASE( "GladderModifiers: Diminishing wall chargers degradation curves", "[gladder][modifiers][chargers]" )
{
	// Health Charger: 50 base, -10 per wave down to floor 20
	CHECK( GladderModifiers::CalculateHealthChargerCapacity( 1, 50.0f ) == Catch::Approx( 50.0f ) );
	CHECK( GladderModifiers::CalculateHealthChargerCapacity( 2, 50.0f ) == Catch::Approx( 40.0f ) );
	CHECK( GladderModifiers::CalculateHealthChargerCapacity( 3, 50.0f ) == Catch::Approx( 30.0f ) );
	CHECK( GladderModifiers::CalculateHealthChargerCapacity( 4, 50.0f ) == Catch::Approx( 20.0f ) );
	CHECK( GladderModifiers::CalculateHealthChargerCapacity( 5, 50.0f ) == Catch::Approx( 20.0f ) ); // Clamped at 20
	CHECK( GladderModifiers::CalculateHealthChargerCapacity( 15, 50.0f ) == Catch::Approx( 20.0f ) );

	// HEV Suit Charger: 75 base, -15 per wave down to floor 25
	CHECK( GladderModifiers::CalculateHEVChargerCapacity( 1, 75.0f ) == Catch::Approx( 75.0f ) );
	CHECK( GladderModifiers::CalculateHEVChargerCapacity( 2, 75.0f ) == Catch::Approx( 60.0f ) );
	CHECK( GladderModifiers::CalculateHEVChargerCapacity( 3, 75.0f ) == Catch::Approx( 45.0f ) );
	CHECK( GladderModifiers::CalculateHEVChargerCapacity( 4, 75.0f ) == Catch::Approx( 30.0f ) );
	CHECK( GladderModifiers::CalculateHEVChargerCapacity( 5, 75.0f ) == Catch::Approx( 25.0f ) ); // Clamped at 25
	CHECK( GladderModifiers::CalculateHEVChargerCapacity( 10, 75.0f ) == Catch::Approx( 25.0f ) );
}

TEST_CASE( "GladderModifiers: Special wave mutator activation and reversion", "[gladder][modifiers][mutators]" )
{
	ResetMockEngine();
	GladderModifiers modifiers;

	// Normal waves (e.g. 1, 2, 3, 4) do not activate mutators
	CHECK( modifiers.RollAndApplyWaveMutator( 1 ) == GLADDER_MUTATOR_NONE );
	CHECK( modifiers.RollAndApplyWaveMutator( 2 ) == GLADDER_MUTATOR_NONE );
	CHECK( modifiers.RollAndApplyWaveMutator( 3 ) == GLADDER_MUTATOR_NONE );
	CHECK( modifiers.RollAndApplyWaveMutator( 4 ) == GLADDER_MUTATOR_NONE );
	CHECK( modifiers.GetActiveMutator() == GLADDER_MUTATOR_NONE );

	// Wave 5 is a milestone wave -> rolls a mutator
	GladderMutatorType mut = modifiers.RollAndApplyWaveMutator( 5 );
	CHECK( mut != GLADDER_MUTATOR_NONE );
	CHECK( modifiers.GetActiveMutator() == mut );

	// Test explicit application
	modifiers.ApplyMutator( GLADDER_MUTATOR_BLACKOUT, 5 );
	CHECK( modifiers.GetActiveMutator() == GLADDER_MUTATOR_BLACKOUT );
	CHECK( std::string( modifiers.GetMutatorName() ) == "BLACKOUT (PITCH DARK)" );

	modifiers.ApplyMutator( GLADDER_MUTATOR_LOW_GRAVITY, 5 );
	CHECK( modifiers.GetActiveMutator() == GLADDER_MUTATOR_LOW_GRAVITY );
	CHECK( std::string( modifiers.GetMutatorName() ) == "LOW GRAVITY (XEN PHYSICS)" );

	modifiers.ApplyMutator( GLADDER_MUTATOR_SWARM, 5 );
	CHECK( modifiers.GetActiveMutator() == GLADDER_MUTATOR_SWARM );
	CHECK_FALSE( modifiers.GetSwarmSpecies().empty() );

	// Clean reversion
	modifiers.RevertMutators();
	CHECK( modifiers.GetActiveMutator() == GLADDER_MUTATOR_NONE );
	CHECK( modifiers.GetSwarmSpecies().empty() );
}

TEST_CASE( "GladderModifiers: Wall station active glow shell and depletion extinction (SPEC §3, §7.2, Issue #46)", "[gladder][modifiers][chargers]" )
{
	ResetMockEngine();
	gpGlobals->maxEntities = 3;

	CBaseEntity healthCharger;
	edict_t *pEdHealth = GetMockClientEntity( 1 );
	REQUIRE( pEdHealth != nullptr );
	memset( pEdHealth, 0, sizeof( edict_t ) );
	pEdHealth->free                = 0;
	pEdHealth->v.pContainingEntity = pEdHealth;
	pEdHealth->pvPrivateData       = &healthCharger;
	pEdHealth->v.classname         = MAKE_STRING( "func_healthcharger" );
	healthCharger.pev              = &pEdHealth->v;
	healthCharger.pev->frame       = 0;

	CBaseEntity hevCharger;
	edict_t *pEdHEV = GetMockClientEntity( 2 );
	REQUIRE( pEdHEV != nullptr );
	memset( pEdHEV, 0, sizeof( edict_t ) );
	pEdHEV->free                = 0;
	pEdHEV->v.pContainingEntity = pEdHEV;
	pEdHEV->pvPrivateData       = &hevCharger;
	pEdHEV->v.classname         = MAKE_STRING( "func_recharge" );
	hevCharger.pev              = &pEdHEV->v;
	hevCharger.pev->frame       = 0;

	SECTION( "RechargeWallStations applies green/amber glow shells to active stations" )
	{
		GladderModifiers::RechargeWallStations();

		// Health charger -> Vibrant Green glow shell
		CHECK( healthCharger.pev->renderfx == kRenderFxGlowShell );
		CHECK( healthCharger.pev->rendercolor.x == Catch::Approx( 32.0f ) );
		CHECK( healthCharger.pev->rendercolor.y == Catch::Approx( 255.0f ) );
		CHECK( healthCharger.pev->rendercolor.z == Catch::Approx( 32.0f ) );
		CHECK( healthCharger.pev->renderamt == Catch::Approx( 16.0f ) );

		// HEV charger -> Vibrant Amber/Orange glow shell
		CHECK( hevCharger.pev->renderfx == kRenderFxGlowShell );
		CHECK( hevCharger.pev->rendercolor.x == Catch::Approx( 255.0f ) );
		CHECK( hevCharger.pev->rendercolor.y == Catch::Approx( 140.0f ) );
		CHECK( hevCharger.pev->rendercolor.z == Catch::Approx( 20.0f ) );
		CHECK( hevCharger.pev->renderamt == Catch::Approx( 16.0f ) );
	}

	SECTION( "UpdateWallStations extinguishes glow when station is depleted (frame == 1)" )
	{
		GladderModifiers::RechargeWallStations();

		// Deplete stations
		healthCharger.pev->frame = 1;
		hevCharger.pev->frame    = 1;

		GladderModifiers::UpdateWallStations();

		// Glow shell extinguished
		CHECK( healthCharger.pev->renderfx == kRenderFxNone );
		CHECK( healthCharger.pev->renderamt == Catch::Approx( 0.0f ) );

		CHECK( hevCharger.pev->renderfx == kRenderFxNone );
		CHECK( hevCharger.pev->renderamt == Catch::Approx( 0.0f ) );

		// Re-energize stations -> glow restored
		healthCharger.pev->frame = 0;
		hevCharger.pev->frame    = 0;

		GladderModifiers::UpdateWallStations();

		CHECK( healthCharger.pev->renderfx == kRenderFxGlowShell );
		CHECK( healthCharger.pev->renderamt == Catch::Approx( 16.0f ) );
		CHECK( hevCharger.pev->renderfx == kRenderFxGlowShell );
		CHECK( hevCharger.pev->renderamt == Catch::Approx( 16.0f ) );
	}

	memset( pEdHealth, 0, sizeof( edict_t ) );
	memset( pEdHEV, 0, sizeof( edict_t ) );
}

