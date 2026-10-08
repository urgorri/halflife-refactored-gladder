/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Gladder Entities: Custom Triggers, Area Volumes & Wave Relays Implementation
 *
 ****/

#include "gladder_entities.h"
#include "gladder_rules.h"
#include <cstdio>
#include <cstdlib>

//
// gladder_trigger_clip
//
LINK_ENTITY_TO_CLASS( gladder_trigger_clip, CGladderTriggerClip );

//
// trigger_gladder_start
//
LINK_ENTITY_TO_CLASS( trigger_gladder_start, CTriggerGladderStart );

void CTriggerGladderStart::Spawn( void )
{
	InitTrigger();
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector( 255, 32, 32 );
	pev->renderamt = 16;
	pev->rendermode = kRenderTransTexture;
	ClearBits( pev->effects, EF_NODRAW );
	SetTouch( &CTriggerGladderStart::StartTouch );
	SetUse( &CTriggerGladderStart::StartUse );

	m_bPlayerCrossed = false;
	m_hActivatingPlayer = nullptr;
	CreateClipBrush();
}

void CTriggerGladderStart::CreateClipBrush( void )
{
	if ( !pev->model )
		return;

	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( "gladder_trigger_clip" ) );
	if ( !FNullEnt( pent ) )
	{
		CBaseEntity *pClip = Instance( pent );
		if ( pClip )
		{
			pClip->pev->model = pev->model;
			pClip->pev->origin = pev->origin;
			pClip->pev->angles = pev->angles;
			DispatchSpawn( pClip->edict() );
			m_hClip = pClip;
		}
	}
}

bool CTriggerGladderStart::IsEntityInside( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->pev )
		return false;

	if ( pEntity->pev->absmax.x < pev->absmin.x || pEntity->pev->absmin.x > pev->absmax.x )
		return false;
	if ( pEntity->pev->absmax.y < pev->absmin.y || pEntity->pev->absmin.y > pev->absmax.y )
		return false;
	if ( pEntity->pev->absmax.z < pev->absmin.z || pEntity->pev->absmin.z > pev->absmax.z )
		return false;

	return true;
}

void CTriggerGladderStart::StartTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	// 1. Monsters and pushables are NEVER allowed to enter or cross the start area
	if ( pOther->MyMonsterPointer() || ( pOther->pev->flags & FL_MONSTER ) || FClassnameIs( pOther->pev, "func_pushable" ) )
	{
		Vector vecCenter = ( pev->absmin + pev->absmax ) * 0.5f;
		Vector vecDir = pOther->pev->origin - vecCenter;
		vecDir.z = 0.0f;
		if ( vecDir.Length() > 0.001f )
			vecDir = vecDir.Normalize();
		else
			vecDir = Vector( 1.0f, 0.0f, 0.0f );

		pOther->pev->velocity = vecDir * 250.0f;
		return;
	}

	if ( !pOther->IsPlayer() )
		return;

	// 2. If the player has already crossed and the wave started, prevent retreating back to staging area
	if ( m_bPlayerCrossed )
	{
		Vector vecCenter = ( pev->absmin + pev->absmax ) * 0.5f;
		Vector vecDir = pOther->pev->origin - vecCenter;
		vecDir.z = 0.0f;
		if ( vecDir.Length() > 0.001f )
			vecDir = vecDir.Normalize();
		else
			vecDir = Vector( 1.0f, 0.0f, 0.0f );

		pOther->pev->velocity = vecDir * 200.0f;
		return;
	}

	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	if ( g_pGameRules )
	{
		CGladderRules *pGladderRules = dynamic_cast<CGladderRules *>( g_pGameRules );
		if ( pGladderRules )
		{
			// Only trigger if waiting for wave start
			if ( pGladderRules->GetWaveManager().GetState() != GLADDER_STATE_WAITING_FOR_START )
				return;

			pGladderRules->OnWaveTriggerStart( pOther );
			SUB_UseTargets( pOther, USE_TOGGLE, 0 );

			// Track activating player to monitor when they finish traversing the start threshold
			m_hActivatingPlayer = pOther;
			SetThink( &CTriggerGladderStart::CrossThink );
			pev->nextthink = gpGlobals ? ( gpGlobals->time + 0.05f ) : 0.05f;
		}
	}
}

void CTriggerGladderStart::CrossThink( void )
{
	if ( m_bPlayerCrossed )
		return;

	CBaseEntity *pPlayer = m_hActivatingPlayer;
	if ( !pPlayer || !IsEntityInside( pPlayer ) )
	{
		// Player has finished crossing through the start volume into the gauntlet arena!
		m_bPlayerCrossed = true;
		SetSolidForPlayer( true );
		return;
	}

	// Player is still crossing through; check again on next frame
	pev->nextthink = gpGlobals ? ( gpGlobals->time + 0.05f ) : 0.05f;
}

void CTriggerGladderStart::SetSolidForPlayer( bool bSolid )
{
	if ( m_hClip )
	{
		CGladderTriggerClip *pClip = (CGladderTriggerClip *)( (CBaseEntity *)m_hClip );
		if ( pClip )
		{
			pClip->SetSolidForPlayer( bSolid );
		}
	}
}

