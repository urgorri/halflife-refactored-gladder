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
#include "ai/monsters.h"

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
	BOOL FAllowSave( void ) override { return m_bCustomSave; }
	BOOL FAllowRestore( void ) override { return m_bCustomRestore; }

	bool m_bCustomSave = true;
	bool m_bCustomRestore = true;
	int m_iSaveDeniedCount = 0;
	int m_iRestoreDeniedCount = 0;

	void OnSaveDenied( void ) override
	{
		m_iSaveDeniedCount++;
	}

	void OnRestoreDenied( void ) override
	{
		m_iRestoreDeniedCount++;
	}

	std::vector<CBaseEntity *> m_spawnedEntities;
	void OnEntitySpawned( CBaseEntity *pEntity ) override
	{
		m_spawnedEntities.push_back( pEntity );
	}

	struct StaticDecalRecord
	{
		Vector origin;
		int decalIndex;
		int entityIndex;
		int modelIndex;
	};
	std::vector<StaticDecalRecord> m_staticDecals;
	void OnStaticDecal( const Vector &origin, int decalIndex, int entityIndex, int modelIndex ) override
	{
		m_staticDecals.push_back( { origin, decalIndex, entityIndex, modelIndex } );
	}

	void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override
	{
		m_bMonsterKilledCalled = true;
		m_pLastVictim = pVictim;
		m_pLastKiller = pKiller;
		m_pLastInflictor = pInflictor;
	}

	bool m_bMonsterYawSpeedCalled = false;
	CBaseMonster *m_pLastYawMonster = nullptr;
	float m_flLastDefaultYawSpeed = 0.0f;
	float m_flCustomYawMultiplier = 1.0f;

	float FlMonsterYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed ) override
	{
		m_bMonsterYawSpeedCalled = true;
		m_pLastYawMonster = pMonster;
		m_flLastDefaultYawSpeed = flDefaultYawSpeed;
		return flDefaultYawSpeed * m_flCustomYawMultiplier;
	}
};

class CTestMonster : public CBaseMonster
{
  public:
	entvars_t m_pevData;

	CTestMonster()
	{
		std::memset( &m_pevData, 0, sizeof( m_pevData ) );
		pev = &m_pevData;
		m_flLastYawTime = 0.0f;
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

TEST_CASE( "GameplayHooks: FAllowSave and FAllowRestore behavior in SP, MP, and custom rules (#167)", "[gameplay][gamerules][hooks][saverestore]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 0.0f;
	CGameRules *pSpRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pSpRules != nullptr );
	CHECK( pSpRules->FAllowSave() == TRUE );
	CHECK( pSpRules->FAllowRestore() == TRUE );
	delete pSpRules;

	gpGlobals->deathmatch = 1.0f;
	CGameRules *pMpRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pMpRules != nullptr );
	CHECK( pMpRules->FAllowSave() == FALSE );
	CHECK( pMpRules->FAllowRestore() == TRUE );
	delete pMpRules;

	CTestHookRules customRules;
	customRules.m_bCustomSave = false;
	customRules.m_bCustomRestore = false;
	CHECK( customRules.FAllowSave() == FALSE );
	CHECK( customRules.FAllowRestore() == FALSE );

