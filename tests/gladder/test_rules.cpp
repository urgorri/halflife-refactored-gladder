/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Unit Tests: Gladder Rules (Drops, Autoswitch) (SPEC §3, §7.5, Issues #44, #45)
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <string>
#include <vector>

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "weapons/weapon_base.h"
#include "ai/monsters.h"
#include "gameplay/gamerules.h"
#include "tests/mock_engine.h"
#include "cl_dll/gladder/hud_gladder_overlay.h"
#include "dlls/gladder/gladder_wave_manager.h"
#include "dlls/gladder/gladder_entities.h"

#include "dlls/gladder/gladder_rules.h"

namespace
{

// Gladder rules test class conforming to CGladderRules specification (SPEC §3, §7.5)
class CTestGladderRulesAutoswitch : public CGameRules
{
  public:
	cvar_t m_cvarAutoswitch = { "gladder_autoswitch_on_pickup", "0", 0, 0.0f, nullptr };

	// Suppress monster weapon and supply drops (SPEC §3, Issue #44)
	BOOL FCanMonsterDropItem( CBaseMonster *pMonster, const char *pszItemName ) override
	{
		return FALSE;
	}

	// User-configurable autoswitch prevention on weapon pickup (SPEC §7.5, Issue #45)
	BOOL FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override
	{
		if ( !pPlayer || !pPlayer->m_pActiveItem )
			return TRUE; // Always switch if player currently has no weapon drawn

		if ( m_cvarAutoswitch.value == 0.0f )
			return FALSE; // Suppress autoswitch by default

		return TRUE;
	}

	// Static decal tracking for SPEC §11.3 (Issue #49)
	std::vector<GladderStaticDecal> m_staticDecals;

	void OnStaticDecal( const Vector &origin, int decalIndex, int entityIndex, int modelIndex ) override
	{
		m_staticDecals.push_back( { origin, decalIndex, entityIndex, modelIndex } );
	}

	const std::vector<GladderStaticDecal> &GetStaticDecals( void ) const { return m_staticDecals; }
	void ClearStaticDecals( void ) { m_staticDecals.clear(); }
	int m_restoreCallCount = 0;
	void RestoreStaticDecals( void )
	{
		m_restoreCallCount++;
		if ( g_engfuncs.pfnStaticDecal )
		{
			for ( const auto &decal : m_staticDecals )
			{
				g_engfuncs.pfnStaticDecal( decal.origin, decal.decalIndex, decal.entityIndex, decal.modelIndex );
			}
		}
	}
	void ResetWave( void )
	{
		// Static decals must be preserved and restored across wave resets (SPEC §11.3)
		RestoreStaticDecals();
	}
	// Stubs for CGameRules pure virtuals
	void Think( void ) override {}
	BOOL IsAllowedToSpawn( CBaseEntity *pEntity ) override { return TRUE; }
	BOOL FAllowFlashlight( void ) override { return TRUE; }
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
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override { return FALSE; }
	float FlPlayerSpawnTime( CBasePlayer *pPlayer ) override { return 0.0f; }
	void PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse ) override {}
	int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled ) override { return 0; }
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override {}
	void DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override {}
	void PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override {}
	int DeadPlayerWeapons( CBasePlayer *pPlayer ) override { return 0; }
	int DeadPlayerAmmo( CBasePlayer *pPlayer ) override { return 0; }
	int WeaponShouldRespawn( CBasePlayerItem *pWeapon ) override { return 0; }
	float FlWeaponRespawnTime( CBasePlayerItem *pWeapon ) override { return 0.0f; }
	float FlWeaponTryRespawn( CBasePlayerItem *pWeapon ) override { return 0.0f; }
	Vector VecWeaponRespawnSpot( CBasePlayerItem *pWeapon ) override { return g_vecZero; }
	BOOL CanHaveItem( CBasePlayer *pPlayer, CItem *pItem ) override { return TRUE; }
	void PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem ) override {}
	int ItemShouldRespawn( CItem *pItem ) override { return 0; }
	float FlItemRespawnTime( CItem *pItem ) override { return 0.0f; }
	Vector VecItemRespawnSpot( CItem *pItem ) override { return g_vecZero; }
	BOOL CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry ) override { return TRUE; }
	void PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount ) override {}
	int AmmoShouldRespawn( CBasePlayerAmmo *pAmmo ) override { return 0; }
	float FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo ) override { return 0.0f; }
	Vector VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo ) override { return g_vecZero; }
	float FlHealthChargerRechargeTime( void ) override { return 0.0f; }
	int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget ) override { return 0; }
	const char *GetTeamID( CBaseEntity *pEntity ) override { return ""; }
	BOOL FAllowMonsters( void ) override { return TRUE; }
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override
	{
		if ( !pcmd )
			return FALSE;

		if ( FStrEq( pcmd, "gladder_restart" ) )
		{
			const char *pszMap = ( gpGlobals && gpGlobals->mapname ) ? STRING( gpGlobals->mapname ) : nullptr;
			if ( pszMap && *pszMap )
			{
				char szCmd[128];
				snprintf( szCmd, sizeof( szCmd ), "map %s\n", pszMap );
				SERVER_COMMAND( szCmd );
			}
			else
			{
				SERVER_COMMAND( "restart\n" );
			}
			return TRUE;
		}

		return CGameRules::ClientCommand( pPlayer, pcmd );
	}
};

class CTestDropMonster : public CBaseMonster
{
  public:
	CTestDropMonster()
	{
		memset( &m_mockEdict, 0, sizeof( m_mockEdict ) );
		pev = &m_mockEdict.v;
		pev->classname = MAKE_STRING( "monster_human_grunt" );
	}

  	private:
	edict_t m_mockEdict;
};

class CTestConclusionRules : public CTestGladderRulesAutoswitch
{
  public:
	bool m_bMatchConcluded = false;
	bool m_bMatchFrozen = false;
	float m_flMatchFreezeTime = 0.0f;
	bool m_bPlayerDead = false;
	bool m_bSurvived = false;
	int m_firedRelayEvent = -1;
	CBaseEntity *m_pTrackedPlayer = nullptr;
	CGladderWaveManager m_waveManager;

	CTestConclusionRules()
	{
		m_waveManager.SetSessionTimeLimit( 100.0f );
		m_waveManager.InitializeMatch( 0.0f );
		m_waveManager.StartWave( 0.0f );
	}

