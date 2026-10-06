#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/func_break.h"
#include "systems/explode.h"
#include "tests/mock_engine.h"

// Subclass demonstrating downstream override capability (#162)
class CTestBreakableSubclass : public CBreakable
{
  public:
	bool m_bCustomDieInvoked = false;

	void Die( void )
	{
		m_bCustomDieInvoked = true;
	}
};

TEST_CASE( "Breakable: CBreakable::Die virtual dispatch allows subclass override (#162)", "[systems][breakable][virtual]" )
{
	ResetMockEngine();

	CTestBreakableSubclass testBreakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	testBreakable.pev = &edict.v;
	testBreakable.m_Material = matGlass;
	testBreakable.pev->health = 0;
	testBreakable.m_iszSpawnObject = 0;
	testBreakable.m_Explosion = expRandom;
	testBreakable.pev->impulse = 0; // ExplosionMagnitude()

	// Call Die() polymorphically through base class pointer
	CBreakable *pBase = &testBreakable;
	pBase->Die();

	// When CBreakable::Die is virtual, the subclass override is invoked.
	// In the red phase (non-virtual), pBase->Die() statically invokes CBreakable::Die()
	// and m_bCustomDieInvoked remains false.
	CHECK( testBreakable.m_bCustomDieInvoked == true );
}

TEST_CASE( "Breakable: CBreakable::Die base execution cleans up entity", "[systems][breakable]" )
{
	ResetMockEngine();

	CBreakable breakable;
	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	edict.v.pContainingEntity = &edict;
	breakable.pev = &edict.v;
	breakable.m_Material = matGlass;
	breakable.pev->health = 0;
	breakable.m_iszSpawnObject = 0;
	breakable.m_Explosion = expRandom;
	breakable.pev->impulse = 0;

	breakable.Die();

	// Canonical base Die() sets think to SUB_Remove with a 0.1s delay
	CHECK( breakable.m_pfnThink == &CBaseEntity::SUB_Remove );
	CHECK( breakable.pev->nextthink > 0.0f );
}

TEST_CASE( "Breakable: Lethal damage propagates attacker ownership to explosion (#184)", "[systems][breakable][combat][attribution]" )
{
	ResetMockEngine();

	edict_t playerEdict;
	std::memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.pContainingEntity = &playerEdict;
	playerEdict.v.flags             = FL_CLIENT;

	CBreakable breakable;
	edict_t breakableEdict;
	std::memset( &breakableEdict, 0, sizeof( breakableEdict ) );
	breakableEdict.v.pContainingEntity = &breakableEdict;
	breakable.pev                      = &breakableEdict.v;
	breakable.m_Material               = matWood;
	breakable.pev->health              = 100;
	breakable.pev->takedamage          = DAMAGE_YES;
	breakable.m_iszSpawnObject         = 0;
	breakable.m_Explosion              = expRandom;
	breakable.ExplosionSetMagnitude( 75 );

	// Lethal damage from player
	int result = breakable.TakeDamage( &playerEdict.v, &playerEdict.v, 150.0f, DMG_GENERIC );
	CHECK( result == 0 );
	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &playerEdict.v );
	CHECK( g_mockRadiusDamageCalls.back().flDamage == Catch::Approx( 75.0f ) );
}

TEST_CASE( "Breakable: Multi-attacker damage credits fatal attacker for explosion (#184)", "[systems][breakable][combat][attribution]" )
{
	ResetMockEngine();

	edict_t attacker1;
	std::memset( &attacker1, 0, sizeof( attacker1 ) );
	attacker1.v.pContainingEntity = &attacker1;
	attacker1.v.flags             = FL_CLIENT;

	edict_t attacker2;
	std::memset( &attacker2, 0, sizeof( attacker2 ) );
	attacker2.v.pContainingEntity = &attacker2;
	attacker2.v.flags             = FL_CLIENT;

	CBreakable breakable;
	edict_t breakableEdict;
	std::memset( &breakableEdict, 0, sizeof( breakableEdict ) );
	breakableEdict.v.pContainingEntity = &breakableEdict;
	breakable.pev                      = &breakableEdict.v;
	breakable.m_Material               = matWood;
	breakable.pev->health              = 100;
	breakable.pev->takedamage          = DAMAGE_YES;
	breakable.m_iszSpawnObject         = 0;
	breakable.m_Explosion              = expRandom;
	breakable.ExplosionSetMagnitude( 90 );

	// Attacker 1 deals non-lethal damage (50 dmg -> 50 HP left)
	int res1 = breakable.TakeDamage( &attacker1.v, &attacker1.v, 50.0f, DMG_GENERIC );
	CHECK( res1 == 1 );
	CHECK( g_mockRadiusDamageCalls.empty() );

	// Attacker 2 deals lethal finishing damage (60 dmg -> dies)
	int res2 = breakable.TakeDamage( &attacker2.v, &attacker2.v, 60.0f, DMG_GENERIC );
	CHECK( res2 == 0 );
	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &attacker2.v );
	CHECK( g_mockRadiusDamageCalls.back().flDamage == Catch::Approx( 90.0f ) );
}

