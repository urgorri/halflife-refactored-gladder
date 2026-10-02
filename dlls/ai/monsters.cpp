/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/
/*

===== monsters.cpp ========================================================

  Monster-related utility code

*/

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/nodes.h"
#include "ai/monsters.h"
#include "core/animation.h"
#include "core/saverestore.h"
#include "weapons/weapon_base.h"
#include "gameplay/scripted.h"
#include "ai/squadmonster.h"
#include "core/decals.h"
#include "ai/soundent.h"
#include "gameplay/gamerules.h"
#include "ai/monster_relationship_manager.h"

#define MONSTER_CUT_CORNER_DIST 8 // 8 means the monster's bounding box is contained without the box of the node in WC

Vector VecBModelOrigin( entvars_t *pevBModel );

extern DLL_GLOBAL BOOL g_fDrawLines;
extern DLL_GLOBAL short g_sModelIndexLaser;    // holds the index for the laser beam
extern DLL_GLOBAL short g_sModelIndexLaserDot; // holds the index for the laser beam dot

extern CGraph WorldGraph; // the world node graph

// Global Savedata for monster
// Schedule state serialization notes
// lose our enemy pointer or other data (goal ent, target, etc)
// that make the current schedule invalid, perhaps it's best
// to just pick a new one when we start up again.
TYPEDESCRIPTION CBaseMonster::m_SaveData[] =
    {
        DEFINE_FIELD( CBaseMonster, m_hEnemy, FIELD_EHANDLE ),
        DEFINE_FIELD( CBaseMonster, m_hTargetEnt, FIELD_EHANDLE ),
        DEFINE_ARRAY( CBaseMonster, m_hOldEnemy, FIELD_EHANDLE, MAX_OLD_ENEMIES ),
        DEFINE_ARRAY( CBaseMonster, m_vecOldEnemy, FIELD_POSITION_VECTOR, MAX_OLD_ENEMIES ),
        DEFINE_FIELD( CBaseMonster, m_flFieldOfView, FIELD_FLOAT ),
        DEFINE_FIELD( CBaseMonster, m_flWaitFinished, FIELD_TIME ),
        DEFINE_FIELD( CBaseMonster, m_flMoveWaitFinished, FIELD_TIME ),

        DEFINE_FIELD( CBaseMonster, m_Activity, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_IdealActivity, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_LastHitGroup, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_MonsterState, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_IdealMonsterState, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_iTaskStatus, FIELD_INTEGER ),

        // Schedule_t			*m_pSchedule;

        DEFINE_FIELD( CBaseMonster, m_iScheduleIndex, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_afConditions, FIELD_INTEGER ),
        // WayPoint_t			m_Route[ ROUTE_SIZE ];
        //	DEFINE_FIELD( CBaseMonster, m_movementGoal, FIELD_INTEGER ),
        //	DEFINE_FIELD( CBaseMonster, m_iRouteIndex, FIELD_INTEGER ),
        //	DEFINE_FIELD( CBaseMonster, m_moveWaitTime, FIELD_FLOAT ),

        DEFINE_FIELD( CBaseMonster, m_vecMoveGoal, FIELD_POSITION_VECTOR ),
        DEFINE_FIELD( CBaseMonster, m_movementActivity, FIELD_INTEGER ),

        //		int					m_iAudibleList; // first index of a linked list of sounds that the monster can hear.
        //	DEFINE_FIELD( CBaseMonster, m_afSoundTypes, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_vecLastPosition, FIELD_POSITION_VECTOR ),
        DEFINE_FIELD( CBaseMonster, m_iHintNode, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_afMemory, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_iMaxHealth, FIELD_INTEGER ),

        DEFINE_FIELD( CBaseMonster, m_vecEnemyLKP, FIELD_POSITION_VECTOR ),
        DEFINE_FIELD( CBaseMonster, m_cAmmoLoaded, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_afCapability, FIELD_INTEGER ),

        DEFINE_FIELD( CBaseMonster, m_flNextAttack, FIELD_TIME ),
        DEFINE_FIELD( CBaseMonster, m_bitsDamageType, FIELD_INTEGER ),
        DEFINE_ARRAY( CBaseMonster, m_rgbTimeBasedDamage, FIELD_CHARACTER, CDMG_TIMEBASED ),
        DEFINE_FIELD( CBaseMonster, m_bloodColor, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_failSchedule, FIELD_INTEGER ),

        DEFINE_FIELD( CBaseMonster, m_flHungryTime, FIELD_TIME ),
        DEFINE_FIELD( CBaseMonster, m_flDistTooFar, FIELD_FLOAT ),
        DEFINE_FIELD( CBaseMonster, m_flDistLook, FIELD_FLOAT ),
        DEFINE_FIELD( CBaseMonster, m_iTriggerCondition, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_iszTriggerTarget, FIELD_STRING ),

        DEFINE_FIELD( CBaseMonster, m_HackedGunPos, FIELD_VECTOR ),

        DEFINE_FIELD( CBaseMonster, m_scriptState, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseMonster, m_pCine, FIELD_CLASSPTR ),
};

