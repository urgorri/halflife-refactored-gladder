/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: GladderMonsterModifiers & Snappy Yawspeed (SPEC §3, Issue #26)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/basemonster.h"
#include "tests/mock_engine.h"
#include "dlls/gladder/gladder_monster_modifiers.h"
#include "dlls/gladder/gladder_modifiers.h"
#include "dlls/gameplay/gamerules.h"

// Test dummy monster
class CTestYawMonster : public CBaseMonster
{
  public:
	CTestYawMonster( const char *szClassname )
	{
		pev = new entvars_t();
		memset( pev, 0, sizeof( entvars_t ) );
		pev->classname = MAKE_STRING( szClassname );
	}

	~CTestYawMonster()
	{
		if ( pev )
		{
			delete pev;
			pev = nullptr;
		}
	}
};

TEST_CASE( "GladderMonsterModifiers: Species specific modern turning rates (SPEC §3, Issue #26)", "[gladder][yawspeed]" )
{
	// Headcrab: default ~30 -> modern >= 80 deg/s
	CTestYawMonster crab( "monster_headcrab" );
	float crabYaw = GladderMonsterModifiers::GetModernYawSpeed( &crab, 30.0f );
	CHECK( crabYaw == Catch::Approx( 80.0f ) );

	// Zombie: default ~30 -> modern >= 75 deg/s
	CTestYawMonster zombie( "monster_zombie" );
	float zombieYaw = GladderMonsterModifiers::GetModernYawSpeed( &zombie, 30.0f );
	CHECK( zombieYaw == Catch::Approx( 75.0f ) );

	// Houndeye: default ~45 -> modern >= 120 deg/s
	CTestYawMonster hound( "monster_houndeye" );
	float houndYaw = GladderMonsterModifiers::GetModernYawSpeed( &hound, 45.0f );
	CHECK( houndYaw == Catch::Approx( 120.0f ) );

	// Bullsquid: default ~40 -> modern >= 100 deg/s
	CTestYawMonster squid( "monster_bullchicken" );
	float squidYaw = GladderMonsterModifiers::GetModernYawSpeed( &squid, 40.0f );
	CHECK( squidYaw == Catch::Approx( 100.0f ) );

	// HECU Grunt: default ~40 -> modern >= 115 deg/s
	CTestYawMonster grunt( "monster_human_grunt" );
	float gruntYaw = GladderMonsterModifiers::GetModernYawSpeed( &grunt, 40.0f );
	CHECK( gruntYaw == Catch::Approx( 115.0f ) );

	// Black Ops Assassin: default ~60 -> modern >= 200 deg/s
	CTestYawMonster assassin( "monster_human_assassin" );
	float assassinYaw = GladderMonsterModifiers::GetModernYawSpeed( &assassin, 60.0f );
	CHECK( assassinYaw == Catch::Approx( 200.0f ) );
}

TEST_CASE( "GladderMonsterModifiers: Exempt species preserve vanilla yawspeed", "[gladder][yawspeed]" )
{
	// Barnacle (ceiling attached): unchanged
	CTestYawMonster barnacle( "monster_barnacle" );
	CHECK( GladderMonsterModifiers::IsExemptSpecies( "monster_barnacle" ) );
	CHECK( GladderMonsterModifiers::GetModernYawSpeed( &barnacle, 30.0f ) == Catch::Approx( 30.0f ) );

	// Tentacle: unchanged
	CTestYawMonster tentacle( "monster_tentacle" );
	CHECK( GladderMonsterModifiers::IsExemptSpecies( "monster_tentacle" ) );
	CHECK( GladderMonsterModifiers::GetModernYawSpeed( &tentacle, 18.0f ) == Catch::Approx( 18.0f ) );

	// Controller / flyer (airborne): unchanged
	CTestYawMonster controller( "monster_alien_controller" );
	CHECK( GladderMonsterModifiers::IsExemptSpecies( "monster_alien_controller" ) );
	CHECK( GladderMonsterModifiers::GetModernYawSpeed( &controller, 50.0f ) == Catch::Approx( 50.0f ) );
}

TEST_CASE( "GladderMonsterModifiers: Elite Champion agility bonus multiplier", "[gladder][yawspeed][champion]" )
{
	CTestYawMonster grunt( "monster_human_grunt" );
	float baseModernYaw = GladderMonsterModifiers::GetModernYawSpeed( &grunt, 40.0f );
	CHECK( baseModernYaw == Catch::Approx( 115.0f ) );

	// Tag as Elite Champion
	GladderModifiers::MakeEliteChampion( &grunt );
	CHECK( GladderModifiers::IsEliteChampion( &grunt ) );

	// Champion receives additional 1.4x agility multiplier
	float champYaw = GladderMonsterModifiers::GetModernYawSpeed( &grunt, 40.0f );
	CHECK( champYaw == Catch::Approx( 115.0f * 1.4f ) );
}

class CTestGladderRules : public CGameRules
{
  public:
	void Think( void ) override {}
	BOOL IsAllowedToSpawn( CBaseEntity *pEntity ) override { return TRUE; }
	BOOL FAllowFlashlight( void ) override { return TRUE; }
	BOOL FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override { return FALSE; }
	BOOL GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon ) override { return FALSE; }
	BOOL IsMultiplayer( void ) override { return FALSE; }
	BOOL IsDeathmatch( void ) override { return FALSE; }
	BOOL IsCoOp( void ) override { return FALSE; }
	BOOL ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] ) override { return TRUE; }
	void InitHUD( CBasePlayer *pl ) override {}
	void ClientDisconnected( edict_t *pClient ) override {}
	float FlPlayerFallDamage( CBasePlayer *pPlayer ) override { return 0.0f; }
	void PlayerSpawn( CBasePlayer *pPlayer ) override {}
	void PlayerThink( CBasePlayer *pPlayer ) override {}
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override { return TRUE; }
	float FlPlayerSpawnTime( CBasePlayer *pPlayer ) override { return 0.0f; }
	void PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse ) override {}
	int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled ) override { return 0; }
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override {}
	void DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override {}
	void PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override {}
	int WeaponShouldRespawn( CBasePlayerItem *pWeapon ) override { return 0; }
	float FlWeaponRespawnTime( CBasePlayerItem *pWeapon ) override { return 0.0f; }
	float FlWeaponTryRespawn( CBasePlayerItem *pWeapon ) override { return 0.0f; }
	Vector VecWeaponRespawnSpot( CBasePlayerItem *pWeapon ) override { return Vector( 0, 0, 0 ); }
	BOOL CanHaveItem( CBasePlayer *pPlayer, CItem *pItem ) override { return TRUE; }
	void PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem ) override {}
	int ItemShouldRespawn( CItem *pItem ) override { return 0; }
	float FlItemRespawnTime( CItem *pItem ) override { return 0.0f; }
	Vector VecItemRespawnSpot( CItem *pItem ) override { return Vector( 0, 0, 0 ); }
	void PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount ) override {}
	int AmmoShouldRespawn( CBasePlayerAmmo *pAmmo ) override { return 0; }
	float FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo ) override { return 0.0f; }
	Vector VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo ) override { return Vector( 0, 0, 0 ); }
	float FlHealthChargerRechargeTime( void ) override { return 0.0f; }
	int DeadPlayerWeapons( CBasePlayer *pPlayer ) override { return 0; }
	int DeadPlayerAmmo( CBasePlayer *pPlayer ) override { return 0; }
	BOOL FAllowMonsters( void ) override { return TRUE; }
	int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget ) override { return 0; }
	const char *GetTeamID( CBaseEntity *pEntity ) override { return ""; }

	float FlMonsterYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed ) override
	{
		return GladderMonsterModifiers::GetModernYawSpeed( pMonster, flDefaultYawSpeed );
	}
};

TEST_CASE( "GladderMonsterModifiers: CGladderRules hook integration", "[gladder][yawspeed][gamerules]" )
{
	ResetMockEngine();
	CTestGladderRules rules;

	CTestYawMonster hound( "monster_houndeye" );
	float speed = rules.FlMonsterYawSpeed( &hound, 30.0f );
	CHECK( speed == Catch::Approx( 120.0f ) );
}
