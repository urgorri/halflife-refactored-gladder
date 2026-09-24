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

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <string>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "core/player.h"

#include "tests/mock_engine.h"
#include "gameplay/gamerules.h"
#include "gameplay/gamerules_factory.h"
#include "core/skill.h"
#include "core/user_message_registry.h"
#include "entities/entity_visual_registry.h"

// Custom test rules to verify override behavior
class CTestHookRules : public CGameRules
{
  public:
	bool m_bMonsterKilledCalled = false;
	CBaseMonster *m_pLastVictim = nullptr;
	entvars_t *m_pLastKiller = nullptr;
	entvars_t *m_pLastInflictor = nullptr;
	bool m_bCustomAutoSave = false;

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

	float FlHealthChargerCapacity( void ) override { return 42.0f; }
	float FlHEVChargerCapacity( void ) override { return 84.0f; }
	BOOL FAllowAutoSave( void ) override { return m_bCustomAutoSave; }

	void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override
	{
		m_bMonsterKilledCalled = true;
		m_pLastVictim = pVictim;
		m_pLastKiller = pKiller;
		m_pLastInflictor = pInflictor;
	}
};

TEST_CASE( "GameplayHooks: Singleplayer PlayerRespawn executes reload command", "[gameplay][gamerules][hooks]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;
	teamplay.value = 0.0f;
	sv_busters.value = 0.0f;

	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );

	pRules->PlayerRespawn( nullptr, FALSE );

	REQUIRE_FALSE( g_mockServerCommands.empty() );
	CHECK( g_mockServerCommands.back() == "reload\n" );

	delete pRules;
}

TEST_CASE( "GameplayHooks: Charger capacity defaults to gSkillData and allows overrides", "[gameplay][gamerules][hooks]" )
{
	ResetMockEngine();
	gSkillData.healthchargerCapacity = 50.0f;
	gSkillData.suitchargerCapacity   = 75.0f;

	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;
	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->FlHealthChargerCapacity() == 50.0f );
	CHECK( pRules->FlHEVChargerCapacity() == 75.0f );
	delete pRules;

	CTestHookRules customRules;
	CHECK( customRules.FlHealthChargerCapacity() == 42.0f );
	CHECK( customRules.FlHEVChargerCapacity() == 84.0f );
}

TEST_CASE( "GameplayHooks: FAllowAutoSave behavior in SP, MP, and custom rules", "[gameplay][gamerules][hooks]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 0.0f;
	CGameRules *pSpRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pSpRules != nullptr );
	CHECK( pSpRules->FAllowAutoSave() == TRUE );
	delete pSpRules;

	gpGlobals->deathmatch = 1.0f;
	CGameRules *pMpRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pMpRules != nullptr );
	CHECK( pMpRules->FAllowAutoSave() == FALSE );
	delete pMpRules;

	CTestHookRules customRules;
	customRules.m_bCustomAutoSave = false;
	CHECK( customRules.FAllowAutoSave() == FALSE );
	customRules.m_bCustomAutoSave = true;
	CHECK( customRules.FAllowAutoSave() == TRUE );
}

TEST_CASE( "GameplayHooks: MonsterKilled passive callback receives correct entities", "[gameplay][gamerules][hooks]" )
{
	ResetMockEngine();
	CTestHookRules customRules;

	edict_t killerEdict;
	edict_t inflictorEdict;
	std::memset( &killerEdict, 0, sizeof( killerEdict ) );
	std::memset( &inflictorEdict, 0, sizeof( inflictorEdict ) );

	CBaseMonster *pFakeVictim = reinterpret_cast<CBaseMonster *>( 0x12345678 );
	entvars_t *pKiller = &killerEdict.v;
	entvars_t *pInflictor = &inflictorEdict.v;

	customRules.MonsterKilled( pFakeVictim, pKiller, pInflictor );

	CHECK( customRules.m_bMonsterKilledCalled == true );
	CHECK( customRules.m_pLastVictim == pFakeVictim );
	CHECK( customRules.m_pLastKiller == pKiller );
	CHECK( customRules.m_pLastInflictor == pInflictor );
}

