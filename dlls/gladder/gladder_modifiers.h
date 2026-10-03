/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Skill Progression, Elite Champions, Mutators & Diminishing Chargers
 *	(SPEC §3, GitHub Issue #10)
 *
 ****/

#pragma once

#ifndef GLADDER_MODIFIERS_H
#define GLADDER_MODIFIERS_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/basemonster.h"
#include <string>

enum GladderMutatorType
{
	GLADDER_MUTATOR_NONE = 0,
	GLADDER_MUTATOR_BLACKOUT,    // Pitch darkness, lights out (LIGHT_STYLE 0 -> "a")
	GLADDER_MUTATOR_LOW_GRAVITY, // sv_gravity -> 200.0f
	GLADDER_MUTATOR_SWARM        // 100% single species horde
};

class GladderModifiers
{
  public:
	// Champion stats scaling constants
	static constexpr float CHAMPION_HEALTH_MULTIPLIER = 2.5f;
	static constexpr float CHAMPION_FRAMERATE_SCALE   = 1.25f;
	static constexpr float CHAMPION_GLOW_RENDERAMT   = 16.0f;

	// Minimum capacity floors for wall stations
	static constexpr float MIN_HEALTH_CHARGER_CAPACITY = 20.0f;
	static constexpr float MIN_HEV_CHARGER_CAPACITY    = 25.0f;

	GladderModifiers();
	~GladderModifiers();

	// Reset all modifiers to default (call on match start or finish)
	void Reset( void );

	// --- 1. Dynamic Engine Skill Progression ---
	static int CalculateSkillTier( int iWaveNumber );
	static void ApplySkillProgression( int iWaveNumber );

	// --- 2. Elite Champion Monsters ---
	static float GetChampionSpawnChance( int iWaveNumber );
	static bool ShouldSpawnAsChampion( int iWaveNumber );
	static void MakeEliteChampion( CBaseMonster *pMonster );
	static bool IsEliteChampion( const CBaseMonster *pMonster );

	// --- 3. Diminishing Wall Chargers ---
	static float CalculateHealthChargerCapacity( int iWaveNumber, float flBaseCapacity = 50.0f );
	static float CalculateHEVChargerCapacity( int iWaveNumber, float flBaseCapacity = 75.0f );
	static void RechargeWallStations( void );
	static void UpdateWallStations( void );

	// --- 4. Special Wave Mutators ---
	GladderMutatorType GetActiveMutator( void ) const { return m_activeMutator; }
	const std::string &GetSwarmSpecies( void ) const { return m_szSwarmSpecies; }
	const char *GetMutatorName( void ) const;

	// Evaluate and activate mutators for an incoming wave
	GladderMutatorType RollAndApplyWaveMutator( int iWaveNumber );

	// Explicitly apply a mutator
	void ApplyMutator( GladderMutatorType mutator, int iWaveNumber );

	// Revert all environment modifiers (lighting, gravity) back to standard defaults
	void RevertMutators( void );

  private:
	GladderMutatorType m_activeMutator;
	std::string m_szSwarmSpecies;
	bool m_bMutatorActive;
};

#endif // GLADDER_MODIFIERS_H
