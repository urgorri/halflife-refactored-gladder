#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "core/saverestore.h"
#include "world/trains.h"
#include "gameplay/gamerules.h"
#include "systems/triggers_point.h"
#include "systems/triggers_brush.h"

//=========================================================
// CAutoTrigger
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_auto, CAutoTrigger );

TYPEDESCRIPTION CAutoTrigger::m_SaveData[] =
    {
        DEFINE_FIELD( CAutoTrigger, m_globalstate, FIELD_STRING ),
        DEFINE_FIELD( CAutoTrigger, triggerType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CAutoTrigger, CBaseDelay );

void CAutoTrigger::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "globalstate" ) )
	{
		m_globalstate  = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "triggerstate" ) )
	{
		int type = atoi( pkvd->szValue );
		switch ( type )
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CAutoTrigger::Spawn( void )
{
	Precache();
}

void CAutoTrigger::Precache( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
}

void CAutoTrigger::Think( void )
{
	if ( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
	{
		SUB_UseTargets( this, triggerType, 0 );
		if ( pev->spawnflags & SF_AUTO_FIREONCE )
			UTIL_Remove( this );
	}
}

//=========================================================
// CTriggerRelay
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_relay, CTriggerRelay );

TYPEDESCRIPTION CTriggerRelay::m_SaveData[] =
    {
        DEFINE_FIELD( CTriggerRelay, triggerType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTriggerRelay, CBaseDelay );

void CTriggerRelay::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "triggerstate" ) )
	{
		int type = atoi( pkvd->szValue );
		switch ( type )
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerRelay::Spawn( void )
{
}

void CTriggerRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SUB_UseTargets( this, triggerType, 0 );
	if ( pev->spawnflags & SF_RELAY_FIREONCE )
		UTIL_Remove( this );
}

//=========================================================
// CFireAndDie
//=========================================================
LINK_ENTITY_TO_CLASS( fireanddie, CFireAndDie );

void CFireAndDie::Spawn( void )
{
	pev->classname = MAKE_STRING( "fireanddie" );
}

void CFireAndDie::Precache( void )
{
	pev->nextthink = gpGlobals->time + m_flDelay;
}

void CFireAndDie::Think( void )
{
	SUB_UseTargets( this, USE_TOGGLE, 0 );
	UTIL_Remove( this );
}

//=========================================================
// CTriggerChangeTarget
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_changetarget, CTriggerChangeTarget );

TYPEDESCRIPTION CTriggerChangeTarget::m_SaveData[] =
    {
        DEFINE_FIELD( CTriggerChangeTarget, m_iszNewTarget, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CTriggerChangeTarget, CBaseDelay );

void CTriggerChangeTarget::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszNewTarget" ) )
	{
		m_iszNewTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeTarget::Spawn( void )
{
}

void CTriggerChangeTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = UTIL_FindEntityByString( NULL, "targetname", STRING( pev->target ) );

	if ( pTarget )
	{
		pTarget->pev->target   = m_iszNewTarget;
		CBaseMonster *pMonster = pTarget->MyMonsterPointer();
		if ( pMonster )
		{
			pMonster->m_pGoalEnt = NULL;
		}
	}
}

//=========================================================
// CMultiManager
//=========================================================
LINK_ENTITY_TO_CLASS( multi_manager, CMultiManager );

TYPEDESCRIPTION CMultiManager::m_SaveData[] =
    {
        DEFINE_FIELD( CMultiManager, m_cTargets, FIELD_INTEGER ),
        DEFINE_FIELD( CMultiManager, m_index, FIELD_INTEGER ),
        DEFINE_FIELD( CMultiManager, m_startTime, FIELD_TIME ),
        DEFINE_ARRAY( CMultiManager, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
        DEFINE_ARRAY( CMultiManager, m_flTargetDelay, FIELD_FLOAT, MAX_MULTI_TARGETS ),
};

IMPLEMENT_SAVERESTORE( CMultiManager, CBaseToggle );

void CMultiManager::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "wait" ) )
	{
		m_flWait       = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		if ( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];

			UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );
			m_iTargetName[m_cTargets]   = ALLOC_STRING( tmp );
			m_flTargetDelay[m_cTargets] = atof( pkvd->szValue );
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