// IMPLEMENT_SAVERESTORE( CBaseMonster, CBaseToggle );
int CBaseMonster::Save( CSave &save )
{
	if ( !CBaseToggle::Save( save ) )
		return 0;
	return save.WriteFields( "CBaseMonster", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CBaseMonster::Restore( CRestore &restore )
{
	if ( !CBaseToggle::Restore( restore ) )
		return 0;
	int status = restore.ReadFields( "CBaseMonster", this, m_SaveData, ARRAYSIZE( m_SaveData ) );

	// We don't save/restore routes yet
	RouteClear();

	// We don't save/restore schedules yet
	m_pSchedule   = NULL;
	m_iTaskStatus = TASKSTATUS_NEW;

	// Reset animation
	m_Activity = ACT_RESET;

	// If we don't have an enemy, clear conditions like see enemy, etc.
	if ( m_hEnemy == NULL )
		m_afConditions = 0;

	return status;
}

//=========================================================
// Eat - makes a monster full for a little while.
//=========================================================
void CBaseMonster ::Eat( float flFullDuration )
{
	m_flHungryTime = gpGlobals->time + flFullDuration;
}

//=========================================================
// FShouldEat - returns true if a monster is hungry.
//=========================================================
BOOL CBaseMonster ::FShouldEat( void )
{
	if ( m_flHungryTime > gpGlobals->time )
	{
		return FALSE;
	}

	return TRUE;
}

//=========================================================
// BarnacleVictimBitten - called
// by Barnacle victims when the barnacle pulls their head
// into its mouth
//=========================================================
void CBaseMonster ::BarnacleVictimBitten( entvars_t *pevBarnacle )
{
	Schedule_t *pNewSchedule;

	pNewSchedule = GetScheduleOfType( SCHED_BARNACLE_VICTIM_CHOMP );

	if ( pNewSchedule )
	{
		ChangeSchedule( pNewSchedule );
	}
}

//=========================================================
// BarnacleVictimReleased - called by barnacle victims when
// the host barnacle is killed.
//=========================================================
void CBaseMonster ::BarnacleVictimReleased( void )
{
	m_IdealMonsterState = MONSTERSTATE_IDLE;

	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_STEP;
}

//=========================================================
// FValidateHintType - tells use whether or not the monster cares
// about the type of Hint Node given
//=========================================================
BOOL CBaseMonster ::FValidateHintType( short sHint )
{
	return FALSE;
}

//=========================================================
// Monster Think - calls out to core AI functions and handles this
// monster's specific animation events
//=========================================================
void CBaseMonster ::MonsterThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1; // keep monster thinking.

	RunAI();

	float flInterval = StudioFrameAdvance(); // animate
	                                         // start or end a fidget
	                                         // This needs a better home -- switching animations over time should be encapsulated on a per-activity basis
	                                         // perhaps MaintainActivity() or a ShiftAnimationOverTime() or something.
	if ( m_MonsterState != MONSTERSTATE_SCRIPT && m_MonsterState != MONSTERSTATE_DEAD && m_Activity == ACT_IDLE && m_fSequenceFinished )
	{
		int iSequence;

		if ( m_fSequenceLoops )
		{
			// animation does loop, which means we're playing subtle idle. Might need to
			// fidget.
			iSequence = LookupActivity( m_Activity );
		}
		else
		{
			// animation that just ended doesn't loop! That means we just finished a fidget
			// and should return to our heaviest weighted idle (the subtle one)
			iSequence = LookupActivityHeaviest( m_Activity );
		}
		if ( iSequence != ACTIVITY_NOT_AVAILABLE )
		{
			pev->sequence = iSequence; // Set to new anim (if it's there)
			ResetSequenceInfo();
		}
	}

	DispatchAnimEvents( flInterval );

	if ( !MovementIsComplete() )
	{
		Move( flInterval );
	}
#if _DEBUG
	else
	{
		if ( !TaskIsRunning() && !TaskIsComplete() )
			ALERT( at_error, "Schedule stalled!!\n" );
	}
#endif
}

//=========================================================
// CBaseMonster - USE - will make a monster angry at whomever
// activated it.
//=========================================================
void CBaseMonster ::MonsterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_IdealMonsterState = MONSTERSTATE_ALERT;
}

//=========================================================
// Ignore conditions - before a set of conditions is allowed
// to interrupt a monster's schedule, this function removes
// conditions that we have flagged to interrupt the current
// schedule, but may not want to interrupt the schedule every
// time. (Pain, for instance)
//=========================================================
int CBaseMonster ::IgnoreConditions( void )
{
	int iIgnoreConditions = 0;

	if ( !FShouldEat() )
	{
		// not hungry? Ignore food smell.
		iIgnoreConditions |= bits_COND_SMELL_FOOD;
	}

	if ( m_MonsterState == MONSTERSTATE_SCRIPT && m_pCine )
		iIgnoreConditions |= m_pCine->IgnoreConditions();

	return iIgnoreConditions;
}

