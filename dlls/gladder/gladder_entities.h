/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Gladder Entities: Custom Triggers, Area Volumes & Wave Relays
 *
 ****/

#pragma once

#ifndef GLADDER_ENTITIES_H
#define GLADDER_ENTITIES_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/triggers_brush.h"

//
// trigger_gladder_start
// Mapper brush trigger placed at Point A (staging area).
// Fires wave start on demand when the player crosses it.
//
class CTriggerGladderStart : public CBaseTrigger
{
  public:
	void Spawn( void );
	void EXPORT StartTouch( CBaseEntity *pOther );
	void EXPORT StartUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

//
// trigger_gladder_finish
// Mapper brush trigger placed at Point B (extraction / finish line).
// Completes current wave, plays reward acoustic cue, cleans up entities,
// and teleports the player back to Point A target destination preserving inventory.
//
class CTriggerGladderFinish : public CBaseTrigger
{
  public:
	void Spawn( void );
	void EXPORT FinishTouch( CBaseEntity *pOther );
	void EXPORT FinishUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

  private:
	void DoFinish( CBaseEntity *pActivator );
};

//
// trigger_gladder_area
// Bounding volume brush entity specifying 3D bounds for spatial indexing.
// KeyValues: netname / areaname (string), areaid (int)
//
class CTriggerGladderArea : public CBaseEntity
{
  public:
	void Spawn( void )
	{
		pev->solid    = SOLID_TRIGGER;
		pev->movetype = MOVETYPE_NONE;

		if ( pev->model )
		{
			SET_MODEL( ENT( pev ), STRING( pev->model ) );
		}
		else
		{
			UTIL_SetSize( pev, pev->mins, pev->maxs );
		}

		SetBits( pev->effects, EF_NODRAW );
		SetObjectCollisionBox();

		// Fallback to pev->netname if areaname was not set via custom KeyValue
		if ( m_szAreaName[0] == '\0' && !FStringNull( pev->netname ) )
		{
			strncpy( m_szAreaName, STRING( pev->netname ), sizeof( m_szAreaName ) - 1 );
			m_szAreaName[sizeof( m_szAreaName ) - 1] = '\0';
		}
	}

	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "areaname" ) || FStrEq( pkvd->szKeyName, "netname" ) )
		{
			strncpy( m_szAreaName, pkvd->szValue, sizeof( m_szAreaName ) - 1 );
			m_szAreaName[sizeof( m_szAreaName ) - 1] = '\0';
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "areaid" ) )
		{
			m_iAreaId      = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
		{
			CBaseEntity::KeyValue( pkvd );
		}
	}

	const char *GetAreaName( void ) const { return m_szAreaName; }
	int GetAreaId( void ) const { return m_iAreaId; }
	Vector GetMins( void ) const
	{
		if ( pev->absmin.x < pev->absmax.x )
			return pev->absmin;
		if ( pev->mins.x < pev->maxs.x )
			return pev->origin + pev->mins;
		return pev->origin;
	}
	Vector GetMaxs( void ) const
	{
		if ( pev->absmin.x < pev->absmax.x )
			return pev->absmax;
		if ( pev->mins.x < pev->maxs.x )
			return pev->origin + pev->maxs;
		return pev->origin;
	}

  private:
	char m_szAreaName[64];
	int m_iAreaId;
};

//
// gladder_wave_relay
// Relay firing specific target outputs on wave events:
// wave_event:
//   0 = on_wave_start
//   1 = on_wave_win / completed
//   2 = on_match_over / victory
//   3 = on_player_death / defeat
//
class CGladderWaveRelay : public CBaseDelay
{
  public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void FireWaveEvent( int iEvent, CBaseEntity *pActivator );

	int GetEventType( void ) const { return m_iWaveEvent; }

  private:
	int m_iWaveEvent;
};

void Gladder_FireWaveRelays( int iEvent, CBaseEntity *pActivator );

#endif // GLADDER_ENTITIES_H
