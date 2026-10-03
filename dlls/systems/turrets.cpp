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

#include "systems/turrets.h"
#include "systems/effects.h"
#include "systems/explode.h"
#include "weapons/weapon_base.h"

extern Vector VecBModelOrigin( entvars_t *pevBModel );

//=========================================================
// CBaseTurret implementation
//=========================================================

TYPEDESCRIPTION CBaseTurret::m_SaveData[] =
    {
        DEFINE_FIELD( CBaseTurret, m_flMaxSpin, FIELD_FLOAT ),
        DEFINE_FIELD( CBaseTurret, m_iSpin, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_pEyeGlow, FIELD_CLASSPTR ),
        DEFINE_FIELD( CBaseTurret, m_eyeBrightness, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_iDeployHeight, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_iRetractHeight, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_iMinPitch, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_iBaseTurnRate, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_fTurnRate, FIELD_FLOAT ),
        DEFINE_FIELD( CBaseTurret, m_iOrientation, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_iOn, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_fBeserk, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_iAutoStart, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_vecLastSight, FIELD_POSITION_VECTOR ),
        DEFINE_FIELD( CBaseTurret, m_flLastSight, FIELD_TIME ),
        DEFINE_FIELD( CBaseTurret, m_flMaxWait, FIELD_FLOAT ),
        DEFINE_FIELD( CBaseTurret, m_iSearchSpeed, FIELD_INTEGER ),
        DEFINE_FIELD( CBaseTurret, m_flStartYaw, FIELD_FLOAT ),
        DEFINE_FIELD( CBaseTurret, m_vecCurAngles, FIELD_VECTOR ),
        DEFINE_FIELD( CBaseTurret, m_vecGoalAngles, FIELD_VECTOR ),
        DEFINE_FIELD( CBaseTurret, m_flPingTime, FIELD_TIME ),
        DEFINE_FIELD( CBaseTurret, m_flSpinUpTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CBaseTurret, CBaseMonster );

void CBaseTurret::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "maxsleep" ) )
	{
		m_flMaxWait    = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "orientation" ) )
	{
		m_iOrientation = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "searchspeed" ) )
	{
		m_iSearchSpeed = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "turnrate" ) )
	{
		m_iBaseTurnRate = atoi( pkvd->szValue );
		pkvd->fHandled  = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "style" ) ||
	          FStrEq( pkvd->szKeyName, "height" ) ||
	          FStrEq( pkvd->szKeyName, "value1" ) ||
	          FStrEq( pkvd->szKeyName, "value2" ) ||
	          FStrEq( pkvd->szKeyName, "value3" ) )
		pkvd->fHandled = TRUE;
	else
		CBaseMonster::KeyValue( pkvd );
}

void CBaseTurret::Spawn()
{
	Precache();
	pev->nextthink  = gpGlobals->time + 1;
	pev->movetype   = MOVETYPE_FLY;
	pev->sequence   = 0;
	pev->frame      = 0;
	pev->solid      = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_AIM;

	SetBits( pev->flags, FL_MONSTER );
	SetUse( &CBaseTurret::TurretUse );

	if ( ( pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE ) && !( pev->spawnflags & SF_MONSTER_TURRET_STARTINACTIVE ) )
	{
		m_iAutoStart = TRUE;
	}

	ResetSequenceInfo();
	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );
	m_flFieldOfView = VIEW_FIELD_FULL;
}

void CBaseTurret::Precache()
{
	PRECACHE_SOUND( "turret/tu_fire1.wav" );
	PRECACHE_SOUND( "turret/tu_ping.wav" );
	PRECACHE_SOUND( "turret/tu_active2.wav" );
	PRECACHE_SOUND( "turret/tu_die.wav" );
	PRECACHE_SOUND( "turret/tu_die2.wav" );
	PRECACHE_SOUND( "turret/tu_die3.wav" );
	PRECACHE_SOUND( "turret/tu_deploy.wav" );
	PRECACHE_SOUND( "turret/tu_spinup.wav" );
	PRECACHE_SOUND( "turret/tu_spindown.wav" );
	PRECACHE_SOUND( "turret/tu_search.wav" );
	PRECACHE_SOUND( "turret/tu_alert.wav" );
}

