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
#include "ai/schedule.h"
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

	int FlMonsterScheduleInterruptMask( CBaseMonster *pMonster, Schedule_t *pSchedule, int iDefaultMask ) override
	{
		return GladderMonsterModifiers::GetScheduleInterruptMask( pMonster, pSchedule, iDefaultMask );
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

TEST_CASE( "GladderMonsterModifiers: Damage interruption during movement schedules (SPEC §3, Issue #64)", "[gladder][combat][interrupt]" )
{
	Task_t chaseTasks[] = {
		{ TASK_GET_PATH_TO_ENEMY, 0.0f },
		{ TASK_RUN_PATH, 0.0f },
		{ TASK_WAIT_FOR_MOVEMENT, 0.0f }
	};
	Schedule_t chaseSched = {
		chaseTasks,
		3,
		bits_COND_NEW_ENEMY | bits_COND_CAN_RANGE_ATTACK1,
		0,
		"Chase Enemy"
	};

	Task_t gruntElofTasks[] = {
		{ TASK_GET_PATH_TO_ENEMY, 0.0f },
		{ TASK_RUN_PATH, 0.0f },
		{ TASK_WAIT_FOR_MOVEMENT, 0.0f }
	};
	Schedule_t gruntElofSched = {
		gruntElofTasks,
		3,
		bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD,
		0,
		"GruntEstablishLineOfFire"
	};

	Task_t squidChaseTasks[] = {
		{ TASK_GET_PATH_TO_ENEMY, 0.0f },
		{ TASK_RUN_PATH, 0.0f },
		{ TASK_WAIT_FOR_MOVEMENT, 0.0f }
	};
	Schedule_t squidChaseSched = {
		squidChaseTasks,
		3,
		bits_COND_NEW_ENEMY | bits_COND_CAN_MELEE_ATTACK1,
		0,
		"Squid Chase Enemy"
	};

	Task_t dieTasks[] = {
		{ TASK_DIE, 0.0f }
	};
	Schedule_t dieSched = {
		dieTasks,
		1,
		0,
		0,
		"Die"
	};

	Task_t flinchTasks[] = {
		{ TASK_SMALL_FLINCH, 0.0f }
	};
	Schedule_t flinchSched = {
		flinchTasks,
		1,
		0,
		0,
		"Small Flinch"
	};

	SECTION( "IsMovementSchedule correctly identifies movement and chase schedules" )
	{
		CHECK( GladderMonsterModifiers::IsMovementSchedule( &chaseSched ) == true );
		CHECK( GladderMonsterModifiers::IsMovementSchedule( &gruntElofSched ) == true );
		CHECK( GladderMonsterModifiers::IsMovementSchedule( &squidChaseSched ) == true );
		CHECK( GladderMonsterModifiers::IsMovementSchedule( &dieSched ) == false );
		CHECK( GladderMonsterModifiers::IsMovementSchedule( &flinchSched ) == false );
	}

	SECTION( "GetScheduleInterruptMask augments movement schedules with light and heavy damage bits" )
	{
		int chaseMask = GladderMonsterModifiers::GetScheduleInterruptMask( nullptr, &chaseSched, chaseSched.iInterruptMask );
		CHECK( ( chaseMask & bits_COND_LIGHT_DAMAGE ) != 0 );
		CHECK( ( chaseMask & bits_COND_HEAVY_DAMAGE ) != 0 );
		CHECK( ( chaseMask & bits_COND_NEW_ENEMY ) != 0 );

		int gruntMask = GladderMonsterModifiers::GetScheduleInterruptMask( nullptr, &gruntElofSched, gruntElofSched.iInterruptMask );
		CHECK( ( gruntMask & bits_COND_LIGHT_DAMAGE ) != 0 );
		CHECK( ( gruntMask & bits_COND_HEAVY_DAMAGE ) != 0 );

		int squidMask = GladderMonsterModifiers::GetScheduleInterruptMask( nullptr, &squidChaseSched, squidChaseSched.iInterruptMask );
		CHECK( ( squidMask & bits_COND_LIGHT_DAMAGE ) != 0 );
		CHECK( ( squidMask & bits_COND_HEAVY_DAMAGE ) != 0 );

		int dieMask = GladderMonsterModifiers::GetScheduleInterruptMask( nullptr, &dieSched, dieSched.iInterruptMask );
		CHECK( ( dieMask & bits_COND_LIGHT_DAMAGE ) == 0 );
		CHECK( ( dieMask & bits_COND_HEAVY_DAMAGE ) == 0 );
	}

	SECTION( "Gamemode hook FlMonsterScheduleInterruptMask delegates to GladderMonsterModifiers" )
	{
		CTestGladderRules rules;
		int mask = rules.FlMonsterScheduleInterruptMask( nullptr, &chaseSched, chaseSched.iInterruptMask );
		CHECK( ( mask & bits_COND_LIGHT_DAMAGE ) != 0 );
		CHECK( ( mask & bits_COND_HEAVY_DAMAGE ) != 0 );
	}
}
