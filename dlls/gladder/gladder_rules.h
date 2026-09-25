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

	// Resource cleanup & recharge
	void PurgeWaveEntities( void );
	void RechargeWallStations( void );

	// Network telemetry broadcast
	void BroadcastWaveUpdate( CBasePlayer *pPlayer = nullptr );
	void BroadcastTelemetryUpdate( CBasePlayer *pPlayer = nullptr );

	// Accessors
	CGladderWaveManager &GetWaveManager( void ) { return m_waveManager; }
	int GetTotalFrags( void ) const { return m_iTotalFrags; }
	int GetCollectiblesCount( void ) const { return m_iCollectiblesCount; }
	void IncrementCollectibles( void ) { m_iCollectiblesCount++; }

  private:
	CGladderWaveManager m_waveManager;
	bool m_bInitialSpawnDone;
	int m_iTotalFrags;
	int m_iCollectiblesCount;
	float m_flLastTelemetryBroadcast;
};

// Factory instantiation and condition
CGameRules *CreateGladderRules( void );
bool ConditionGladder( void );

#endif // GLADDER_RULES_H