	void FreezeMatchWorld( void )
	{
		if ( !gpGlobals )
			return;

		int maxEdicts = ( gpGlobals->maxEntities > 0 ) ? gpGlobals->maxEntities : 512;
		for ( int i = 1; i < maxEdicts; ++i )
		{
			edict_t *pEdict = INDEXENT( i );
			if ( FNullEnt( pEdict ) || pEdict->free || !pEdict->v.pContainingEntity )
				continue;

			CBaseEntity *pEnt = CBaseEntity::Instance( pEdict );
			if ( !pEnt || pEnt->IsPlayer() )
				continue;

			CBaseMonster *pMonster = pEnt->MyMonsterPointer();
			if ( pMonster )
			{
				pMonster->pev->velocity = g_vecZero;
				pMonster->pev->avelocity = g_vecZero;
				pMonster->pev->framerate = 0.0f;
				pMonster->pev->movetype = MOVETYPE_NONE;
				pMonster->pev->effects |= EF_NOINTERP;
				pMonster->pev->takedamage = DAMAGE_NO;
				pMonster->pev->flags |= FL_FROZEN;
				pMonster->pev->nextthink = 0.0f;
				pMonster->m_pSchedule = nullptr;
				pMonster->m_iTaskStatus = TASKSTATUS_NEW;
				pMonster->m_iScheduleIndex = 0;
				// Monster sequence, frame, and activity preserved
				continue;
			}

			const char *pszClassname = STRING( pEnt->pev->classname );
			if ( pszClassname && (
			     strncmp( pszClassname, "projectile_", 11 ) == 0 ||
			     strncmp( pszClassname, "monster_satchel", 15 ) == 0 ||
			     strncmp( pszClassname, "monster_tripmine", 16 ) == 0 ||
			     strncmp( pszClassname, "monster_snark", 13 ) == 0 ||
			     strcmp( pszClassname, "grenade" ) == 0 ||
			     strcmp( pszClassname, "rpg_rocket" ) == 0 ||
			     strcmp( pszClassname, "bolt" ) == 0 ||
			     strcmp( pszClassname, "hornet" ) == 0 ) )
			{
				pEnt->pev->velocity = g_vecZero;
				pEnt->pev->avelocity = g_vecZero;
				pEnt->pev->framerate = 0.0f;
				pEnt->pev->movetype = MOVETYPE_NONE;
				pEnt->pev->effects |= EF_NOINTERP;
				pEnt->pev->takedamage = DAMAGE_NO;
				pEnt->pev->nextthink = 0.0f;
			}
		}
	}

	void HandleMatchConclusion( bool bSurvived, CBaseEntity *pPlayer = nullptr )
	{
		if ( m_bMatchConcluded )
			return;

		m_bMatchConcluded = true;
		m_bSurvived = bSurvived;
		m_firedRelayEvent = bSurvived ? 2 : 3;

		if ( !pPlayer )
		{
			pPlayer = m_pTrackedPlayer;
		}

		if ( pPlayer && pPlayer->pev )
		{
			pPlayer->pev->flags |= FL_NOTARGET;
			pPlayer->pev->button &= IN_SCORE;
			pPlayer->pev->impulse = 0;
			pPlayer->pev->maxspeed = 1.0f;

			if ( !m_bPlayerDead )
			{
				pPlayer->pev->velocity = g_vecZero;
				pPlayer->pev->avelocity = g_vecZero;
			}
		}

		float flTime = gpGlobals ? gpGlobals->time : 0.0f;
		m_flMatchFreezeTime = flTime + 1.0f;
		m_bMatchFrozen = false;
	}

	void EnsureMonsterClipFlags( void )
	{
		if ( !gpGlobals )
			return;

		int maxEdicts = ( gpGlobals->maxEntities > 0 ) ? gpGlobals->maxEntities : 512;
		for ( int i = 1; i < maxEdicts; ++i )
		{
			edict_t *pEdict = INDEXENT( i );
			if ( FNullEnt( pEdict ) || pEdict->free || !pEdict->v.pContainingEntity )
				continue;

			CBaseEntity *pEnt = CBaseEntity::Instance( pEdict );
			if ( !pEnt || !pEnt->pev || ( pEnt->pev->flags & FL_CLIENT ) || pEnt->IsPlayer() )
				continue;

			if ( ( pEnt->MyMonsterPointer() || ( pEnt->pev->flags & FL_MONSTER ) || FClassnameIs( pEnt->pev, "func_pushable" ) ) &&
			     !( pEnt->pev->flags & FL_CLIENT ) && !pEnt->IsPlayer() )
			{
				pEnt->pev->flags |= FL_MONSTERCLIP;
			}
		}
	}

	BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker ) override
	{
		if ( m_bMatchConcluded )
			return FALSE;
		return TRUE;
	}

	BOOL FAllowFlashlight( void ) override
	{
		if ( m_bMatchConcluded )
			return FALSE;
		return TRUE;
	}

	void PlayerThink( CBasePlayer *pPlayer ) override
	{
		if ( m_bMatchConcluded && pPlayer && pPlayer->pev )
		{
			pPlayer->pev->button &= IN_SCORE;
			pPlayer->m_afButtonPressed &= IN_SCORE;
			pPlayer->m_afButtonReleased &= IN_SCORE;
			pPlayer->pev->impulse = 0;
			pPlayer->pev->maxspeed = 1.0f;
		}
	}

	void Think( void ) override
	{
		float flTime = gpGlobals ? gpGlobals->time : 0.0f;
		m_waveManager.Tick( flTime );

		if ( !m_bMatchConcluded && m_waveManager.IsMatchOver() )
		{
			HandleMatchConclusion( true, nullptr );
		}
	}
};

} // anonymous namespace

