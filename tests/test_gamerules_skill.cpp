/***
 *
 *	Behavioral Equivalence Verification - GameRules Factory & Skill Manager Tests (Layer 3)
 *	Verifies GameRulesFactory (#91) and dynamic SkillManager registry (#94)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#include "tests/mock_engine.h"
#include "gameplay/gamerules.h"
#include "gameplay/gamerules_factory.h"
#include "core/skill.h"
#include "core/skill_manager.h"

// Custom dummy rules for testing registration and priority
class CCustomCoopRules : public CGameRules
{
  public:
	void Think( void ) override {}
	BOOL IsAllowedToSpawn( CBaseEntity *pEntity ) override { return TRUE; }
	BOOL FAllowFlashlight( void ) override { return TRUE; }
	BOOL FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override { return FALSE; }
	BOOL GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon ) override { return FALSE; }
	BOOL IsMultiplayer( void ) override { return TRUE; }
	BOOL IsDeathmatch( void ) override { return FALSE; }
	BOOL IsTeamplay( void ) override { return FALSE; }
	BOOL IsCoOp( void ) override { return TRUE; }
	const char *GetGameDescription( void ) override { return "HL Co-Op Mode"; }
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
};

static CGameRules *CreateCoopRules( void )
{
	return new CCustomCoopRules;
}

static bool s_bCoopActive = false;
static bool ConditionCoop( void )
{
	return s_bCoopActive;
}

// ============================================================================
// Issue #91: GameRulesFactory Tests
// ============================================================================

TEST_CASE( "GameRulesFactory: vanilla registrations and priority ordering (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	const auto &regs = GameRulesFactory::GetRegistrations();
	REQUIRE( regs.size() == 4 );

	// Priority order: teamplay (50) > busters (40) > singleplay (30) > multiplay (10)
	CHECK( std::string( regs[0].pszName ) == "teamplay" );
	CHECK( regs[0].iPriority == 50 );

	CHECK( std::string( regs[1].pszName ) == "busters" );
	CHECK( regs[1].iPriority == 40 );

	CHECK( std::string( regs[2].pszName ) == "singleplay" );
	CHECK( regs[2].iPriority == 30 );

	CHECK( std::string( regs[3].pszName ) == "multiplay" );
	CHECK( regs[3].iPriority == 10 );
}

TEST_CASE( "GameRulesFactory: instantiation in singleplayer mode (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 0.0f;
	teamplay.value        = 0.0f;
	sv_busters.value      = 0.0f;

	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsDeathmatch() == FALSE );
	CHECK( pRules->IsTeamplay() == FALSE );
	CHECK( g_teamplay == 0 );
	delete pRules;
}

TEST_CASE( "GameRulesFactory: singleplayer takes precedence over teamplay/busters if deathmatch is 0 (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 0.0f;
	teamplay.value        = 1.0f;
	sv_busters.value      = 1.0f;

	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsDeathmatch() == FALSE );
	CHECK( pRules->IsTeamplay() == FALSE );
	CHECK( g_teamplay == 0 );
	delete pRules;
}

TEST_CASE( "GameRulesFactory: instantiation in deathmatch mode (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 1.0f;
	teamplay.value        = 0.0f;
	sv_busters.value      = 0.0f;

	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsDeathmatch() == TRUE );
	CHECK( pRules->IsTeamplay() == FALSE );
	CHECK( g_teamplay == 0 );
	delete pRules;
}

TEST_CASE( "GameRulesFactory: instantiation in teamplay mode (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 1.0f;
	teamplay.value        = 1.0f;
	sv_busters.value      = 0.0f;

	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsDeathmatch() == TRUE );
	CHECK( pRules->IsTeamplay() == TRUE );
	CHECK( g_teamplay == 1 );
	delete pRules;
}

TEST_CASE( "GameRulesFactory: instantiation in busters mode (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 1.0f;
	teamplay.value        = 0.0f;
	sv_busters.value      = 1.0f;

	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsDeathmatch() == TRUE );
	CHECK( pRules->IsTeamplay() == FALSE );
	CHECK( g_teamplay == 0 );
	delete pRules;
}

TEST_CASE( "GameRulesFactory: teamplay takes precedence over busters when both active (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	gpGlobals->deathmatch = 1.0f;
	teamplay.value        = 1.0f;
	sv_busters.value      = 1.0f;

	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsTeamplay() == TRUE );
	CHECK( g_teamplay == 1 );
	delete pRules;
}

TEST_CASE( "GameRulesFactory: custom game mode registration and priority override (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	s_bCoopActive = false;
	GameRulesRegistration coopReg;
	coopReg.pszName      = "coop";
	coopReg.pfnCreator   = CreateCoopRules;
	coopReg.pfnCondition = ConditionCoop;
	coopReg.iPriority    = 100; // Higher than all vanilla modes
	GameRulesFactory::Register( coopReg );

	// When inactive, singleplayer loads
	gpGlobals->deathmatch = 0.0f;
	CGameRules *pRules = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsCoOp() == FALSE );
	delete pRules;

	// When condition is true, custom coop mode loads
	s_bCoopActive = true;
	pRules        = GameRulesFactory::CreateGameRules();
	REQUIRE( pRules != nullptr );
	CHECK( pRules->IsCoOp() == TRUE );
	CHECK( std::string( pRules->GetGameDescription() ) == "HL Co-Op Mode" );
	delete pRules;

	// Cleanup
	s_bCoopActive = false;
	GameRulesFactory::Reset();
}

TEST_CASE( "GameRulesFactory: duplicate registration prevention and FindByName (#91)", "[gameplay][gamerules]" )
{
	ResetMockEngine();
	GameRulesFactory::Reset();

	size_t initialCount = GameRulesFactory::GetRegistrations().size();

	GameRulesRegistration duplicate;
	duplicate.pszName      = "singleplay";
	duplicate.pfnCreator   = CreateCoopRules;
	duplicate.pfnCondition = nullptr;
	duplicate.iPriority    = 999;
	GameRulesFactory::Register( duplicate );

	// Count should remain unchanged because previous "singleplay" was replaced
	CHECK( GameRulesFactory::GetRegistrations().size() == initialCount );

	const GameRulesRegistration *pFound = GameRulesFactory::FindByName( "singleplay" );
	REQUIRE( pFound != nullptr );
	CHECK( pFound->iPriority == 999 );

	// Non-existent search
	CHECK( GameRulesFactory::FindByName( "non_existent" ) == nullptr );
	CHECK( GameRulesFactory::FindByName( nullptr ) == nullptr );

	GameRulesFactory::Reset();
}

// ============================================================================
// Issue #94: SkillManager Dynamic Registry Tests
// ============================================================================

TEST_CASE( "SkillManager: query difficulty-scaled cvars across skill levels (#94)", "[skill][skillmanager]" )
{
	ResetMockEngine();
	SkillManager::Reset();

	SetMockCvar( "sk_plr_crowbar1", 10.0f );
	SetMockCvar( "sk_plr_crowbar2", 20.0f );
	SetMockCvar( "sk_plr_crowbar3", 30.0f );

	// Easy (1)
	SkillManager::SetSkillLevel( SKILL_EASY );
	CHECK( SkillManager::GetSkillLevel() == SKILL_EASY );
	CHECK( SkillManager::Query( "sk_plr_crowbar" ) == 10.0f );

	// Medium (2)
	SkillManager::SetSkillLevel( SKILL_MEDIUM );
	CHECK( SkillManager::GetSkillLevel() == SKILL_MEDIUM );
	CHECK( SkillManager::Query( "sk_plr_crowbar" ) == 20.0f );

	// Hard (3)
	SkillManager::SetSkillLevel( SKILL_HARD );
	CHECK( SkillManager::GetSkillLevel() == SKILL_HARD );
	CHECK( SkillManager::Query( "sk_plr_crowbar" ) == 30.0f );

	SkillManager::Reset();
}

TEST_CASE( "SkillManager: query caching and cache invalidation (#94)", "[skill][skillmanager]" )
{
	ResetMockEngine();
	SkillManager::Reset();

	SetMockCvar( "sk_agrunt_health1", 60.0f );
	SkillManager::SetSkillLevel( SKILL_EASY );

	CHECK( SkillManager::GetCacheSize() == 0 );
	CHECK( SkillManager::Query( "sk_agrunt_health" ) == 60.0f );
	CHECK( SkillManager::GetCacheSize() == 1 );

	// Modify engine cvar directly behind the scenes
	SetMockCvar( "sk_agrunt_health1", 999.0f );

	// Cache should still return 60.0f
	CHECK( SkillManager::Query( "sk_agrunt_health" ) == 60.0f );

	// Invalidate cache
	SkillManager::InvalidateCache();
	CHECK( SkillManager::GetCacheSize() == 0 );

	// Now it queries the updated value
	CHECK( SkillManager::Query( "sk_agrunt_health" ) == 999.0f );
	CHECK( SkillManager::GetCacheSize() == 1 );

	SkillManager::Reset();
}

TEST_CASE( "SkillManager: fallback to registered defaults when cvar is missing (#94)", "[skill][skillmanager]" )
{
	ResetMockEngine();
	SkillManager::Reset();

	// Custom parameter not in engine cvars
	SkillManager::RegisterDefault( "sk_custom_laser_dmg", 15.0f, 25.0f, 35.0f );

	SkillManager::SetSkillLevel( SKILL_EASY );
	CHECK( SkillManager::Query( "sk_custom_laser_dmg" ) == 15.0f );

	SkillManager::SetSkillLevel( SKILL_MEDIUM );
	CHECK( SkillManager::Query( "sk_custom_laser_dmg" ) == 25.0f );

	SkillManager::SetSkillLevel( SKILL_HARD );
	CHECK( SkillManager::Query( "sk_custom_laser_dmg" ) == 35.0f );

	SkillManager::Reset();
}

TEST_CASE( "SkillManager: final fallback to passed flDefault (#94)", "[skill][skillmanager]" )
{
	ResetMockEngine();
	SkillManager::Reset();

	CHECK( SkillManager::Query( "sk_completely_unknown_cvar", 42.0f ) == 42.0f );
	CHECK( SkillManager::Query( "", 5.0f ) == 5.0f );
	CHECK( SkillManager::Query( nullptr, 7.0f ) == 7.0f );

	SkillManager::Reset();
}

TEST_CASE( "SkillManager: runtime overrides take precedence (#94)", "[skill][skillmanager]" )
{
	ResetMockEngine();
	SkillManager::Reset();

	SetMockCvar( "sk_hgrunt_health1", 50.0f );
	SkillManager::SetSkillLevel( SKILL_EASY );
	CHECK( SkillManager::Query( "sk_hgrunt_health" ) == 50.0f );

	// Apply override
	SkillManager::SetOverride( "sk_hgrunt_health", 500.0f );
	CHECK( SkillManager::Query( "sk_hgrunt_health" ) == 500.0f );

	// Clear overrides
	SkillManager::ClearOverrides();
	CHECK( SkillManager::Query( "sk_hgrunt_health" ) == 50.0f );

	SkillManager::Reset();
}

TEST_CASE( "SkillManager: skill level clamping and synchronization (#94)", "[skill][skillmanager]" )
{
	ResetMockEngine();
	SkillManager::Reset();

	SkillManager::SetSkillLevel( -5 );
	CHECK( SkillManager::GetSkillLevel() == SKILL_EASY );
	CHECK( gSkillData.iSkillLevel == SKILL_EASY );
	CHECK( g_iSkillLevel == SKILL_EASY );

	SkillManager::SetSkillLevel( 10 );
	CHECK( SkillManager::GetSkillLevel() == SKILL_HARD );
	CHECK( gSkillData.iSkillLevel == SKILL_HARD );
	CHECK( g_iSkillLevel == SKILL_HARD );

	SkillManager::Reset();
}

TEST_CASE( "SkillManager: GetSkillCvar backward compatibility delegation (#94)", "[skill][skillmanager]" )
{
	ResetMockEngine();
	SkillManager::Reset();

	SetMockCvar( "sk_plr_9mm_bullet1", 8.0f );
	SkillManager::SetSkillLevel( SKILL_EASY );

	char cvarName[] = "sk_plr_9mm_bullet";
	float val = GetSkillCvar( cvarName );
	CHECK( val == 8.0f );

	SkillManager::Reset();
}