void CMultiManager::Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetUse( &CMultiManager::ManagerUse );
	SetThink( &CMultiManager::ManagerThink );

	int swapped = 1;

	while ( swapped )
	{
		swapped = 0;
		for ( int i = 1; i < m_cTargets; i++ )
		{
			if ( m_flTargetDelay[i] < m_flTargetDelay[i - 1] )
			{
				int name               = m_iTargetName[i];
				float delay            = m_flTargetDelay[i];
				m_iTargetName[i]       = m_iTargetName[i - 1];
				m_flTargetDelay[i]     = m_flTargetDelay[i - 1];
				m_iTargetName[i - 1]   = name;
				m_flTargetDelay[i - 1] = delay;
				swapped                = 1;
			}
		}
	}
}

BOOL CMultiManager::HasTarget( string_t targetname )
{
	for ( int i = 0; i < m_cTargets; i++ )
		if ( FStrEq( STRING( targetname ), STRING( m_iTargetName[i] ) ) )
			return TRUE;

	return FALSE;
}

void CMultiManager::ManagerThink( void )
{
	float time = gpGlobals->time - m_startTime;
	while ( m_index < m_cTargets && m_flTargetDelay[m_index] <= time )
	{
		FireTargets( STRING( m_iTargetName[m_index] ), m_hActivator, this, USE_TOGGLE, 0 );
		m_index++;
	}

	if ( m_index >= m_cTargets )
	{
		SetThink( NULL );
		if ( IsClone() )
		{
			UTIL_Remove( this );
			return;
		}
		SetUse( &CMultiManager::ManagerUse );
	}
	else
		pev->nextthink = m_startTime + m_flTargetDelay[m_index];
}

CMultiManager *CMultiManager::Clone( void )
{
	CMultiManager *pMulti = GetClassPtr( (CMultiManager *)NULL );

	edict_t *pEdict = pMulti->pev->pContainingEntity;
	memcpy( pMulti->pev, pev, sizeof( *pev ) );
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= SF_MULTIMAN_CLONE;
	pMulti->m_cTargets = m_cTargets;
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ) );
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ) );

	return pMulti;
}

void CMultiManager::ManagerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldClone() )
	{
		CMultiManager *pClone = Clone();
		pClone->ManagerUse( pActivator, pCaller, useType, value );
		return;
	}

	m_hActivator = pActivator;
	m_index      = 0;
	m_startTime  = gpGlobals->time;

	SetUse( NULL );

	SetThink( &CMultiManager::ManagerThink );
	pev->nextthink = gpGlobals->time;
}

#if _DEBUG
void CMultiManager::ManagerReport( void )
{
	for ( int cIndex = 0; cIndex < m_cTargets; cIndex++ )
	{
		ALERT( at_console, "%s %f\n", STRING( m_iTargetName[cIndex] ), m_flTargetDelay[cIndex] );
	}
}
#endif

//=========================================================
// CTriggerCamera
//=========================================================
LINK_ENTITY_TO_CLASS( trigger_camera, CTriggerCamera );

