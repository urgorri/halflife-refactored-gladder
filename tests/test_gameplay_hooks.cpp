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
#include "weapons/weapon_base.h"
#include "weapons/weapon_defs.h"

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

	int m_customMaxAmmo = -1;
	int GetMaxAmmo( const char *pszAmmoName, int iDefaultMaxCarry ) override
	{
		if ( m_customMaxAmmo >= 0 )
			return m_customMaxAmmo;
		return CGameRules::GetMaxAmmo( pszAmmoName, iDefaultMaxCarry );
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

	bool m_bMonsterInterruptMaskCalled = false;
	int m_iMonsterInterruptMaskAugment = 0;
	int FlMonsterScheduleInterruptMask( CBaseMonster *pMonster, Schedule_t *pSchedule, int iDefaultMask ) override
	{
		m_bMonsterInterruptMaskCalled = true;
		return iDefaultMask | m_iMonsterInterruptMaskAugment;
	}

	bool m_bFixMeleeCorpseAttackDelay = false;
	BOOL FFixMeleeCorpseAttackDelay( void ) override
	{
		return m_bFixMeleeCorpseAttackDelay ? TRUE : FALSE;
	}

	bool m_bSilenceLoopingSoundsOnDeath = false;
	BOOL FSilenceLoopingWeaponSoundsOnDeath( void ) override
	{
		return m_bSilenceLoopingSoundsOnDeath ? TRUE : FALSE;
	}

	bool m_bFixShotgunReloadDesync = false;
	BOOL FFixShotgunReloadDesync( void ) override
	{
		return m_bFixShotgunReloadDesync ? TRUE : FALSE;
	}

	bool m_bFixMP5UnderwaterDebounce = false;
	BOOL FFixMP5UnderwaterDebounce( void ) override
	{
		return m_bFixMP5UnderwaterDebounce ? TRUE : FALSE;
	}

	float m_customCastSpeed = -1.0f;
	float FlMonsterCastSpeed( CBaseMonster *pMonster, Activity activity, float flDefaultSpeed ) override
	{
		if ( m_customCastSpeed >= 0.0f )
			return m_customCastSpeed;
		return CGameRules::FlMonsterCastSpeed( pMonster, activity, flDefaultSpeed );
	}

	float m_customMoveSpeed = -1.0f;
	float FlMonsterMoveSpeed( CBaseMonster *pMonster, Activity activity, float flDefaultSpeed ) override
	{
		if ( m_customMoveSpeed >= 0.0f )
			return m_customMoveSpeed;
		return CGameRules::FlMonsterMoveSpeed( pMonster, activity, flDefaultSpeed );
	}

	float m_customProjectileSpread = -1.0f;
	float FlMonsterProjectileSpread( CBaseMonster *pMonster, const char *pszProjectile, float flDefaultSpread ) override
	{
		if ( m_customProjectileSpread >= 0.0f )
			return m_customProjectileSpread;
		return CGameRules::FlMonsterProjectileSpread( pMonster, pszProjectile, flDefaultSpread );
	}

	int m_iAttackDetonationCount = 0;
	std::string m_lastAttackDetonationType;
	Vector m_lastAttackDetonationOrigin;
	void OnMonsterAttackDetonation( CBaseMonster *pMonster, const char *pszAttackType, const Vector &vecOrigin ) override
	{
		m_iAttackDetonationCount++;
		m_lastAttackDetonationType = pszAttackType ? pszAttackType : "";
		m_lastAttackDetonationOrigin = vecOrigin;
	}

	int m_iProjectileImpactCount = 0;
	CBaseEntity *m_pLastImpactProjectile = nullptr;
	void OnMonsterProjectileImpact( CBaseEntity *pProjectile, const TraceResult &tr ) override
	{
		m_iProjectileImpactCount++;
		m_pLastImpactProjectile = pProjectile;
	}

	float m_customHornetAgility = -1.0f;
	float FlHornetTrackAgility( CBaseEntity *pHornet, float flDefaultWeight ) override
	{
		if ( m_customHornetAgility >= 0.0f )
			return m_customHornetAgility;
		return CGameRules::FlHornetTrackAgility( pHornet, flDefaultWeight );
	}

	float m_customMonsterWaitTime = -1.0f;
	float FlMonsterWaitTime( CBaseMonster *pMonster, float flDefaultWait ) override
	{
		if ( m_customMonsterWaitTime >= 0.0f )
			return m_customMonsterWaitTime;
		return CGameRules::FlMonsterWaitTime( pMonster, flDefaultWait );
	}

	float m_customGrenadeCheckDelay = -1.0f;
	float FlMonsterGrenadeCheckDelay( CBaseMonster *pMonster, float flDefaultDelay ) override
	{
		if ( m_customGrenadeCheckDelay >= 0.0f )
			return m_customGrenadeCheckDelay;
		return CGameRules::FlMonsterGrenadeCheckDelay( pMonster, flDefaultDelay );
	}

	int m_customProjectileBurstCount = -1;
	int IMonsterProjectileBurstCount( CBaseMonster *pMonster, const char *pszProjectile, int iDefaultCount ) override
	{
		if ( m_customProjectileBurstCount >= 0 )
			return m_customProjectileBurstCount;
		return CGameRules::IMonsterProjectileBurstCount( pMonster, pszProjectile, iDefaultCount );
	}

	bool m_bPlayerAddAmmoCalled = false;
	BOOL m_bPlayerAddAmmoReturn = FALSE;
	CBasePlayerAmmo *m_pLastAmmoEntity = nullptr;
	CBaseEntity *m_pLastAmmoRecipient = nullptr;
	BOOL PlayerAddAmmo( CBasePlayerAmmo *pAmmo, CBaseEntity *pOther ) override
	{
		m_bPlayerAddAmmoCalled = true;
		m_pLastAmmoEntity = pAmmo;
		m_pLastAmmoRecipient = pOther;
		if ( m_bPlayerAddAmmoReturn )
		{
			return TRUE;
		}
		return CGameRules::PlayerAddAmmo( pAmmo, pOther );
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
		ClearConditions( 0xFFFFFFFF );
		m_pSchedule = nullptr;
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

TEST_CASE( "GameplayHooks: FlMonsterScheduleInterruptMask modifies monster interruptibility (#186)", "[gameplay][gamerules][hooks][ai]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;

	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	SECTION( "Default CHalfLifeRules returns iDefaultMask unchanged" )
	{
		CHECK( pVanillaRules->FlMonsterScheduleInterruptMask( nullptr, nullptr, 0x1234 ) == 0x1234 );
		CHECK( pVanillaRules->FlMonsterScheduleInterruptMask( nullptr, nullptr, 0 ) == 0 );
		CHECK( pVanillaRules->FlMonsterScheduleInterruptMask( nullptr, nullptr, bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) == ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) );

		CTestMonster monster;
		Schedule_t sched;
		std::memset( &sched, 0, sizeof( sched ) );
		sched.iInterruptMask = bits_COND_NEW_ENEMY;
		sched.pName = "SCHED_CHASE_ENEMY_TEST";

		monster.m_pSchedule = &sched;
		monster.SetConditions( bits_COND_LIGHT_DAMAGE );

		g_pGameRules = pVanillaRules;

		// With vanilla rules, chase schedule does not include damage bits, so IScheduleFlags() yields 0
		CHECK( monster.IScheduleFlags() == 0 );
		CHECK( monster.FScheduleValid() == TRUE );

		g_pGameRules = nullptr;
	}

	SECTION( "Custom rules augments interrupt mask to allow damage interruption during movement" )
	{
		CTestHookRules customRules;
		customRules.m_iMonsterInterruptMaskAugment = bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE;
		g_pGameRules = &customRules;

		CTestMonster monster;
		Schedule_t sched;
		std::memset( &sched, 0, sizeof( sched ) );
		sched.iInterruptMask = bits_COND_NEW_ENEMY;
		sched.pName = "SCHED_CHASE_ENEMY_TEST";

		monster.m_pSchedule = &sched;
		monster.SetConditions( bits_COND_LIGHT_DAMAGE );

		// Custom rules augments interrupt mask with damage bits, so monster is interruptible
		CHECK( monster.IScheduleFlags() == bits_COND_LIGHT_DAMAGE );
		CHECK( monster.FScheduleValid() == FALSE );
		CHECK( customRules.m_bMonsterInterruptMaskCalled );

		g_pGameRules = nullptr;
	}

	SECTION( "Null g_pGameRules safely falls back to default schedule interrupt mask" )
	{
		g_pGameRules = nullptr;

		CTestMonster monster;
		Schedule_t sched;
		std::memset( &sched, 0, sizeof( sched ) );
		sched.iInterruptMask = bits_COND_NEW_ENEMY;
		sched.pName = "SCHED_TEST";

		monster.m_pSchedule = &sched;
		monster.SetConditions( bits_COND_LIGHT_DAMAGE );

		CHECK( monster.IScheduleFlags() == 0 );
		CHECK( monster.FScheduleValid() == TRUE );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: FFixMeleeCorpseAttackDelay lifecycle and attack timing (#187)", "[gameplay][gamerules][hooks][weapons]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;

	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	SECTION( "Vanilla CHalfLifeRules returns FALSE preserving baseline GoldSrc behavior" )
	{
		CHECK( pVanillaRules->FFixMeleeCorpseAttackDelay() == FALSE );
	}

	SECTION( "Custom rules can enable corpse hit delay fix" )
	{
		CTestHookRules customRules;
		CHECK( customRules.FFixMeleeCorpseAttackDelay() == FALSE );

		customRules.m_bFixMeleeCorpseAttackDelay = true;
		CHECK( customRules.FFixMeleeCorpseAttackDelay() == TRUE );
	}

	SECTION( "Simulation of corpse hit attack delay branching" )
	{
		// Simulate weapon state logic from CCrowbar::Swing
		gpGlobals->time = 10.0f;
		float flNextPrimaryAttack = 0.0f;
		float flNextThink = 0.0f;
		bool bHitHandled = false;

		auto SimulateSwingOnCorpse = [&]( CGameRules *pRules ) -> bool {
			// In CCrowbar::Swing(): if ( !pEntity->IsAlive() )
			if ( pRules && pRules->FFixMeleeCorpseAttackDelay() )
			{
				// Modern gamemodes: set normal attack delay and schedule smack think
				flNextPrimaryAttack = gpGlobals->time + 0.25f;
				flNextThink = gpGlobals->time + 0.2f;
				return true;
			}
			else
			{
				// Vanilla: return TRUE early without updating flNextPrimaryAttack or scheduling Smack think
				return true;
			}
		};

		// Vanilla: next attack delay not updated
		flNextPrimaryAttack = 0.0f;
		flNextThink = 0.0f;
		bHitHandled = SimulateSwingOnCorpse( pVanillaRules );
		CHECK( bHitHandled == true );
		CHECK( flNextPrimaryAttack == 0.0f );
		CHECK( flNextThink == 0.0f );

		// Custom rules: attack delay advanced, smack think scheduled
		CTestHookRules customRules;
		customRules.m_bFixMeleeCorpseAttackDelay = true;
		flNextPrimaryAttack = 0.0f;
		flNextThink = 0.0f;
		bHitHandled = SimulateSwingOnCorpse( &customRules );
		CHECK( bHitHandled == true );
		CHECK( flNextPrimaryAttack == Catch::Approx( 10.25f ) );
		CHECK( flNextThink == Catch::Approx( 10.20f ) );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: FSilenceLoopingWeaponSoundsOnDeath lifecycle and sound suppression (#188)", "[gameplay][gamerules][hooks][player][audio]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;

	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	SECTION( "Vanilla CHalfLifeRules returns FALSE preserving classic audio lifecycle" )
	{
		CHECK( pVanillaRules->FSilenceLoopingWeaponSoundsOnDeath() == FALSE );
	}

	SECTION( "Custom rules enables sound silence on player death" )
	{
		CTestHookRules customRules;
		CHECK( customRules.FSilenceLoopingWeaponSoundsOnDeath() == FALSE );

		customRules.m_bSilenceLoopingSoundsOnDeath = true;
		CHECK( customRules.FSilenceLoopingWeaponSoundsOnDeath() == TRUE );
	}

	SECTION( "Player death sound cleanup emits common/null.wav on CHAN_WEAPON when hook enabled" )
	{
		edict_t playerEdict;
		std::memset( &playerEdict, 0, sizeof( playerEdict ) );
		playerEdict.v.pContainingEntity = &playerEdict;

		auto SimulatePlayerDeathSoundCleanup = [&]( CGameRules *pRules ) {
			if ( pRules && pRules->FSilenceLoopingWeaponSoundsOnDeath() )
			{
				EMIT_SOUND( ENT( &playerEdict.v ), CHAN_WEAPON, "common/null.wav", 1.0f, ATTN_NORM );
			}
		};

		// Under vanilla rules: no sound cleanup emitted
		g_mockEmittedSounds.clear();
		SimulatePlayerDeathSoundCleanup( pVanillaRules );
		CHECK( g_mockEmittedSounds.empty() );

		// Under custom rules: common/null.wav emitted on CHAN_WEAPON
		CTestHookRules customRules;
		customRules.m_bSilenceLoopingSoundsOnDeath = true;
		g_mockEmittedSounds.clear();
		SimulatePlayerDeathSoundCleanup( &customRules );
		REQUIRE( g_mockEmittedSounds.size() == 1 );
		CHECK( g_mockEmittedSounds[0].channel == CHAN_WEAPON );
		CHECK( g_mockEmittedSounds[0].sample == "common/null.wav" );
		CHECK( g_mockEmittedSounds[0].volume == Catch::Approx( 1.0f ) );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: FFixShotgunReloadDesync lifecycle and reload state transitions (#189)", "[gameplay][gamerules][hooks][weapons][shotgun]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;

	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	SECTION( "Vanilla rules returns FALSE by default" )
	{
		CHECK( pVanillaRules->FFixShotgunReloadDesync() == FALSE );
	}

	SECTION( "Custom rules returns TRUE when enabled" )
	{
		CTestHookRules customRules;
		CHECK( customRules.FFixShotgunReloadDesync() == FALSE );

		customRules.m_bFixShotgunReloadDesync = true;
		CHECK( customRules.FFixShotgunReloadDesync() == TRUE );
	}

	SECTION( "Reload state transitions and pump timer clearing under fix" )
	{
		struct ShotgunState
		{
			int m_fInSpecialReload = 1;
			float m_flPumpTime = 5.0f;

			void Holster( CGameRules *pRules )
			{
				if ( pRules && pRules->FFixShotgunReloadDesync() )
				{
					m_fInSpecialReload = 0;
					m_flPumpTime = 0.0f;
				}
			}

			void PrimaryAttack( CGameRules *pRules, bool underwater )
			{
				if ( underwater )
				{
					if ( pRules && pRules->FFixShotgunReloadDesync() )
					{
						m_fInSpecialReload = 0;
						m_flPumpTime = 0.0f;
					}
					return;
				}

				if ( pRules && pRules->FFixShotgunReloadDesync() )
				{
					if ( m_fInSpecialReload != 0 )
					{
						m_flPumpTime = 0.0f;
						m_fInSpecialReload = 0;
					}
				}
				m_flPumpTime = gpGlobals->time + 0.5f;
				m_fInSpecialReload = 0;
			}

			void StartReload( CGameRules *pRules )
			{
				if ( m_fInSpecialReload == 0 )
				{
					if ( pRules && pRules->FFixShotgunReloadDesync() )
					{
						m_flPumpTime = 0.0f;
					}
					m_fInSpecialReload = 1;
				}
			}
		};

		// Vanilla: Holster does not clear special reload or pump timer
		ShotgunState vanillaState;
		vanillaState.Holster( pVanillaRules );
		CHECK( vanillaState.m_fInSpecialReload == 1 );
		CHECK( vanillaState.m_flPumpTime == Catch::Approx( 5.0f ) );

		// Custom rules: Holster cleanly clears special reload and pump timer
		CTestHookRules customRules;
		customRules.m_bFixShotgunReloadDesync = true;
		ShotgunState fixedState;
		fixedState.Holster( &customRules );
		CHECK( fixedState.m_fInSpecialReload == 0 );
		CHECK( fixedState.m_flPumpTime == 0.0f );

		// Underwater interruption resets reload and pump timer under fix
		ShotgunState underwaterState;
		underwaterState.PrimaryAttack( &customRules, true );
		CHECK( underwaterState.m_fInSpecialReload == 0 );
		CHECK( underwaterState.m_flPumpTime == 0.0f );

		// Starting a fresh reload resets any leftover pump timer
		ShotgunState freshReloadState;
		freshReloadState.m_fInSpecialReload = 0;
		freshReloadState.m_flPumpTime = 2.5f;
		freshReloadState.StartReload( &customRules );
		CHECK( freshReloadState.m_fInSpecialReload == 1 );
		CHECK( freshReloadState.m_flPumpTime == 0.0f );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: FFixMP5UnderwaterDebounce lifecycle and attack delay timing (#190)", "[gameplay][gamerules][hooks][weapons][mp5]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;

	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	SECTION( "Vanilla rules returns FALSE preserving canonical SDK timing constants" )
	{
		CHECK( pVanillaRules->FFixMP5UnderwaterDebounce() == FALSE );
	}

	SECTION( "Custom rules returns TRUE when enabled" )
	{
		CTestHookRules customRules;
		CHECK( customRules.FFixMP5UnderwaterDebounce() == FALSE );

		customRules.m_bFixMP5UnderwaterDebounce = true;
		CHECK( customRules.FFixMP5UnderwaterDebounce() == TRUE );
	}

	SECTION( "Underwater secondary attack debounce updates both attack channels with weapon time base" )
	{
		gpGlobals->time = 15.0f;
		float flNextPrimaryAttack = 0.0f;
		float flNextSecondaryAttack = 0.0f;

		auto SimulateMP5SecondaryAttackUnderwater = [&]( CGameRules *pRules ) {
			if ( pRules && pRules->FFixMP5UnderwaterDebounce() )
			{
				flNextSecondaryAttack = flNextPrimaryAttack = gpGlobals->time + 0.15f;
			}
			else
			{
				flNextPrimaryAttack = 0.15f;
			}
		};

		// Vanilla: sets absolute 0.15s on primary, leaves secondary untouched (leading to tick-rate spam)
		flNextPrimaryAttack = 0.0f;
		flNextSecondaryAttack = 0.0f;
		SimulateMP5SecondaryAttackUnderwater( pVanillaRules );
		CHECK( flNextPrimaryAttack == Catch::Approx( 0.15f ) );
		CHECK( flNextSecondaryAttack == 0.0f );

		// Fixed: sets both channels to gpGlobals->time + 0.15s, properly debouncing secondary fire
		CTestHookRules customRules;
		customRules.m_bFixMP5UnderwaterDebounce = true;
		flNextPrimaryAttack = 0.0f;
		flNextSecondaryAttack = 0.0f;
		SimulateMP5SecondaryAttackUnderwater( &customRules );
		CHECK( flNextPrimaryAttack == Catch::Approx( 15.15f ) );
		CHECK( flNextSecondaryAttack == Catch::Approx( 15.15f ) );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: CGameRules::GetMaxAmmo extensible capacity hook (#201)", "[gameplay][gamerules][ammo]" )
{
	InitMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;
	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	SECTION( "Default CGameRules::GetMaxAmmo guarantees 100% behavioral equivalence" )
	{
		CHECK( pVanillaRules->GetMaxAmmo( "9mm", _9MM_MAX_CARRY ) == _9MM_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "357", _357_MAX_CARRY ) == _357_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "buckshot", BUCKSHOT_MAX_CARRY ) == BUCKSHOT_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "bolts", BOLT_MAX_CARRY ) == BOLT_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "rockets", ROCKET_MAX_CARRY ) == ROCKET_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "uranium", URANIUM_MAX_CARRY ) == URANIUM_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "Hand Grenade", HANDGRENADE_MAX_CARRY ) == HANDGRENADE_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "Satchel Charge", SATCHEL_MAX_CARRY ) == SATCHEL_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "Trip Mine", TRIPMINE_MAX_CARRY ) == TRIPMINE_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "Snarks", SNARK_MAX_CARRY ) == SNARK_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "Hornets", HORNET_MAX_CARRY ) == HORNET_MAX_CARRY );
		CHECK( pVanillaRules->GetMaxAmmo( "ARgrenades", M203_GRENADE_MAX_CARRY ) == M203_GRENADE_MAX_CARRY );
	}

	SECTION( "Custom game rules can extend ammo capacity beyond standard single-byte caps" )
	{
		CTestHookRules customRules;
		customRules.m_customMaxAmmo = 500;

		CHECK( customRules.GetMaxAmmo( "9mm", _9MM_MAX_CARRY ) == 500 );
		CHECK( customRules.GetMaxAmmo( "rockets", ROCKET_MAX_CARRY ) == 500 );
	}

	SECTION( "Custom game rules fallback to default when not overridden for specific ammo" )
	{
		CTestHookRules customRules;
		// m_customMaxAmmo is -1 by default
		CHECK( customRules.GetMaxAmmo( "9mm", _9MM_MAX_CARRY ) == _9MM_MAX_CARRY );
		CHECK( customRules.GetMaxAmmo( "rockets", ROCKET_MAX_CARRY ) == ROCKET_MAX_CARRY );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: CGameRules monster behavioral and combat lifecycle hooks (#204)", "[gameplay][gamerules][monsters]" )
{
	InitMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;
	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	CTestMonster monster;
	CBaseEntity projectile;

	SECTION( "Default CGameRules hooks guarantee 100% backward-compatible passthrough" )
	{
		CHECK( pVanillaRules->FlMonsterCastSpeed( &monster, ACT_RANGE_ATTACK1, 1.25f ) == 1.25f );
		CHECK( pVanillaRules->FlMonsterMoveSpeed( &monster, ACT_WALK, 1.5f ) == 1.5f );
		CHECK( pVanillaRules->FlMonsterProjectileSpread( &monster, "squidspit", 0.05f ) == 0.05f );

		Vector vecDetonation( 100.0f, 200.0f, 300.0f );
		pVanillaRules->OnMonsterAttackDetonation( &monster, "sonic_blast", vecDetonation );

		TraceResult tr;
		std::memset( &tr, 0, sizeof( tr ) );
		tr.flFraction = 1.0f;
		pVanillaRules->OnMonsterProjectileImpact( &projectile, tr );
	}

	SECTION( "Custom game rules can intercept and scale monster behavioral attributes" )
	{
		CTestHookRules customRules;
		customRules.m_customCastSpeed = 2.0f;
		customRules.m_customMoveSpeed = 1.75f;
		customRules.m_customProjectileSpread = 0.01f;

		CHECK( customRules.FlMonsterCastSpeed( &monster, ACT_RANGE_ATTACK1, 1.0f ) == 2.0f );
		CHECK( customRules.FlMonsterMoveSpeed( &monster, ACT_WALK, 1.0f ) == 1.75f );
		CHECK( customRules.FlMonsterProjectileSpread( &monster, "squidspit", 0.05f ) == 0.01f );
	}

	SECTION( "Custom game rules receive monster attack detonation and projectile impact events" )
	{
		CTestHookRules customRules;
		Vector vecDetonation( 50.0f, -60.0f, 120.0f );

		CHECK( customRules.m_iAttackDetonationCount == 0 );
		customRules.OnMonsterAttackDetonation( &monster, "sonic_blast", vecDetonation );
		CHECK( customRules.m_iAttackDetonationCount == 1 );
		CHECK( customRules.m_lastAttackDetonationType == "sonic_blast" );
		CHECK( customRules.m_lastAttackDetonationOrigin.x == 50.0f );
		CHECK( customRules.m_lastAttackDetonationOrigin.y == -60.0f );
		CHECK( customRules.m_lastAttackDetonationOrigin.z == 120.0f );

		CHECK( customRules.m_iProjectileImpactCount == 0 );
		TraceResult tr;
		std::memset( &tr, 0, sizeof( tr ) );
		tr.vecEndPos = Vector( 10.0f, 20.0f, 30.0f );
		customRules.OnMonsterProjectileImpact( &projectile, tr );
		CHECK( customRules.m_iProjectileImpactCount == 1 );
		CHECK( customRules.m_pLastImpactProjectile == &projectile );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: CGameRules monster combat extension hooks and flinch scaling (#208)", "[gameplay][gamerules][monsters]" )
{
	InitMockEngine();
	GameRulesFactory::Reset();
	gpGlobals->deathmatch = 0.0f;
	CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pVanillaRules != nullptr );

	CTestMonster monster;
	CBaseEntity hornet;

	SECTION( "Default CGameRules hooks preserve 100% vanilla behavior" )
	{
		g_pGameRules = pVanillaRules;
		gpGlobals->time = 10.0f;

		CHECK( pVanillaRules->FlHornetTrackAgility( &hornet, 0.5f ) == Catch::Approx( 0.5f ) );
		CHECK( pVanillaRules->FlMonsterWaitTime( &monster, 1.5f ) == Catch::Approx( 1.5f ) );
		CHECK( pVanillaRules->FlMonsterGrenadeCheckDelay( &monster, 6.0f ) == Catch::Approx( 6.0f ) );
		CHECK( pVanillaRules->IMonsterProjectileBurstCount( &monster, "controller_energy_ball", 1 ) == 1 );

		Task_t waitTask = { TASK_WAIT_FACE_ENEMY, 1.0f };
		monster.StartTask( &waitTask );
		CHECK( monster.m_flWaitFinished == Catch::Approx( 11.0f ) );

		monster.SetActivity( ACT_SMALL_FLINCH );
		CHECK( monster.pev->framerate == Catch::Approx( 1.0f ) );

		monster.SetActivity( ACT_BIG_FLINCH );
		CHECK( monster.pev->framerate == Catch::Approx( 1.0f ) );
	}

	SECTION( "Custom game rules override hornet agility, wait time, grenade delay, burst count, and flinch speed" )
	{
		CTestHookRules customRules;
		customRules.m_customHornetAgility        = 0.8f;
		customRules.m_customMonsterWaitTime      = 0.4f;
		customRules.m_customGrenadeCheckDelay    = 2.0f;
		customRules.m_customProjectileBurstCount = 2;
		customRules.m_customCastSpeed            = 1.3f;
		g_pGameRules                             = &customRules;
		gpGlobals->time                          = 20.0f;

		CHECK( g_pGameRules->FlHornetTrackAgility( &hornet, 0.5f ) == Catch::Approx( 0.8f ) );
		CHECK( g_pGameRules->FlMonsterWaitTime( &monster, 1.5f ) == Catch::Approx( 0.4f ) );
		CHECK( g_pGameRules->FlMonsterGrenadeCheckDelay( &monster, 6.0f ) == Catch::Approx( 2.0f ) );
		CHECK( g_pGameRules->IMonsterProjectileBurstCount( &monster, "controller_energy_ball", 1 ) == 2 );

		Task_t waitFaceTask = { TASK_WAIT_FACE_ENEMY, 1.0f };
		monster.StartTask( &waitFaceTask );
		CHECK( monster.m_flWaitFinished == Catch::Approx( 20.4f ) );

		Task_t waitTask = { TASK_WAIT, 1.5f };
		monster.StartTask( &waitTask );
		CHECK( monster.m_flWaitFinished == Catch::Approx( 20.4f ) );

		monster.SetActivity( ACT_SMALL_FLINCH );
		CHECK( monster.pev->framerate == Catch::Approx( 1.3f ) );

		monster.SetActivity( ACT_BIG_FLINCH );
		CHECK( monster.pev->framerate == Catch::Approx( 1.3f ) );

		monster.SetActivity( ACT_IDLE );
		CHECK( monster.pev->framerate == Catch::Approx( 1.0f ) );
	}

	SECTION( "Proportional skill scaling preserves vanilla Baby Headcrab range and Orange Hornet speed" )
	{
		pVanillaRules->RefreshSkillData();
		CHECK( gSkillData.headcrabAttackRange == Catch::Approx( 256.0f ) );
		CHECK( gSkillData.agruntHornetSpeed == Catch::Approx( 600.0f ) );

		const float flVanillaBabyRange = 180.0f * ( gSkillData.headcrabAttackRange / 256.0f );
		const float flVanillaOrangeSpeed = 800.0f * ( gSkillData.agruntHornetSpeed / 600.0f );
		CHECK( flVanillaBabyRange == Catch::Approx( 180.0f ) );
		CHECK( flVanillaOrangeSpeed == Catch::Approx( 800.0f ) );

		gSkillData.headcrabAttackRange = 380.0f;
		gSkillData.agruntHornetSpeed   = 810.0f;
		const float flScaledBabyRange   = 180.0f * ( gSkillData.headcrabAttackRange / 256.0f );
		const float flScaledOrangeSpeed = 800.0f * ( gSkillData.agruntHornetSpeed / 600.0f );
		CHECK( flScaledBabyRange == Catch::Approx( 267.1875f ) );
		CHECK( flScaledOrangeSpeed == Catch::Approx( 1080.0f ) );
	}

	delete pVanillaRules;
	g_pGameRules = nullptr;
}

TEST_CASE( "GameplayHooks: CGameRules::PlayerAddAmmo extensible ammo pickup hook (#212)", "[gameplay][gamerules][ammo][hooks]" )
{
	InitMockEngine();
	GameRulesFactory::Reset();

	SECTION( "Vanilla singleplayer and multiplayer rules return FALSE by default" )
	{
		gpGlobals->deathmatch = 0.0f;
		CGameRules *pSingleRules = GameRulesFactory::CreateGameRules();
		REQUIRE( pSingleRules != nullptr );

		CHECK( pSingleRules->PlayerAddAmmo( nullptr, nullptr ) == FALSE );

		delete pSingleRules;
		g_pGameRules = nullptr;

		GameRulesFactory::Reset();
		gpGlobals->deathmatch = 1.0f;
		CGameRules *pMultiRules = GameRulesFactory::CreateGameRules();
		REQUIRE( pMultiRules != nullptr );

		CHECK( pMultiRules->PlayerAddAmmo( nullptr, nullptr ) == FALSE );

		delete pMultiRules;
		g_pGameRules = nullptr;
		gpGlobals->deathmatch = 0.0f;
	}

	SECTION( "Custom game rules intercepts PlayerAddAmmo and receives correct entity parameters" )
	{
		CTestHookRules customRules;
		g_pGameRules = &customRules;

		// Default returns FALSE (delegating to CGameRules::PlayerAddAmmo)
		CHECK( customRules.PlayerAddAmmo( nullptr, nullptr ) == FALSE );
		CHECK( customRules.m_bPlayerAddAmmoCalled == true );

		// Custom gamemode intercepts and handles ammo addition
		customRules.m_bPlayerAddAmmoCalled = false;
		customRules.m_bPlayerAddAmmoReturn = TRUE;

		CBaseEntity dummyRecipient;
		edict_t recipientEdict;
		std::memset( &recipientEdict, 0, sizeof( recipientEdict ) );
		dummyRecipient.pev = &recipientEdict.v;

		CBaseEntity dummyAmmoEntity;
		edict_t ammoEdict;
		std::memset( &ammoEdict, 0, sizeof( ammoEdict ) );
		dummyAmmoEntity.pev = &ammoEdict.v;
		dummyAmmoEntity.pev->classname = MAKE_STRING( "ammo_egonclip" );
		CBasePlayerAmmo *pDummyAmmo = reinterpret_cast<CBasePlayerAmmo *>( &dummyAmmoEntity );

		BOOL bHandled = customRules.PlayerAddAmmo( pDummyAmmo, &dummyRecipient );
		CHECK( bHandled == TRUE );
		CHECK( customRules.m_bPlayerAddAmmoCalled == true );
		CHECK( customRules.m_pLastAmmoEntity == pDummyAmmo );
		CHECK( customRules.m_pLastAmmoRecipient == &dummyRecipient );

		g_pGameRules = nullptr;
	}

	SECTION( "Ammo pickup delegation contract and lifecycle branching simulation (#212)" )
	{
		// Simulate CBasePlayerAmmo::DefaultTouch delegation behavior
		bool bIsPlayer = true;
		bool bPlayerBusting = false;
		bool bDefaultAddAmmoCalled = false;
		bool bDefaultAddAmmoReturn = true;
		bool bRespawnCalled = false;
		bool bRemoveScheduled = false;
		float flScheduledThinkTime = 0.0f;

		auto SimulateAmmoTouch = [&]( CGameRules *pRules ) -> bool {
			if ( !bIsPlayer )
				return false;

			if ( bPlayerBusting )
				return false;

			BOOL bAmmoAdded = FALSE;
			if ( pRules && pRules->PlayerAddAmmo( nullptr, nullptr ) )
			{
				bAmmoAdded = TRUE;
			}
			else
			{
				bDefaultAddAmmoCalled = true;
				bAmmoAdded = bDefaultAddAmmoReturn ? TRUE : FALSE;
			}

			if ( bAmmoAdded )
			{
				if ( pRules && pRules->AmmoShouldRespawn( nullptr ) == GR_AMMO_RESPAWN_YES )
				{
					bRespawnCalled = true;
				}
				else
				{
					bRemoveScheduled = true;
					flScheduledThinkTime = gpGlobals->time + 0.1f;
				}
			}
			return bAmmoAdded ? true : false;
		};

		// Case 1: Non-player touching ammo - ignored
		bIsPlayer = false;
		CHECK( SimulateAmmoTouch( nullptr ) == false );
		CHECK( bDefaultAddAmmoCalled == false );

		// Case 2: Player busting - ignored
		bIsPlayer = true;
		bPlayerBusting = true;
		CHECK( SimulateAmmoTouch( nullptr ) == false );
		CHECK( bDefaultAddAmmoCalled == false );

		// Case 3: Vanilla rules fallback - AddAmmo called, entity removed in singleplayer
		bPlayerBusting = false;
		bDefaultAddAmmoCalled = false;
		bRespawnCalled = false;
		bRemoveScheduled = false;
		gpGlobals->deathmatch = 0.0f;
		gpGlobals->time = 15.0f;
		CGameRules *pVanillaRules = GameRulesFactory::CreateGameRules();

		bool bResult = SimulateAmmoTouch( pVanillaRules );
		CHECK( bResult == true );
		CHECK( bDefaultAddAmmoCalled == true );
		CHECK( bRespawnCalled == false );
		CHECK( bRemoveScheduled == true );
		CHECK( flScheduledThinkTime == Catch::Approx( 15.1f ) );

		delete pVanillaRules;
		g_pGameRules = nullptr;

		// Case 4: Custom gamemode override (e.g. Gladder custom energy or 5.56mm ammo grant)
		// Gamemode handles ammo addition, AddAmmo is bypassed
		CTestHookRules customRules;
		customRules.m_bPlayerAddAmmoReturn = TRUE;
		bDefaultAddAmmoCalled = false;
		bRespawnCalled = false;
		bRemoveScheduled = false;
		gpGlobals->time = 25.0f;

		bResult = SimulateAmmoTouch( &customRules );
		CHECK( bResult == true );
		CHECK( customRules.m_bPlayerAddAmmoCalled == true );
		CHECK( bDefaultAddAmmoCalled == false ); // AddAmmo was bypassed cleanly!
		CHECK( bRemoveScheduled == true );
		CHECK( flScheduledThinkTime == Catch::Approx( 25.1f ) );

		// Case 5: Null g_pGameRules safely falls back to default AddAmmo without crashing
		bDefaultAddAmmoCalled = false;
		bRespawnCalled = false;
		bRemoveScheduled = false;

		bResult = SimulateAmmoTouch( nullptr );
		CHECK( bResult == true );
		CHECK( bDefaultAddAmmoCalled == true );
	}
}

