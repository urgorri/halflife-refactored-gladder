/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Modernized Monster Turning Speed (yawspeed) & Champion Agility Modifier Implementation
 *	(SPEC §3, GitHub Issue #26)
 *
 ****/

#include "gladder_monster_modifiers.h"
#include "ai/monsters.h"
#include "ai/schedule.h"
#include "ai/activity.h"
#include <algorithm>
#include <cstring>

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

bool GladderMonsterModifiers::IsMovementSchedule( Schedule_t *pSchedule, CBaseMonster *pMonster )
{
	if ( !pSchedule )
		return false;

	// Check if the schedule contains any movement or path navigation tasks
	if ( pSchedule->pTasklist && pSchedule->cTasks > 0 )
	{
		for ( int i = 0; i < pSchedule->cTasks; ++i )
		{
			int task = pSchedule->pTasklist[i].iTask;
			if ( task == TASK_RUN_PATH ||
			     task == TASK_WALK_PATH ||
			     task == TASK_STRAFE_PATH ||
			     task == TASK_WAIT_FOR_MOVEMENT ||
			     task == TASK_WALK_TO_TARGET ||
			     task == TASK_RUN_TO_TARGET ||
			     task == TASK_MOVE_TO_TARGET_RANGE ||
			     task == TASK_GET_PATH_TO_ENEMY ||
			     task == TASK_GET_PATH_TO_ENEMY_LKP ||
			     task == TASK_GET_PATH_TO_TARGET ||
			     task == TASK_GET_PATH_TO_SPOT )
			{
				return true;
			}
		}
	}

	// Name-based detection for custom, squad, or species-specific schedules
	if ( pSchedule->pName )
	{
		const char *psz = pSchedule->pName;
		if ( strstr( psz, "Chase" ) || strstr( psz, "chase" ) ||
		     strstr( psz, "LineOfFire" ) || strstr( psz, "Route" ) ||
		     strstr( psz, "Path" ) || strstr( psz, "Follow" ) ||
		     strstr( psz, "Run" ) || strstr( psz, "Walk" ) ||
		     strstr( psz, "Move" ) || strstr( psz, "Approach" ) ||
		     strstr( psz, "Hunt" ) )
		{
			return true;
		}
	}

	// Active locomotion state on the monster itself
	if ( pMonster )
	{
		if ( pMonster->m_movementGoal != 0 /* MOVEGOAL_NONE */ ||
		     pMonster->m_Activity == ACT_WALK ||
		     pMonster->m_Activity == ACT_RUN )
		{
			return true;
		}
	}

	return false;
}

int GladderMonsterModifiers::GetScheduleInterruptMask( CBaseMonster *pMonster, Schedule_t *pSchedule, int iDefaultMask )
{
	// Guard against modifying scripted, frozen, or deliberate-wait monsters (SPEC §3, Issue #104)
	if ( pMonster )
	{
		if ( pMonster->m_MonsterState == MONSTERSTATE_SCRIPT )
			return iDefaultMask;

		if ( pMonster->pev && ( pMonster->pev->flags & FL_FROZEN ) )
			return iDefaultMask;

		if ( pMonster->pev && ( pMonster->pev->spawnflags & SF_MONSTER_WAIT_TILL_SEEN ) )
			return iDefaultMask;
	}

	if ( IsMovementSchedule( pSchedule, pMonster ) )
	{
		int mask = iDefaultMask | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE |
		           bits_COND_NEW_ENEMY |
		           bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2 |
		           bits_COND_CAN_MELEE_ATTACK1 | bits_COND_CAN_MELEE_ATTACK2;

		// Guard against schedule thrashing (SPEC §3, Issue #104):
		// When a monster is actively chasing an enemy (e.g. SCHED_CHASE_ENEMY), seeing the enemy
		// is the expected condition and should not abort the chase every single frame.
		// For all other movement schedules (idle walk, patrol, goal-walking, route traversal),
		// seeing an enemy must immediately break the route so the monster acquires and attacks.
		bool bIsChaseSchedule = false;
		if ( pSchedule && pSchedule->pName )
		{
			if ( strstr( pSchedule->pName, "Chase" ) || strstr( pSchedule->pName, "chase" ) ||
			     strstr( pSchedule->pName, "Hunt" ) )
			{
				bIsChaseSchedule = true;
			}
		}

		if ( !bIsChaseSchedule )
		{
			mask |= bits_COND_SEE_ENEMY;
		}

		return mask;
	}

	return iDefaultMask;
}