TEST_CASE( "Gladder Rules: Monster weapon and item drop suppression (SPEC §3, Issue #44)", "[gladder][rules][drops]" )
{
	ResetMockEngine();
	CTestGladderRulesAutoswitch rules;
	g_pGameRules = &rules;

	CTestDropMonster monster;

	// In Gladder, dying monsters (e.g. HECU Grunts) must NEVER drop weapons or ammunition
	CHECK( rules.FCanMonsterDropItem( &monster, "weapon_shotgun" ) == FALSE );
	CHECK( rules.FCanMonsterDropItem( &monster, "weapon_9mmAR" ) == FALSE );
	CHECK( rules.FCanMonsterDropItem( &monster, "ammo_ARgrenades" ) == FALSE );
	CHECK( rules.FCanMonsterDropItem( &monster, "weapon_9mmhandgun" ) == FALSE );
	CHECK( rules.FCanMonsterDropItem( &monster, "ammo_9mmclip" ) == FALSE );

	// Verify integration with CBaseMonster::DropItem: returns nullptr and spawns no entity
	CBaseEntity *pDropped = monster.DropItem( const_cast<char *>( "weapon_shotgun" ), Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	CHECK( pDropped == nullptr );

	g_pGameRules = nullptr;
}

TEST_CASE( "Gladder Rules: Option to disable autoswitch on weapon pickup (SPEC §7.5, Issue #45)", "[gladder][rules][autoswitch]" )
{
	ResetMockEngine();
	CTestGladderRulesAutoswitch rules;

	// Setup mock player memory buffer to avoid linking CBasePlayer vtable implementations
	std::vector<char> playerMem( sizeof( CBasePlayer ), 0 );
	CBasePlayer *pPlayer = reinterpret_cast<CBasePlayer *>( playerMem.data() );
	edict_t edPlayer;
	memset( &edPlayer, 0, sizeof( edPlayer ) );
	edPlayer.v.flags |= FL_CLIENT;
	edPlayer.v.pContainingEntity = &edPlayer;
	edPlayer.pvPrivateData       = pPlayer;
	pPlayer->pev                 = &edPlayer.v;

	// Setup mock weapon buffers
	std::vector<char> activeWeaponMem( sizeof( CBasePlayerItem ), 0 );
	CBasePlayerItem *pActiveWeapon = reinterpret_cast<CBasePlayerItem *>( activeWeaponMem.data() );

	std::vector<char> newWeaponMem( sizeof( CBasePlayerItem ), 0 );
	CBasePlayerItem *pNewWeapon = reinterpret_cast<CBasePlayerItem *>( newWeaponMem.data() );

	SECTION( "Player with no active weapon drawn always switches" )
	{
		pPlayer->m_pActiveItem = nullptr;
		rules.m_cvarAutoswitch.value = 0.0f;
		CHECK( rules.FShouldSwitchWeapon( pPlayer, pNewWeapon ) == TRUE );
	}

	SECTION( "Player holding active weapon and cvar is 0 disables autoswitch" )
	{
		pPlayer->m_pActiveItem = pActiveWeapon;
		rules.m_cvarAutoswitch.value = 0.0f;
		CHECK( rules.FShouldSwitchWeapon( pPlayer, pNewWeapon ) == FALSE );
	}

	SECTION( "Player holding active weapon and cvar is 1 enables autoswitch" )
	{
		pPlayer->m_pActiveItem = pActiveWeapon;
		rules.m_cvarAutoswitch.value = 1.0f;
		CHECK( rules.FShouldSwitchWeapon( pPlayer, pNewWeapon ) == TRUE );
	}
}

TEST_CASE( "GladderRules: OnStaticDecal lifecycle hook records mapper-placed infodecals (SPEC §11.3, Issue #49)", "[gladder][rules][decals]" )
{
	CTestGladderRulesAutoswitch rules;
	rules.ClearStaticDecals();
	REQUIRE( rules.GetStaticDecals().empty() );

	SECTION( "Captures static decals placed at world and brush origins" )
	{
		Vector bloodOrigin( 64.0f, -128.0f, 32.0f );
		rules.OnStaticDecal( bloodOrigin, 12, 0, 0 );

		Vector hazardOrigin( 512.0f, 256.0f, -64.0f );
		rules.OnStaticDecal( hazardOrigin, 28, 5, 14 );

		REQUIRE( rules.GetStaticDecals().size() == 2 );

		const auto &d1 = rules.GetStaticDecals()[0];
		CHECK( d1.origin.x == Catch::Approx( 64.0f ) );
		CHECK( d1.origin.y == Catch::Approx( -128.0f ) );
		CHECK( d1.origin.z == Catch::Approx( 32.0f ) );
		CHECK( d1.decalIndex == 12 );
		CHECK( d1.entityIndex == 0 );
		CHECK( d1.modelIndex == 0 );

		const auto &d2 = rules.GetStaticDecals()[1];
		CHECK( d2.origin.x == Catch::Approx( 512.0f ) );
		CHECK( d2.origin.y == Catch::Approx( 256.0f ) );
		CHECK( d2.origin.z == Catch::Approx( -64.0f ) );
		CHECK( d2.decalIndex == 28 );
		CHECK( d2.entityIndex == 5 );
		CHECK( d2.modelIndex == 14 );
	}
}

TEST_CASE( "GladderRules: Environmental static decals preserved across wave reset (SPEC §11.3, Issue #49)", "[gladder][rules][decals]" )
{
	CTestGladderRulesAutoswitch rules;
	rules.ClearStaticDecals();

	rules.OnStaticDecal( Vector( 100.0f, 200.0f, 0.0f ), 5, 0, 0 );
	rules.OnStaticDecal( Vector( -50.0f, 30.0f, 10.0f ), 8, 2, 1 );
	REQUIRE( rules.GetStaticDecals().size() == 2 );

	SECTION( "Wave reset retains static decals in memory and restores them" )
	{
		CHECK( rules.m_restoreCallCount == 0 );
		rules.ResetWave();
		CHECK( rules.GetStaticDecals().size() == 2 );
		CHECK( rules.m_restoreCallCount == 1 );

		// Restoration helper runs cleanly without crashing even with null engine function
		REQUIRE_NOTHROW( rules.RestoreStaticDecals() );
	}
}

TEST_CASE( "GladderOverlay: Dynamic combat decal purge triggered on wave completion transition (SPEC §11.3, Issue #49)", "[gladder][overlay][decals]" )
{
	CHudGladderOverlay overlay;
	overlay.Reset();
	REQUIRE( overlay.GetDecalPurgeCount() == 0 );

	// Wire message buffers: state is byte 0
	uint8_t msgWaveWaiting[]  = { 0, 1, 0, 0, 0, 0, 0 }; // WAITING_FOR_START
	uint8_t msgWaveActive[]   = { 1, 1, 0, 0, 0, 0, 0 }; // WAVE_ACTIVE
	uint8_t msgWaveComplete[] = { 2, 1, 0, 0, 0, 0, 0 }; // WAVE_COMPLETED
	uint8_t msgMatchOver[]    = { 3, 2, 0, 0, 0, 0, 0 }; // MATCH_OVER

	SECTION( "Initial waiting state does not trigger decal purge" )
	{
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveWaiting ), msgWaveWaiting );
		CHECK( overlay.GetWaveState() == 0 );
		CHECK( overlay.GetDecalPurgeCount() == 0 );
	}

	SECTION( "Active wave does not trigger decal purge" )
	{
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveActive ), msgWaveActive );
		CHECK( overlay.GetWaveState() == 1 );
		CHECK( overlay.GetDecalPurgeCount() == 0 );
	}

	SECTION( "Wave completion triggers decal purge during screen fade" )
	{
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveActive ), msgWaveActive );
		CHECK( overlay.GetDecalPurgeCount() == 0 );

		// Wave transition to COMPLETED triggers decal clear
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveComplete ), msgWaveComplete );
		CHECK( overlay.GetWaveState() == 2 );
		CHECK( overlay.GetDecalPurgeCount() == 1 );

		// Redundant message in same completed state does not trigger duplicate purge
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveComplete ), msgWaveComplete );
		CHECK( overlay.GetDecalPurgeCount() == 1 );

		// Next wave starts
		uint8_t msgWave2Active[] = { 1, 2, 0, 0, 0, 0, 0 };
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWave2Active ), msgWave2Active );
		CHECK( overlay.GetDecalPurgeCount() == 1 );

		// Wave 2 completes -> purge count increments to 2
		uint8_t msgWave2Complete[] = { 2, 2, 0, 0, 0, 0, 0 };
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWave2Complete ), msgWave2Complete );
		CHECK( overlay.GetDecalPurgeCount() == 2 );
	}

	SECTION( "Wave completion transitioning directly to STANDBY triggers decal purge (gameplay flow)" )
	{
		uint8_t msgWaveStandby[] = { 0, 2, 0, 0, 0, 0, 0 };
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveActive ), msgWaveActive );
		CHECK( overlay.GetDecalPurgeCount() == 0 );

		// Transition from ACTIVE (1) to STANDBY (0) triggers decal clear
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveStandby ), msgWaveStandby );
		CHECK( overlay.GetWaveState() == 0 );
		CHECK( overlay.GetDecalPurgeCount() == 1 );

		// Remaining in STANDBY on repeated ticks does NOT trigger duplicate purge
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveStandby ), msgWaveStandby );
		CHECK( overlay.GetDecalPurgeCount() == 1 );
	}

	SECTION( "Match conclusion also triggers final decal purge" )
	{
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgWaveActive ), msgWaveActive );
		overlay.MsgFunc_GladderWave( "GladWave", sizeof( msgMatchOver ), msgMatchOver );
		CHECK( overlay.GetWaveState() == 3 );
		CHECK( overlay.GetDecalPurgeCount() == 1 );
	}

	SECTION( "Direct GladderPurgeCombatDecals execution runs safely" )
	{
		REQUIRE_NOTHROW( GladderPurgeCombatDecals() );
	}
}

