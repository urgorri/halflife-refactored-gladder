/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Behavioral Equivalence & Unit Test Suite - Phase 1 Foundation & Lifecycle Core
 *
 ****/

#include "external/catch2/catch_amalgamated.hpp"
#include <cstring>
#include <vector>
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#endif

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "core/player.h"

#include "tests/mock_engine.h"
#include "gameplay/gamerules.h"
#include "gameplay/gamerules_factory.h"
#include "dlls/gladder/gladder_wave_manager.h"

#include "dlls/gladder/gladder_usermsg.h"
#include "cl_dll/gladder/hud_gladder_overlay.h"


TEST_CASE( "Gladder Phase 1: Wave State Machine Transitions", "[gladder][wave_manager]" )
{
	CGladderWaveManager manager;
	manager.SetSessionTimeLimit( 600.0f );
	manager.InitializeMatch( 100.0f );

	SECTION( "Initial state after match initialization" )
	{
		REQUIRE( manager.GetState() == GLADDER_STATE_WAITING_FOR_START );
		REQUIRE( manager.GetWaveNumber() == 1 );
		REQUIRE( manager.GetCompletedWavesCount() == 0 );
		REQUIRE( manager.GetSessionTimeRemaining( 100.0f ) == Catch::Approx( 600.0f ) );
		REQUIRE( manager.GetActiveLapTime( 100.0f ) == 0.0f );
		REQUIRE( manager.GetFastestLapTime() == 0.0f );
		REQUIRE( manager.GetSlowestLapTime() == 0.0f );
		REQUIRE( manager.GetAverageLapTime() == 0.0f );
	}

	SECTION( "Start wave transitions to GLADDER_STATE_WAVE_ACTIVE" )
	{
		manager.StartWave( 110.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_WAVE_ACTIVE );
		REQUIRE( manager.GetActiveLapTime( 125.0f ) == Catch::Approx( 15.0f ) );
	}

	SECTION( "Completing a wave records lap telemetry and increments wave" )
	{
		manager.StartWave( 100.0f );
		manager.CompleteWave( 120.0f ); // 20 second lap
		// CompleteWave now transitions directly to STANDBY (WAITING_FOR_START) for the
		// next wave, skipping the transient WAVE_COMPLETED state. The HUD shows
		// "WAVE N+1 [STANDBY]" immediately so the player can prepare without confusion.
		REQUIRE( manager.GetState() == GLADDER_STATE_WAITING_FOR_START );
		REQUIRE( manager.GetCompletedWavesCount() == 1 );
		REQUIRE( manager.GetWaveNumber() == 2 );
		REQUIRE( manager.GetLapTimes().back() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetFastestLapTime() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetSlowestLapTime() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetAverageLapTime() == Catch::Approx( 20.0f ) );

		// Start wave 2 and complete with a slower lap (40 seconds)
		manager.StartWave( 130.0f );
		manager.CompleteWave( 170.0f );
		REQUIRE( manager.GetCompletedWavesCount() == 2 );
		REQUIRE( manager.GetWaveNumber() == 3 );
		REQUIRE( manager.GetLapTimes().back() == Catch::Approx( 40.0f ) );
		REQUIRE( manager.GetFastestLapTime() == Catch::Approx( 20.0f ) );
		REQUIRE( manager.GetSlowestLapTime() == Catch::Approx( 40.0f ) );
		REQUIRE( manager.GetAverageLapTime() == Catch::Approx( 30.0f ) );

		// Start wave 3 and complete with a faster lap (12 seconds)
		manager.StartWave( 180.0f );
		manager.CompleteWave( 192.0f );
		REQUIRE( manager.GetCompletedWavesCount() == 3 );
		REQUIRE( manager.GetWaveNumber() == 4 );
		REQUIRE( manager.GetFastestLapTime() == Catch::Approx( 12.0f ) );
		REQUIRE( manager.GetSlowestLapTime() == Catch::Approx( 40.0f ) );
		REQUIRE( manager.GetAverageLapTime() == Catch::Approx( ( 20.0f + 40.0f + 12.0f ) / 3.0f ) );
	}

	SECTION( "Match ends immediately when EndMatch is called" )
	{
		manager.StartWave( 100.0f );
		manager.EndMatch( 150.0f, false );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
		REQUIRE( manager.IsMatchOver() == true );

		// Subsequent start/complete actions must not change MATCH_OVER state
		manager.StartWave( 160.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
		manager.CompleteWave( 170.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
	}

	SECTION( "Session timer expiration during Tick triggers GLADDER_STATE_MATCH_OVER" )
	{
		manager.StartWave( 100.0f );
		// Advance tick beyond the 600s session limit (100 + 601 = 701.0f)
		manager.Tick( 701.0f );
		REQUIRE( manager.GetState() == GLADDER_STATE_MATCH_OVER );
		REQUIRE( manager.IsMatchOver() == true );
		REQUIRE( manager.GetSessionTimeRemaining( 701.0f ) == 0.0f );
	}
}

TEST_CASE( "Gladder Phase 1: Wall Charger Capacity Degradation", "[gladder][rules]" )
{
	// Formula:
	// Wave 1: 1.0f * 100%
	// Wave 2: 0.85f * 100%
	// Wave 3: 0.70f * 100%
	// Wave >= 6: 0.25f (clamped minimum)
	auto CalcHealthCapacity = []( int waveNum ) -> float {
		float factor = 1.0f - ( waveNum - 1 ) * 0.15f;
		if ( factor < 0.25f )
			factor = 0.25f;
		return factor * 100.0f; // assuming base 100.0f
	};

	auto CalcHEVCapacity = []( int waveNum ) -> float {
		float factor = 1.0f - ( waveNum - 1 ) * 0.15f;
		if ( factor < 0.25f )
			factor = 0.25f;
		return factor * 100.0f; // assuming base 100.0f
	};

	REQUIRE( CalcHealthCapacity( 1 ) == Catch::Approx( 100.0f ) );
	REQUIRE( CalcHealthCapacity( 2 ) == Catch::Approx( 85.0f ) );
	REQUIRE( CalcHealthCapacity( 3 ) == Catch::Approx( 70.0f ) );
	REQUIRE( CalcHealthCapacity( 4 ) == Catch::Approx( 55.0f ) );
	REQUIRE( CalcHealthCapacity( 5 ) == Catch::Approx( 40.0f ) );
	REQUIRE( CalcHealthCapacity( 6 ) == Catch::Approx( 25.0f ) );
	REQUIRE( CalcHealthCapacity( 10 ) == Catch::Approx( 25.0f ) ); // minimum clamped

	REQUIRE( CalcHEVCapacity( 1 ) == Catch::Approx( 100.0f ) );
	REQUIRE( CalcHEVCapacity( 6 ) == Catch::Approx( 25.0f ) );
}

class CGladderRulesTestRules : public CGameRules
{
  public:
	// Implementation conforming to CGladderRules specification (SPEC §1/§2, Issue #31)
	BOOL FAllowAutoSave( void ) override { return FALSE; }
	BOOL FAllowSave( void ) override { return FALSE; }
	BOOL FAllowRestore( void ) override { return FALSE; }
	void OnSaveDenied( void ) override
	{
		m_bSaveDeniedCalled = true;
		m_bPendingSavePurge = true;
	}
	void OnRestoreDenied( void ) override
	{
		m_bRestoreDeniedCalled = true;
		m_bRestoreAttempted = true;
		SERVER_COMMAND( "disconnect\n" );
	}
	bool m_bSaveDeniedCalled = false;
	bool m_bRestoreDeniedCalled = false;
	bool m_bRestoreAttempted = false;
	bool m_bPendingSavePurge = false;
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override { return FALSE; }
	void PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse ) override {}
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
	float FlPlayerSpawnTime( CBasePlayer *pPlayer ) override { return 0.0f; }
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

// Neutralizes client console save/load commands and keybinds via client-side aliases (SPEC §1/§2, Issue #31)
static void Gladder_InstallClientSaveAliases( edict_t *pPlayerEdict )
{
	if ( !pPlayerEdict )
		return;

	CLIENT_COMMAND( pPlayerEdict, "alias save \"echo [Gladder] Saving is disabled in Half-Life: Gladder.\"\n" );
	CLIENT_COMMAND( pPlayerEdict, "alias load \"echo [Gladder] Loading is disabled in Half-Life: Gladder.\"\n" );
	CLIENT_COMMAND( pPlayerEdict, "alias quicksave \"echo [Gladder] QuickSave is disabled in Half-Life: Gladder.\"\n" );
	CLIENT_COMMAND( pPlayerEdict, "alias quickload \"echo [Gladder] QuickLoad is disabled in Half-Life: Gladder.\"\n" );
	CLIENT_COMMAND( pPlayerEdict, "alias autosave \"echo [Gladder] AutoSave is disabled in Half-Life: Gladder.\"\n" );
	CLIENT_COMMAND( pPlayerEdict, "alias reload \"echo [Gladder] Reload is disabled in Half-Life: Gladder.\"\n" );
}

static void Gladder_PurgeSaveFiles( void )
{
#ifdef _WIN32
	const char *searchPatterns[] = { "SAVE\\*.sav", "save\\*.sav" };
	for ( size_t i = 0; i < sizeof( searchPatterns ) / sizeof( searchPatterns[0] ); ++i )
	{
		WIN32_FIND_DATAA fd;
		HANDLE hFind = FindFirstFileA( searchPatterns[i], &fd );
		if ( hFind != INVALID_HANDLE_VALUE )
		{
			do
			{
				if ( !( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					char szPath[MAX_PATH];
					const char *dir = ( i == 0 ) ? "SAVE" : "save";
					snprintf( szPath, sizeof( szPath ), "%s\\%s", dir, fd.cFileName );
					DeleteFileA( szPath );
				}
			} while ( FindNextFileA( hFind, &fd ) );
			FindClose( hFind );
		}
	}
#else
	const char *dirs[] = { "SAVE", "save" };
	for ( size_t i = 0; i < sizeof( dirs ) / sizeof( dirs[0] ); ++i )
	{
		DIR *dir = opendir( dirs[i] );
		if ( dir )
		{
			struct dirent *entry;
			while ( ( entry = readdir( dir ) ) != NULL )
			{
				const char *name = entry->d_name;
				size_t len = strlen( name );
				if ( len > 4 && strcmp( name + len - 4, ".sav" ) == 0 )
				{
					char szPath[512];
					snprintf( szPath, sizeof( szPath ), "%s/%s", dirs[i], name );
					unlink( szPath );
				}
			}
			closedir( dir );
		}
	}
#endif
}

TEST_CASE( "Gladder Phase 1: Save & Respawn Prevention Logic (#31)", "[gladder][rules][saverestore]" )
{
	SECTION( "Gladder GameRules must forbid saving and restoring mid-game" )
	{
		ResetMockEngine();
		CGladderRulesTestRules rules;
		g_pGameRules = &rules;

		// Gladder must veto all save/restore hooks:
		REQUIRE( rules.FAllowSave() == FALSE );
		REQUIRE( rules.FAllowRestore() == FALSE );
		REQUIRE( rules.FAllowAutoSave() == FALSE );
		REQUIRE( rules.FPlayerCanRespawn( nullptr ) == FALSE );

		edict_t edict;
		std::memset( &edict, 0, sizeof( edict ) );
		CBaseEntity testEntity;
		edict.pvPrivateData = &testEntity;
		testEntity.pev       = &edict.v;

		SAVERESTOREDATA saveData;
		std::memset( &saveData, 0, sizeof( saveData ) );

		DispatchSave( &edict, &saveData );
		CHECK_FALSE( g_mockSaveCalled );

		int res = DispatchRestore( &edict, &saveData, 0 );
		CHECK( res == 0 );
		CHECK_FALSE( g_mockRestoreCalled );

		// SaveGlobalState triggers OnSaveDenied callback and sets pending purge flag
		SaveGlobalState( &saveData );
		CHECK( rules.m_bSaveDeniedCalled );
		CHECK( rules.m_bPendingSavePurge );
		CHECK_FALSE( g_mockSaveCalled );

		// RestoreGlobalState triggers OnRestoreDenied callback and issues disconnect command
		RestoreGlobalState( &saveData );
		CHECK( rules.m_bRestoreDeniedCalled );
		CHECK( rules.m_bRestoreAttempted );
		CHECK_FALSE( g_mockRestoreCalled );
		REQUIRE_FALSE( g_mockServerCommands.empty() );
		CHECK( g_mockServerCommands.back() == "disconnect\n" );

		g_pGameRules = nullptr;
	}

	SECTION( "Gladder savefile purge removes .sav files from disk" )
	{
#ifdef _WIN32
		CreateDirectoryA( "SAVE", NULL );
		FILE *fp = fopen( "SAVE\\test_gladder_dummy.sav", "wb" );
		if ( fp )
		{
			fputs( "dummy", fp );
			fclose( fp );
		}
		FILE *checkFp = fopen( "SAVE\\test_gladder_dummy.sav", "rb" );
		REQUIRE( checkFp != nullptr );
		fclose( checkFp );

		Gladder_PurgeSaveFiles();

		FILE *afterFp = fopen( "SAVE\\test_gladder_dummy.sav", "rb" );
		CHECK( afterFp == nullptr );
#endif
	}

	SECTION( "Gladder client alias installer must neutralize all 6 save/load/reload commands" )
	{
		ResetMockEngine();
		edict_t playerEdict;
		std::memset( &playerEdict, 0, sizeof( playerEdict ) );

		Gladder_InstallClientSaveAliases( &playerEdict );

		REQUIRE( g_mockClientCommands.size() >= 6 );
		CHECK( g_mockClientCommands[0].find( "alias save" ) != std::string::npos );
		CHECK( g_mockClientCommands[1].find( "alias load" ) != std::string::npos );
		CHECK( g_mockClientCommands[2].find( "alias quicksave" ) != std::string::npos );
		CHECK( g_mockClientCommands[3].find( "alias quickload" ) != std::string::npos );
		CHECK( g_mockClientCommands[4].find( "alias autosave" ) != std::string::npos );
		CHECK( g_mockClientCommands[5].find( "alias reload" ) != std::string::npos );
	}


	SECTION( "Case-insensitive detection of blocked save/load commands" )
	{
		const char *blockedCommands[] = {
			"save", "load", "reload", "autosave", "quickload", "quicksave",
			"SAVE", "LOAD", "RELOAD", "AutoSave", "QuickSave"
		};

		auto CaseInsensitiveEquals = []( const char *s1, const char *s2 ) -> bool {
			if ( !s1 || !s2 )
				return s1 == s2;
			while ( *s1 && *s2 )
			{
				if ( tolower( (unsigned char)*s1 ) != tolower( (unsigned char)*s2 ) )
					return false;
				s1++;
				s2++;
			}
			return *s1 == *s2;
		};

		auto IsBlockedCommand = [&]( const char *pcmd ) -> bool {
			if ( !pcmd )
				return false;
			if ( CaseInsensitiveEquals( pcmd, "save" ) ||
			     CaseInsensitiveEquals( pcmd, "load" ) ||
			     CaseInsensitiveEquals( pcmd, "reload" ) ||
			     CaseInsensitiveEquals( pcmd, "autosave" ) ||
			     CaseInsensitiveEquals( pcmd, "quickload" ) ||
			     CaseInsensitiveEquals( pcmd, "quicksave" ) )
			{
				return true;
			}
			return false;
		};

		for ( const char *cmd : blockedCommands )
		{
			CHECK( IsBlockedCommand( cmd ) == true );
		}

		CHECK( IsBlockedCommand( "attack" ) == false );
		CHECK( IsBlockedCommand( "use" ) == false );
		CHECK( IsBlockedCommand( "jump" ) == false );
	}
}


TEST_CASE( "Gladder Phase 1: Client HUD Overlay Telemetry Sync", "[gladder][hud]" )
{
	CHudGladderOverlay hud;
	hud.Reset();

	REQUIRE( hud.GetWaveNumber() == 1 );
	REQUIRE( hud.GetTimeRemaining() == Catch::Approx( 600.0f ) );
	REQUIRE( hud.GetFrags() == 0 );
	REQUIRE( hud.GetCompletedLaps() == 0 );

	// Simulate wave update
	hud.SetWaveState( GLADDER_STATE_WAVE_ACTIVE, 3, 450.0f, 15.5f );
	REQUIRE( hud.GetWaveState() == GLADDER_STATE_WAVE_ACTIVE );
	REQUIRE( hud.GetWaveNumber() == 3 );
	REQUIRE( hud.GetTimeRemaining() == Catch::Approx( 450.0f ) );
	REQUIRE( hud.GetLapTime() == Catch::Approx( 15.5f ) );

	// Simulate telemetry metrics update
	hud.SetTelemetry( 12.0f, 35.0f, 21.5f, 2, 47 );
	REQUIRE( hud.GetFastestLap() == Catch::Approx( 12.0f ) );
	REQUIRE( hud.GetSlowestLap() == Catch::Approx( 35.0f ) );
	REQUIRE( hud.GetAverageLap() == Catch::Approx( 21.5f ) );
	REQUIRE( hud.GetCompletedLaps() == 2 );
	REQUIRE( hud.GetFrags() == 47 );
}