void CBaseTurret::Initialize( void )
{
	m_iOn     = 0;
	m_fBeserk = 0;
	m_iSpin   = 0;

	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );

	if ( m_iBaseTurnRate == 0 )
		m_iBaseTurnRate = TURRET_TURNRATE;
	if ( m_flMaxWait == 0 )
		m_flMaxWait = TURRET_MAXWAIT;
	m_flStartYaw = pev->angles.y;
	if ( m_iOrientation == 1 )
	{
		pev->idealpitch = 180;
		pev->angles.x   = 180;
		pev->view_ofs.z = -pev->view_ofs.z;
		pev->effects |= EF_INVLIGHT;
		pev->angles.y = pev->angles.y + 180;
		if ( pev->angles.y > 360 )
			pev->angles.y = pev->angles.y - 360;
	}

	m_vecGoalAngles.x = 0;

	if ( m_iAutoStart )
	{
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink( &CBaseTurret::AutoSearchThink );
		pev->nextthink = gpGlobals->time + .1;
	}
	else
		SetThink( &CBaseTurret::SUB_DoNothing );
}

void CBaseTurret::TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_iOn ) )
		return;

	if ( m_iOn )
	{
		m_hEnemy       = NULL;
		pev->nextthink = gpGlobals->time + 0.1;
		m_iAutoStart   = FALSE;
		SetThink( &CBaseTurret::Retire );
	}
	else
	{
		pev->nextthink = gpGlobals->time + 0.1;

		if ( pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE )
		{
			m_iAutoStart = TRUE;
		}

		SetThink( &CBaseTurret::Deploy );
	}
}

void CBaseTurret::Ping( void )
{
	if ( m_flPingTime == 0 )
		m_flPingTime = gpGlobals->time + 1;
	else if ( m_flPingTime <= gpGlobals->time )
	{
		m_flPingTime = gpGlobals->time + 1;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "turret/tu_ping.wav", 1, ATTN_NORM );
		EyeOn();
	}
	else if ( m_eyeBrightness > 0 )
	{
		EyeOff();
	}
}

void CBaseTurret::EyeOn()
{
	if ( m_pEyeGlow )
	{
		if ( m_eyeBrightness != 255 )
		{
			m_eyeBrightness = 255;
		}
		m_pEyeGlow->SetBrightness( m_eyeBrightness );
	}
}

void CBaseTurret::EyeOff()
{
	if ( m_pEyeGlow )
	{
		if ( m_eyeBrightness > 0 )
		{
			m_eyeBrightness = max( 0, m_eyeBrightness - 30 );
			m_pEyeGlow->SetBrightness( m_eyeBrightness );
		}
	}
}

void CBaseTurret::UpdateOnRemove( void )
{
	if ( m_pEyeGlow )
	{
		UTIL_Remove( m_pEyeGlow );
		m_pEyeGlow = NULL;
	}
	CBaseMonster::UpdateOnRemove();
}