void CTriggerGladderStart::ResetTrigger( void )
{
	m_bPlayerCrossed = false;
	m_hActivatingPlayer = nullptr;
	SetSolidForPlayer( false );
}

void CTriggerGladderStart::StartUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && pActivator->IsPlayer() )
	{
		StartTouch( pActivator );
	}
}

//
// trigger_gladder_finish
//
LINK_ENTITY_TO_CLASS( trigger_gladder_finish, CTriggerGladderFinish );

void CTriggerGladderFinish::Spawn( void )
{
	InitTrigger();
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector( 255, 32, 32 );
	pev->renderamt = 16;
	pev->rendermode = kRenderTransTexture;
	ClearBits( pev->effects, EF_NODRAW );
	SetTouch( &CTriggerGladderFinish::FinishTouch );
	SetUse( &CTriggerGladderFinish::FinishUse );

	CreateClipBrush();
}

void CTriggerGladderFinish::CreateClipBrush( void )
{
	if ( !pev->model )
		return;

	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( "gladder_trigger_clip" ) );
	if ( !FNullEnt( pent ) )
	{
		CBaseEntity *pClip = Instance( pent );
		if ( pClip )
		{
			pClip->pev->model = pev->model;
			pClip->pev->origin = pev->origin;
			pClip->pev->angles = pev->angles;
			DispatchSpawn( pClip->edict() );
			m_hClip = pClip;
		}
	}
}

void CTriggerGladderFinish::FinishTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	// 1. Monsters and pushables are NEVER allowed to enter or cross the finish area
	if ( pOther->MyMonsterPointer() || ( pOther->pev->flags & FL_MONSTER ) || FClassnameIs( pOther->pev, "func_pushable" ) )
	{
		Vector vecCenter = ( pev->absmin + pev->absmax ) * 0.5f;
		Vector vecDir = pOther->pev->origin - vecCenter;
		vecDir.z = 0.0f;
		if ( vecDir.Length() > 0.001f )
			vecDir = vecDir.Normalize();
		else
			vecDir = Vector( 1.0f, 0.0f, 0.0f );

		pOther->pev->velocity = vecDir * 250.0f;
		return;
	}

	if ( !pOther->IsPlayer() )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	DoFinish( pOther );
}

void CTriggerGladderFinish::FinishUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && pActivator->IsPlayer() )
	{
		DoFinish( pActivator );
	}
}

void CTriggerGladderFinish::DoFinish( CBaseEntity *pActivator )
{
	// 1. Notify Gladder game rules
	if ( g_pGameRules )
	{
		CGladderRules *pGladderRules = dynamic_cast<CGladderRules *>( g_pGameRules );
		if ( pGladderRules )
		{
			// Only process if wave was active
			if ( !pGladderRules->GetWaveManager().IsWaveActive() )
			{
				return;
			}
			pGladderRules->OnWaveTriggerFinish( pActivator );
		}
	}

	// 2. Fire targets (relays, doors, sound effects)
	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );

	// 3. Teleport player back to Point A target destination preserving inventory
	if ( !FStringNull( pev->target ) )
	{
		edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
		if ( !FNullEnt( pentTarget ) )
		{
			entvars_t *pevToucher = pActivator->pev;
			Vector vecDest        = VARS( pentTarget )->origin;

			if ( pActivator->IsPlayer() )
			{
				vecDest.z -= pevToucher->mins.z;
			}
			vecDest.z += 1.0f;

			pevToucher->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin( pevToucher, vecDest );
			pevToucher->angles = VARS( pentTarget )->angles;

			if ( pActivator->IsPlayer() )
			{
				pevToucher->v_angle = VARS( pentTarget )->angles;
			}

			pevToucher->fixangle     = TRUE;
			pevToucher->velocity     = g_vecZero;
			pevToucher->basevelocity = g_vecZero;
		}
	}
}

//
// trigger_gladder_area
//
LINK_ENTITY_TO_CLASS( trigger_gladder_area, CTriggerGladderArea );

//
// gladder_wave_relay
//
LINK_ENTITY_TO_CLASS( gladder_wave_relay, CGladderWaveRelay );

void CGladderWaveRelay::Spawn( void )
{
	pev->solid    = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
}

void CGladderWaveRelay::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "wave_event" ) )
	{
		m_iWaveEvent   = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue( pkvd );
	}
}

void CGladderWaveRelay::FireWaveEvent( int iEvent, CBaseEntity *pActivator )
{
	if ( m_iWaveEvent == iEvent )
	{
		SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
	}
}

void Gladder_FireWaveRelays( int iEvent, CBaseEntity *pActivator )
{
	CBaseEntity *pEnt = nullptr;
	while ( ( pEnt = UTIL_FindEntityByClassname( pEnt, "gladder_wave_relay" ) ) != nullptr )
	{
		CGladderWaveRelay *pRelay = dynamic_cast<CGladderWaveRelay *>( pEnt );
		if ( pRelay )
		{
			pRelay->FireWaveEvent( iEvent, pActivator );
		}
	}
}

