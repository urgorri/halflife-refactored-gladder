/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Procedural Spawner, Dynamic Weighted Tier Distribution & Edict Garbage Collection
 *	(SPEC §2, §3, §4.4, §5, §7.3, §11, GitHub Issue #7)
 *
 ****/

#pragma once

#ifndef GLADDER_SPAWNER_H
#define GLADDER_SPAWNER_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "gladder_grid_indexer.h"
#include "gladder_modifiers.h"
#include <vector>
#include <string>
#include <cstdint>

// Monster species definition & tier attribution
struct GladderMonsterDef
{
	const char *szClassname;
	int tier;             // Tier 1 to 4
	float relativeWeight; // Relative weight within tier
	bool isFlying;        // Airborne monster (variable altitude spawning)
	bool isBarnacle;      // Ceiling-attached monster
};

// Pickup & supply definition
struct GladderPickupDef
{
	const char *szClassname;
	int tier;             // Tier 1 to 3
	float relativeWeight; // Relative weight within category
	bool isWeapon;
	bool isAmmo;
	bool isHealthArmor;
	bool isConsumableWeapon;
};

// Map-specific configuration profile (SPEC §5)
class GladderMapConfig
{
  public:
	std::string mapName;
	std::vector<std::string> monsterWhitelist;
	std::vector<std::string> monsterBlacklist;
	std::vector<std::string> weaponWhitelist;
	std::vector<std::string> weaponBlacklist;

	int baseMonsters;
	float monstersPerWave;
	int maxMonsters;

	int basePickups;
	float pickupsPerWave;
	int maxPickups;

	bool allowBarnacles;
	bool allowFlying;

	float routeBiasFraction;   // Fraction of spawns biased to the Point A -> Point B corridor (default 0.60f) (SPEC §3, Issue #103)
	float routeCorridorRadius; // Maximum distance to route axis to be considered in-corridor (default 512.0f)

	GladderMapConfig();

	// Load configuration profile from "gladder/maps/<mapname>.cfg"
	bool LoadForMap( const char *pszMapName );
	bool LoadFromFile( const char *pszFilePath );

	bool IsMonsterAllowed( const char *szClassname ) const;
	bool IsWeaponAllowed( const char *szClassname ) const;
};

class GladderSpawner
{
  public:
	// Threshold of minimum free edicts required before safety clamp halts spawning
	static constexpr int SAFE_FREE_EDICT_THRESHOLD = 24;

	GladderSpawner();
	~GladderSpawner();

	// Precache all monster models, sounds and weapon/pickup assets during map load
	void Precache( void );

	// Spawn full procedural wave across indexed spatial grid cells (SPEC §2, §3, §7.3)
	int SpawnWave( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, const std::string &szSwarmSpecies = "", CBasePlayer *pPlayer = nullptr, GladderMutatorType mutator = GLADDER_MUTATOR_NONE );

	// Comprehensive GoldSrc edict garbage collection (SPEC §2, §11)
	void PurgeWaveEntities();

	// Tier probability calculation (SPEC §3)
	static float GetMonsterTierWeight( int tier, int iWaveNumber );
	static float GetWeaponTierWeight( int tier, int iWaveNumber );
	static float GetOrdnanceTierWeight( int tier, int iWaveNumber );

	// Edict budget metrics (SPEC §11)
	static int GetActiveEdictCount();
	static int GetFreeEdictCount();

	// Query spawned entities
	size_t GetTrackedEntityCount() const { return m_spawnedEntities.size(); }
	CBaseEntity *GetTrackedEntity( size_t index )
	{
		return ( index < m_spawnedEntities.size() ) ? (CBaseEntity *)m_spawnedEntities[index] : nullptr;
	}
	int GetLastSpawnedMonsterCount() const { return m_iLastMonsterCount; }
	int GetLastSpawnedPickupCount() const { return m_iLastPickupCount; }
	int GetLastSpawnedChampionCount() const { return m_iLastChampionCount; }
	bool WasLambdaSpawned() const { return m_bLambdaSpawned; }

	// Manual entity creation helper that respects edict budget and registers with spawner
	CBaseEntity *CreateWaveEntity( const char *szClassname, const Vector &vecOrigin, const Vector &vecAngles = g_vecZero );

	// Entity properties query
	static bool IsLargeMonster( const char *szClassname );

	// Roster query (SPEC §3, §7.1, §7.12)
	static const GladderPickupDef *GetPickupRoster( size_t &outCount );

	// Procedural rolling helpers (SPEC §3, Issue #39, #78, #79, #93)
	static const char *RollMonsterSpecies( int iWaveNumber, const GladderMapConfig &config, bool &outIsFlying, bool &outIsBarnacle );
	static const char *RollPickupItem( int iWaveNumber, const GladderMapConfig &config, CBasePlayer *pPlayer = nullptr, bool allowFixedWeapons = true, const std::vector<std::string> *pExtraOwnedWeapons = nullptr );

	// Pity tracking helpers (SPEC §3, Issue #93)
	static void ResetPickupPity( void );
	static void UpdateAmmoPityTracking( int iWaveNumber );
	static void RecordAmmoSpawned( const char *szPickup );

	// Internal procedural generation routines (with cell occupancy tracking)
	void SpawnMonsters( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count, std::vector<bool> *pOccupiedCells = nullptr, const std::string &szSwarmSpecies = "", GladderMutatorType mutator = GLADDER_MUTATOR_NONE );
	void SpawnPickups( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count, std::vector<bool> *pOccupiedCells = nullptr, CBasePlayer *pPlayer = nullptr );
	void SpawnLambdaCollectible( const GladderGridIndexer &indexer, std::vector<bool> *pOccupiedCells = nullptr );

	// Route detection and simulation helpers (SPEC §3, Issue #103)
	void SetSimulatedRoute( const Vector &vecStart, const Vector &vecFinish )
	{
		m_vecSimulatedStart  = vecStart;
		m_vecSimulatedFinish = vecFinish;
		m_bHasSimulatedRoute = true;
	}
	void ClearSimulatedRoute( void )
	{
		m_bHasSimulatedRoute = false;
	}
	bool HasSimulatedRoute( void ) const { return m_bHasSimulatedRoute; }
	bool GetActiveRoute( Vector &outStart, Vector &outFinish ) const;

  private:
	std::vector<EHANDLE> m_spawnedEntities;
	std::vector<Vector> m_spawnedMonsterPositions;
	int m_iLastMonsterCount;
	int m_iLastPickupCount;
	int m_iLastChampionCount;
	bool m_bLambdaSpawned;
	Vector m_vecSimulatedStart;
	Vector m_vecSimulatedFinish;
	bool m_bHasSimulatedRoute;
};

#endif // GLADDER_SPAWNER_H