TEST_CASE( "UserMessageRegistry: Register, LinkAll, and GetMessageId", "[network][usermsg]" )
{
	ResetMockEngine();
	UserMessageRegistry::Clear();

	int comboMsgId = 0;
	int waveMsgId  = 0;

	UserMessageRegistry::Register( "Combo", 4, &comboMsgId );
	UserMessageRegistry::Register( "WaveStart", -1, &waveMsgId );

	const auto &descriptors = UserMessageRegistry::GetDescriptors();
	REQUIRE( descriptors.size() == 2 );
	CHECK( std::string( descriptors[0].pszName ) == "Combo" );
	CHECK( descriptors[0].iSize == 4 );
	CHECK( std::string( descriptors[1].pszName ) == "WaveStart" );
	CHECK( descriptors[1].iSize == -1 );

	UserMessageRegistry::LinkAll();

	CHECK( comboMsgId != 0 );
	CHECK( waveMsgId != 0 );
	CHECK( UserMessageRegistry::GetMessageId( "Combo" ) == comboMsgId );
	CHECK( UserMessageRegistry::GetMessageId( "WaveStart" ) == waveMsgId );
	CHECK( UserMessageRegistry::GetMessageId( "NonExistent" ) == 0 );

	UserMessageRegistry::Clear();
	CHECK( UserMessageRegistry::GetDescriptors().empty() );
}

static int s_testModifierCallCount = 0;
static int s_testLastType = -1;
static const char *s_testLastModel = nullptr;

static void TestVisualModifier( int type, struct cl_entity_s *ent, const char *modelname )
{
	s_testModifierCallCount++;
	s_testLastType = type;
	s_testLastModel = modelname;
}

TEST_CASE( "EntityVisualRegistry: RegisterModifier, HasModifiers, and ApplyModifiers", "[client][visuals]" )
{
	EntityVisualRegistry::Clear();
	CHECK( EntityVisualRegistry::HasModifiers() == false );

	s_testModifierCallCount = 0;
	s_testLastType = -1;
	s_testLastModel = nullptr;

	EntityVisualRegistry::RegisterModifier( TestVisualModifier );
	CHECK( EntityVisualRegistry::HasModifiers() == true );
	REQUIRE( EntityVisualRegistry::GetModifiers().size() == 1 );

	EntityVisualRegistry::ApplyModifiers( 42, nullptr, "models/w_battery.mdl" );

	CHECK( s_testModifierCallCount == 1 );
	CHECK( s_testLastType == 42 );
	CHECK( std::string( s_testLastModel ) == "models/w_battery.mdl" );

	EntityVisualRegistry::Clear();
	CHECK( EntityVisualRegistry::HasModifiers() == false );
	CHECK( EntityVisualRegistry::GetModifiers().empty() );
}

TEST_CASE( "Player: entity factory export creates valid CBasePlayer via CBaseEntity::Create", "[player][factory]" )
{
	ResetMockEngine();

	Vector vecOrigin( 100.0f, 200.0f, 300.0f );
	Vector vecAngles( 0.0f, 90.0f, 0.0f );

	CBaseEntity *pEntity = CBaseEntity::Create( "player", vecOrigin, vecAngles, NULL );
	REQUIRE( pEntity != nullptr );

	CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
	REQUIRE( pPlayer->pev != nullptr );
	CHECK( pPlayer->edict() != nullptr );
	CHECK( pPlayer->pev->pContainingEntity == pPlayer->edict() );
	CHECK( pPlayer->pev->origin.x == 100.0f );
	CHECK( pPlayer->pev->origin.y == 200.0f );
	CHECK( pPlayer->pev->origin.z == 300.0f );
	CHECK( pPlayer->pev->angles.x == 0.0f );
	CHECK( pPlayer->pev->angles.y == 90.0f );
	CHECK( pPlayer->pev->angles.z == 0.0f );

	// Nonexistent entity classname should return NULL
	CBaseEntity *pInvalid = CBaseEntity::Create( "nonexistent_class", g_vecZero, g_vecZero, NULL );
	CHECK( pInvalid == nullptr );
}