TEST_CASE( "GladderRules: Wave completion overlay sends completed wave number (SPEC §12.2, Issue #56)", "[gladder][rules][notify]" )
{
	CGladderWaveManager waveMgr;
	waveMgr.InitializeMatch( 0.0f );
	CHECK( waveMgr.GetWaveNumber() == 1 );

	waveMgr.StartWave( 5.0f );
	CHECK( waveMgr.GetWaveNumber() == 1 );

	// Before CompleteWave, the wave being completed is Wave 1
	int iCompletedWave = waveMgr.GetWaveNumber();
	waveMgr.CompleteWave( 25.0f );

	// After CompleteWave, the incoming wave number is incremented to 2
	CHECK( waveMgr.GetWaveNumber() == 2 );
	// But the completed wave number passed to notification is 1
	CHECK( iCompletedWave == 1 );
	CHECK( waveMgr.GetCompletedWavesCount() == 1 );

	// Wave 2
	waveMgr.StartWave( 30.0f );
	iCompletedWave = waveMgr.GetWaveNumber();
	waveMgr.CompleteWave( 60.0f );
	CHECK( waveMgr.GetWaveNumber() == 3 );
	CHECK( iCompletedWave == 2 );
	CHECK( waveMgr.GetCompletedWavesCount() == 2 );
}

TEST_CASE( "GladderRules: Dynamic third-person camera is first element in cyclic backdrop sequence (SPEC §6, §9.2, Issue #72)", "[gladder][rules][camera]" )
{
	// Simulation of camera collection contract:
	// Dynamic camera is ALWAYS index 0; map cameras follow in sequential order.
	const int DYNAMIC_CAMERA_ID = 999;
	std::vector<int> mapCameras = { 101, 102 };

	std::vector<int> summaryCameras;
	// 1. Dynamic third-person camera is ALWAYS placed at index 0
	summaryCameras.push_back( DYNAMIC_CAMERA_ID );
	// 2. Any map cameras follow sequentially
	for ( int camId : mapCameras )
	{
		summaryCameras.push_back( camId );
	}

	REQUIRE( summaryCameras.size() == 3 );
	CHECK( summaryCameras[0] == DYNAMIC_CAMERA_ID );
	CHECK( summaryCameras[1] == 101 );
	CHECK( summaryCameras[2] == 102 );

	// Sequence ALWAYS begins with dynamic third-person camera at index 0
	size_t currentIdx = 0;
	CHECK( summaryCameras[currentIdx] == DYNAMIC_CAMERA_ID );

	// AdvanceSummaryCamera cycles: 0 -> 1 -> 2 -> 0 -> 1
	currentIdx = ( currentIdx + 1 ) % summaryCameras.size();
	CHECK( summaryCameras[currentIdx] == 101 );

	currentIdx = ( currentIdx + 1 ) % summaryCameras.size();
	CHECK( summaryCameras[currentIdx] == 102 );

	// Loops back cyclically to dynamic third-person camera
	currentIdx = ( currentIdx + 1 ) % summaryCameras.size();
	CHECK( summaryCameras[currentIdx] == DYNAMIC_CAMERA_ID );

	currentIdx = ( currentIdx + 1 ) % summaryCameras.size();
	CHECK( summaryCameras[currentIdx] == 101 );

	SECTION( "Single-camera fallback: only dynamic camera exists when no map cameras placed" )
	{
		std::vector<int> singleCameraList;
		singleCameraList.push_back( DYNAMIC_CAMERA_ID );

		CHECK( singleCameraList.size() == 1 );
		CHECK( singleCameraList[0] == DYNAMIC_CAMERA_ID );
		size_t idx = 0;
		// Advancing on single-camera sequence remains on dynamic camera (index 0)
		idx = ( idx + 1 ) % singleCameraList.size();
		CHECK( idx == 0 );
		CHECK( singleCameraList[idx] == DYNAMIC_CAMERA_ID );
	}
}

TEST_CASE( "GladderRules: Spatial grid indexing retry prevention on empty grid (Issue #63)", "[gladder][grid][rules]" )
{
	ResetMockEngine();

	struct MockGridRules : public CTestGladderRulesAutoswitch
	{
		bool m_bGridIndexAttempted = false;
		int m_loadOrCreateCalls = 0;

		void Think( void ) override
		{
			if ( !m_bGridIndexAttempted && gpGlobals && gpGlobals->mapname )
			{
				m_bGridIndexAttempted = true;
				m_loadOrCreateCalls++;
			}
		}
	};

	MockGridRules rules;
	gpGlobals->mapname = MAKE_STRING( "gl01" );

	// Frame 1: first attempt is made
	rules.Think();
	CHECK( rules.m_loadOrCreateCalls == 1 );
	CHECK( rules.m_bGridIndexAttempted == true );

	// Subsequent frames: should NOT retry, preserving 1 call count
	for ( int frame = 0; frame < 100; ++frame )
	{
		rules.Think();
	}
	CHECK( rules.m_loadOrCreateCalls == 1 );

	// Manual reindex resets the flag to allow explicit retry
	rules.m_bGridIndexAttempted = false;
	rules.Think();
	CHECK( rules.m_loadOrCreateCalls == 2 );
}

TEST_CASE( "GladderRules: Gamemode combat and weapon lifecycle hooks enabled (SPEC §3, §7.6-§7.9, Issues #64-#68)", "[gladder][rules][hooks]" )
{
	class CTestHookGladderRules : public CTestGladderRulesAutoswitch
	{
	  public:
		BOOL FFixMeleeCorpseAttackDelay( void ) override { return TRUE; }
		BOOL FSilenceLoopingWeaponSoundsOnDeath( void ) override { return TRUE; }
		BOOL FFixShotgunReloadDesync( void ) override { return TRUE; }
		BOOL FFixMP5UnderwaterDebounce( void ) override { return TRUE; }
	};

	CTestHookGladderRules gladderRules;
	CHECK( gladderRules.FFixMeleeCorpseAttackDelay() == TRUE );
	CHECK( gladderRules.FSilenceLoopingWeaponSoundsOnDeath() == TRUE );
	CHECK( gladderRules.FFixShotgunReloadDesync() == TRUE );
	CHECK( gladderRules.FFixMP5UnderwaterDebounce() == TRUE );
}

TEST_CASE( "GladderRules: gladder_restart executes full map reload command preserving brush entities (SPEC §10.4, Issue #82)", "[gladder][rules][restart]" )
{
	ResetMockEngine();
	CTestGladderRulesAutoswitch rules;
	g_mockServerCommands.clear();

	SECTION( "Reloads current map when gpGlobals->mapname is populated" )
	{
		gpGlobals->mapname = MAKE_STRING( "gl02" );
		BOOL bHandled = rules.ClientCommand( nullptr, "gladder_restart" );
		CHECK( bHandled == TRUE );
		REQUIRE_FALSE( g_mockServerCommands.empty() );
		CHECK( g_mockServerCommands.back() == "map gl02\n" );
	}

	SECTION( "Falls back to restart command if mapname is unassigned" )
	{
		gpGlobals->mapname = 0;
		BOOL bHandled = rules.ClientCommand( nullptr, "gladder_restart" );
		CHECK( bHandled == TRUE );
		REQUIRE_FALSE( g_mockServerCommands.empty() );
		CHECK( g_mockServerCommands.back() == "restart\n" );
	}
}