void CBaseTurret::ActiveThink( void )
{
	int fAttack = 0;
	Vector vecDirToEnemy;

	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if ( ( !m_iOn ) || ( m_hEnemy == NULL ) )
	{
		m_hEnemy      = NULL;
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink( &CBaseTurret::SearchThink );
		return;
	}

	if ( !m_hEnemy->IsAlive() )
	{
		if ( !m_flLastSight )
		{
			m_flLastSight = gpGlobals->time + 0.5;
		}
		else
		{
			if ( gpGlobals->time > m_flLastSight )
			{
				m_hEnemy      = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink( &CBaseTurret::SearchThink );
				return;
			}
		}
	}

	Vector vecMid      = pev->origin + pev->view_ofs;
	Vector vecMidEnemy = m_hEnemy->BodyTarget( vecMid );

	int fEnemyVisible = FBoxVisible( pev, m_hEnemy->pev, vecMidEnemy );

	vecDirToEnemy       = vecMidEnemy - vecMid;
	float flDistToEnemy = vecDirToEnemy.Length();

	Vector vec = UTIL_VecToAngles( vecMidEnemy - vecMid );

	if ( !fEnemyVisible || ( flDistToEnemy > TURRET_RANGE ) )
	{
		if ( !m_flLastSight )
			m_flLastSight = gpGlobals->time + 0.5;
		else
		{
			if ( gpGlobals->time > m_flLastSight )
			{
				m_hEnemy      = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink( &CBaseTurret::SearchThink );
				return;
			}
		}
		fEnemyVisible = 0;
	}
	else
	{
		m_vecLastSight = vecMidEnemy;
	}

	UTIL_MakeAimVectors( m_vecCurAngles );

	Vector vecLOS = vecDirToEnemy;
	vecLOS        = vecLOS.Normalize();

	if ( DotProduct( vecLOS, gpGlobals->v_forward ) <= 0.866 )
		fAttack = FALSE;
	else
		fAttack = TRUE;

	if ( m_iSpin && ( ( fAttack ) || ( m_fBeserk ) ) )
	{
		Vector vecSrc, vecAng;
		GetAttachment( 0, vecSrc, vecAng );
		SetTurretAnim( TURRET_ANIM_FIRE );
		Shoot( vecSrc, gpGlobals->v_forward );
	}
	else
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
	}

	if ( m_fBeserk )
	{
		if ( RANDOM_LONG( 0, 9 ) == 0 )
		{
			m_vecGoalAngles.y = RANDOM_FLOAT( 0, 360 );
			m_vecGoalAngles.x = RANDOM_FLOAT( 0, 90 ) - 90 * m_iOrientation;
			TakeDamage( pev, pev, 1, DMG_GENERIC );
			return;
		}
	}
	else if ( fEnemyVisible )
	{
		if ( vec.y > 360 )
			vec.y -= 360;

		if ( vec.y < 0 )
			vec.y += 360;

		if ( vec.x < -180 )
			vec.x += 360;

		if ( vec.x > 180 )
			vec.x -= 360;

		if ( m_iOrientation == 0 )
		{
			if ( vec.x > 90 )
				vec.x = 90;
			else if ( vec.x < m_iMinPitch )
				vec.x = m_iMinPitch;
		}
		else
		{
			if ( vec.x < -90 )
				vec.x = -90;
			else if ( vec.x > -m_iMinPitch )
				vec.x = -m_iMinPitch;
		}

		m_vecGoalAngles.y = vec.y;
		m_vecGoalAngles.x = vec.x;
	}

	SpinUpCall();
	MoveTurret();
}

void CBaseTurret::Deploy( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if ( pev->sequence != TURRET_ANIM_DEPLOY )
	{
		m_iOn = 1;
		SetTurretAnim( TURRET_ANIM_DEPLOY );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM );
		SUB_UseTargets( this, USE_ON, 0 );
	}

	if ( m_fSequenceFinished )
	{
		pev->maxs.z = m_iDeployHeight;
		pev->mins.z = -m_iDeployHeight;
		UTIL_SetSize( pev, pev->mins, pev->maxs );

		m_vecCurAngles.x = 0;

		if ( m_iOrientation == 1 )
		{
			m_vecCurAngles.y = UTIL_AngleMod( pev->angles.y + 180 );
		}
		else
		{
			m_vecCurAngles.y = UTIL_AngleMod( pev->angles.y );
		}

		SetTurretAnim( TURRET_ANIM_SPIN );
		pev->framerate = 0;
		SetThink( &CBaseTurret::SearchThink );
	}

	m_flLastSight = gpGlobals->time + m_flMaxWait;
}

void CBaseTurret::Retire( void )
{
	m_vecGoalAngles.x = 0;
	m_vecGoalAngles.y = m_flStartYaw;

	pev->nextthink = gpGlobals->time + 0.1;

	StudioFrameAdvance();
	EyeOff();

	if ( !MoveTurret() )
	{
		if ( m_iSpin )
		{
			SpinDownCall();
		}
		else if ( pev->sequence != TURRET_ANIM_RETIRE )
		{
			SetTurretAnim( TURRET_ANIM_RETIRE );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM, 0, 120 );
			SUB_UseTargets( this, USE_OFF, 0 );
		}
		else if ( m_fSequenceFinished )
		{
			m_iOn         = 0;
			m_flLastSight = 0;
			SetTurretAnim( TURRET_ANIM_NONE );
			pev->maxs.z = m_iRetractHeight;
			pev->mins.z = -m_iRetractHeight;
			UTIL_SetSize( pev, pev->mins, pev->maxs );
			if ( m_iAutoStart )
			{
				SetThink( &CBaseTurret::AutoSearchThink );
				pev->nextthink = gpGlobals->time + .1;
			}
			else
				SetThink( &CBaseTurret::SUB_DoNothing );
		}
	}
	else
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
	}
}