//=========================================================
// SetActivity
//=========================================================
void CBaseMonster ::SetActivity( Activity NewActivity )
{
	int iSequence;

	iSequence = LookupActivity( NewActivity );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			// don't reset frame between walk and run
			if ( !( m_Activity == ACT_WALK || m_Activity == ACT_RUN ) || !( NewActivity == ACT_WALK || NewActivity == ACT_RUN ) )
				pev->frame = 0;
		}

		pev->sequence = iSequence; // Set to the reset anim (if it's there)
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_aiconsole, "%s has no sequence for act:%d\n", STRING( pev->classname ), NewActivity );
		pev->sequence = 0; // Set to the reset anim (if it's there)
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// In case someone calls this with something other than the ideal activity
	m_IdealActivity = m_Activity;
}

//=========================================================
// SetSequenceByName
//=========================================================
void CBaseMonster ::SetSequenceByName( char *szSequence )
{
	int iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence; // Set to the reset anim (if it's there)
		ResetSequenceInfo();
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_aiconsole, "%s has no sequence named:%f\n", STRING( pev->classname ), szSequence );
		pev->sequence = 0; // Set to the reset anim (if it's there)
	}
}

//=========================================================
// CheckLocalMove - returns TRUE if the caller can walk a
// straight line from its current origin to the given
// location. If so, don't use the node graph!
//
// if a valid pointer to a int is passed, the function
// will fill that int with the distance that the check
// reached before hitting something. THIS ONLY HAPPENS
// IF THE LOCAL MOVE CHECK FAILS!
//
// !!!PERFORMANCE - should we try to load balance this?
// DON"T USE SETORIGIN!
//=========================================================
#define LOCAL_STEP_SIZE 16
int CBaseMonster ::CheckLocalMove( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist )
{
	Vector vecStartPos; // record monster's position before trying the move
	float flYaw;
	float flDist;
	float flStep, stepSize;
	int iReturn;

	vecStartPos = pev->origin;

	flYaw   = UTIL_VecToYaw( vecEnd - vecStart ); // build a yaw that points to the goal.
	flDist  = ( vecEnd - vecStart ).Length2D();   // get the distance.
	iReturn = LOCALMOVE_VALID;                    // assume everything will be ok.

	// move the monster to the start of the local move that's to be checked.
	UTIL_SetOrigin( pev, vecStart ); // !!!BUGBUG - won't this fire triggers? - nope, SetOrigin doesn't fire

	if ( !( pev->flags & ( FL_FLY | FL_SWIM ) ) )
	{
		DROP_TO_FLOOR( ENT( pev ) ); // make sure monster is on the floor!
	}

	// pev->origin.z = vecStartPos.z;//!!!HACKHACK

	//	pev->origin = vecStart;

	/*
	    if ( flDist > 1024 )
	    {
	        // !!!PERFORMANCE - this operation may be too CPU intensive to try checks this large.
	        // We don't lose much here, because a distance this great is very likely
	        // to have something in the way.

	        // since we've actually moved the monster during the check, undo the move.
	        pev->origin = vecStartPos;
	        return FALSE;
	    }
	*/
	// this loop takes single steps to the goal.
	for ( flStep = 0; flStep < flDist; flStep += LOCAL_STEP_SIZE )
	{
		stepSize = LOCAL_STEP_SIZE;

		if ( ( flStep + LOCAL_STEP_SIZE ) >= ( flDist - 1 ) )
			stepSize = ( flDist - flStep ) - 1;

		//		UTIL_ParticleEffect ( pev->origin, g_vecZero, 255, 25 );

		if ( !WALK_MOVE( ENT( pev ), flYaw, stepSize, WALKMOVE_CHECKONLY ) )
		{ // can't take the next step, fail!

			if ( pflDist != NULL )
			{
				*pflDist = flStep;
			}
			if ( pTarget && pTarget->edict() == gpGlobals->trace_ent )
			{
				// if this step hits target ent, the move is legal.
				iReturn = LOCALMOVE_VALID;
				break;
			}
			else
			{
				// If we're going toward an entity, and we're almost getting there, it's OK.
				//				if ( pTarget && fabs( flDist - iStep ) < LOCAL_STEP_SIZE )
				//					fReturn = TRUE;
				//				else
				iReturn = LOCALMOVE_INVALID;
				break;
			}
		}
	}

	if ( iReturn == LOCALMOVE_VALID && !( pev->flags & ( FL_FLY | FL_SWIM ) ) && ( !pTarget || ( pTarget->pev->flags & FL_ONGROUND ) ) )
	{
		// The monster can move to a spot UNDER the target, but not to it. Don't try to triangulate, go directly to the node graph.
		// Use standard eye level offset (64 units) for robust line-of-sight checks
		if ( fabs( vecEnd.z - pev->origin.z ) > 64 )
		{
			iReturn = LOCALMOVE_INVALID_DONT_TRIANGULATE;
		}
	}
	/*
	// uncommenting this block will draw a line representing the nearest legal move.
	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_SHOWLINE);
	WRITE_COORD(MSG_BROADCAST, pev->origin.x);
	WRITE_COORD(MSG_BROADCAST, pev->origin.y);
	WRITE_COORD(MSG_BROADCAST, pev->origin.z);
	WRITE_COORD(MSG_BROADCAST, vecStart.x);
	WRITE_COORD(MSG_BROADCAST, vecStart.y);
	WRITE_COORD(MSG_BROADCAST, vecStart.z);
	*/

	// since we've actually moved the monster during the check, undo the move.
	UTIL_SetOrigin( pev, vecStartPos );

	return iReturn;
}