TYPEDESCRIPTION CTriggerCamera::m_SaveData[] =
    {
        DEFINE_FIELD( CTriggerCamera, m_hPlayer, FIELD_EHANDLE ),
        DEFINE_FIELD( CTriggerCamera, m_hTarget, FIELD_EHANDLE ),
        DEFINE_FIELD( CTriggerCamera, m_pentPath, FIELD_CLASSPTR ),
        DEFINE_FIELD( CTriggerCamera, m_sPath, FIELD_STRING ),
        DEFINE_FIELD( CTriggerCamera, m_flWait, FIELD_FLOAT ),
        DEFINE_FIELD( CTriggerCamera, m_flReturnTime, FIELD_TIME ),
        DEFINE_FIELD( CTriggerCamera, m_flStopTime, FIELD_TIME ),
        DEFINE_FIELD( CTriggerCamera, m_moveDistance, FIELD_FLOAT ),
        DEFINE_FIELD( CTriggerCamera, m_targetSpeed, FIELD_FLOAT ),
        DEFINE_FIELD( CTriggerCamera, m_initialSpeed, FIELD_FLOAT ),
        DEFINE_FIELD( CTriggerCamera, m_acceleration, FIELD_FLOAT ),
        DEFINE_FIELD( CTriggerCamera, m_deceleration, FIELD_FLOAT ),
        DEFINE_FIELD( CTriggerCamera, m_state, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTriggerCamera, CBaseDelay );

void CTriggerCamera::Spawn( void )
{
	pev->movetype   = MOVETYPE_NOCLIP;
	pev->solid      = SOLID_NOT;
	pev->renderamt  = 0;
	pev->rendermode = kRenderTransTexture;

	m_initialSpeed = pev->speed;
	if ( m_acceleration == 0 )
		m_acceleration = 500;
	if ( m_deceleration == 0 )
		m_deceleration = 500;
}

void CTriggerCamera::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "wait" ) )
	{
		m_flWait       = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "moveto" ) )
	{
		m_sPath        = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "acceleration" ) )
	{
		m_acceleration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "deceleration" ) )
	{
		m_deceleration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerCamera::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_state ) )
		return;

	m_state = !m_state;
	if ( m_state == 0 )
	{
		m_flReturnTime = gpGlobals->time;
		return;
	}
	if ( !pActivator || !pActivator->IsPlayer() )
	{
		pActivator = CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ) );
	}

	m_hPlayer = pActivator;

	m_flReturnTime = gpGlobals->time + m_flWait;
	pev->speed     = m_initialSpeed;
	m_targetSpeed  = m_initialSpeed;

	if ( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TARGET ) )
	{
		m_hTarget = m_hPlayer;
	}
	else
	{
		m_hTarget = GetNextTarget();
	}

	if ( m_hTarget == NULL )
		return;

	if ( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL ) )
	{
		( (CBasePlayer *)pActivator )->EnableControl( FALSE );
	}

	if ( m_sPath )
	{
		m_pentPath = Instance( FIND_ENTITY_BY_TARGETNAME( NULL, STRING( m_sPath ) ) );
	}
	else
	{
		m_pentPath = NULL;
	}

	m_flStopTime = gpGlobals->time;
	if ( m_pentPath )
	{
		if ( m_pentPath->pev->speed != 0 )
			m_targetSpeed = m_pentPath->pev->speed;

		m_flStopTime += m_pentPath->GetDelay();
	}

	if ( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_POSITION ) )
	{
		UTIL_SetOrigin( pev, pActivator->pev->origin + pActivator->pev->view_ofs );
		pev->angles.x = -pActivator->pev->angles.x;
		pev->angles.y = pActivator->pev->angles.y;
		pev->angles.z = 0;
		pev->velocity = pActivator->pev->velocity;
	}
	else
	{
		pev->velocity = Vector( 0, 0, 0 );
	}

	SET_VIEW( pActivator->edict(), edict() );
	SET_MODEL( ENT( pev ), STRING( pActivator->pev->model ) );

	SetThink( &CTriggerCamera::FollowTarget );
	pev->nextthink = gpGlobals->time;

	m_moveDistance = 0;
	Move();
}

void CTriggerCamera::FollowTarget()
{
	if ( m_hPlayer == NULL )
		return;

	if ( m_hTarget == NULL || m_flReturnTime < gpGlobals->time )
	{
		if ( m_hPlayer->IsAlive() )
		{
			SET_VIEW( m_hPlayer->edict(), m_hPlayer->edict() );
			( (CBasePlayer *)( (CBaseEntity *)m_hPlayer ) )->EnableControl( TRUE );
		}
		SUB_UseTargets( this, USE_TOGGLE, 0 );
		pev->avelocity = Vector( 0, 0, 0 );
		m_state        = 0;
		return;
	}

	Vector vecGoal = UTIL_VecToAngles( m_hTarget->pev->origin - pev->origin );
	vecGoal.x      = -vecGoal.x;

	if ( pev->angles.y > 360 )
		pev->angles.y -= 360;

	if ( pev->angles.y < 0 )
		pev->angles.y += 360;

	float dx = vecGoal.x - pev->angles.x;
	float dy = vecGoal.y - pev->angles.y;

	if ( dx < -180 )
		dx += 360;
	if ( dx > 180 )
		dx = dx - 360;

	if ( dy < -180 )
		dy += 360;
	if ( dy > 180 )
		dy = dy - 360;

	pev->avelocity.x = dx * 40 * gpGlobals->frametime;
	pev->avelocity.y = dy * 40 * gpGlobals->frametime;

	if ( !( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL ) ) )
	{
		pev->velocity = pev->velocity * 0.8;
		if ( pev->velocity.Length() < 10.0 )
			pev->velocity = g_vecZero;
	}

	pev->nextthink = gpGlobals->time;

	Move();
}

