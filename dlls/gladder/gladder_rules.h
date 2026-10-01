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
	BOOL FPlayerCanRespawn( CBasePlayer *pPlayer ) override { return FALSE; }
	void PlayerRespawn( CBasePlayer *pPlayer, BOOL fCopyCorpse ) override;
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;

	// Player lifecycle & initial loadout (Point A)
	void PlayerSpawn( CBasePlayer *pPlayer ) override;
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;
	void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller, entvars_t *pInflictor ) override;

	// Wall chargers capacity degradation & recharge
	float FlHealthChargerCapacity( void ) override;
	float FlHEVChargerCapacity( void ) override;

	// Wave lifecycle callbacks invoked by triggers
	void OnWaveTriggerStart( CBaseEntity *pActivator );
	void OnWaveTriggerFinish( CBaseEntity *pActivator );

	// Resource cleanup, wave reset & recharge
	void ResetWave( void );
	void PurgeWaveEntities( void );
	void RechargeWallStations( void );
	void ResetBreakableEntities( void );

	// Network telemetry broadcast
	void BroadcastWaveUpdate( CBasePlayer *pPlayer = nullptr );
	void BroadcastTelemetryUpdate( CBasePlayer *pPlayer = nullptr );

	// Accessors
	CGladderWaveManager &GetWaveManager( void ) { return m_waveManager; }
	GladderGridIndexer &GetGridIndexer( void ) { return m_gridIndexer; }
	GladderSpawner &GetSpawner( void ) { return m_spawner; }
	GladderMapConfig &GetMapConfig( void ) { return m_mapConfig; }
	int GetTotalFrags( void ) const { return m_iTotalFrags; }
	int GetCollectiblesCount( void ) const { return m_iCollectiblesCount; }
	void IncrementCollectibles( void ) { m_iCollectiblesCount++; }

  private:
	CGladderWaveManager m_waveManager;
	GladderGridIndexer m_gridIndexer;
	GladderSpawner m_spawner;
	GladderMapConfig m_mapConfig;
	bool m_bInitialSpawnDone;
	int m_iTotalFrags;
	int m_iCollectiblesCount;
	float m_flLastTelemetryBroadcast;
};

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