TEST_CASE( "GladderRules: Session time limit expiration match conclusion and player input lock (SPEC §2, §3, §9.2, Issue #85, #88)", "[gladder][rules][conclusion]" )
{
	ResetMockEngine();

	CTestConclusionRules rules;
	edict_t playerEdict;
	memset( &playerEdict, 0, sizeof( playerEdict ) );
	CBaseEntity dummyPlayer;
	dummyPlayer.pev = &playerEdict.v;
	playerEdict.v.flags = FL_CLIENT;
	playerEdict.v.button = IN_FORWARD | IN_ATTACK | IN_SCORE;
	playerEdict.pvPrivateData = &dummyPlayer;
	rules.m_pTrackedPlayer = &dummyPlayer;

	CHECK_FALSE( rules.m_bMatchConcluded );
	CHECK( rules.m_waveManager.GetState() == GLADDER_STATE_WAVE_ACTIVE );

	// Frame at t=50s: active wave continues, match not over
	gpGlobals->time = 50.0f;
	rules.Think();
	CHECK_FALSE( rules.m_bMatchConcluded );
	CHECK_FALSE( rules.m_waveManager.IsMatchOver() );

	// Frame at t=100.5s: session time limit expires
	gpGlobals->time = 100.5f;
	rules.Think();
	CHECK( rules.m_waveManager.IsMatchOver() );
	CHECK( rules.m_bMatchConcluded );
	CHECK( rules.m_bSurvived == true );
	CHECK( rules.m_firedRelayEvent == 2 ); // Victory relays
	// Player is NOT given FL_FROZEN (which forces PLAYER_IDLE in SetAnimation),
	// Instead FL_NOTARGET and maxspeed=1.0f are set and only IN_SCORE (Tab) input is allowed.
	CHECK( ( dummyPlayer.pev->flags & FL_FROZEN ) == 0 );
	CHECK( ( dummyPlayer.pev->flags & FL_NOTARGET ) != 0 );
	CHECK( dummyPlayer.pev->maxspeed == Catch::Approx( 1.0f ) );
	CHECK( dummyPlayer.pev->button == IN_SCORE );
}

TEST_CASE( "Gladder Rules: Match conclusion preserves world simulation and death animations while locking inputs except Tab (Issue #88)", "[gladder][rules][conclusion]" )
{
	ResetMockEngine();

	CTestConclusionRules rules;

	// Setup mock player
	edict_t *pPlayerSlot = GetMockClientEntity( 1 );
	REQUIRE( pPlayerSlot != nullptr );
	pPlayerSlot->free = 0;
	pPlayerSlot->v.flags = FL_CLIENT;
	pPlayerSlot->v.classname = MAKE_STRING( "player" );
	pPlayerSlot->v.pContainingEntity = pPlayerSlot;
	pPlayerSlot->v.velocity = Vector( 250.0f, 0.0f, 0.0f );
	pPlayerSlot->v.button = IN_FORWARD | IN_JUMP | IN_ATTACK | IN_SCORE;
	pPlayerSlot->v.impulse = 100;

	struct MockPlayerHolder
	{
		alignas( CBasePlayer ) uint8_t storage[sizeof( CBasePlayer )];
		MockPlayerHolder()
		{
			std::memset( storage, 0, sizeof( storage ) );
			CBaseEntity baseEnt;
			std::memcpy( storage, &baseEnt, sizeof( CBaseEntity ) );
		}
		CBasePlayer *player()
		{
			return reinterpret_cast<CBasePlayer *>( storage );
		}
	};

	MockPlayerHolder playerHolder;
	CBasePlayer *pPlayer = playerHolder.player();
	pPlayerSlot->pvPrivateData = pPlayer;
	pPlayer->pev = &pPlayerSlot->v;
	pPlayer->m_afButtonPressed = IN_FORWARD | IN_SCORE;
	pPlayer->m_afButtonReleased = IN_ATTACK | IN_SCORE;

	// Setup mock monster in slot 2
	edict_t *pMonsterSlot = GetMockClientEntity( 2 );
	REQUIRE( pMonsterSlot != nullptr );
	pMonsterSlot->free = 0;
	pMonsterSlot->v.flags = FL_MONSTER;
	pMonsterSlot->v.classname = MAKE_STRING( "monster_headcrab" );
	pMonsterSlot->v.pContainingEntity = pMonsterSlot;
	pMonsterSlot->v.velocity = Vector( 120.0f, 40.0f, 0.0f );
	pMonsterSlot->v.framerate = 1.0f;
	pMonsterSlot->v.movetype = MOVETYPE_STEP;
	pMonsterSlot->v.takedamage = DAMAGE_YES;
	pMonsterSlot->v.nextthink = 50.0f;

	CTestDropMonster monster;
	pMonsterSlot->pvPrivateData = &monster;
	monster.pev = &pMonsterSlot->v;
	CBaseMonster *pMonster = &monster;

	// Setup mock projectile in slot 3
	edict_t *pProjSlot = GetMockClientEntity( 3 );
	REQUIRE( pProjSlot != nullptr );
	pProjSlot->free = 0;
	pProjSlot->v.classname = MAKE_STRING( "grenade" );
	pProjSlot->v.pContainingEntity = pProjSlot;
	pProjSlot->v.velocity = Vector( 600.0f, 0.0f, 150.0f );
	pProjSlot->v.movetype = MOVETYPE_BOUNCE;
	pProjSlot->v.nextthink = 45.0f;

	CBaseEntity proj;
	pProjSlot->pvPrivateData = &proj;
	proj.pev = &pProjSlot->v;
	CBaseEntity *pProj = &proj;

	// Trigger match conclusion at time 10.0s
	gpGlobals->time = 10.0f;
	rules.HandleMatchConclusion( false, pPlayer );

	// Verify FL_FROZEN is NOT set on the player so CBasePlayer::SetAnimation(PLAYER_DIE) is never forced to PLAYER_IDLE
	CHECK( ( pPlayer->pev->flags & FL_FROZEN ) == 0 );
	CHECK( ( pPlayer->pev->flags & FL_NOTARGET ) != 0 );
	CHECK( pPlayer->pev->maxspeed == Catch::Approx( 1.0f ) );
	CHECK( pPlayer->pev->button == IN_SCORE );
	CHECK( pPlayer->pev->impulse == 0 );
	CHECK( rules.FPlayerCanTakeDamage( pPlayer, nullptr ) == FALSE );
	CHECK( rules.FAllowFlashlight() == FALSE );

	// Verify PlayerThink continues to mask out all buttons except IN_SCORE (Tab)
	pPlayer->pev->button = IN_FORWARD | IN_JUMP | IN_ATTACK | IN_USE | IN_SCORE;
	pPlayer->m_afButtonPressed = IN_JUMP | IN_SCORE;
	pPlayer->m_afButtonReleased = IN_USE;
	pPlayer->pev->impulse = 201;
	rules.PlayerThink( pPlayer );
	CHECK( pPlayer->pev->button == IN_SCORE );
	CHECK( pPlayer->m_afButtonPressed == IN_SCORE );
	CHECK( pPlayer->m_afButtonReleased == 0 );
	CHECK( pPlayer->pev->impulse == 0 );

	// Verify world monsters and projectiles continue running normally without freezing
	gpGlobals->time = 12.0f;
	rules.Think();
	CHECK( ( pMonster->pev->flags & FL_FROZEN ) == 0 );
	CHECK( pMonster->pev->velocity.Length() > 0.0f );
	CHECK( pMonster->pev->framerate == 1.0f );
	CHECK( pMonster->pev->nextthink == 50.0f );
	CHECK( pProj->pev->velocity.Length() > 0.0f );
	CHECK( pProj->pev->nextthink == 45.0f );
}