float CBaseMonster ::OpenDoorAndWait( entvars_t *pevDoor )
{
	float flTravelTime = 0;

	// ALERT(at_aiconsole, "A door. ");
	CBaseEntity *pcbeDoor = CBaseEntity::Instance( pevDoor );
	if ( pcbeDoor && !pcbeDoor->IsLockedByMaster() )
	{
		// ALERT(at_aiconsole, "unlocked! ");
		pcbeDoor->Use( this, this, USE_ON, 0.0 );
		// ALERT(at_aiconsole, "pevDoor->nextthink = %d ms\n", (int)(1000*pevDoor->nextthink));
		// ALERT(at_aiconsole, "pevDoor->ltime = %d ms\n", (int)(1000*pevDoor->ltime));
		// ALERT(at_aiconsole, "pev-> nextthink = %d ms\n", (int)(1000*pev->nextthink));
		// ALERT(at_aiconsole, "pev->ltime = %d ms\n", (int)(1000*pev->ltime));
		flTravelTime = pevDoor->nextthink - pevDoor->ltime;
		// ALERT(at_aiconsole, "Waiting %d ms\n", (int)(1000*flTravelTime));
		if ( pcbeDoor->pev->targetname )
		{
			edict_t *pentTarget = NULL;
			for ( ;; )
			{
				pentTarget = FIND_ENTITY_BY_TARGETNAME( pentTarget, STRING( pcbeDoor->pev->targetname ) );

				if ( VARS( pentTarget ) != pcbeDoor->pev )
				{
					if ( FNullEnt( pentTarget ) )
						break;

					if ( FClassnameIs( pentTarget, STRING( pcbeDoor->pev->classname ) ) )
					{
						CBaseEntity *pDoor = Instance( pentTarget );
						if ( pDoor )
							pDoor->Use( this, this, USE_ON, 0.0 );
					}
				}
			}
		}
	}

	return gpGlobals->time + flTravelTime;
}

//=========================================================
// MonsterInit - after a monster is spawned, it needs to
// be dropped into the world, checked for mobility problems,
// and put on the proper path, if any. This function does
// all of those things after the monster spawns. Any
// initialization that should take place for all monsters
// goes here.
//=========================================================
void CBaseMonster ::MonsterInit( void )
{
	if ( !g_pGameRules->FAllowMonsters() )
	{
		pev->flags |= FL_KILLME; // Post this because some monster code modifies class data after calling this function
		                         //		REMOVE_ENTITY(ENT(pev));
		return;
	}

	// Set fields common to all monsters
	pev->effects        = 0;
	pev->takedamage     = DAMAGE_AIM;
	pev->ideal_yaw      = pev->angles.y;
	pev->max_health     = pev->health;
	pev->deadflag       = DEAD_NO;
	m_IdealMonsterState = MONSTERSTATE_IDLE; // Assume monster will be idle, until proven otherwise

	m_IdealActivity = ACT_IDLE;

	SetBits( pev->flags, FL_MONSTER );
	if ( pev->spawnflags & SF_MONSTER_HITMONSTERCLIP )
		pev->flags |= FL_MONSTERCLIP;

	ClearSchedule();
	RouteClear();
	InitBoneControllers(); // FIX: should be done in Spawn

	m_iHintNode = NO_NODE;

	m_afMemory = MEMORY_CLEAR;

	m_hEnemy = NULL;

	m_flDistTooFar = 1024.0;
	m_flDistLook   = 2048.0;

	// set eye position
	SetEyePosition();

	SetThink( &CBaseMonster::MonsterInitThink );
	pev->nextthink = gpGlobals->time + 0.1;
	SetUse( &CBaseMonster::MonsterUse );
}

//=========================================================
// MonsterInitThink - Calls StartMonster. Startmonster is
// virtual, but this function cannot be
//=========================================================
void CBaseMonster ::MonsterInitThink( void )
{
	StartMonster();
}

