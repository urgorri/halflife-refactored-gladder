#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <string>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "gameplay/gamerules.h"
#include "gameplay/gamerules_factory.h"
#include "tests/mock_engine.h"

class CTestDropItem : public CBaseEntity
{
};

static void MockItemFactory( entvars_t *pev )
{
	if ( !pev )
		return;
	if ( pev->pContainingEntity && pev->pContainingEntity->pvPrivateData == NULL )
	{
		ALLOC_PRIVATE( pev->pContainingEntity, sizeof( CTestDropItem ) );
		CBaseEntity *pEntity = (CBaseEntity *)pev->pContainingEntity->pvPrivateData;
		if ( pEntity )
			pEntity->pev = pev;
	}
}

// Minimal test monster entity for testing item dropping
class CTestDropMonster : public CBaseMonster
{
  public:
	CTestDropMonster()
	{
		memset( &m_mockEdict, 0, sizeof( m_mockEdict ) );
		pev = &m_mockEdict.v;
		pev->classname = MAKE_STRING( "monster_test" );
		pev->velocity = Vector( 10, 20, 30 );
	}

  private:
	edict_t m_mockEdict;
};

// Custom test gamerules to test overriding FCanMonsterDropItem
class CTestDropRules : public CGameRules
{
  public:
	bool m_bDropHookCalled = false;
	CBaseMonster *m_pLastMonster = nullptr;
	std::string m_lastItemName;
	bool m_bAllowDrops = true;
	bool m_bFilterWeaponsOnly = false;

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
	const char *GetTeamID( CBaseEntity *pEntity ) override { return ""; }
	int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget ) override { return 0; }
	BOOL FAllowMonsters( void ) override { return TRUE; }

	BOOL FCanMonsterDropItem( CBaseMonster *pMonster, const char *pszItemName ) override
	{
		m_bDropHookCalled = true;
		m_pLastMonster    = pMonster;
		m_lastItemName    = pszItemName ? pszItemName : "";

		if ( m_bFilterWeaponsOnly && pszItemName )
		{
			// Suppress weapon drops (starting with "weapon_"), but permit ammo drops
			if ( strncmp( pszItemName, "weapon_", 7 ) == 0 )
				return FALSE;
			return TRUE;
		}

		return m_bAllowDrops ? TRUE : FALSE;
	}
};

TEST_CASE( "GameplayRules: FCanMonsterDropItem extension hook allows filtering or suppressing monster item drops (#177)", "[gameplay][gamerules][monster][drops]" )
{
	ResetMockEngine();
	RegisterMockEntityFactory( "weapon_9mmhandgun", MockItemFactory );
	RegisterMockEntityFactory( "weapon_shotgun", MockItemFactory );
	RegisterMockEntityFactory( "weapon_9mmAR", MockItemFactory );
	RegisterMockEntityFactory( "ammo_ARgrenades", MockItemFactory );

	SECTION( "Vanilla CGameRules default returns TRUE for all drops (behavioral equivalence)" )
	{
		g_pGameRules = nullptr;
		GameRulesFactory::Reset();
		gpGlobals->deathmatch = 0.0f;
		CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
		REQUIRE( pVanillaRules != nullptr );

		CTestDropMonster monster;
		CHECK( pVanillaRules->FCanMonsterDropItem( &monster, "weapon_shotgun" ) == TRUE );
		CHECK( pVanillaRules->FCanMonsterDropItem( &monster, "weapon_9mmAR" ) == TRUE );
		CHECK( pVanillaRules->FCanMonsterDropItem( &monster, "weapon_9mmhandgun" ) == TRUE );
		CHECK( pVanillaRules->FCanMonsterDropItem( &monster, "ammo_ARgrenades" ) == TRUE );
		CHECK( pVanillaRules->FCanMonsterDropItem( nullptr, "weapon_shotgun" ) == TRUE );

		delete pVanillaRules;
		g_pGameRules = nullptr;
	}

	SECTION( "Vanilla default rules permit CBaseMonster::DropItem to create entity" )
	{
		g_pGameRules = nullptr;
		GameRulesFactory::Reset();
		gpGlobals->deathmatch = 0.0f;
		CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
		g_pGameRules = pVanillaRules;

		CTestDropMonster monster;
		Vector dropPos( 100, 200, 300 );
		Vector dropAng( 0, 45, 0 );

		CBaseEntity *pDropped = monster.DropItem( "weapon_9mmhandgun", dropPos, dropAng );
		REQUIRE( pDropped != nullptr );
		CHECK( pDropped->pev->origin == dropPos );
		CHECK( pDropped->pev->velocity == monster.pev->velocity );

		delete pVanillaRules;
		g_pGameRules = nullptr;
	}

	SECTION( "Custom CGameRules returning FALSE suppresses monster drop" )
	{
		CTestDropRules customRules;
		customRules.m_bAllowDrops = false;
		g_pGameRules = &customRules;

		CTestDropMonster monster;
		Vector dropPos( 100, 200, 300 );
		Vector dropAng( 0, 45, 0 );

		CBaseEntity *pDropped = monster.DropItem( "weapon_shotgun", dropPos, dropAng );

		CHECK( customRules.m_bDropHookCalled == true );
		CHECK( customRules.m_pLastMonster == &monster );
		CHECK( customRules.m_lastItemName == "weapon_shotgun" );
		CHECK( pDropped == nullptr );

		g_pGameRules = nullptr;
	}

	SECTION( "Custom CGameRules selective filtering permits ammo but suppresses weapons" )
	{
		CTestDropRules customRules;
		customRules.m_bFilterWeaponsOnly = true;
		g_pGameRules = &customRules;

		CTestDropMonster monster;
		Vector dropPos( 100, 200, 300 );
		Vector dropAng( 0, 45, 0 );

		// Weapon should be suppressed
		CBaseEntity *pWeapon = monster.DropItem( "weapon_9mmAR", dropPos, dropAng );
		CHECK( pWeapon == nullptr );
		CHECK( customRules.m_lastItemName == "weapon_9mmAR" );

		// Ammo should be permitted
		CBaseEntity *pAmmo = monster.DropItem( "ammo_ARgrenades", dropPos, dropAng );
		REQUIRE( pAmmo != nullptr );
		CHECK( customRules.m_lastItemName == "ammo_ARgrenades" );

		g_pGameRules = nullptr;
	}

	SECTION( "CBaseMonster::DropItem succeeds when g_pGameRules is nullptr (standalone safety)" )
	{
		g_pGameRules = nullptr;

		CTestDropMonster monster;
		Vector dropPos( 50, 60, 70 );
		Vector dropAng( 0, 0, 0 );

		CBaseEntity *pDropped = monster.DropItem( "weapon_9mmhandgun", dropPos, dropAng );
		REQUIRE( pDropped != nullptr );

		g_pGameRules = nullptr;
	}

	SECTION( "CBaseMonster::DropItem handles nullptr item name safely" )
	{
		CTestDropRules customRules;
		g_pGameRules = &customRules;

		CTestDropMonster monster;
		CBaseEntity *pDropped = monster.DropItem( nullptr, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
		CHECK( pDropped == nullptr );
		CHECK( customRules.m_bDropHookCalled == false );

		g_pGameRules = nullptr;
	}
}
