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

// High capacity ceiling tending to infinity for ammunition and ordnance (SPEC §3, §7.14, Issue #97)
#define GLADDER_EXTENDED_AMMO_MAX 9999

inline bool GladderIsExtendedAmmo( const char *pszAmmoName )
{
	if ( !pszAmmoName )
		return false;

	return ( !stricmp( pszAmmoName, "9mm" ) ||
	         !stricmp( pszAmmoName, "357" ) ||
	         !stricmp( pszAmmoName, "buckshot" ) ||
	         !stricmp( pszAmmoName, "bolts" ) ||
	         !stricmp( pszAmmoName, "rockets" ) ||
	         !stricmp( pszAmmoName, "uranium" ) ||
	         !stricmp( pszAmmoName, "ARgrenades" ) ||
	         !stricmp( pszAmmoName, "762" ) ||
	         !stricmp( pszAmmoName, "556" ) ||
	         !stricmp( pszAmmoName, "Hand Grenade" ) ||
	         !stricmp( pszAmmoName, "Satchel Charge" ) ||
	         !stricmp( pszAmmoName, "Trip Mine" ) ||
	         !stricmp( pszAmmoName, "Snarks" ) );
}

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
	BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker ) override { return m_bMatchConcluded ? FALSE : TRUE; }
	BOOL FAllowFlashlight( void ) override { return m_bMatchConcluded ? FALSE : TRUE; }

	// Player lifecycle & initial loadout (Point A)
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	void PlayerThink( CBasePlayer *pPlayer ) override;
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;
	void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;

	// Skill data & monster combat behavior hooks (SPEC §3, Issue #26, #116, #121, #123)
	void RefreshSkillData( void ) override;
	void OnEntitySpawned( CBaseEntity *pEntity ) override;
	float FlMonsterYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed ) override;
	float FlMonsterCastSpeed( CBaseMonster *pMonster, Activity activity, float flDefaultSpeed ) override;
	float FlMonsterMoveSpeed( CBaseMonster *pMonster, Activity activity, float flDefaultSpeed ) override;
	float FlMonsterProjectileSpread( CBaseMonster *pMonster, const char *pszProjectile, float flDefaultSpread ) override;
	float FlHornetTrackAgility( CBaseEntity *pHornet, float flDefaultWeight ) override;
	float FlMonsterWaitTime( CBaseMonster *pMonster, float flDefaultWait ) override;
	float FlMonsterGrenadeCheckDelay( CBaseMonster *pMonster, float flDefaultDelay ) override;
	int IMonsterProjectileBurstCount( CBaseMonster *pMonster, const char *pszProjectile, int iDefaultCount ) override;
	void OnMonsterAttackDetonation( CBaseMonster *pMonster, const char *pszAttackType, const Vector &vecOrigin ) override;
	void OnMonsterProjectileImpact( CBaseEntity *pProjectile, const TraceResult &tr ) override;

	// Player movement slow debuff management (SPEC §3, Issue #116)
	void ApplyPlayerSlowDebuff( CBasePlayer *pPlayer, float flDuration = 1.75f, float flMaxSpeed = 175.0f )
	{
		if ( !pPlayer || !pPlayer->pev )
			return;

		pPlayer->pev->maxspeed = flMaxSpeed;
		pPlayer->pev->fuser4 = ( gpGlobals ? gpGlobals->time : 0.0f ) + flDuration;
	}

	float GetPlayerSlowRemainingTime( CBasePlayer *pPlayer ) const
	{
		if ( !pPlayer || !pPlayer->pev || pPlayer->pev->fuser4 <= 0.0f )
			return 0.0f;

		float flCurrentTime = gpGlobals ? gpGlobals->time : 0.0f;
		return ( pPlayer->pev->fuser4 > flCurrentTime ) ? ( pPlayer->pev->fuser4 - flCurrentTime ) : 0.0f;
	}

	// Suppress monster weapon and supply drops to preserve procedural item weights (SPEC §3, Issue #44)
	BOOL FCanMonsterDropItem( CBaseMonster *pMonster, const char *pszItemName ) override;

	// User-configurable autoswitch prevention on weapon pickup (SPEC §7.5, Issue #45)
	BOOL FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon ) override;

	// Extended ammo and explosive ordnance carrying capacity tending to infinity (SPEC §3, §7.14, Issue #97)
	int GetMaxAmmo( const char *pszAmmoName, int iDefaultMaxCarry ) override;

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
	void ResetStartTriggers( void );
	void ResetFinishTriggers( void );
	void EnsureMonsterClipFlags( void );
	void EnsureDeadMonsterProps( void );

	// Scoring & match conclusion (SPEC §2, §3, §9.2, Issues #9, #85)
	void HandleMatchConclusion( bool bSurvived, CBasePlayer *pPlayer = nullptr );
	void FreezeMatchWorld( void );
	void CalculateFinalScore( bool bSurvived );
	void ShiftPlayerCamera( CBaseEntity *pPlr );

	// Multi-camera backdrop rotation during summary screen (SPEC §6, §9.2, Issue #52, #72)
	CBaseEntity *GetOrCreateDynamicCamera( CBaseEntity *pPlr );
	void CollectSummaryCameras( CBaseEntity *pPlr = nullptr );
	void ActivateSummaryCamera( CBaseEntity *pCamera, CBaseEntity *pPlr );
	void AdvanceSummaryCamera( void );
	const std::vector<EHANDLE> &GetSummaryCameras( void ) const { return m_summaryCameras; }

	// Network telemetry broadcast
	void BroadcastWaveUpdate( CBasePlayer *pPlayer = nullptr );
	void BroadcastTelemetryUpdate( CBasePlayer *pPlayer = nullptr );
	void BroadcastComboUpdate( CBasePlayer *pPlayer = nullptr );
	void BroadcastWaveNotify( CBasePlayer *pPlayer, int iType, int iWaveNumber, int iMutatorType, int iHostileQuota = 0 );
	void BroadcastWaveClear( CBasePlayer *pPlayer, int iWaveNumber );
	void BroadcastSummaryUpdate( CBasePlayer *pPlayer, const GladderScoreBreakdown &score, const GladderKillStats &kills );

	// Accessors
	CGladderWaveManager &GetWaveManager( void ) { return m_waveManager; }
	// Arcade monster damage reactivity & path interruption (SPEC §3, Issue #64)
	int FlMonsterScheduleInterruptMask( CBaseMonster *pMonster, Schedule_t *pSchedule, int iDefaultMask ) override;

	// Melee combat cadence normalization & corpse impact decals (SPEC §7.6, Issue #65)
	BOOL FFixMeleeCorpseAttackDelay( void ) override { return TRUE; }

	// Looping weapon audio termination on player death (SPEC §7.7, Issue #66)
	BOOL FSilenceLoopingWeaponSoundsOnDeath( void ) override { return TRUE; }

	// Shotgun interrupted reload & pump state synchronization (SPEC §7.8, Issue #67)
	BOOL FFixShotgunReloadDesync( void ) override { return TRUE; }

	// Submerged weapon firing debounce normalization (SPEC §7.9, Issue #68)
	BOOL FFixMP5UnderwaterDebounce( void ) override { return TRUE; }

	GladderGridIndexer &GetGridIndexer( void ) { return m_gridIndexer; }
	bool IsGridIndexAttempted( void ) const { return m_bGridIndexAttempted; }
	void SetGridIndexAttempted( bool bAttempted ) { m_bGridIndexAttempted = bAttempted; }
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
	const Vector &GetDeathAngles( void ) const { return m_vecDeathAngles; }
	bool IsPlayerDead( void ) const { return m_bPlayerDead; }
	bool IsMatchConcluded( void ) const { return m_bMatchConcluded; }
	bool IsMatchFrozen( void ) const { return m_bMatchFrozen; }
	float GetMatchFreezeTime( void ) const { return m_flMatchFreezeTime; }
	bool IsPlayerFrozen( void ) const { return m_bPlayerFrozen; }
	int GetFrozenPlayerSequence( void ) const { return m_iFrozenPlayerSequence; }
	float GetFrozenPlayerFrame( void ) const { return m_flFrozenPlayerFrame; }
	size_t GetSummaryCameraCount( void ) const { return m_summaryCameras.size(); }
	size_t GetCurrentSummaryCameraIndex( void ) const { return m_iCurrentSummaryCameraIndex; }

	// Wave hostile tracking & finish gate enforcement (SPEC §2, §9.1, Issue #118)
	int GetWaveMonstersTotal( void ) const { return m_iWaveMonstersTotal; }
	int GetWaveMonstersKilled( void ) const { return m_iWaveMonstersKilled; }
	int GetWaveMonstersRemaining( void ) const { return ( m_iWaveMonstersTotal > m_iWaveMonstersKilled ) ? ( m_iWaveMonstersTotal - m_iWaveMonstersKilled ) : 0; }
	bool AreWaveHostilesEliminated( void ) const;
	void SetWaveMonstersTotal( int count ) { m_iWaveMonstersTotal = count; }
	void SetWaveMonstersKilled( int count ) { m_iWaveMonstersKilled = count; }
	void CheckAllHostilesEliminated( CBaseEntity *pActivator = nullptr );
	int CountLivingWaveHostiles( void ) const;

  private:
	CGladderWaveManager m_waveManager;
	GladderGridIndexer m_gridIndexer;
	bool m_bGridIndexAttempted;
	GladderSpawner m_spawner;
	GladderMapConfig m_mapConfig;
	GladderComboTracker m_comboTracker;
	GladderModifiers m_modifiers;
	GladderScoreBreakdown m_lastScoreBreakdown;
	std::vector<GladderStaticDecal> m_staticDecals;
	std::vector<EHANDLE> m_summaryCameras;
	size_t m_iCurrentSummaryCameraIndex;
	float m_flNextCameraSwitchTime;
	Vector m_vecDeathAngles;
	bool m_bPlayerDead;
	bool m_bPlayerFrozen;
	int m_iFrozenPlayerSequence;
	float m_flFrozenPlayerFrame;
	int m_iFrozenPlayerGaitSequence;
	bool m_bMatchConcluded;
	bool m_bMatchFrozen;
	float m_flMatchFreezeTime;
	bool m_bInitialSpawnDone;
	bool m_bRestoreAttempted;
	bool m_bPendingSavePurge;
	int m_iTotalFrags;
	int m_iCollectiblesCount;
	float m_flTotalDamageTaken;
	float m_flLastPlayerHealth;
	float m_flLastPlayerArmor;
	float m_flLastTelemetryBroadcast;
	int m_iWaveMonstersTotal;
	int m_iWaveMonstersKilled;
	bool m_bHostilesEliminatedFired;
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