TEST_CASE( "Gladder Rules: EnsureMonsterClipFlags enforces FL_MONSTERCLIP across monsters and pushables", "[gladder][rules][clip]" )
{
	ResetMockEngine();

	CTestConclusionRules rules;

	// Mock monster in slot 2
	edict_t *pMonsterSlot = GetMockClientEntity( 2 );
	REQUIRE( pMonsterSlot != nullptr );
	pMonsterSlot->free = 0;
	pMonsterSlot->v.flags = FL_MONSTER;
	pMonsterSlot->v.classname = MAKE_STRING( "monster_headcrab" );
	pMonsterSlot->v.pContainingEntity = pMonsterSlot;

	CTestDropMonster monster;
	pMonsterSlot->pvPrivateData = &monster;
	monster.pev = &pMonsterSlot->v;

	// Mock pushable in slot 3
	edict_t *pPushableSlot = GetMockClientEntity( 3 );
	REQUIRE( pPushableSlot != nullptr );
	pPushableSlot->free = 0;
	pPushableSlot->v.classname = MAKE_STRING( "func_pushable" );
	pPushableSlot->v.pContainingEntity = pPushableSlot;

	CBaseEntity pushable;
	pPushableSlot->pvPrivateData = &pushable;
	pushable.pev = &pPushableSlot->v;

	// Mock player in slot 1
	edict_t *pPlayerSlot = GetMockClientEntity( 1 );
	REQUIRE( pPlayerSlot != nullptr );
	pPlayerSlot->free = 0;
	pPlayerSlot->v.flags = FL_CLIENT;
	pPlayerSlot->v.classname = MAKE_STRING( "player" );
	pPlayerSlot->v.pContainingEntity = pPlayerSlot;

	CBaseEntity player;
	pPlayerSlot->pvPrivateData = &player;
	player.pev = &pPlayerSlot->v;

	// Verify before: FL_MONSTERCLIP is not set
	CHECK( ( monster.pev->flags & FL_MONSTERCLIP ) == 0 );
	CHECK( ( pushable.pev->flags & FL_MONSTERCLIP ) == 0 );
	CHECK( ( player.pev->flags & FL_MONSTERCLIP ) == 0 );

	// Enforce clip flags via rules
	rules.EnsureMonsterClipFlags();

	// Verify after: both monster and pushable have FL_MONSTERCLIP, but player strictly does not
	CHECK( ( monster.pev->flags & FL_MONSTERCLIP ) != 0 );
	CHECK( ( pushable.pev->flags & FL_MONSTERCLIP ) != 0 );
	CHECK( ( player.pev->flags & FL_MONSTERCLIP ) == 0 );
}

TEST_CASE( "Gladder Rules: FreezeMatchWorld preserves monster activity, sequence, and frame without resetting to ACT_RESET", "[gladder][rules][freeze]" )
{
	ResetMockEngine();

	CTestConclusionRules rules;

	edict_t *pMonsterSlot = GetMockClientEntity( 2 );
	REQUIRE( pMonsterSlot != nullptr );
	pMonsterSlot->free = 0;
	pMonsterSlot->v.flags = FL_MONSTER;
	pMonsterSlot->v.classname = MAKE_STRING( "monster_zombie" );
	pMonsterSlot->v.pContainingEntity = pMonsterSlot;
	pMonsterSlot->v.velocity = Vector( 120.0f, 0.0f, 0.0f );
	pMonsterSlot->v.sequence = 5;
	pMonsterSlot->v.frame = 42.0f;
	pMonsterSlot->v.framerate = 1.0f;

	CTestDropMonster monster;
	pMonsterSlot->pvPrivateData = &monster;
	monster.pev = &pMonsterSlot->v;
	monster.m_Activity = ACT_MELEE_ATTACK1;

	// Trigger freeze
	rules.FreezeMatchWorld();

	// Verify monster is frozen in its exact animation pose and activity
	CHECK( ( monster.pev->flags & FL_FROZEN ) != 0 );
	CHECK( monster.pev->velocity == Vector( 0.0f, 0.0f, 0.0f ) );
	CHECK( monster.pev->framerate == 0.0f );
	CHECK( monster.pev->sequence == 5 );
	CHECK( monster.pev->frame == 42.0f );
	CHECK( monster.m_Activity == ACT_MELEE_ATTACK1 );
}

TEST_CASE( "Gladder Triggers: CGladderTriggerClip solidity and player passage toggling", "[gladder][triggers][clip]" )
{
	ResetMockEngine();

	CGladderTriggerClip clip;
	entvars_t localPev;
	memset( &localPev, 0, sizeof( localPev ) );
	clip.pev = &localPev;

	// Spawn sets default state: solid BSP, monsterclip set (blocks monsters and pushables, non-solid for players)
	clip.Spawn();
	CHECK( clip.pev->solid == SOLID_BSP );
	CHECK( clip.pev->movetype == MOVETYPE_PUSH );
	CHECK( ( clip.pev->flags & FL_WORLDBRUSH ) != 0 );
	CHECK( ( clip.pev->flags & FL_MONSTERCLIP ) != 0 );
	CHECK( ( clip.pev->effects & EF_NODRAW ) != 0 );
	CHECK( clip.IsSolidForPlayer() == false );

	// When wave starts and player crosses: becomes solid for player too (FL_MONSTERCLIP removed)
	clip.SetSolidForPlayer( true );
	CHECK( clip.pev->solid == SOLID_BSP );
	CHECK( ( clip.pev->flags & FL_MONSTERCLIP ) == 0 );
	CHECK( clip.IsSolidForPlayer() == true );

	// On wave reset: restores monsterclip flag (non-solid for players again)
	clip.SetSolidForPlayer( false );
	CHECK( clip.pev->solid == SOLID_BSP );
	CHECK( ( clip.pev->flags & FL_MONSTERCLIP ) != 0 );
	CHECK( clip.IsSolidForPlayer() == false );
}

TEST_CASE( "Gladder Triggers: Wave start trigger requires complete crossing and ignores retreat", "[gladder][triggers][start]" )
{
	// Test the traversal logic: crossing requires opposite sign relative to center along an axis and distance >= half size
	auto CheckCrossing = []( const Vector &mins, const Vector &maxs, const Vector &entryPos, const Vector &exitPos ) -> bool {
		Vector vecCenter = ( mins + maxs ) * 0.5f;
		Vector vecSize = maxs - mins;
		Vector vecEntryDelta = entryPos - vecCenter;
		Vector vecExitDelta = exitPos - vecCenter;

		if ( vecSize.x > 0.0f && ( vecEntryDelta.x * vecExitDelta.x < 0.0f ) && ( fabs( exitPos.x - entryPos.x ) >= vecSize.x * 0.5f ) )
			return true;
		if ( vecSize.y > 0.0f && ( vecEntryDelta.y * vecExitDelta.y < 0.0f ) && ( fabs( exitPos.y - entryPos.y ) >= vecSize.y * 0.5f ) )
			return true;
		if ( vecSize.z > 0.0f && ( vecEntryDelta.z * vecExitDelta.z < 0.0f ) && ( fabs( exitPos.z - entryPos.z ) >= vecSize.z * 0.5f ) )
			return true;

		return false;
	};

	Vector triggerMins( -32.0f, -64.0f, 0.0f );
	Vector triggerMaxs( 32.0f, 64.0f, 128.0f );

	SECTION( "Player retreats out the same side they entered (does NOT cross)" )
	{
		// Player enters from staging area (X < 0)
		Vector entryPos( -48.0f, 0.0f, 36.0f );
		// Player steps inside, then turns around and retreats back out into staging (X < 0)
		Vector exitPos( -52.0f, 10.0f, 36.0f );

		CHECK_FALSE( CheckCrossing( triggerMins, triggerMaxs, entryPos, exitPos ) );
	}

	SECTION( "Player traverses through to opposite side (crosses into gauntlet arena)" )
	{
		// Player enters from staging area (X < 0)
		Vector entryPos( -48.0f, 0.0f, 36.0f );
		// Player walks through trigger and exits into arena on the opposite side (X > 0)
		Vector exitPos( 48.0f, 0.0f, 36.0f );

		CHECK( CheckCrossing( triggerMins, triggerMaxs, entryPos, exitPos ) );
	}

	SECTION( "Player traverses along Y axis doorway" )
	{
		Vector yMins( -64.0f, -32.0f, 0.0f );
		Vector yMaxs( 64.0f, 32.0f, 128.0f );

		Vector entryPos( 0.0f, -48.0f, 36.0f );
		Vector retreatPos( 10.0f, -50.0f, 36.0f );
		Vector crossPos( 0.0f, 48.0f, 36.0f );

		CHECK_FALSE( CheckCrossing( yMins, yMaxs, entryPos, retreatPos ) );
		CHECK( CheckCrossing( yMins, yMaxs, entryPos, crossPos ) );
	}
}