	customRules.m_bCustomSave = true;
	customRules.m_bCustomRestore = true;
	CHECK( customRules.FAllowSave() == TRUE );
	CHECK( customRules.FAllowRestore() == TRUE );
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

extern int USENTENCEG_Pick( int isentenceg, char *szfound );
extern int USENTENCEG_PickSequential( int isentenceg, char *szfound, int ipick, int freset );
extern int fSentencesInit;

TEST_CASE( "SoundSentences: boundary safety and group indexing validation", "[systems][sound][sentences]" )
{
	ResetMockEngine();

	SECTION( "Uninitialized sentences state safely returns error codes" )
	{
		int originalInit = fSentencesInit;
		fSentencesInit   = FALSE;

		char name[64] = { 0 };
		CHECK( SENTENCEG_GetIndex( "BA_ATTACK" ) == -1 );
		CHECK( SENTENCEG_GetIndex( NULL ) == -1 );
		CHECK( USENTENCEG_Pick( 0, name ) == -1 );
		CHECK( USENTENCEG_PickSequential( 0, name, 0, 0 ) == -1 );
		CHECK( SENTENCEG_Lookup( "!BA_ATTACK0", name ) == -1 );

		// Stop sound and suit functions with uninitialized state should not crash
		edict_t testEd;
		memset( &testEd, 0, sizeof( testEd ) );
		SENTENCEG_Stop( &testEd, 0, 0 );
		EMIT_GROUPID_SUIT( &testEd, 0 );

		fSentencesInit = originalInit;
	}

	SECTION( "Initialized state bounds validation prevents negative and out-of-range indexing" )
	{
		int originalInit = fSentencesInit;
		fSentencesInit   = TRUE;

		char name[64] = { 0 };

		// Negative and out-of-range sentence group indices
		CHECK( USENTENCEG_Pick( -1, name ) == -1 );
		CHECK( USENTENCEG_Pick( 200, name ) == -1 );
		CHECK( USENTENCEG_Pick( 999, name ) == -1 );

		CHECK( USENTENCEG_PickSequential( -1, name, 0, 0 ) == -1 );
		CHECK( USENTENCEG_PickSequential( 200, name, 0, 0 ) == -1 );
		CHECK( USENTENCEG_PickSequential( 999, name, 0, 0 ) == -1 );

		CHECK( SENTENCEG_GetIndex( NULL ) == -1 );
		CHECK( SENTENCEG_GetIndex( "NONEXISTENT_GROUP_NAME" ) == -1 );

		edict_t testEd;
		memset( &testEd, 0, sizeof( testEd ) );

		// Should not crash or dereference out of bounds memory
		SENTENCEG_Stop( &testEd, -1, 0 );
		SENTENCEG_Stop( &testEd, 200, 0 );
		SENTENCEG_Stop( &testEd, 0, -1 );

		EMIT_GROUPID_SUIT( &testEd, -1 );
		EMIT_GROUPID_SUIT( &testEd, 200 );

		fSentencesInit = originalInit;
	}
}

TEST_CASE( "GameplayHooks: CGameRules::OnEntitySpawned lifecycle hook (#163)", "[gameplay][gamerules][spawn]" )
{
	ResetMockEngine();

	CTestHookRules rules;
	g_pGameRules = &rules;

	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );

	CBaseEntity testEntity;
	edict.pvPrivateData = &testEntity;
	testEntity.pev       = &edict.v;

