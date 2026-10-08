/***
 *
 *	Behavioral Equivalence Verification - Wall Chargers Unit Tests
 *	Verifies func_healthcharger and func_recharge capacity limits, loop termination,
 *	and denial sound cue emission when player is at maximum capacity (#192).
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "dlls/systems/chargers.h"
#include "tests/mock_engine.h"

namespace
{

class CMockPlayer : public CBaseEntity
{
public:
	BOOL IsPlayer( void ) override { return TRUE; }
};

CBaseEntity *CreateMockPlayerWithSuit( float health, float maxHealth, float armor, BOOL takeDamage = DAMAGE_AIM )
{
	static edict_t playerEdict;
	memset( &playerEdict, 0, sizeof( playerEdict ) );
	playerEdict.v.flags |= FL_CLIENT;
	playerEdict.v.weapons |= ( 1 << WEAPON_SUIT );
	playerEdict.v.health     = health;
	playerEdict.v.max_health = maxHealth;
	playerEdict.v.armorvalue = armor;
	playerEdict.v.takedamage = takeDamage;

	static CMockPlayer playerEnt;
	playerEnt.pev = &playerEdict.v;
	return &playerEnt;
}

} // namespace

TEST_CASE( "Chargers: CanGiveResource evaluation for health and suit chargers", "[chargers][capacity]" )
{
	ResetMockEngine();

	CWallHealth healthCharger;
	CWallRecharge suitCharger;

	CBaseEntity *pInjuredPlayer = CreateMockPlayerWithSuit( 60.0f, 100.0f, 40.0f, DAMAGE_AIM );
	CHECK( healthCharger.CanGiveResource( pInjuredPlayer ) == TRUE );
	CHECK( suitCharger.CanGiveResource( pInjuredPlayer ) == TRUE );

	CBaseEntity *pFullPlayer = CreateMockPlayerWithSuit( 100.0f, 100.0f, 100.0f, DAMAGE_AIM );
	CHECK( healthCharger.CanGiveResource( pFullPlayer ) == FALSE );
	CHECK( suitCharger.CanGiveResource( pFullPlayer ) == FALSE );

	// Player with DAMAGE_NO (god mode) cannot take health
	CBaseEntity *pGodPlayer = CreateMockPlayerWithSuit( 80.0f, 100.0f, 50.0f, DAMAGE_NO );
	CHECK( healthCharger.CanGiveResource( pGodPlayer ) == FALSE );
	// But can still receive armor
	CHECK( suitCharger.CanGiveResource( pGodPlayer ) == TRUE );
}

TEST_CASE( "Chargers: Initial interaction by full-capacity player emits denial cue without starting loop", "[chargers][audio]" )
{
	ResetMockEngine();
	g_mockEmittedSounds.clear();
	gpGlobals->time = 10.0f;

	CWallHealth healthCharger;
	healthCharger.m_iJuice = 50;
	healthCharger.m_iOn    = 0;
	healthCharger.m_flSoundTime = 0.0f;
	edict_t chargerEdict;
	memset( &chargerEdict, 0, sizeof( chargerEdict ) );
	healthCharger.pev = &chargerEdict.v;

	CBaseEntity *pFullPlayer = CreateMockPlayerWithSuit( 100.0f, 100.0f, 100.0f, DAMAGE_AIM );

	// Player with full health interacts with wall charger
	healthCharger.Use( pFullPlayer, pFullPlayer, USE_SET, 0.0f );

	// Must NOT start charging loop
	CHECK( healthCharger.m_iOn == 0 );
	CHECK( healthCharger.m_iJuice == 50 );

	// Verify denial sound was emitted and no looping or startup sound was played
	bool heardDenial = false;
	bool heardLoop   = false;
	bool heardStart  = false;

	for ( const auto &call : g_mockEmittedSounds )
	{
		if ( call.sample == "items/medshotno1.wav" )
			heardDenial = true;
		if ( call.sample == "items/medcharge4.wav" )
			heardLoop = true;
		if ( call.sample == "items/medshot4.wav" )
			heardStart = true;
	}

	CHECK( heardDenial == true );
	CHECK( heardLoop == false );
	CHECK( heardStart == false );
}

TEST_CASE( "Chargers: Healing loop terminates and stops sound when capacity is reached during use", "[chargers][loop]" )
{
	ResetMockEngine();
	g_mockEmittedSounds.clear();
	gpGlobals->time = 10.0f;

	CWallHealth healthCharger;
	healthCharger.m_iJuice = 20;
	healthCharger.m_iOn    = 2; // Active looping charge state
	healthCharger.m_flSoundTime = 5.0f; // Sound time in the past
	healthCharger.m_flNextCharge = 9.0f;
	edict_t chargerEdict;
	memset( &chargerEdict, 0, sizeof( chargerEdict ) );
	healthCharger.pev = &chargerEdict.v;

	// Player is now at 100 HP (e.g. just reached full capacity on previous tick)
	CBaseEntity *pFullPlayer = CreateMockPlayerWithSuit( 100.0f, 100.0f, 50.0f, DAMAGE_AIM );

	// Next Use invocation while player holds +USE
	healthCharger.Use( pFullPlayer, pFullPlayer, USE_SET, 0.0f );

	// Charger must immediately transition Off
	CHECK( healthCharger.m_iOn == 0 );

	// Looping sound must have been stopped (STOP_SOUND emitted) and denial sound emitted
	bool stoppedLoop = false;
	bool heardDenial = false;

	for ( const auto &call : g_mockEmittedSounds )
	{
		if ( call.sample == "items/medcharge4.wav" && ( call.flags & SND_STOP ) )
			stoppedLoop = true;
		if ( call.sample == "items/medshotno1.wav" )
			heardDenial = true;
	}

	CHECK( stoppedLoop == true );
	CHECK( heardDenial == true );
}

TEST_CASE( "Chargers: Recharge HEV suit charger emits suitchargeno1 when armor is full", "[chargers][recharge]" )
{
	ResetMockEngine();
	g_mockEmittedSounds.clear();
	gpGlobals->time = 20.0f;

	CWallRecharge suitCharger;
	suitCharger.m_iJuice = 30;
	suitCharger.m_iOn    = 0;
	suitCharger.m_flSoundTime = 0.0f;
	edict_t suitChargerEdict;
	memset( &suitChargerEdict, 0, sizeof( suitChargerEdict ) );
	suitCharger.pev = &suitChargerEdict.v;

	CBaseEntity *pFullArmorPlayer = CreateMockPlayerWithSuit( 50.0f, 100.0f, 100.0f, DAMAGE_AIM );

	suitCharger.Use( pFullArmorPlayer, pFullArmorPlayer, USE_SET, 0.0f );

	CHECK( suitCharger.m_iOn == 0 );
	CHECK( suitCharger.m_iJuice == 30 );

	bool heardDenial = false;
	for ( const auto &call : g_mockEmittedSounds )
	{
		if ( call.sample == "items/suitchargeno1.wav" )
			heardDenial = true;
	}

	CHECK( heardDenial == true );
}