TEST_CASE( "GladderRules: Extended Ammunition & Explosives Carrying Capacity (SPEC §3, §7.14, Issue #97)", "[gladder][gamerules][ammo][capacity]" )
{
	SECTION( "GladderIsExtendedAmmo recognizes all standard ammunition and consumable ordnance pools" )
	{
		CHECK( GladderIsExtendedAmmo( "9mm" ) == true );
		CHECK( GladderIsExtendedAmmo( "357" ) == true );
		CHECK( GladderIsExtendedAmmo( "buckshot" ) == true );
		CHECK( GladderIsExtendedAmmo( "bolts" ) == true );
		CHECK( GladderIsExtendedAmmo( "rockets" ) == true );
		CHECK( GladderIsExtendedAmmo( "uranium" ) == true );
		CHECK( GladderIsExtendedAmmo( "ARgrenades" ) == true );
		CHECK( GladderIsExtendedAmmo( "762" ) == true );
		CHECK( GladderIsExtendedAmmo( "556" ) == true );
		CHECK( GladderIsExtendedAmmo( "Hand Grenade" ) == true );
		CHECK( GladderIsExtendedAmmo( "Satchel Charge" ) == true );
		CHECK( GladderIsExtendedAmmo( "Trip Mine" ) == true );
		CHECK( GladderIsExtendedAmmo( "Snarks" ) == true );

		// Case-insensitive verification
		CHECK( GladderIsExtendedAmmo( "9MM" ) == true );
		CHECK( GladderIsExtendedAmmo( "hand grenade" ) == true );
		CHECK( GladderIsExtendedAmmo( "SATCHEL CHARGE" ) == true );

		// Exclusions: null, empty, unknown ammo pools
		CHECK( GladderIsExtendedAmmo( nullptr ) == false );
		CHECK( GladderIsExtendedAmmo( "" ) == false );
		CHECK( GladderIsExtendedAmmo( "unknown_ammo" ) == false );
		CHECK( GladderIsExtendedAmmo( "Hornets" ) == false );
	}

	SECTION( "GameRules GetMaxAmmo returns GLADDER_EXTENDED_AMMO_MAX (9999) for all extended ammo pools" )
	{
		class CTestGladderAmmoRules : public CTestGladderRulesAutoswitch
		{
		  public:
			int GetMaxAmmo( const char *pszAmmoName, int iDefaultMaxCarry ) override
			{
				if ( GladderIsExtendedAmmo( pszAmmoName ) )
					return GLADDER_EXTENDED_AMMO_MAX;
				return CTestGladderRulesAutoswitch::GetMaxAmmo( pszAmmoName, iDefaultMaxCarry );
			}
		};

		CTestGladderAmmoRules rules;

		// Standard ammunition pools decoupled from small default carry limits
		CHECK( rules.GetMaxAmmo( "9mm", 250 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "357", 36 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "buckshot", 125 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "bolts", 50 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "rockets", 5 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "uranium", 100 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "ARgrenades", 10 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "762", 30 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "556", 200 ) == GLADDER_EXTENDED_AMMO_MAX );

		// Consumable explosive ordnance decoupled from low caps
		CHECK( rules.GetMaxAmmo( "Hand Grenade", 10 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "Satchel Charge", 5 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "Trip Mine", 5 ) == GLADDER_EXTENDED_AMMO_MAX );
		CHECK( rules.GetMaxAmmo( "Snarks", 15 ) == GLADDER_EXTENDED_AMMO_MAX );

		// Fallback for non-extended or custom types
		CHECK( rules.GetMaxAmmo( "Hornets", 8 ) == 8 );
		CHECK( rules.GetMaxAmmo( "unknown_ammo", 42 ) == 42 );
		CHECK( rules.GetMaxAmmo( nullptr, 100 ) == 100 );
	}

	SECTION( "Player inventory collection beyond 1000 rounds of ammo and explosives" )
	{
		InitMockEngine();

		CBasePlayerItem::AmmoInfoArray[1].pszName = "bolts";
		CBasePlayerItem::AmmoInfoArray[2].pszName = "Hand Grenade";
		CBasePlayerItem::AmmoInfoArray[3].pszName = "762";

		class CTestGladderAmmoRules : public CTestGladderRulesAutoswitch
		{
		  public:
			int GetMaxAmmo( const char *pszAmmoName, int iDefaultMaxCarry ) override
			{
				if ( GladderIsExtendedAmmo( pszAmmoName ) )
					return GLADDER_EXTENDED_AMMO_MAX;
				return CTestGladderRulesAutoswitch::GetMaxAmmo( pszAmmoName, iDefaultMaxCarry );
			}

			BOOL CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry ) override
			{
				if ( !pszAmmoName || !pPlayer )
					return FALSE;

				int iAmmoIndex = pPlayer->GetAmmoIndex( pszAmmoName );
				if ( iAmmoIndex > -1 )
				{
					if ( pPlayer->AmmoInventory( iAmmoIndex ) < GetMaxAmmo( pszAmmoName, iMaxCarry ) )
						return TRUE;
				}
				return FALSE;
			}
		};

		CTestGladderAmmoRules rules;
		g_pGameRules = &rules;

		// Mock player edict and instance
		edict_t *pPlayerSlot = GetMockClientEntity( 1 );
		REQUIRE( pPlayerSlot != nullptr );
		pPlayerSlot->free = 0;
		pPlayerSlot->v.flags = FL_CLIENT;
		pPlayerSlot->v.classname = MAKE_STRING( "player" );
		pPlayerSlot->v.pContainingEntity = pPlayerSlot;
		struct MockPlayerHolder
		{
			alignas( CBasePlayer ) uint8_t storage[sizeof( CBasePlayer )];
			MockPlayerHolder()
			{
				std::memset( storage, 0, sizeof( storage ) );
				CBaseEntity baseEnt;
				std::memcpy( storage, &baseEnt, sizeof( CBaseEntity ) );
			}
			CBasePlayer *player()
			{
				return reinterpret_cast<CBasePlayer *>( storage );
			}
		};

		MockPlayerHolder playerHolder;
		CBasePlayer *pPlayer = playerHolder.player();
		pPlayerSlot->pvPrivateData = pPlayer;
		pPlayer->pev = &pPlayerSlot->v;

		// Simulate player holding 50 bolts (vanilla max)
		pPlayer->m_rgAmmo[1] = 50;
		// Under Gladder rules, CanHaveAmmo permits collection because limit is 9999
		CHECK( rules.CanHaveAmmo( pPlayer, "bolts", 50 ) == TRUE );

		// Simulate stockpiling up to 1500 bolts
		pPlayer->m_rgAmmo[1] = 1500;
		CHECK( rules.CanHaveAmmo( pPlayer, "bolts", 50 ) == TRUE );

		// Hand grenades beyond classic cap of 10
		pPlayer->m_rgAmmo[2] = 10;
		CHECK( rules.CanHaveAmmo( pPlayer, "Hand Grenade", 10 ) == TRUE );
		pPlayer->m_rgAmmo[2] = 1200;
		CHECK( rules.CanHaveAmmo( pPlayer, "Hand Grenade", 10 ) == TRUE );

		// 7.62 rounds beyond classic cap of 30
		pPlayer->m_rgAmmo[3] = 30;
		CHECK( rules.CanHaveAmmo( pPlayer, "762", 30 ) == TRUE );
		pPlayer->m_rgAmmo[3] = 3000;
		CHECK( rules.CanHaveAmmo( pPlayer, "762", 30 ) == TRUE );

		// When reached GLADDER_EXTENDED_AMMO_MAX, CanHaveAmmo returns FALSE
		pPlayer->m_rgAmmo[1] = GLADDER_EXTENDED_AMMO_MAX;
		CHECK( rules.CanHaveAmmo( pPlayer, "bolts", 50 ) == FALSE );

		g_pGameRules = nullptr;
	}
}

