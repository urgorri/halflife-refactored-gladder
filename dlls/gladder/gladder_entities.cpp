/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Gladder Entities: Custom Triggers, Area Volumes & Wave Relays Implementation
 *
 ****/

#include "gladder_entities.h"
#include "gladder_rules.h"
#include "gladder_audio.h"
#include "gladder_palette.h"
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
	SetActiveState( true );
	pev->renderamt = 0;
	pev->rendermode = kRenderTransTexture;
	ClearBits( pev->effects, EF_NODRAW );
	SetTouch( &CTriggerGladderStart::StartTouch );
	SetUse( &CTriggerGladderStart::StartUse );

	m_bPlayerCrossed = false;
	m_hActivatingPlayer = nullptr;
	m_vecEntryPos = g_vecZero;
	CreateClipBrush();
}

void CTriggerGladderStart::SetActiveState( bool bActive )
{
	if ( !pev )
		return;

	if ( bActive )
	{
		pev->rendercolor = Vector( GladderPalette::Triggers.r, GladderPalette::Triggers.g, GladderPalette::Triggers.b );
	}
	else
	{
		pev->rendercolor = Vector( GladderPalette::TriggersInactive.r, GladderPalette::TriggersInactive.g, GladderPalette::TriggersInactive.b );
	}
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

	// 1. Players can freely traverse into the gauntlet arena until they completely cross it
	if ( pOther->IsPlayer() )
	{
		// If the player has already crossed and the wave started, prevent retreating back to staging area
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
				int state = pGladderRules->GetWaveManager().GetState();
				if ( state != GLADDER_STATE_WAITING_FOR_START && state != GLADDER_STATE_WAVE_COMPLETED )
				{
					return;
				}

				// Only record entry position and engage thinker when initially starting traversal
				if ( m_hActivatingPlayer == nullptr )
				{
					m_hActivatingPlayer = pOther;
					m_vecEntryPos = pOther->pev->origin;
					SetThink( &CTriggerGladderStart::CrossThink );
					pev->nextthink = gpGlobals ? ( gpGlobals->time + 0.05f ) : 0.05f;
				}
			}
		}
		return;
	}

	// 2. Monsters and pushables are NEVER allowed to enter or cross the start area
	if ( ( pOther->MyMonsterPointer() || ( pOther->pev->flags & FL_MONSTER ) || FClassnameIs( pOther->pev, "func_pushable" ) ) && !pOther->IsPlayer() )
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
}

void CTriggerGladderStart::CrossThink( void )
{
	if ( m_bPlayerCrossed )
		return;

	CBaseEntity *pPlayer = m_hActivatingPlayer;
	if ( !pPlayer || !pPlayer->IsAlive() )
	{
		m_hActivatingPlayer = nullptr;
		SetThink( nullptr );
		return;
	}

	if ( IsEntityInside( pPlayer ) )
	{
		// Player is still traversing inside the start trigger volume; check again on next tick
		pev->nextthink = gpGlobals ? ( gpGlobals->time + 0.05f ) : 0.05f;
		return;
	}

	// Player is no longer inside the trigger volume!
	// Determine whether the player actually crossed from one side through to the other,
	// or merely backed out on the same side they entered.
	Vector vecCenter = ( pev->absmin + pev->absmax ) * 0.5f;
	Vector vecSize = pev->absmax - pev->absmin;
	Vector vecExitPos = pPlayer->pev->origin;
	Vector vecEntryDelta = m_vecEntryPos - vecCenter;
	Vector vecExitDelta = vecExitPos - vecCenter;

	bool bCrossed = false;

	// Crossing requires starting on one side of center and exiting on the opposite side across an axis
	// with a non-trivial traversal distance across the trigger.
	if ( vecSize.x > 0.0f && ( vecEntryDelta.x * vecExitDelta.x < 0.0f ) && ( fabs( vecExitPos.x - m_vecEntryPos.x ) >= vecSize.x * 0.5f ) )
	{
		bCrossed = true;
	}
	else if ( vecSize.y > 0.0f && ( vecEntryDelta.y * vecExitDelta.y < 0.0f ) && ( fabs( vecExitPos.y - m_vecEntryPos.y ) >= vecSize.y * 0.5f ) )
	{
		bCrossed = true;
	}
	else if ( vecSize.z > 0.0f && ( vecEntryDelta.z * vecExitDelta.z < 0.0f ) && ( fabs( vecExitPos.z - m_vecEntryPos.z ) >= vecSize.z * 0.5f ) )
	{
		bCrossed = true;
	}

	if ( bCrossed )
	{
		// Player completely crossed through the start threshold into the gauntlet arena!
		// Seal the passage solid for the player, transition glow to Gray, and trigger wave start.
		m_bPlayerCrossed = true;
		SetSolidForPlayer( true );
		SetActiveState( false );

		if ( g_pGameRules )
		{
			CGladderRules *pGladderRules = dynamic_cast<CGladderRules *>( g_pGameRules );
			if ( pGladderRules )
			{
				int state = pGladderRules->GetWaveManager().GetState();
				if ( state == GLADDER_STATE_WAITING_FOR_START || state == GLADDER_STATE_WAVE_COMPLETED )
				{
					pGladderRules->OnWaveTriggerStart( pPlayer );
					SUB_UseTargets( pPlayer, USE_TOGGLE, 0 );
				}
			}
		}

		m_hActivatingPlayer = nullptr;
		SetThink( nullptr );
	}
	else
	{
		// Player retreated / backed out the same side they entered.
		// Do NOT start wave, do NOT seal door. Reset tracking so they can try again.
		m_hActivatingPlayer = nullptr;
		SetThink( nullptr );
	}
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
	m_vecEntryPos = g_vecZero;
	SetSolidForPlayer( false );
	SetActiveState( true );
	SetThink( nullptr );
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
	SetUnlockedState( false );
	pev->renderamt = 0;
	pev->rendermode = kRenderTransTexture;
	ClearBits( pev->effects, EF_NODRAW );
	SetTouch( &CTriggerGladderFinish::FinishTouch );
	SetUse( &CTriggerGladderFinish::FinishUse );

	CreateClipBrush();
}

