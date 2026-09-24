/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include <algorithm>
#include <cstring>
#include <vector>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/game.h"
#include "gameplay/gamerules.h"
#include "gameplay/teamplay_gamerules.h"
#include "gameplay/gamerules_factory.h"

#ifdef HL_TESTS
class MockHLRules : public CGameRules
{
  public:
	explicit MockHLRules( BOOL bTeamplay = FALSE, BOOL bDeathmatch = FALSE )
	    : m_bTeamplay( bTeamplay ), m_bDeathmatch( bDeathmatch )
	{
	}

	void Think( void ) override {}
	BOOL IsAllowedToSpawn( CBaseEntity *pEntity ) override { return TRUE; }
	BOOL FAllowFlashlight( void ) override { return TRUE; }
	BOOL FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override { return FALSE; }
	BOOL GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon ) override { return FALSE; }
	BOOL IsMultiplayer( void ) override { return m_bDeathmatch; }
	BOOL IsDeathmatch( void ) override { return m_bDeathmatch; }
	BOOL IsTeamplay( void ) override { return m_bTeamplay; }
	BOOL IsCoOp( void ) override { return FALSE; }
	BOOL ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] ) override { return TRUE; }
	void InitHUD( CBasePlayer *pl ) override {}
	void ClientDisconnected( edict_t *pClient ) override {}
	float FlPlayerFallDamage( CBasePlayer *pPlayer ) override { return 0.0f; }
	void PlayerSpawn( CBasePlayer *pPlayer ) override {}
	void PlayerThink( CBasePlayer *pPlayer ) override {}
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override { return TRUE; }
	float FlPlayerSpawnTime( CBasePlayer *pPlayer ) override { return 0.0f; }
	void PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse ) override
	{
		if ( !m_bDeathmatch )
		{
			SERVER_COMMAND( "reload\n" );
		}
	}
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

  private:
	BOOL m_bTeamplay;
	BOOL m_bDeathmatch;
};
#endif

static CGameRules *CreateSinglePlayerRules( void )
{
	g_teamplay = 0;
#ifdef HL_TESTS
	return new MockHLRules( FALSE, FALSE );
#else
	return new CHalfLifeRules;
#endif
}

static CGameRules *CreateTeamplayRules( void )
{
	g_teamplay = 1;
#ifdef HL_TESTS
	return new MockHLRules( TRUE, TRUE );
#else
	return new CHalfLifeTeamplay;
#endif
}

static CGameRules *CreateBustersRules( void )
{
	g_teamplay = 0;
#ifdef HL_TESTS
	return new MockHLRules( FALSE, TRUE );
#else
	return new CMultiplayBusters;
#endif
}

static CGameRules *CreateMultiplayRules( void )
{
	g_teamplay = 0;
#ifdef HL_TESTS
	return new MockHLRules( FALSE, TRUE );
#else
	return new CHalfLifeMultiplay;
#endif
}

static bool ConditionSinglePlayer( void )
{
	return !gpGlobals || ( gpGlobals->deathmatch == 0 );
}

static bool ConditionTeamplay( void )
{
	return gpGlobals && ( gpGlobals->deathmatch != 0 ) && ( teamplay.value > 0 );
}

static bool ConditionBusters( void )
{
	return gpGlobals && ( gpGlobals->deathmatch != 0 ) && ( sv_busters.value == 1 );
}

static bool ConditionMultiplay( void )
{
	return gpGlobals && ( gpGlobals->deathmatch != 0 );
}

static std::vector<GameRulesRegistration> &GetRegistrationsStorage( void )
{
	static std::vector<GameRulesRegistration> s_registrations;
	return s_registrations;
}