void CBaseTurret::SetTurretAnim( TURRET_ANIM anim )
{
	if ( pev->sequence != anim )
	{
		switch ( anim )
		{
		case TURRET_ANIM_FIRE:
		case TURRET_ANIM_SPIN:
			if ( pev->sequence != TURRET_ANIM_FIRE && pev->sequence != TURRET_ANIM_SPIN )
			{
				pev->frame = 0;
			}
			break;
		default:
			pev->frame = 0;
			break;
		}

		pev->sequence = anim;
		ResetSequenceInfo();

		switch ( anim )
		{
		case TURRET_ANIM_RETIRE:
			pev->frame     = 255;
			pev->framerate = -1.0;
			break;
		case TURRET_ANIM_DIE:
			pev->framerate = 1.0;
			break;
		}
	}
}

void CBaseTurret::SearchThink( void )
{
	SetTurretAnim( TURRET_ANIM_SPIN );
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if ( m_flSpinUpTime == 0 && m_flMaxSpin )
		m_flSpinUpTime = gpGlobals->time + m_flMaxSpin;

	Ping();

	if ( m_hEnemy != NULL )
	{
		if ( !m_hEnemy->IsAlive() )
			m_hEnemy = NULL;
	}

	if ( m_hEnemy == NULL )
	{
		Look( TURRET_RANGE );
		m_hEnemy = BestVisibleEnemy();
	}

	if ( m_hEnemy != NULL )
	{
		m_flLastSight  = 0;
		m_flSpinUpTime = 0;
		SetThink( &CBaseTurret::ActiveThink );
	}
	else
	{
		if ( gpGlobals->time > m_flLastSight )
		{
			m_flLastSight  = 0;
			m_flSpinUpTime = 0;
			SetThink( &CBaseTurret::Retire );
		}
		else if ( ( m_flSpinUpTime ) && ( gpGlobals->time > m_flSpinUpTime ) )
		{
			SpinDownCall();
		}

		m_vecGoalAngles.y = ( m_vecGoalAngles.y + 0.1 * m_fTurnRate );
		if ( m_vecGoalAngles.y >= 360 )
			m_vecGoalAngles.y -= 360;
		MoveTurret();
	}
}

void CBaseTurret::AutoSearchThink( void )
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.3;

	if ( m_hEnemy != NULL )
	{
		if ( !m_hEnemy->IsAlive() )
			m_hEnemy = NULL;
	}

	if ( m_hEnemy == NULL )
	{
		Look( TURRET_RANGE );
		m_hEnemy = BestVisibleEnemy();
	}

	if ( m_hEnemy != NULL )
	{
		SetThink( &CBaseTurret::Deploy );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_alert.wav", TURRET_MACHINE_VOLUME, ATTN_NORM );
	}
}

void CBaseTurret::TurretDeath( void )
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->deadflag != DEAD_DEAD )
	{
		pev->deadflag = DEAD_DEAD;

		float flRndSound = RANDOM_FLOAT( 0, 1 );

		if ( flRndSound <= 0.33 )
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_die.wav", 1.0, ATTN_NORM );
		else if ( flRndSound <= 0.66 )
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_die2.wav", 1.0, ATTN_NORM );
		else
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_die3.wav", 1.0, ATTN_NORM );

		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "turret/tu_active2.wav", 0, 0, SND_STOP, 100 );

		if ( m_iOrientation == 0 )
			m_vecGoalAngles.x = -15;
		else
			m_vecGoalAngles.x = -90;

		SetTurretAnim( TURRET_ANIM_DIE );

		EyeOn();
	}

	EyeOff();

	if ( pev->dmgtime + RANDOM_FLOAT( 0, 2 ) > gpGlobals->time )
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ) );
		WRITE_COORD( RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ) );
		WRITE_COORD( pev->origin.z - m_iOrientation * 64 );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 25 );
		WRITE_BYTE( 10 - m_iOrientation * 5 );
		MESSAGE_END();
	}

	if ( pev->dmgtime + RANDOM_FLOAT( 0, 5 ) > gpGlobals->time )
	{
		Vector vecSrc = Vector( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ), RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ), 0 );
		if ( m_iOrientation == 0 )
			vecSrc = vecSrc + Vector( 0, 0, RANDOM_FLOAT( pev->origin.z, pev->absmax.z ) );
		else
			vecSrc = vecSrc + Vector( 0, 0, RANDOM_FLOAT( pev->absmin.z, pev->origin.z ) );

		UTIL_Sparks( vecSrc );
	}

	if ( m_fSequenceFinished && !MoveTurret() && pev->dmgtime + 5 < gpGlobals->time )
	{
		pev->framerate = 0;
		SetThink( NULL );
	}
}