//=========================================================
// StartMonster - final bit of initization before a monster
// is turned over to the AI.
//=========================================================
void CBaseMonster ::StartMonster( void )
{
	// update capabilities
	if ( LookupActivity( ACT_RANGE_ATTACK1 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_RANGE_ATTACK1;
	}
	if ( LookupActivity( ACT_RANGE_ATTACK2 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_RANGE_ATTACK2;
	}
	if ( LookupActivity( ACT_MELEE_ATTACK1 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_MELEE_ATTACK1;
	}
	if ( LookupActivity( ACT_MELEE_ATTACK2 ) != ACTIVITY_NOT_AVAILABLE )
	{
		m_afCapability |= bits_CAP_MELEE_ATTACK2;
	}

	// Raise monster off the floor one unit, then drop to floor
	if ( pev->movetype != MOVETYPE_FLY && !FBitSet( pev->spawnflags, SF_MONSTER_FALL_TO_GROUND ) )
	{
		pev->origin.z += 1;
		DROP_TO_FLOOR( ENT( pev ) );
		// Try to move the monster to make sure it's not stuck in a brush.
		if ( !WALK_MOVE( ENT( pev ), 0, 0, WALKMOVE_NORMAL ) )
		{
			ALERT( at_error, "Monster %s stuck in wall--level design error", STRING( pev->classname ) );
			pev->effects = EF_BRIGHTFIELD;
		}
	}
	else
	{
		pev->flags &= ~FL_ONGROUND;
	}

	if ( !FStringNull( pev->target ) ) // this monster has a target
	{
		// Find the monster's initial target entity, stash it
		m_pGoalEnt = CBaseEntity::Instance( FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) ) );

		if ( !m_pGoalEnt )
		{
			ALERT( at_error, "ReadyMonster()--%s couldn't find target %s", STRING( pev->classname ), STRING( pev->target ) );
		}
		else
		{
			// Monster will start turning towards his destination
			MakeIdealYaw( m_pGoalEnt->pev->origin );

			// set the monster up to walk a path corner path.
			// Adjust sequence if entity is currently in dynamic transition.
			// JAYJAY
			m_movementGoal = MOVEGOAL_PATHCORNER;

			if ( pev->movetype == MOVETYPE_FLY )
				m_movementActivity = ACT_FLY;
			else
				m_movementActivity = ACT_WALK;

			if ( !FRefreshRoute() )
			{
				ALERT( at_aiconsole, "Can't Create Route!\n" );
			}
			SetState( MONSTERSTATE_IDLE );
			ChangeSchedule( GetScheduleOfType( SCHED_IDLE_WALK ) );
		}
	}

	// SetState ( m_IdealMonsterState );
	// SetActivity ( m_IdealActivity );

	// Delay drop to floor to make sure each door in the level has had its chance to spawn
	// Spread think times so that they don't all happen at the same time (Carmack)
	SetThink( &CBaseMonster::CallMonsterThink );
	pev->nextthink += RANDOM_FLOAT( 0.1, 0.4 ); // spread think times.

	if ( !FStringNull( pev->targetname ) ) // wait until triggered
	{
		SetState( MONSTERSTATE_IDLE );
		// Fallback animation selection when standard idle is missing
		SetActivity( ACT_IDLE );
		ChangeSchedule( GetScheduleOfType( SCHED_WAIT_TRIGGER ) );
	}
}

//=========================================================
// IRelationship - returns an integer that describes the
// relationship between two types of monster.
//=========================================================
int CBaseMonster::IRelationship( CBaseEntity *pTarget )
{
	if ( !pTarget )
	{
		return R_NO;
	}

	return MonsterRelationshipManager::GetInstance().GetRelationship( this, pTarget );
}

//=========================================================
// MakeIdealYaw - gets a yaw value for the caller that would
// face the supplied vector. Value is stuffed into the monster's
// ideal_yaw
//=========================================================
void CBaseMonster ::MakeIdealYaw( Vector vecTarget )
{
	Vector vecProjection;

	// strafing monster needs to face 90 degrees away from its goal
	if ( m_movementActivity == ACT_STRAFE_LEFT )
	{
		vecProjection.x = -vecTarget.y;
		vecProjection.y = vecTarget.x;

		pev->ideal_yaw = UTIL_VecToYaw( vecProjection - pev->origin );
	}
	else if ( m_movementActivity == ACT_STRAFE_RIGHT )
	{
		vecProjection.x = vecTarget.y;
		vecProjection.y = vecTarget.x;

		pev->ideal_yaw = UTIL_VecToYaw( vecProjection - pev->origin );
	}
	else
	{
		pev->ideal_yaw = UTIL_VecToYaw( vecTarget - pev->origin );
	}
}

//=========================================================
// FlYawDiff - returns the difference ( in degrees ) between
// monster's current yaw and ideal_yaw
//
// Positive result is left turn, negative is right turn
//=========================================================
float CBaseMonster::FlYawDiff( void )
{
	float flCurrentYaw;

	flCurrentYaw = UTIL_AngleMod( pev->angles.y );

	if ( flCurrentYaw == pev->ideal_yaw )
	{
		return 0;
	}

	return UTIL_AngleDiff( pev->ideal_yaw, flCurrentYaw );
}

//=========================================================
// Changeyaw - turns a monster towards its ideal_yaw
//=========================================================
float CBaseMonster::ChangeYaw( int yawSpeed )
{
	float ideal, current, move, speed;

	current = UTIL_AngleMod( pev->angles.y );
	ideal   = pev->ideal_yaw;
	if ( current != ideal )
	{
		if ( m_flLastYawTime == 0.f )
		{
			m_flLastYawTime = gpGlobals->time - gpGlobals->frametime;
		}

		float delta     = gpGlobals->time - m_flLastYawTime;
		m_flLastYawTime = gpGlobals->time;

		// Clamp delta like the engine does with frametime
		if ( delta > 0.25f )
			delta = 0.25f;

		float flSpeed = (float)yawSpeed;
		if ( g_pGameRules )
		{
			flSpeed = g_pGameRules->FlMonsterYawSpeed( this, flSpeed );
		}

		speed = flSpeed * delta * 2;
		move  = ideal - current;

		if ( ideal > current )
		{
			if ( move >= 180 )
				move = move - 360;
		}
		else
		{
			if ( move <= -180 )
				move = move + 360;
		}

		if ( move > 0 )
		{ // turning to the monster's left
			if ( move > speed )
				move = speed;
		}
		else
		{ // turning to the monster's right
			if ( move < -speed )
				move = -speed;
		}

		pev->angles.y = UTIL_AngleMod( current + move );

		// turn head in desired direction only if they have a turnable head
		if ( m_afCapability & bits_CAP_TURN_HEAD )
		{
			float yaw = pev->ideal_yaw - pev->angles.y;
			if ( yaw > 180 )
				yaw -= 360;
			if ( yaw < -180 )
				yaw += 360;
			// yaw *= 0.8;
			SetBoneController( 0, yaw );
		}
	}
	else
		move = 0;

	return move;
}

//=========================================================
// VecToYaw - turns a directional vector into a yaw value
// that points down that vector.
//=========================================================
float CBaseMonster::VecToYaw( Vector vecDir )
{
	if ( vecDir.x == 0 && vecDir.y == 0 && vecDir.z == 0 )
		return pev->angles.y;

	return UTIL_VecToYaw( vecDir );
}

//=========================================================
// SetEyePosition
//
// queries the monster's model for $eyeposition and copies
// that vector to the monster's view_ofs
//
//=========================================================
void CBaseMonster ::SetEyePosition( void )
{
	Vector vecEyePosition;
	void *pmodel = GET_MODEL_PTR( ENT( pev ) );

	GetEyePosition( pmodel, vecEyePosition );

	pev->view_ofs = vecEyePosition;

	if ( pev->view_ofs == g_vecZero )
	{
		ALERT( at_aiconsole, "%s has no view_ofs!\n", STRING( pev->classname ) );
	}
}

void CBaseMonster::ReportAIState( void )
{
	ALERT_TYPE level = at_console;

	static const char *pStateNames[] = { "None", "Idle", "Combat", "Alert", "Hunt", "Prone", "Scripted", "Dead" };

	ALERT( level, "%s: ", STRING( pev->classname ) );
	if ( (int)m_MonsterState < ARRAYSIZE( pStateNames ) )
		ALERT( level, "State: %s, ", pStateNames[m_MonsterState] );
	int i = 0;
	while ( activity_map[i].type != 0 )
	{
		if ( activity_map[i].type == (int)m_Activity )
		{
			ALERT( level, "Activity %s, ", activity_map[i].name );
			break;
		}
		i++;
	}

	if ( m_pSchedule )
	{
		const char *pName = NULL;
		pName             = m_pSchedule->pName;
		if ( !pName )
			pName = "Unknown";
		ALERT( level, "Schedule %s, ", pName );
		Task_t *pTask = GetTask();
		if ( pTask )
			ALERT( level, "Task %d (#%d), ", pTask->iTask, m_iScheduleIndex );
	}
	else
		ALERT( level, "No Schedule, " );

	if ( m_hEnemy != NULL )
		ALERT( level, "\nEnemy is %s", STRING( m_hEnemy->pev->classname ) );
	else
		ALERT( level, "No enemy" );

	if ( IsMoving() )
	{
		ALERT( level, " Moving " );
		if ( m_flMoveWaitFinished > gpGlobals->time )
			ALERT( level, ": Stopped for %.2f. ", m_flMoveWaitFinished - gpGlobals->time );
		else if ( m_IdealActivity == GetStoppedActivity() )
			ALERT( level, ": In stopped anim. " );
	}

	CSquadMonster *pSquadMonster = MySquadMonsterPointer();

	if ( pSquadMonster )
	{
		if ( !pSquadMonster->InSquad() )
		{
			ALERT( level, "not " );
		}

		ALERT( level, "In Squad, " );

		if ( !pSquadMonster->IsLeader() )
		{
			ALERT( level, "not " );
		}

		ALERT( level, "Leader." );
	}

	ALERT( level, "\n" );
	ALERT( level, "Yaw speed:%3.1f,Health: %3.1f\n", pev->yaw_speed, pev->health );
	if ( pev->spawnflags & SF_MONSTER_PRISONER )
		ALERT( level, " PRISONER! " );
	if ( pev->spawnflags & SF_MONSTER_PREDISASTER )
		ALERT( level, " Pre-Disaster! " );
	ALERT( level, "\n" );
}

//=========================================================
// KeyValue
//
// !!! netname entvar field is used in squadmonster for groupname!!!
//=========================================================
void CBaseMonster ::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "TriggerTarget" ) )
	{
		m_iszTriggerTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled     = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "TriggerCondition" ) )
	{
		m_iTriggerCondition = atoi( pkvd->szValue );
		pkvd->fHandled      = TRUE;
	}
	else
	{
		CBaseToggle::KeyValue( pkvd );
	}
}

