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
// trigger_gladder_start
//
LINK_ENTITY_TO_CLASS( trigger_gladder_start, CTriggerGladderStart );

void CTriggerGladderStart::Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerGladderStart::StartTouch );
	SetUse( &CTriggerGladderStart::StartUse );
}

void CTriggerGladderStart::StartTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

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
		}
	}
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
	SetTouch( &CTriggerGladderFinish::FinishTouch );
	SetUse( &CTriggerGladderFinish::FinishUse );
}

void CTriggerGladderFinish::FinishTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
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