void CBaseTurret::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( ptr->iHitgroup == 10 )
	{
		if ( pev->dmgtime != gpGlobals->time || ( RANDOM_LONG( 0, 10 ) < 1 ) )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2 ) );
			pev->dmgtime = gpGlobals->time;
		}

		flDamage = 0.1;
	}

	if ( !pev->takedamage )
		return;

	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}

int CBaseTurret::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !pev->takedamage )
		return 0;

	if ( !m_iOn )
		flDamage /= 10.0;

	pev->health -= flDamage;
	if ( pev->health <= 0 )
	{
		pev->health     = 0;
		pev->takedamage = DAMAGE_NO;
		pev->dmgtime    = gpGlobals->time;

		ClearBits( pev->flags, FL_MONSTER );

		SetUse( NULL );
		SetThink( &CBaseTurret::TurretDeath );
		SUB_UseTargets( this, USE_ON, 0 );
		pev->nextthink = gpGlobals->time + 0.1;

		return 0;
	}

	if ( pev->health <= 10 )
	{
		if ( m_iOn && ( 1 || RANDOM_LONG( 0, 0x7FFF ) > 800 ) )
		{
			m_fBeserk = 1;
			SetThink( &CBaseTurret::SearchThink );
		}
	}

	return 1;
}

int CBaseTurret::MoveTurret( void )
{
	int state = 0;

	if ( m_vecCurAngles.x != m_vecGoalAngles.x )
	{
		float flDir = m_vecGoalAngles.x > m_vecCurAngles.x ? 1 : -1;

		m_vecCurAngles.x += 0.1 * m_fTurnRate * flDir;

		if ( flDir == 1 )
		{
			if ( m_vecCurAngles.x > m_vecGoalAngles.x )
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}
		else
		{
			if ( m_vecCurAngles.x < m_vecGoalAngles.x )
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}

		if ( m_iOrientation == 0 )
			SetBoneController( 1, -m_vecCurAngles.x );
		else
			SetBoneController( 1, m_vecCurAngles.x );
		state = 1;
	}

	if ( m_vecCurAngles.y != m_vecGoalAngles.y )
	{
		float flDir  = m_vecGoalAngles.y > m_vecCurAngles.y ? 1 : -1;
		float flDist = fabs( m_vecGoalAngles.y - m_vecCurAngles.y );

		if ( flDist > 180 )
		{
			flDist = 360 - flDist;
			flDir  = -flDir;
		}
		if ( flDist > 30 )
		{
			if ( m_fTurnRate < m_iBaseTurnRate * 10 )
			{
				m_fTurnRate += m_iBaseTurnRate;
			}
		}
		else if ( m_fTurnRate > 45 )
		{
			m_fTurnRate -= m_iBaseTurnRate;
		}
		else
		{
			m_fTurnRate += m_iBaseTurnRate;
		}

		m_vecCurAngles.y += 0.1 * m_fTurnRate * flDir;

		if ( m_vecCurAngles.y < 0 )
			m_vecCurAngles.y += 360;
		else if ( m_vecCurAngles.y >= 360 )
			m_vecCurAngles.y -= 360;

		if ( flDist < ( 0.05 * m_iBaseTurnRate ) )
			m_vecCurAngles.y = m_vecGoalAngles.y;

		if ( m_iOrientation == 0 )
			SetBoneController( 0, m_vecCurAngles.y - pev->angles.y );
		else
			SetBoneController( 0, pev->angles.y - 180 - m_vecCurAngles.y );
		state = 1;
	}

	if ( !state )
		m_fTurnRate = m_iBaseTurnRate;

	return state;
}

int CBaseTurret::Classify( void )
{
	if ( m_iOn || m_iAutoStart )
		return CLASS_MACHINE;
	return CLASS_NONE;
}

//=========================================================
// CTurret implementation
//=========================================================

