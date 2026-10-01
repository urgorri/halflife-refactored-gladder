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
	static constexpr int SAFE_FREE_EDICT_THRESHOLD = 64;

	GladderSpawner();
	~GladderSpawner();

	// Precache all monster models, sounds and weapon/pickup assets during map load
	void Precache( void );

	// Spawn full procedural wave across indexed spatial grid cells (SPEC §2, §3, §7.3)
	int SpawnWave( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, const std::string &szSwarmSpecies = "" );

	// Comprehensive GoldSrc edict garbage collection (SPEC §2, §11)
	void PurgeWaveEntities();

	// Tier probability calculation (SPEC §3)
	static float GetMonsterTierWeight( int tier, int iWaveNumber );
	static float GetWeaponTierWeight( int tier, int iWaveNumber );

	// Edict budget metrics (SPEC §11)
	static int GetActiveEdictCount();
	static int GetFreeEdictCount();

	// Query spawned entities
	size_t GetTrackedEntityCount() const { return m_spawnedEntities.size(); }
	int GetLastSpawnedMonsterCount() const { return m_iLastMonsterCount; }
	int GetLastSpawnedPickupCount() const { return m_iLastPickupCount; }
	int GetLastSpawnedChampionCount() const { return m_iLastChampionCount; }
	bool WasLambdaSpawned() const { return m_bLambdaSpawned; }

	// Manual entity creation helper that respects edict budget and registers with spawner
	CBaseEntity *CreateWaveEntity( const char *szClassname, const Vector &vecOrigin, const Vector &vecAngles = g_vecZero );

	// Entity properties query
	static bool IsLargeMonster( const char *szClassname );

  private:
	std::vector<EHANDLE> m_spawnedEntities;
	int m_iLastMonsterCount;
	int m_iLastPickupCount;
	int m_iLastChampionCount;
	bool m_bLambdaSpawned;

	// Internal procedural generation routines (with cell occupancy tracking)
	void SpawnMonsters( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count, std::vector<bool> *pOccupiedCells = nullptr, const std::string &szSwarmSpecies = "" );
	void SpawnPickups( int iWaveNumber, const GladderGridIndexer &indexer, const GladderMapConfig &config, int count, std::vector<bool> *pOccupiedCells = nullptr );
	void SpawnLambdaCollectible( const GladderGridIndexer &indexer, std::vector<bool> *pOccupiedCells = nullptr );

	const char *RollMonsterSpecies( int iWaveNumber, const GladderMapConfig &config, bool &outIsFlying, bool &outIsBarnacle );
	const char *RollPickupItem( int iWaveNumber, const GladderMapConfig &config );
};

#endif // GLADDER_SPAWNER_H