	SECTION( "DispatchSpawn calls OnEntitySpawned on successful entity spawn" )
	{
		int res = DispatchSpawn( &edict );
		CHECK( res == 0 );

		// Red phase check: DispatchSpawn in red phase does not invoke OnEntitySpawned yet
		REQUIRE( rules.m_spawnedEntities.size() == 1 );
		CHECK( rules.m_spawnedEntities[0] == &testEntity );
	}

	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: Save and restore pipeline respects FAllowSave and FAllowRestore (#167)", "[gameplay][gamerules][hooks][saverestore]" )
{
	ResetMockEngine();
	CTestHookRules customRules;
	g_pGameRules = &customRules;

	edict_t edict;
	std::memset( &edict, 0, sizeof( edict ) );
	CBaseEntity testEntity;
	edict.pvPrivateData = &testEntity;
	testEntity.pev       = &edict.v;

	SAVERESTOREDATA saveData;
	std::memset( &saveData, 0, sizeof( saveData ) );

	SECTION( "DispatchSave is vetoed when FAllowSave returns FALSE" )
	{
		customRules.m_bCustomSave = false;
		DispatchSave( &edict, &saveData );
		CHECK_FALSE( g_mockSaveCalled );

		customRules.m_bCustomSave = true;
		DispatchSave( &edict, &saveData );
		CHECK( g_mockSaveCalled );
	}

	SECTION( "DispatchRestore is vetoed when FAllowRestore returns FALSE" )
	{
		customRules.m_bCustomRestore = false;
		int res = DispatchRestore( &edict, &saveData, 0 );
		CHECK( res == 0 );
		CHECK_FALSE( g_mockRestoreCalled );

		customRules.m_bCustomRestore = true;
		res = DispatchRestore( &edict, &saveData, 0 );
		CHECK( g_mockRestoreCalled );
	}

	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: OnSaveDenied and OnRestoreDenied callbacks invoked on veto (#169)", "[gameplay][gamerules][hooks][saverestore]" )
{
	ResetMockEngine();
	CTestHookRules customRules;
	g_pGameRules = &customRules;

	SAVERESTOREDATA saveData;
	std::memset( &saveData, 0, sizeof( saveData ) );

	SECTION( "SaveGlobalState triggers OnSaveDenied when FAllowSave is FALSE" )
	{
		customRules.m_bCustomSave = false;
		customRules.m_iSaveDeniedCount = 0;
		g_mockSaveCalled = false;

		SaveGlobalState( &saveData );

		CHECK( customRules.m_iSaveDeniedCount == 1 );
		CHECK_FALSE( g_mockSaveCalled );

		// When allowed, OnSaveDenied is not called
		customRules.m_bCustomSave = true;
		customRules.m_iSaveDeniedCount = 0;
		g_mockSaveCalled = false;

		SaveGlobalState( &saveData );

		CHECK( customRules.m_iSaveDeniedCount == 0 );
		CHECK( g_mockSaveCalled );
	}

	SECTION( "RestoreGlobalState triggers OnRestoreDenied when FAllowRestore is FALSE" )
	{
		customRules.m_bCustomRestore = false;
		customRules.m_iRestoreDeniedCount = 0;
		g_mockRestoreCalled = false;

		RestoreGlobalState( &saveData );

		CHECK( customRules.m_iRestoreDeniedCount == 1 );
		CHECK_FALSE( g_mockRestoreCalled );

		// When allowed, OnRestoreDenied is not called
		customRules.m_bCustomRestore = true;
		customRules.m_iRestoreDeniedCount = 0;
		g_mockRestoreCalled = false;

		RestoreGlobalState( &saveData );

		CHECK( customRules.m_iRestoreDeniedCount == 0 );
		CHECK( g_mockRestoreCalled );
	}

	SECTION( "Vanilla default CGameRules no-op callbacks do not crash" )
	{
		g_pGameRules = nullptr;
		GameRulesFactory::Reset();
		gpGlobals->deathmatch = 0.0f;
		CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
		REQUIRE( pVanillaRules != nullptr );

		// Calling default implementations directly should be safe no-ops
		pVanillaRules->OnSaveDenied();
		pVanillaRules->OnRestoreDenied();

		delete pVanillaRules;
	}

	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: FlMonsterYawSpeed delegates and scales monster turn rate (#171)", "[gameplay][gamerules][hooks][ai][monster]" )
{
	ResetMockEngine();

	SECTION( "Vanilla default CGameRules returns unmodified flDefaultYawSpeed" )
	{
		g_pGameRules = nullptr;
		GameRulesFactory::Reset();
		gpGlobals->deathmatch = 0.0f;
		CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
		REQUIRE( pVanillaRules != nullptr );

		CTestMonster monster;
		CHECK( pVanillaRules->FlMonsterYawSpeed( &monster, 45.0f ) == 45.0f );
		CHECK( pVanillaRules->FlMonsterYawSpeed( &monster, 120.0f ) == 120.0f );
		CHECK( pVanillaRules->FlMonsterYawSpeed( nullptr, 60.0f ) == 60.0f );

		delete pVanillaRules;
	}

	SECTION( "CBaseMonster::ChangeYaw delegates to g_pGameRules->FlMonsterYawSpeed" )
	{
		CTestHookRules customRules;
		g_pGameRules = &customRules;

		customRules.m_flCustomYawMultiplier = 2.0f;
		customRules.m_bMonsterYawSpeedCalled = false;
		customRules.m_pLastYawMonster = nullptr;
		customRules.m_flLastDefaultYawSpeed = 0.0f;

		CTestMonster monster;
		monster.pev->angles.y = 0.0f;
		monster.pev->ideal_yaw = 90.0f;
		gpGlobals->time = 1.0f;
		gpGlobals->frametime = 0.1f;
		monster.m_flLastYawTime = 0.9f; // delta = 0.1s

		// Default speed = 30 * 0.1 * 2 = 6.0
		// With 2x multiplier: speed = 60 * 0.1 * 2 = 12.0
		float move = monster.ChangeYaw( 30 );

		CHECK( customRules.m_bMonsterYawSpeedCalled );
		CHECK( customRules.m_pLastYawMonster == &monster );
		CHECK( customRules.m_flLastDefaultYawSpeed == 30.0f );
		CHECK( move == Catch::Approx( 12.0f ) );
		CHECK( monster.pev->angles.y == Catch::Approx( 12.0f ) );

		g_pGameRules = nullptr;
	}

	SECTION( "CBaseMonster::ChangeYaw behavior with vanilla rules vs custom scaling" )
	{
		GameRulesFactory::Reset();
		gpGlobals->deathmatch = 0.0f;
		CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
		REQUIRE( pVanillaRules != nullptr );
		g_pGameRules = pVanillaRules;

		CTestMonster vanillaMonster;
		vanillaMonster.pev->angles.y = 0.0f;
		vanillaMonster.pev->ideal_yaw = 90.0f;
		gpGlobals->time = 2.0f;
		gpGlobals->frametime = 0.05f;
		vanillaMonster.m_flLastYawTime = 1.95f; // delta = 0.05s

		// speed = 40 * 0.05 * 2 = 4.0
		float vanillaMove = vanillaMonster.ChangeYaw( 40 );
		CHECK( vanillaMove == Catch::Approx( 4.0f ) );
		CHECK( vanillaMonster.pev->angles.y == Catch::Approx( 4.0f ) );

		delete pVanillaRules;

		// Now test with 0.5x scaling
		CTestHookRules halfSpeedRules;
		halfSpeedRules.m_flCustomYawMultiplier = 0.5f;
		g_pGameRules = &halfSpeedRules;

		CTestMonster scaledMonster;
		scaledMonster.pev->angles.y = 0.0f;
		scaledMonster.pev->ideal_yaw = 90.0f;
		gpGlobals->time = 2.0f;
		gpGlobals->frametime = 0.05f;
		scaledMonster.m_flLastYawTime = 1.95f; // delta = 0.05s

		float scaledMove = scaledMonster.ChangeYaw( 40 );
		CHECK( scaledMove == Catch::Approx( 2.0f ) );
		CHECK( scaledMonster.pev->angles.y == Catch::Approx( 2.0f ) );

		g_pGameRules = nullptr;
	}

	SECTION( "ChangeYaw operates safely when g_pGameRules is null" )
	{
		g_pGameRules = nullptr;

		CTestMonster monster;
		monster.pev->angles.y = 0.0f;
		monster.pev->ideal_yaw = 45.0f;
		gpGlobals->time = 3.0f;
		gpGlobals->frametime = 0.1f;
		monster.m_flLastYawTime = 2.9f;

		// speed = 50 * 0.1 * 2 = 10.0
		float move = monster.ChangeYaw( 50 );
		CHECK( move == Catch::Approx( 10.0f ) );
		CHECK( monster.pev->angles.y == Catch::Approx( 10.0f ) );
	}

	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: OnStaticDecal lifecycle notification (#180)", "[gameplay][gamerules][hooks][decal]" )
{
	ResetMockEngine();

	SECTION( "Vanilla default CGameRules executes safe empty no-op" )
	{
		g_pGameRules = nullptr;
		GameRulesFactory::Reset();
		gpGlobals->deathmatch = 0.0f;
		CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
		REQUIRE( pVanillaRules != nullptr );

		REQUIRE_NOTHROW( pVanillaRules->OnStaticDecal( Vector( 100.0f, 200.0f, 300.0f ), 5, 0, 0 ) );
		REQUIRE_NOTHROW( pVanillaRules->OnStaticDecal( Vector( -50.0f, 0.0f, 10.0f ), 22, 3, 14 ) );

		delete pVanillaRules;
	}

	SECTION( "Custom game rules intercepts static world decals with exact parameters" )
	{
		CTestHookRules customRules;
		g_pGameRules = &customRules;

		Vector bloodOrigin( 64.0f, -128.0f, 16.0f );
		int bloodDecalIndex = 14; // e.g. blood splatter
		int worldEntityIndex = 0;
		int worldModelIndex = 0;

		customRules.OnStaticDecal( bloodOrigin, bloodDecalIndex, worldEntityIndex, worldModelIndex );

		Vector hazardOrigin( 256.0f, 512.0f, -64.0f );
		int hazardDecalIndex = 29; // radioactive hazard
		int brushEntityIndex = 7;
		int brushModelIndex = 42;

		customRules.OnStaticDecal( hazardOrigin, hazardDecalIndex, brushEntityIndex, brushModelIndex );

		REQUIRE( customRules.m_staticDecals.size() == 2 );

		CHECK( customRules.m_staticDecals[0].origin.x == Catch::Approx( 64.0f ) );
		CHECK( customRules.m_staticDecals[0].origin.y == Catch::Approx( -128.0f ) );
		CHECK( customRules.m_staticDecals[0].origin.z == Catch::Approx( 16.0f ) );
		CHECK( customRules.m_staticDecals[0].decalIndex == 14 );
		CHECK( customRules.m_staticDecals[0].entityIndex == 0 );
		CHECK( customRules.m_staticDecals[0].modelIndex == 0 );

		CHECK( customRules.m_staticDecals[1].origin.x == Catch::Approx( 256.0f ) );
		CHECK( customRules.m_staticDecals[1].origin.y == Catch::Approx( 512.0f ) );
		CHECK( customRules.m_staticDecals[1].origin.z == Catch::Approx( -64.0f ) );
		CHECK( customRules.m_staticDecals[1].decalIndex == 29 );
		CHECK( customRules.m_staticDecals[1].entityIndex == 7 );
		CHECK( customRules.m_staticDecals[1].modelIndex == 42 );

		g_pGameRules = nullptr;
	}
}