TYPEDESCRIPTION CTurret::m_SaveData[] =
    {
        DEFINE_FIELD( CTurret, m_iStartSpin, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTurret, CBaseTurret );

LINK_ENTITY_TO_CLASS( monster_turret, CTurret );

#define TURRET_GLOW_SPRITE "sprites/flare3.spr"

void CTurret::Spawn()
{
	Precache();
	SET_MODEL( ENT( pev ), "models/turret.mdl" );
	pev->health     = gSkillData.turretHealth;
	m_HackedGunPos  = Vector( 0, 0, 12.75 );
	m_flMaxSpin     = TURRET_MAXSPIN;
	pev->view_ofs.z = 12.75;

	CBaseTurret::Spawn();

	m_iRetractHeight = 16;
	m_iDeployHeight  = 32;
	m_iMinPitch      = -15;
	UTIL_SetSize( pev, Vector( -32, -32, -m_iRetractHeight ), Vector( 32, 32, m_iRetractHeight ) );

	SetThink( &CTurret::Initialize );

	m_pEyeGlow = CSprite::SpriteCreate( TURRET_GLOW_SPRITE, pev->origin, FALSE );
	m_pEyeGlow->SetTransparency( kRenderGlow, 255, 0, 0, 0, kRenderFxNoDissipation );
	m_pEyeGlow->SetAttachment( edict(), 2 );
	m_eyeBrightness = 0;

	pev->nextthink = gpGlobals->time + 0.3;
}

void CTurret::Precache()
{
	CBaseTurret::Precache();
	PRECACHE_MODEL( "models/turret.mdl" );
	PRECACHE_MODEL( TURRET_GLOW_SPRITE );
}

void CTurret::Shoot( Vector &vecSrc, Vector &vecDirToEnemy )
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, TURRET_RANGE, BULLET_MONSTER_12MM, 1 );
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "turret/tu_fire1.wav", 1, 0.6 );
	pev->effects = pev->effects | EF_MUZZLEFLASH;
}

void CTurret::SpinUpCall( void )
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if ( !m_iSpin )
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
		if ( !m_iStartSpin )
		{
			pev->nextthink = gpGlobals->time + 1.0;
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_spinup.wav", TURRET_MACHINE_VOLUME, ATTN_NORM );
			m_iStartSpin   = 1;
			pev->framerate = 0.1;
		}
		else if ( pev->framerate >= 1.0 )
		{
			pev->nextthink = gpGlobals->time + 0.1;
			EMIT_SOUND( ENT( pev ), CHAN_STATIC, "turret/tu_active2.wav", TURRET_MACHINE_VOLUME, ATTN_NORM );
			SetThink( &CTurret::ActiveThink );
			m_iStartSpin = 0;
			m_iSpin      = 1;
		}
		else
		{
			pev->framerate += 0.075;
		}
	}

	if ( m_iSpin )
	{
		SetThink( &CTurret::ActiveThink );
	}
}

void CTurret::SpinDownCall( void )
{
	if ( m_iSpin )
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
		if ( pev->framerate == 1.0 )
		{
			EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "turret/tu_active2.wav", 0, 0, SND_STOP, 100 );
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "turret/tu_spindown.wav", TURRET_MACHINE_VOLUME, ATTN_NORM );
		}
		pev->framerate -= 0.02;
		if ( pev->framerate <= 0 )
		{
			pev->framerate = 0;
			m_iSpin        = 0;
		}
	}
}

//=========================================================
// CMiniTurret implementation
//=========================================================

LINK_ENTITY_TO_CLASS( monster_miniturret, CMiniTurret );

void CMiniTurret::Spawn()
{
	Precache();
	SET_MODEL( ENT( pev ), "models/miniturret.mdl" );
	pev->health     = gSkillData.miniturretHealth;
	m_HackedGunPos  = Vector( 0, 0, 12.75 );
	m_flMaxSpin     = 0;
	pev->view_ofs.z = 12.75;

	CBaseTurret::Spawn();
	m_iRetractHeight = 16;
	m_iDeployHeight  = 32;
	m_iMinPitch      = -15;
	UTIL_SetSize( pev, Vector( -16, -16, -m_iRetractHeight ), Vector( 16, 16, m_iRetractHeight ) );

	SetThink( &CMiniTurret::Initialize );
	pev->nextthink = gpGlobals->time + 0.3;
}

void CMiniTurret::Precache()
{
	CBaseTurret::Precache();
	PRECACHE_MODEL( "models/miniturret.mdl" );
	PRECACHE_SOUND( "weapons/hks1.wav" );
	PRECACHE_SOUND( "weapons/hks2.wav" );
	PRECACHE_SOUND( "weapons/hks3.wav" );
}

void CMiniTurret::Shoot( Vector &vecSrc, Vector &vecDirToEnemy )
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, TURRET_RANGE, BULLET_MONSTER_9MM, 1 );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM );
		break;
	case 1:
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/hks3.wav", 1, ATTN_NORM );
		break;
	}
	pev->effects = pev->effects | EF_MUZZLEFLASH;
}

