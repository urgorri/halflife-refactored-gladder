/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Skill Progression, Elite Champions, Mutators & Diminishing Chargers Implementation
 *	(SPEC §3, GitHub Issue #10)
 *
 ****/

#include "gladder_modifiers.h"
#include "systems/chargers.h"
#include "core/skill_manager.h"
#include <algorithm>

GladderModifiers::GladderModifiers()
    : m_activeMutator( GLADDER_MUTATOR_NONE ),
      m_bMutatorActive( false )
{
}

GladderModifiers::~GladderModifiers()
{
	RevertMutators();
}

void GladderModifiers::Reset( void )
{
	RevertMutators();
	m_activeMutator  = GLADDER_MUTATOR_NONE;
	m_szSwarmSpecies.clear();
	m_bMutatorActive = false;
}

int GladderModifiers::CalculateSkillTier( int iWaveNumber )
{
	if ( iWaveNumber >= 10 )
		return SKILL_HARD;
	if ( iWaveNumber >= 5 )
		return SKILL_MEDIUM;
	return SKILL_EASY;
}

void GladderModifiers::ApplySkillProgression( int iWaveNumber )
{
	int newSkill = CalculateSkillTier( iWaveNumber );
	if ( SkillManager::GetSkillLevel() != newSkill )
	{
		SkillManager::SetSkillLevel( newSkill );
		if ( g_pGameRules )
		{
			g_pGameRules->RefreshSkillData();
		}
	}
}

float GladderModifiers::GetChampionSpawnChance( int iWaveNumber )
{
	if ( iWaveNumber <= 2 )
		return 0.0f;
	if ( iWaveNumber <= 4 )
		return 0.10f;
	if ( iWaveNumber <= 8 )
		return 0.20f;
	if ( iWaveNumber <= 12 )
		return 0.30f;

	return 0.35f; // Cap at 35% chance
}

bool GladderModifiers::ShouldSpawnAsChampion( int iWaveNumber )
{
	float chance = GetChampionSpawnChance( iWaveNumber );
	if ( chance <= 0.0f )
		return false;

	return ( RANDOM_FLOAT( 0.0f, 1.0f ) < chance );
}

void GladderModifiers::MakeEliteChampion( CBaseMonster *pMonster )
{
	if ( !pMonster || !pMonster->pev )
		return;

	// Glowing red shell visual cue
	pMonster->pev->renderfx    = kRenderFxGlowShell;
	pMonster->pev->rendercolor = Vector( 255, 32, 32 );
	pMonster->pev->renderamt   = CHAMPION_GLOW_RENDERAMT;

	// Enhanced health pool
	pMonster->pev->health     *= CHAMPION_HEALTH_MULTIPLIER;
	pMonster->pev->max_health  = pMonster->pev->health;

	// Heightened animation and attack cadence
	pMonster->pev->framerate   = CHAMPION_FRAMERATE_SCALE;
}

bool GladderModifiers::IsEliteChampion( const CBaseMonster *pMonster )
{
	if ( !pMonster || !pMonster->pev )
		return false;

	return ( pMonster->pev->renderfx == kRenderFxGlowShell &&
	         pMonster->pev->rendercolor.x >= 200.0f &&
	         pMonster->pev->rendercolor.y <= 64.0f );
}

float GladderModifiers::CalculateHealthChargerCapacity( int iWaveNumber, float flBaseCapacity )
{
	// Capacity degrades by 10 points per wave down to safe floor
	float degraded = flBaseCapacity - static_cast<float>( ( iWaveNumber - 1 ) * 10 );
	return (std::max)( MIN_HEALTH_CHARGER_CAPACITY, degraded );
}

float GladderModifiers::CalculateHEVChargerCapacity( int iWaveNumber, float flBaseCapacity )
{
	// Capacity degrades by 15 points per wave down to safe floor
	float degraded = flBaseCapacity - static_cast<float>( ( iWaveNumber - 1 ) * 15 );
	return (std::max)( MIN_HEV_CHARGER_CAPACITY, degraded );
}

void GladderModifiers::RechargeWallStations( void )
{
	// Re-energize all wall health chargers
	CBaseEntity *pCharger = nullptr;
	while ( ( pCharger = UTIL_FindEntityByClassname( pCharger, "func_healthcharger" ) ) != nullptr )
	{
		CBaseWallCharger *pWall = dynamic_cast<CBaseWallCharger *>( pCharger );
		if ( pWall )
		{
			pWall->Recharge();
		}
		else
		{
			pCharger->pev->frame = 0;
		}

		// Vibrant Green glow shell for active health chargers (SPEC §3, §7.2, Issue #46)
		pCharger->pev->renderfx = kRenderFxGlowShell;
		pCharger->pev->rendercolor = Vector( 32, 255, 32 );
		pCharger->pev->renderamt = 16;
	}

	// Re-energize all wall HEV suit chargers
	pCharger = nullptr;
	while ( ( pCharger = UTIL_FindEntityByClassname( pCharger, "func_recharge" ) ) != nullptr )
	{
		CBaseWallCharger *pWall = dynamic_cast<CBaseWallCharger *>( pCharger );
		if ( pWall )
		{
			pWall->Recharge();
		}
		else
		{
			pCharger->pev->frame = 0;
		}

		// Vibrant Golden-Amber / Orange glow shell for active HEV suit chargers (SPEC §3, §7.2, Issue #46)
		pCharger->pev->renderfx = kRenderFxGlowShell;
		pCharger->pev->rendercolor = Vector( 255, 140, 20 );
		pCharger->pev->renderamt = 16;
	}
}

void GladderModifiers::UpdateWallStations( void )
{
	// Inspect active wall health chargers: extinguish glow if depleted
	CBaseEntity *pCharger = nullptr;
	while ( ( pCharger = UTIL_FindEntityByClassname( pCharger, "func_healthcharger" ) ) != nullptr )
	{
		if ( pCharger->pev->frame == 1 )
		{
			if ( pCharger->pev->renderfx != kRenderFxNone )
			{
				pCharger->pev->renderfx = kRenderFxNone;
				pCharger->pev->rendercolor = Vector( 0, 0, 0 );
				pCharger->pev->renderamt = 0;
			}
		}
		else if ( pCharger->pev->frame == 0 )
		{
			if ( pCharger->pev->renderfx != kRenderFxGlowShell )
			{
				pCharger->pev->renderfx = kRenderFxGlowShell;
				pCharger->pev->rendercolor = Vector( 32, 255, 32 );
				pCharger->pev->renderamt = 16;
			}
		}
	}

	// Inspect active wall HEV suit chargers: extinguish glow if depleted
	pCharger = nullptr;
	while ( ( pCharger = UTIL_FindEntityByClassname( pCharger, "func_recharge" ) ) != nullptr )
	{
		if ( pCharger->pev->frame == 1 )
		{
			if ( pCharger->pev->renderfx != kRenderFxNone )
			{
				pCharger->pev->renderfx = kRenderFxNone;
				pCharger->pev->rendercolor = Vector( 0, 0, 0 );
				pCharger->pev->renderamt = 0;
			}
		}
		else if ( pCharger->pev->frame == 0 )
		{
			if ( pCharger->pev->renderfx != kRenderFxGlowShell )
			{
				pCharger->pev->renderfx = kRenderFxGlowShell;
				pCharger->pev->rendercolor = Vector( 255, 140, 20 );
				pCharger->pev->renderamt = 16;
			}
		}
	}
}

const char *GladderModifiers::GetMutatorName( void ) const
{
	switch ( m_activeMutator )
	{
		case GLADDER_MUTATOR_BLACKOUT:
			return "BLACKOUT (PITCH DARK)";
		case GLADDER_MUTATOR_LOW_GRAVITY:
			return "LOW GRAVITY (XEN PHYSICS)";
		case GLADDER_MUTATOR_SWARM:
			return "SWARM HORDE";
		default:
			return "STANDARD";
	}
}

GladderMutatorType GladderModifiers::RollAndApplyWaveMutator( int iWaveNumber )
{
	// Always revert any prior wave mutator before evaluating incoming wave
	RevertMutators();

	// Mutators activate on every 5th wave milestone (Wave 5, 10, 15, 20, ...)
	if ( iWaveNumber <= 1 || ( iWaveNumber % 5 != 0 ) )
	{
		m_activeMutator = GLADDER_MUTATOR_NONE;
		return GLADDER_MUTATOR_NONE;
	}

	// Roll evenly between Blackout, Low Gravity, and Swarm
	int roll = RANDOM_LONG( 1, 3 );
	GladderMutatorType rolledMutator = static_cast<GladderMutatorType>( roll );

	ApplyMutator( rolledMutator, iWaveNumber );
	return rolledMutator;
}

void GladderModifiers::ApplyMutator( GladderMutatorType mutator, int iWaveNumber )
{
	m_activeMutator  = mutator;
	m_bMutatorActive = ( mutator != GLADDER_MUTATOR_NONE );

	switch ( mutator )
	{
		case GLADDER_MUTATOR_BLACKOUT:
		{
			// Extinguish world lights to pitch darkness (SPEC §3)
			LIGHT_STYLE( 0, "a" );
			ALERT( at_console, "[Gladder] Mutator ACTIVE: Blackout (Pitch Dark) at wave %d\n", iWaveNumber );
			break;
		}
		case GLADDER_MUTATOR_LOW_GRAVITY:
		{
			// Apply Xen-like low gravity to world physics
			CVAR_SET_FLOAT( "sv_gravity", 200.0f );
			ALERT( at_console, "[Gladder] Mutator ACTIVE: Low Gravity (sv_gravity = 200) at wave %d\n", iWaveNumber );
			break;
		}
		case GLADDER_MUTATOR_SWARM:
		{
			// Pick a random eligible ground species for the swarm
			static const char *s_swarmCandidates[] = {
				"monster_headcrab",
				"monster_zombie",
				"monster_houndeye",
				"monster_bullchicken",
				"monster_alien_slave",
				"monster_alien_grunt"
			};
			int idx = RANDOM_LONG( 0, 5 );
			m_szSwarmSpecies = s_swarmCandidates[idx];
			ALERT( at_console, "[Gladder] Mutator ACTIVE: Swarm Horde (%s) at wave %d\n",
			       m_szSwarmSpecies.c_str(), iWaveNumber );
			break;
		}
		default:
			break;
	}
}

void GladderModifiers::RevertMutators( void )
{
	if ( !m_bMutatorActive && m_activeMutator == GLADDER_MUTATOR_NONE )
		return;

	// 1. Restore default world illumination
	LIGHT_STYLE( 0, "m" );

	// 2. Restore standard GoldSrc gravity
	CVAR_SET_FLOAT( "sv_gravity", 800.0f );

	m_szSwarmSpecies.clear();
	m_activeMutator  = GLADDER_MUTATOR_NONE;
	m_bMutatorActive = false;
}