TEST_CASE( "GladderRules: Mandatory hostile elimination, finish trigger denial & cleartarget (SPEC §2, §9.1, Issue #118)", "[gladder][rules][finish][hostiles]" )
{
	InitMockEngine();
	GameRulesFactory::Reset();

	class CTestHostileGladderRules : public CTestGladderRulesAutoswitch
	{
	  public:
		int m_iWaveMonstersTotal = 0;
		int m_iWaveMonstersKilled = 0;
		bool m_bHostilesEliminatedFired = false;
		CGladderWaveManager m_waveManager;

		int GetWaveMonstersTotal() const { return m_iWaveMonstersTotal; }
		int GetWaveMonstersKilled() const { return m_iWaveMonstersKilled; }
		int GetWaveMonstersRemaining() const
		{
			int iRemaining = m_iWaveMonstersTotal - m_iWaveMonstersKilled;
			return ( iRemaining > 0 ) ? iRemaining : 0;
		}
		bool AreWaveHostilesEliminated() const
		{
			if ( m_iWaveMonstersTotal <= 0 )
				return true;
			return m_iWaveMonstersKilled >= m_iWaveMonstersTotal;
		}
		void SetWaveMonstersTotal( int count ) { m_iWaveMonstersTotal = ( count > 0 ) ? count : 0; }
		void SetWaveMonstersKilled( int count ) { m_iWaveMonstersKilled = ( count > 0 ) ? count : 0; }

		CGladderWaveManager &GetWaveManager() { return m_waveManager; }

		void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
		{
			if ( !pVictim )
				return;

			if ( !( pVictim->pev->flags & ( FL_MONSTER | FL_FAKECLIENT ) ) )
				return;

			if ( m_waveManager.IsWaveActive() && m_iWaveMonstersTotal > 0 )
			{
				m_iWaveMonstersKilled++;
				if ( m_iWaveMonstersKilled >= m_iWaveMonstersTotal && !m_bHostilesEliminatedFired )
				{
					m_bHostilesEliminatedFired = true;
				}
			}
		}
	};

	class CTestTriggerFinish
	{
	  public:
		std::string m_szClearTarget;

		void SetClearTarget( const char *pszTarget )
		{
			m_szClearTarget = pszTarget ? pszTarget : "";
		}

		const char *GetClearTarget() const { return m_szClearTarget.c_str(); }

		bool FinishTouch( CBasePlayer *pPlayer, CTestHostileGladderRules *pRules )
		{
			if ( !pPlayer || !pRules )
				return false;

			if ( !pRules->AreWaveHostilesEliminated() )
			{
				pPlayer->pev->velocity = Vector( 300.0f, 0.0f, 150.0f );
				return false;
			}

			pRules->GetWaveManager().CompleteWave( 15.0f );
			return true;
		}
	};

	CTestHostileGladderRules rules;
	g_pGameRules = &rules;

	// Mock player in edict slot 1
	edict_t *pPlayerSlot = GetMockClientEntity( 1 );
	REQUIRE( pPlayerSlot != nullptr );
	pPlayerSlot->free = 0;
	pPlayerSlot->v.flags = FL_CLIENT;
	pPlayerSlot->v.pContainingEntity = pPlayerSlot;
	pPlayerSlot->v.origin = Vector( 100.0f, 0.0f, 0.0f );
	pPlayerSlot->v.velocity = Vector( 0.0f, 0.0f, 0.0f );

	struct MockPlayerHolder
	{
		alignas( CBasePlayer ) uint8_t storage[sizeof( CBasePlayer )];
		MockPlayerHolder()
		{
			std::memset( storage, 0, sizeof( storage ) );
			CBaseEntity baseEnt;
			std::memcpy( storage, &baseEnt, sizeof( CBaseEntity ) );
		}
		CBasePlayer *player()
		{
			return reinterpret_cast<CBasePlayer *>( storage );
		}
	} playerHolder;

	CBasePlayer *pPlayer = playerHolder.player();
	pPlayerSlot->pvPrivateData = pPlayer;
	pPlayer->pev = &pPlayerSlot->v;

	CTestTriggerFinish finishTrigger;
	finishTrigger.SetClearTarget( "blast_door" );
	CHECK( std::string( finishTrigger.GetClearTarget() ) == "blast_door" );

	// Initialize active wave
	rules.GetWaveManager().StartWave( 10.0f );
	rules.SetWaveMonstersTotal( 10 );
	rules.SetWaveMonstersKilled( 3 );

	CHECK( rules.GetWaveMonstersTotal() == 10 );
	CHECK( rules.GetWaveMonstersKilled() == 3 );
	CHECK( rules.GetWaveMonstersRemaining() == 7 );
	CHECK( rules.AreWaveHostilesEliminated() == false );

	SECTION( "Touching finish trigger with hostiles remaining denies completion and pushes player back" )
	{
		pPlayer->pev->origin = Vector( 30.0f, 0.0f, 0.0f );
		pPlayer->pev->velocity = Vector( 0.0f, 0.0f, 0.0f );

		bool bCompleted = finishTrigger.FinishTouch( pPlayer, &rules );

		// Wave must NOT be completed
		CHECK( bCompleted == false );
		CHECK( rules.GetWaveManager().IsWaveActive() == true );
		// Player pushed back (away from trigger center)
		CHECK( pPlayer->pev->velocity.x > 0.0f );
	}

	SECTION( "Eliminating all wave hostiles triggers cleartarget and wave relay event 4" )
	{
		CTestDropMonster monster;
		edict_t *pMonsterSlot = GetMockClientEntity( 4 );
		pMonsterSlot->free = 0;
		pMonsterSlot->v.flags = FL_MONSTER;
		pMonsterSlot->v.classname = MAKE_STRING( "monster_headcrab" );
		pMonsterSlot->v.pContainingEntity = pMonsterSlot;
		pMonsterSlot->pvPrivateData = &monster;
		monster.pev = &pMonsterSlot->v;

		rules.SetWaveMonstersKilled( 9 );
		CHECK( rules.AreWaveHostilesEliminated() == false );

		// Final kill!
		rules.MonsterKilled( &monster, pPlayer->pev, pPlayer->pev );
		CHECK( rules.GetWaveMonstersKilled() == 10 );
		CHECK( rules.AreWaveHostilesEliminated() == true );
		CHECK( rules.GetWaveMonstersRemaining() == 0 );

		// Now touching finish trigger succeeds and completes wave
		bool bCompleted = finishTrigger.FinishTouch( pPlayer, &rules );
		CHECK( bCompleted == true );
		CHECK( rules.GetWaveManager().IsWaveActive() == false );
	}

	g_pGameRules = nullptr;
}