TEST_CASE( "Breakable: Lethal damage without attacker falls back to breakable edict (#184)", "[systems][breakable][combat][attribution]" )
{
	ResetMockEngine();

	edict_t inflictor;
	std::memset( &inflictor, 0, sizeof( inflictor ) );
	inflictor.v.pContainingEntity = &inflictor;

	CBreakable breakable;
	edict_t breakableEdict;
	std::memset( &breakableEdict, 0, sizeof( breakableEdict ) );
	breakableEdict.v.pContainingEntity = &breakableEdict;
	breakable.pev                      = &breakableEdict.v;
	breakable.m_Material               = matWood;
	breakable.pev->health              = 50;
	breakable.pev->takedamage          = DAMAGE_YES;
	breakable.m_iszSpawnObject         = 0;
	breakable.m_Explosion              = expRandom;
	breakable.ExplosionSetMagnitude( 60 );

	int result = breakable.TakeDamage( &inflictor.v, NULL, 100.0f, DMG_CRUSH );
	CHECK( result == 0 );
	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	// When pevAttacker is NULL, falls back to breakable itself
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &breakableEdict.v );
}

class CTestPlayerEntity : public CBaseEntity
{
  public:
	BOOL IsPlayer( void ) override { return TRUE; }
};

TEST_CASE( "Breakable: Triggered Use passes activator as explosion attacker (#184)", "[systems][breakable][combat][attribution]" )
{
	ResetMockEngine();

	edict_t playerEdict;
	std::memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.pContainingEntity = &playerEdict;
	playerEdict.v.flags             = FL_CLIENT;
	CTestPlayerEntity player;
	player.pev                      = &playerEdict.v;
	playerEdict.pvPrivateData       = &player;

	CBreakable breakable;
	edict_t breakableEdict;
	std::memset( &breakableEdict, 0, sizeof( breakableEdict ) );
	breakableEdict.v.pContainingEntity = &breakableEdict;
	breakable.pev                      = &breakableEdict.v;
	breakable.m_Material               = matWood;
	breakable.pev->health              = 100;
	breakable.m_iszSpawnObject         = 0;
	breakable.m_Explosion              = expRandom;
	breakable.ExplosionSetMagnitude( 50 );

	breakable.Use( &player, &player, USE_TOGGLE, 0.0f );

	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &playerEdict.v );
}

TEST_CASE( "Breakable: BreakTouch on pressure plate passes toucher as explosion attacker (#184)", "[systems][breakable][combat][attribution]" )
{
	ResetMockEngine();

	edict_t playerEdict;
	std::memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.pContainingEntity = &playerEdict;
	playerEdict.v.flags             = FL_CLIENT;
	playerEdict.v.absmin.z          = 10.0f;
	CTestPlayerEntity player;
	player.pev                      = &playerEdict.v;
	playerEdict.pvPrivateData       = &player;

	CBreakable breakable;
	edict_t breakableEdict;
	std::memset( &breakableEdict, 0, sizeof( breakableEdict ) );
	breakableEdict.v.pContainingEntity = &breakableEdict;
	breakable.pev                      = &breakableEdict.v;
	breakable.m_Material               = matWood;
	breakable.pev->health              = 100;
	breakable.pev->maxs.z              = 10.0f;
	breakable.pev->spawnflags          = SF_BREAK_PRESSURE;
	breakable.m_iszSpawnObject         = 0;
	breakable.m_Explosion              = expRandom;
	breakable.ExplosionSetMagnitude( 85 );

	breakable.BreakTouch( &player );

	// BreakTouch schedules Die() on think
	CHECK( breakable.m_pfnThink == &CBreakable::Die );
	( breakable.*breakable.m_pfnThink )();

	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &playerEdict.v );
}