void CTriggerGladderFinish::SetUnlockedState( bool bUnlocked )
{
	if ( !pev )
		return;

	if ( bUnlocked )
	{
		pev->rendercolor = Vector( GladderPalette::Triggers.r, GladderPalette::Triggers.g, GladderPalette::Triggers.b );
	}
	else
	{
		pev->rendercolor = Vector( GladderPalette::TriggersInactive.r, GladderPalette::TriggersInactive.g, GladderPalette::TriggersInactive.b );
	}
}

void CTriggerGladderFinish::ResetTrigger( void )
{
	SetUnlockedState( false );
	m_flNextDenySoundTime = 0.0f;
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

void CTriggerGladderFinish::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "cleartarget" ) )
	{
		m_iszClearTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseTrigger::KeyValue( pkvd );
	}
}

void CTriggerGladderFinish::FireClearTarget( CBaseEntity *pActivator )
{
	SetUnlockedState( true );
	if ( !FStringNull( m_iszClearTarget ) )
	{
		FireTargets( STRING( m_iszClearTarget ), pActivator, this, USE_TOGGLE, 0 );
	}
}

void Gladder_FireClearTargets( CBaseEntity *pActivator )
{
	CBaseEntity *pEnt = nullptr;
	while ( ( pEnt = UTIL_FindEntityByClassname( pEnt, "trigger_gladder_finish" ) ) != nullptr )
	{
		CTriggerGladderFinish *pFinish = dynamic_cast<CTriggerGladderFinish *>( pEnt );
		if ( pFinish )
		{
			pFinish->FireClearTarget( pActivator );
		}
		else if ( pEnt->pev )
		{
			pEnt->pev->rendercolor = Vector( GladderPalette::Triggers.r, GladderPalette::Triggers.g, GladderPalette::Triggers.b );
		}
	}
}

void CTriggerGladderFinish::FinishTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	// 1. Players can touch finish trigger to complete wave and teleport to staging
	if ( pOther->IsPlayer() )
	{
		if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
			return;

		CGladderRules *pGladderRules = dynamic_cast<CGladderRules *>( g_pGameRules );
		if ( pGladderRules && !pGladderRules->AreWaveHostilesEliminated() )
		{
			// Wave completion denied: living hostiles remain (SPEC §2, Issue #118)
			int iRemaining = pGladderRules->GetWaveMonstersRemaining();
			float flTime = gpGlobals ? gpGlobals->time : 0.0f;
			if ( flTime >= m_flNextDenySoundTime )
			{
				m_flNextDenySoundTime = flTime + 1.0f;
				GladderAudio::PlayCue( GLADDER_CUE_DENY, pOther );
				ClientPrint( pOther->pev, HUD_PRINTCENTER, UTIL_VarArgs( "OBJECTIVE INCOMPLETE: ELIMINATE ALL HOSTILES FIRST (%d REMAINING)", iRemaining ) );
			}

			// Push player slightly back from trigger bounds so they do not remain stuck in the brush
			Vector vecCenter = ( pev->absmin + pev->absmax ) * 0.5f;
			Vector vecDir = pOther->pev->origin - vecCenter;
			vecDir.z = 0.0f;
			if ( vecDir.Length() > 0.001f )
				vecDir = vecDir.Normalize();
			else
				vecDir = Vector( -1.0f, 0.0f, 0.0f );

			pOther->pev->velocity = vecDir * 250.0f;
			return;
		}

		DoFinish( pOther );
		return;
	}

	// 2. Monsters and pushables are NEVER allowed to enter or cross the finish area
	if ( ( pOther->MyMonsterPointer() || ( pOther->pev->flags & FL_MONSTER ) || FClassnameIs( pOther->pev, "func_pushable" ) ) && !pOther->IsPlayer() )
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
}

void CTriggerGladderFinish::FinishUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && pActivator->IsPlayer() )
	{
		FinishTouch( pActivator );
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