void GameRulesFactory::Register( const GameRulesRegistration &entry )
{
	auto &storage = GetRegistrationsStorage();

	// Remove duplicate registration with the same name if present
	if ( entry.pszName )
	{
		storage.erase(
		    std::remove_if( storage.begin(), storage.end(),
		                    [&entry]( const GameRulesRegistration &item ) {
			                    return item.pszName && strcmp( item.pszName, entry.pszName ) == 0;
		                    } ),
		    storage.end() );
	}

	storage.push_back( entry );

	// Sort descending by priority (higher priority evaluated first)
	std::stable_sort( storage.begin(), storage.end(),
	                  []( const GameRulesRegistration &a, const GameRulesRegistration &b ) {
		                  return a.iPriority > b.iPriority;
	                  } );
}

void GameRulesFactory::RegisterVanillaRules( void )
{
	// 1. Teamplay (priority 50): deathmatch && teamplay > 0
	GameRulesRegistration teamplayReg;
	teamplayReg.pszName      = "teamplay";
	teamplayReg.pfnCreator   = CreateTeamplayRules;
	teamplayReg.pfnCondition = ConditionTeamplay;
	teamplayReg.iPriority    = 50;
	Register( teamplayReg );

	// 2. Busters (priority 40): deathmatch && sv_busters == 1
	GameRulesRegistration bustersReg;
	bustersReg.pszName      = "busters";
	bustersReg.pfnCreator   = CreateBustersRules;
	bustersReg.pfnCondition = ConditionBusters;
	bustersReg.iPriority    = 40;
	Register( bustersReg );

	// 3. Singleplayer (priority 30): !deathmatch
	GameRulesRegistration spReg;
	spReg.pszName      = "singleplay";
	spReg.pfnCreator   = CreateSinglePlayerRules;
	spReg.pfnCondition = ConditionSinglePlayer;
	spReg.iPriority    = 30;
	Register( spReg );

	// 4. Vanilla Deathmatch fallback (priority 10): deathmatch != 0
	GameRulesRegistration mpReg;
	mpReg.pszName      = "multiplay";
	mpReg.pfnCreator   = CreateMultiplayRules;
	mpReg.pfnCondition = ConditionMultiplay;
	mpReg.iPriority    = 10;
	Register( mpReg );
}

CGameRules *GameRulesFactory::CreateGameRules( void )
{
	auto &storage = GetRegistrationsStorage();
	if ( storage.empty() )
	{
		RegisterVanillaRules();
	}

	for ( const auto &entry : storage )
	{
		if ( !entry.pfnCondition || entry.pfnCondition() )
		{
			if ( entry.pfnCreator )
			{
				CGameRules *pRules = entry.pfnCreator();
				if ( pRules )
				{
					g_teamplay = pRules->IsTeamplay() ? 1 : 0;
					return pRules;
				}
			}
		}
	}

	// Fallback in case no condition matched
	if ( gpGlobals && gpGlobals->deathmatch )
	{
		g_teamplay = 0;
#ifdef HL_TESTS
		return new MockHLRules( FALSE, TRUE );
#else
		return new CHalfLifeMultiplay;
#endif
	}

	g_teamplay = 0;
#ifdef HL_TESTS
	return new MockHLRules( FALSE, FALSE );
#else
	return new CHalfLifeRules;
#endif
}

const std::vector<GameRulesRegistration> &GameRulesFactory::GetRegistrations( void )
{
	auto &storage = GetRegistrationsStorage();
	if ( storage.empty() )
	{
		RegisterVanillaRules();
	}
	return storage;
}

const GameRulesRegistration *GameRulesFactory::FindByName( const char *pszName )
{
	if ( !pszName )
		return nullptr;

	const auto &storage = GetRegistrations();
	for ( const auto &entry : storage )
	{
		if ( entry.pszName && strcmp( entry.pszName, pszName ) == 0 )
		{
			return &entry;
		}
	}
	return nullptr;
}

void GameRulesFactory::Clear( void )
{
	GetRegistrationsStorage().clear();
}

void GameRulesFactory::Reset( void )
{
	Clear();
	RegisterVanillaRules();
}
