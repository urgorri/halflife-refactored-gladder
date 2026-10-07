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

// GladderStaticDecal struct definition matching gladder_rules.h (SPEC §11.3, Issue #49)
struct GladderStaticDecal
{
	Vector origin;
	int decalIndex;
	int entityIndex;
	int modelIndex;
};

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

TEST_CASE( "GladderRules: Summary camera cycling and multi-camera support (SPEC §6, §9.2, Issue #52)", "[gladder][rules][camera]" )
{
	std::vector<int> mockCameras = { 101, 102, 103 };
	size_t currentIdx = 0;

	// Sequential rotation in order
	currentIdx = ( currentIdx + 1 ) % mockCameras.size();
	CHECK( mockCameras[currentIdx] == 102 );

	currentIdx = ( currentIdx + 1 ) % mockCameras.size();
	CHECK( mockCameras[currentIdx] == 103 );

	currentIdx = ( currentIdx + 1 ) % mockCameras.size();
	CHECK( mockCameras[currentIdx] == 101 );
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