//=========================================================
// CSentry implementation
//=========================================================

LINK_ENTITY_TO_CLASS( monster_sentry, CSentry );

void CSentry::Precache()
{
	CBaseTurret::Precache();
	PRECACHE_MODEL( "models/sentry.mdl" );
}

void CSentry::Spawn()
{
	Precache();
	SET_MODEL( ENT( pev ), "models/sentry.mdl" );
	pev->health     = gSkillData.sentryHealth;
	m_HackedGunPos  = Vector( 0, 0, 48 );
	pev->view_ofs.z = 48;
	m_flMaxWait     = 1E6;
	m_flMaxSpin     = 1E6;

	CBaseTurret::Spawn();
	m_iRetractHeight = 64;
	m_iDeployHeight  = 64;
	m_iMinPitch      = -60;
	UTIL_SetSize( pev, Vector( -16, -16, -m_iRetractHeight ), Vector( 16, 16, m_iRetractHeight ) );

	SetTouch( &CSentry::SentryTouch );
	SetThink( &CSentry::Initialize );
	pev->nextthink = gpGlobals->time + 0.3;
}

void CSentry::Shoot( Vector &vecSrc, Vector &vecDirToEnemy )
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, TURRET_RANGE, BULLET_MONSTER_MP5, 1 );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM );
		break;
	case 1:
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/hks3.wav", 1, ATTN_NORM );
		break;
	}
	pev->effects = pev->effects | EF_MUZZLEFLASH;
}

int CSentry::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !pev->takedamage )
		return 0;

	if ( !m_iOn )
	{
		SetThink( &CSentry::Deploy );
		SetUse( NULL );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	pev->health -= flDamage;
	if ( pev->health <= 0 )
	{
		pev->health     = 0;
		pev->takedamage = DAMAGE_NO;
		pev->dmgtime    = gpGlobals->time;

		ClearBits( pev->flags, FL_MONSTER );

		SetUse( NULL );
		SetThink( &CSentry::SentryDeath );
		SUB_UseTargets( this, USE_ON, 0 );
		pev->nextthink = gpGlobals->time + 0.1;

		return 0;
	}

	return 1;
}

void CSentry::SentryTouch( CBaseEntity *pOther )
{
	if ( pOther && ( pOther->IsPlayer() || ( pOther->pev->flags & FL_MONSTER ) ) )
	{
		TakeDamage( pOther->pev, pOther->pev, 0, 0 );
	}
}

void CSentry::SentryDeath( void )
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->deadflag != DEAD_DEAD )
	{
		pev->deadflag = DEAD_DEAD;

		float flRndSound = RANDOM_FLOAT( 0, 1 );

		if ( flRndSound <= 0.33 )
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_die.wav", 1.0, ATTN_NORM );
		else if ( flRndSound <= 0.66 )
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_die2.wav", 1.0, ATTN_NORM );
		else
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "turret/tu_die3.wav", 1.0, ATTN_NORM );

		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "turret/tu_active2.wav", 0, 0, SND_STOP, 100 );

		SetBoneController( 0, 0 );
		SetBoneController( 1, 0 );

		SetTurretAnim( TURRET_ANIM_DIE );

		pev->solid    = SOLID_NOT;
		pev->angles.y = UTIL_AngleMod( pev->angles.y + RANDOM_LONG( 0, 2 ) * 120 );

		EyeOn();
	}

	EyeOff();

	Vector vecSrc, vecAng;
	GetAttachment( 1, vecSrc, vecAng );

	if ( pev->dmgtime + RANDOM_FLOAT( 0, 2 ) > gpGlobals->time )
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x + RANDOM_FLOAT( -16, 16 ) );
		WRITE_COORD( vecSrc.y + RANDOM_FLOAT( -16, 16 ) );
		WRITE_COORD( vecSrc.z - 32 );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 15 );
		WRITE_BYTE( 8 );
		MESSAGE_END();
	}

	if ( pev->dmgtime + RANDOM_FLOAT( 0, 8 ) > gpGlobals->time )
	{
		UTIL_Sparks( vecSrc );
	}

	if ( m_fSequenceFinished && pev->dmgtime + 5 < gpGlobals->time )
	{
		pev->framerate = 0;
		SetThink( NULL );
	}
}