void CTriggerCamera::Move()
{
	if ( !m_pentPath )
		return;

	m_moveDistance -= pev->speed * gpGlobals->frametime;

	if ( m_moveDistance <= 0 )
	{
		if ( m_pentPath->pev->message )
		{
			FireTargets( STRING( m_pentPath->pev->message ), this, this, USE_TOGGLE, 0 );
			if ( FBitSet( m_pentPath->pev->spawnflags, SF_CORNER_FIREONCE ) )
				m_pentPath->pev->message = 0;
		}
		m_pentPath = m_pentPath->GetNextTarget();

		if ( !m_pentPath )
		{
			pev->velocity = g_vecZero;
		}
		else
		{
			if ( m_pentPath->pev->speed != 0 )
				m_targetSpeed = m_pentPath->pev->speed;

			Vector delta   = m_pentPath->pev->origin - pev->origin;
			m_moveDistance = delta.Length();
			pev->movedir   = delta.Normalize();
			m_flStopTime   = gpGlobals->time + m_pentPath->GetDelay();
		}
	}

	if ( m_flStopTime > gpGlobals->time )
		pev->speed = UTIL_Approach( 0, pev->speed, m_deceleration * gpGlobals->frametime );
	else
		pev->speed = UTIL_Approach( m_targetSpeed, pev->speed, m_acceleration * gpGlobals->frametime );

	float fraction = 2 * gpGlobals->frametime;
	pev->velocity  = ( ( pev->movedir * pev->speed ) * fraction ) + ( pev->velocity * ( 1 - fraction ) );
}

//=========================================================
// CRenderFxManager
//=========================================================
LINK_ENTITY_TO_CLASS( env_render, CRenderFxManager );

void CRenderFxManager::Spawn( void )
{
	pev->solid = SOLID_NOT;
}

void CRenderFxManager::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !FStringNull( pev->target ) )
	{
		edict_t *pentTarget = NULL;
		while ( 1 )
		{
			pentTarget = FIND_ENTITY_BY_TARGETNAME( pentTarget, STRING( pev->target ) );
			if ( FNullEnt( pentTarget ) )
				break;

			entvars_t *pevTarget = VARS( pentTarget );
			if ( !pevTarget || ( pevTarget->flags & FL_KILLME ) )
				continue;

			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKFX ) )
				pevTarget->renderfx = pev->renderfx;
			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
				pevTarget->renderamt = pev->renderamt;
			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKMODE ) )
				pevTarget->rendermode = pev->rendermode;
			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKCOLOR ) )
				pevTarget->rendercolor = pev->rendercolor;
		}
	}
}

//=========================================================
// CTargetCDAudio
//=========================================================
LINK_ENTITY_TO_CLASS( target_cdaudio, CTargetCDAudio );

void CTargetCDAudio::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "radius" ) )
	{
		pev->scale     = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTargetCDAudio::Spawn( void )
{
	pev->solid    = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if ( pev->scale > 0 )
		pev->nextthink = gpGlobals->time + 1.0;
}

void CTargetCDAudio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Play();
}

void CTargetCDAudio::Think( void )
{
	edict_t *pClient = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	if ( !pClient )
		return;

	pev->nextthink = gpGlobals->time + 0.5;

	if ( ( pClient->v.origin - pev->origin ).Length() <= pev->scale )
		Play();
}

void CTargetCDAudio::Play( void )
{
	PlayCDTrack( (int)pev->health );
	UTIL_Remove( this );
}