TEST_CASE( "Breakable: Freed attacker gracefully falls back to breakable edict (#184)", "[systems][breakable][combat][attribution]" )
{
	ResetMockEngine();

	edict_t tempEdict;
	std::memset( &tempEdict, 0, sizeof( tempEdict ) );
	tempEdict.v.pContainingEntity = &tempEdict;
	CBaseEntity *pTemp            = (CBaseEntity *)std::calloc( 1, sizeof( CBaseEntity ) );
	pTemp->pev                    = &tempEdict.v;
	tempEdict.pvPrivateData       = pTemp;

	CBreakable breakable;
	edict_t breakableEdict;
	std::memset( &breakableEdict, 0, sizeof( breakableEdict ) );
	breakableEdict.v.pContainingEntity = &breakableEdict;
	breakable.pev                      = &breakableEdict.v;
	breakable.m_Material               = matWood;
	breakable.pev->health              = 100;
	breakable.m_iszSpawnObject         = 0;
	breakable.m_Explosion              = expRandom;
	breakable.ExplosionSetMagnitude( 40 );

	breakable.m_hAttacker = pTemp;
	// Attacker entity is freed before Die() runs
	tempEdict.free = 1;

	breakable.Die();

	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &breakableEdict.v );

	std::free( pTemp );
}

TEST_CASE( "Explosion: CEnvExplosion::Use resolves attacker from owner and clears owner to allow damage (#184)", "[systems][explode][attribution]" )
{
	ResetMockEngine();

	edict_t playerEdict;
	std::memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.pContainingEntity = &playerEdict;

	CEnvExplosion explosion;
	edict_t explosionEdict;
	std::memset( &explosionEdict, 0, sizeof( explosionEdict ) );
	explosionEdict.v.pContainingEntity = &explosionEdict;
	explosion.pev                      = &explosionEdict.v;
	explosion.m_iMagnitude             = 120;
	explosion.pev->owner               = &playerEdict;

	explosion.Use( NULL, NULL, USE_TOGGLE, 0.0f );

	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &playerEdict.v );
	CHECK( g_mockRadiusDamageCalls.back().pevInflictor == &explosionEdict.v );
	CHECK( g_mockRadiusDamageCalls.back().flDamage == Catch::Approx( 120.0f ) );
	// Owner must be cleared to ensure owner is not immune to explosion blast
	CHECK( explosion.pev->owner == nullptr );
}

TEST_CASE( "Explosion: CEnvExplosion with SF_ENVEXPLOSION_REPEATABLE restores owner after damage (#184)", "[systems][explode][attribution]" )
{
	ResetMockEngine();

	edict_t playerEdict;
	std::memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.pContainingEntity = &playerEdict;

	CEnvExplosion explosion;
	edict_t explosionEdict;
	std::memset( &explosionEdict, 0, sizeof( explosionEdict ) );
	explosionEdict.v.pContainingEntity = &explosionEdict;
	explosion.pev                      = &explosionEdict.v;
	explosion.m_iMagnitude             = 100;
	explosion.pev->spawnflags          = SF_ENVEXPLOSION_REPEATABLE;
	explosion.pev->owner               = &playerEdict;

	explosion.Use( NULL, NULL, USE_TOGGLE, 0.0f );

	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &playerEdict.v );
	// Repeatable explosion restores owner for subsequent uses
	CHECK( explosion.pev->owner == &playerEdict );
}

TEST_CASE( "Explosion: CEnvExplosion without owner maintains backward-compatible map trigger attribution (#184)", "[systems][explode][attribution]" )
{
	ResetMockEngine();

	CEnvExplosion explosion;
	edict_t explosionEdict;
	std::memset( &explosionEdict, 0, sizeof( explosionEdict ) );
	explosionEdict.v.pContainingEntity = &explosionEdict;
	explosion.pev                      = &explosionEdict.v;
	explosion.m_iMagnitude             = 80;
	explosion.pev->owner               = nullptr;

	explosion.Use( NULL, NULL, USE_TOGGLE, 0.0f );

	REQUIRE( !g_mockRadiusDamageCalls.empty() );
	// When owner is null, attacker defaults to explosion entity itself
	CHECK( g_mockRadiusDamageCalls.back().pevAttacker == &explosionEdict.v );
	CHECK( g_mockRadiusDamageCalls.back().pevInflictor == &explosionEdict.v );
}
