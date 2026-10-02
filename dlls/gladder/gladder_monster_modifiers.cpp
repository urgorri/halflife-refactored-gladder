/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Modernized Monster Turning Speed (yawspeed) & Champion Agility Modifier Implementation
 *	(SPEC §3, GitHub Issue #26)
 *
 ****/

#include "gladder_monster_modifiers.h"
#include <algorithm>

bool GladderMonsterModifiers::IsExemptSpecies( const std::string &szClassname )
{
	// Stationary monsters attached to ceilings or structures
	if ( szClassname == "monster_barnacle" || szClassname == "monster_tentacle" )
	{
		return true;
	}

	// Flying monsters use 3D directional flight velocity rather than planar yaw navigation
	if ( szClassname == "monster_flyer" || szClassname == "monster_alien_controller" )
	{
		return true;
	}

	return false;
}

float GladderMonsterModifiers::GetSpeciesYawSpeed( const std::string &szClassname, float flDefaultYawSpeed )
{
	if ( IsExemptSpecies( szClassname ) )
	{
		return flDefaultYawSpeed;
	}

	// Hand-tuned arcade turn rates for core gauntlet roster (approx. 2.5x to 3x vanilla values)
	if ( szClassname == "monster_headcrab" || szClassname == "monster_babycrab" )
	{
		// Vanilla 20-30 deg/s -> Snappy 80 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 80.0f );
	}
	if ( szClassname == "monster_zombie" )
	{
		// Vanilla 20-30 deg/s -> Snappy 75 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 75.0f );
	}
	if ( szClassname == "monster_houndeye" )
	{
		// Vanilla 30-60 deg/s -> Fast flanker 120 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 120.0f );
	}
	if ( szClassname == "monster_bullchicken" )
	{
		// Vanilla 30-40 deg/s -> Snappy 100 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 100.0f );
	}
	if ( szClassname == "monster_alien_slave" || szClassname == "monster_vortigaunt" )
	{
		// Vanilla 30-40 deg/s -> Snappy 110 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 110.0f );
	}
	if ( szClassname == "monster_alien_grunt" )
	{
		// Vanilla 30-40 deg/s -> Aggressive 95 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 95.0f );
	}
	if ( szClassname == "monster_human_grunt" )
	{
		// Vanilla 30-45 deg/s -> Tactical tracker 115 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 115.0f );
	}
	if ( szClassname == "monster_human_assassin" )
	{
		// Vanilla 60-90 deg/s -> Hyper-agile 200 deg/s
		return (std::max)( flDefaultYawSpeed * 2.5f, 200.0f );
	}

	// Default fallback: 2.5x multiplier on vanilla turning rate
	return flDefaultYawSpeed * DEFAULT_ARCADE_YAW_MULTIPLIER;
}

float GladderMonsterModifiers::GetModernYawSpeed( CBaseMonster *pMonster, float flDefaultYawSpeed )
{
	if ( !pMonster || !pMonster->pev )
		return flDefaultYawSpeed;

	std::string szClassname;
	if ( pMonster->pev->classname )
	{
		const char *psz = STRING( pMonster->pev->classname );
		if ( psz && *psz )
		{
			szClassname = psz;
		}
	}

	float speed = GetSpeciesYawSpeed( szClassname, flDefaultYawSpeed );

	// Check if this monster is an Elite Champion (tagged with red glow shell)
	if ( pMonster->pev->renderfx == kRenderFxGlowShell &&
	     pMonster->pev->rendercolor.x >= 200.0f &&
	     pMonster->pev->rendercolor.y <= 64.0f )
	{
		speed *= CHAMPION_YAW_MULTIPLIER;
	}

	return speed;
}