//=========================================================
// FCheckAITrigger - checks the monster's AI Trigger Conditions,
// if there is a condition, then checks to see if condition is
// met. If yes, the monster's TriggerTarget is fired.
//
// Returns TRUE if the target is fired.
//=========================================================
BOOL CBaseMonster ::FCheckAITrigger( void )
{
	BOOL fFireTarget;

	if ( m_iTriggerCondition == AITRIGGER_NONE )
	{
		// no conditions, so this trigger is never fired.
		return FALSE;
	}

	fFireTarget = FALSE;

	switch ( m_iTriggerCondition )
	{
	case AITRIGGER_SEEPLAYER_ANGRY_AT_PLAYER:
		if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() && HasConditions( bits_COND_SEE_ENEMY ) )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_SEEPLAYER_UNCONDITIONAL:
		if ( HasConditions( bits_COND_SEE_CLIENT ) )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_SEEPLAYER_NOT_IN_COMBAT:
		if ( HasConditions( bits_COND_SEE_CLIENT ) &&
		     m_MonsterState != MONSTERSTATE_COMBAT &&
		     m_MonsterState != MONSTERSTATE_PRONE &&
		     m_MonsterState != MONSTERSTATE_SCRIPT )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_TAKEDAMAGE:
		if ( m_afConditions & ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_DEATH:
		if ( pev->deadflag != DEAD_NO )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_HALFHEALTH:
		if ( IsAlive() && pev->health <= ( pev->max_health / 2 ) )
		{
			fFireTarget = TRUE;
		}
		break;
		/*

		  // Inter-level transition tracking for dynamic entities

		    case AITRIGGER_SQUADMEMBERDIE:
		        break;
		    case AITRIGGER_SQUADLEADERDIE:
		        break;
		*/
	case AITRIGGER_HEARWORLD:
		if ( m_afConditions & bits_COND_HEAR_SOUND && m_afSoundTypes & bits_SOUND_WORLD )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_HEARPLAYER:
		if ( m_afConditions & bits_COND_HEAR_SOUND && m_afSoundTypes & bits_SOUND_PLAYER )
		{
			fFireTarget = TRUE;
		}
		break;
	case AITRIGGER_HEARCOMBAT:
		if ( m_afConditions & bits_COND_HEAR_SOUND && m_afSoundTypes & bits_SOUND_COMBAT )
		{
			fFireTarget = TRUE;
		}
		break;
	}

	if ( fFireTarget )
	{
		// fire the target, then set the trigger conditions to NONE so we don't fire again
		ALERT( at_aiconsole, "AI Trigger Fire Target\n" );
		FireTargets( STRING( m_iszTriggerTarget ), this, this, USE_TOGGLE, 0 );
		m_iTriggerCondition = AITRIGGER_NONE;
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CanPlaySequence - determines whether or not the monster
// can play the scripted sequence or AI sequence that is
// trying to possess it. If DisregardState is set, the monster
// will be sucked into the script no matter what state it is
// in. ONLY Scripted AI ents should allow this.
//=========================================================
int CBaseMonster ::CanPlaySequence( BOOL fDisregardMonsterState, int interruptLevel )
{
	if ( m_pCine || !IsAlive() || m_MonsterState == MONSTERSTATE_PRONE )
	{
		// monster is already running a scripted sequence or dead!
		return FALSE;
	}

	if ( fDisregardMonsterState )
	{
		// ok to go, no matter what the monster state. (scripted AI)
		return TRUE;
	}

	if ( m_MonsterState == MONSTERSTATE_NONE || m_MonsterState == MONSTERSTATE_IDLE || m_IdealMonsterState == MONSTERSTATE_IDLE )
	{
		// ok to go, but only in these states
		return TRUE;
	}

	if ( m_MonsterState == MONSTERSTATE_ALERT && interruptLevel >= SS_INTERRUPT_BY_NAME )
		return TRUE;

	// unknown situation
	return FALSE;
}

//=========================================================
// FindLateralCover - attempts to locate a spot in the world
// directly to the left or right of the caller that will
// conceal them from view of pSightEnt
//=========================================================
#define COVER_CHECKS 5 // how many checks are made
#define COVER_DELTA 48 // distance between checks

//=========================================================
// FacingIdeal - tells us if a monster is facing its ideal
// yaw. Created this function because many spots in the
// code were checking the yawdiff against this magic
// number. Nicer to have it in one place if we're gonna
// be stuck with it.
//=========================================================
BOOL CBaseMonster ::FacingIdeal( void )
{
	if ( fabs( FlYawDiff() ) <= 0.006 ) //!!!BUGBUG - no magic numbers!!!
	{
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// FCanActiveIdle
//=========================================================
BOOL CBaseMonster ::FCanActiveIdle( void )
{
	/*
	if ( m_MonsterState == MONSTERSTATE_IDLE && m_IdealMonsterState == MONSTERSTATE_IDLE && !IsMoving() )
	{
	    return TRUE;
	}
	*/
	return FALSE;
}

void CBaseMonster::CorpseFallThink( void )
{
	if ( pev->flags & FL_ONGROUND )
	{
		SetThink( NULL );

		SetSequenceBox();
		UTIL_SetOrigin( pev, pev->origin ); // link into world.
	}
	else
		pev->nextthink = gpGlobals->time + 0.1;
}

// Call after animation/pose is set up
void CBaseMonster ::MonsterInitDead( void )
{
	InitBoneControllers();

	pev->solid    = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS; // so he'll fall to ground

	pev->frame = 0;
	ResetSequenceInfo();
	pev->framerate = 0;

	// Copy health
	pev->max_health = pev->health;
	pev->deadflag   = DEAD_DEAD;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	UTIL_SetOrigin( pev, pev->origin );

	// Setup health counters, etc.
	BecomeDead();
	SetThink( &CBaseMonster::CorpseFallThink );
	pev->nextthink = gpGlobals->time + 0.5;
}

//=========================================================
// BBoxIsFlat - check to see if the monster's bounding box
// is lying flat on a surface (traces from all four corners
// are same length.)
//=========================================================
BOOL CBaseMonster ::BBoxFlat( void )
{
	TraceResult tr;
	Vector vecPoint;
	float flXSize, flYSize;
	float flLength;
	float flLength2;

	flXSize = pev->size.x / 2;
	flYSize = pev->size.y / 2;

	vecPoint.x = pev->origin.x + flXSize;
	vecPoint.y = pev->origin.y + flYSize;
	vecPoint.z = pev->origin.z;

	UTIL_TraceLine( vecPoint, vecPoint - Vector( 0, 0, 100 ), ignore_monsters, ENT( pev ), &tr );
	flLength = ( vecPoint - tr.vecEndPos ).Length();

	vecPoint.x = pev->origin.x - flXSize;
	vecPoint.y = pev->origin.y - flYSize;

	UTIL_TraceLine( vecPoint, vecPoint - Vector( 0, 0, 100 ), ignore_monsters, ENT( pev ), &tr );
	flLength2 = ( vecPoint - tr.vecEndPos ).Length();
	if ( flLength2 > flLength )
	{
		return FALSE;
	}
	flLength = flLength2;

	vecPoint.x = pev->origin.x - flXSize;
	vecPoint.y = pev->origin.y + flYSize;
	UTIL_TraceLine( vecPoint, vecPoint - Vector( 0, 0, 100 ), ignore_monsters, ENT( pev ), &tr );
	flLength2 = ( vecPoint - tr.vecEndPos ).Length();
	if ( flLength2 > flLength )
	{
		return FALSE;
	}
	flLength = flLength2;

	vecPoint.x = pev->origin.x + flXSize;
	vecPoint.y = pev->origin.y - flYSize;
	UTIL_TraceLine( vecPoint, vecPoint - Vector( 0, 0, 100 ), ignore_monsters, ENT( pev ), &tr );
	flLength2 = ( vecPoint - tr.vecEndPos ).Length();
	if ( flLength2 > flLength )
	{
		return FALSE;
	}
	flLength = flLength2;

	return TRUE;
}

//=========================================================
// DropItem - dead monster drops named item
//=========================================================
CBaseEntity *CBaseMonster ::DropItem( char *pszItemName, const Vector &vecPos, const Vector &vecAng )
{
	if ( !pszItemName )
	{
		ALERT( at_console, "DropItem() - No item name!\n" );
		return NULL;
	}

	CBaseEntity *pItem = CBaseEntity::Create( pszItemName, vecPos, vecAng, edict() );

	if ( pItem )
	{
		// do we want this behavior to be default?! (sjb)
		pItem->pev->velocity  = pev->velocity;
		pItem->pev->avelocity = Vector( 0, RANDOM_FLOAT( 0, 100 ), 0 );
		return pItem;
	}
	else
	{
		ALERT( at_console, "DropItem() - Didn't create!\n" );
		return FALSE;
	}
}

BOOL CBaseMonster ::ShouldFadeOnDeath( void )
{
	// if flagged to fade out or I have an owner (I came from a monster spawner)
	if ( ( pev->spawnflags & SF_MONSTER_FADECORPSE ) || !FNullEnt( pev->owner ) )
		return TRUE;

	return FALSE;
}
