/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Gladder Game Rules Definition
 *
 ****/

#pragma once

#ifndef GLADDER_RULES_H
#define GLADDER_RULES_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "gameplay/gamerules.h"
#include "gameplay/gamerules_factory.h"
#include "gladder_wave_manager.h"
#include "gladder_grid_indexer.h"
#include "gladder_spawner.h"
#include "gladder_combo_tracker.h"
#include "gladder_scoring.h"
#include "gladder_modifiers.h"
#include "gladder_monster_modifiers.h"
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#endif
extern cvar_t gladder_autoswitch_on_pickup;

// Static world decal (infodecal) descriptor tracked across wave transitions (SPEC §11.3, Issue #49)
struct GladderStaticDecal
{
	Vector origin;
	int decalIndex;
	int entityIndex;
	int modelIndex;
};

class CGladderRules : public CHalfLifeRules
{
  public:
	CGladderRules();
	virtual ~CGladderRules();

	// Core tick
	void Think( void ) override;

	// SP vs MP status (dedicated single-player gauntlet)
	BOOL IsMultiplayer( void ) override { return FALSE; }
	BOOL IsDeathmatch( void ) override { return FALSE; }
	BOOL IsCoOp( void ) override { return FALSE; }
	const char *GetGameDescription( void ) override { return "Half-Life: Gladder"; }

	// Save/Load elimination & death flow
	BOOL FAllowAutoSave( void ) override { return FALSE; }
	BOOL FAllowSave( void ) override { return FALSE; }
	BOOL FAllowRestore( void ) override { return FALSE; }
	void OnSaveDenied( void ) override;
	void OnRestoreDenied( void ) override;
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override { return FALSE; }
	void PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse ) override;
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;

	// Player lifecycle & initial loadout (Point A)
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	void PlayerThink( CBasePlayer *pPlayer ) override;
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;
	void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;

	// Modernized monster turning speed hook (Issue #26)
	float FlMonsterYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed ) override;

	// Suppress monster weapon and supply drops to preserve procedural item weights (SPEC §3, Issue #44)
	BOOL FCanMonsterDropItem( CBaseMonster *pMonster, const char *pszItemName ) override;

	// User-configurable autoswitch prevention on weapon pickup (SPEC §7.5, Issue #45)
	BOOL FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override;

	// Wall chargers capacity degradation & recharge
	float FlHealthChargerCapacity( void ) override;
	float FlHEVChargerCapacity( void ) override;

	// Intercept and cache mapper-authored static world decals (infodecal) at map initialization (SPEC §11.3, Issue #49)
	void OnStaticDecal( const Vector &origin, int decalIndex, int entityIndex, int modelIndex ) override;

	// Static decal tracking and audit helpers (SPEC §11.3, Issue #49)
	const std::vector<GladderStaticDecal> &GetStaticDecals( void ) const { return m_staticDecals; }
	void ClearStaticDecals( void ) { m_staticDecals.clear(); }
	void RestoreStaticDecals( void );

	// Wave lifecycle callbacks invoked by triggers
	void OnWaveTriggerStart( CBaseEntity *pActivator );
	void OnWaveTriggerFinish( CBaseEntity *pActivator );

	// Resource cleanup, wave reset & recharge
	void ResetWave( void );
	void PurgeWaveEntities( void );
	void RechargeWallStations( void );
	void ResetBreakableEntities( void );

	// Scoring & match conclusion (Issue #9)
	void CalculateFinalScore( bool bSurvived );

	// Network telemetry broadcast
	void BroadcastWaveUpdate( CBasePlayer *pPlayer = nullptr );
	void BroadcastTelemetryUpdate( CBasePlayer *pPlayer = nullptr );

	// Accessors
	CGladderWaveManager &GetWaveManager( void ) { return m_waveManager; }
	GladderGridIndexer &GetGridIndexer( void ) { return m_gridIndexer; }
	GladderSpawner &GetSpawner( void ) { return m_spawner; }
	GladderMapConfig &GetMapConfig( void ) { return m_mapConfig; }
	GladderComboTracker &GetComboTracker( void ) { return m_comboTracker; }
	GladderModifiers &GetModifiers( void ) { return m_modifiers; }
	const GladderScoreBreakdown &GetLastScoreBreakdown( void ) const { return m_lastScoreBreakdown; }
	float GetTotalDamageTaken( void ) const { return m_flTotalDamageTaken; }
	int GetTotalFrags( void ) const { return m_iTotalFrags; }
	int GetCollectiblesCount( void ) const { return m_iCollectiblesCount; }
	void IncrementCollectibles( void ) { m_iCollectiblesCount++; }
	bool IsRestoreAttempted( void ) const { return m_bRestoreAttempted; }
	bool IsPendingSavePurge( void ) const { return m_bPendingSavePurge; }

  private:
	CGladderWaveManager m_waveManager;
	GladderGridIndexer m_gridIndexer;
	GladderSpawner m_spawner;
	GladderMapConfig m_mapConfig;
	GladderComboTracker m_comboTracker;
	GladderModifiers m_modifiers;
	GladderScoreBreakdown m_lastScoreBreakdown;
	std::vector<GladderStaticDecal> m_staticDecals;
	bool m_bInitialSpawnDone;
	bool m_bRestoreAttempted;
	bool m_bPendingSavePurge;
	int m_iTotalFrags;
	int m_iCollectiblesCount;
	float m_flTotalDamageTaken;
	float m_flLastPlayerHealth;
	float m_flLastPlayerArmor;
	float m_flLastTelemetryBroadcast;
};

// Purges any emitted .sav files from disk to maintain clean state
inline void Gladder_PurgeSaveFiles( void )
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

// Factory instantiation and condition
CGameRules *CreateGladderRules( void );
bool ConditionGladder( void );

// Neutralizes client console save/load commands and keybinds via client-side aliases
inline void Gladder_InstallClientSaveAliases( edict_t *pPlayerEdict )
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

#endif // GLADDER_RULES_H


