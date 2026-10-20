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
#include "systems/triggers_point.h"

//
// gladder_trigger_clip
// Companion solid BSP brush created for wave start and finish triggers.
// Provides geometric clipping for monsters and pushables, and dynamically for players.
//
class CGladderTriggerClip : public CBaseEntity
{
  public:
	void Spawn( void )
	{
		pev->angles   = g_vecZero;
		pev->movetype = MOVETYPE_PUSH;
		pev->solid    = SOLID_BSP;
		if ( pev->model )
		{
			SET_MODEL( ENT( pev ), STRING( pev->model ) );
		}
		pev->effects = EF_NODRAW;
		pev->flags |= FL_WORLDBRUSH;
		pev->flags |= FL_MONSTERCLIP;
	}

	void SetSolidForPlayer( bool bSolid )
	{
		if ( bSolid )
		{
			pev->flags &= ~FL_MONSTERCLIP;
			pev->solid = SOLID_BSP;
		}
		else
		{
			pev->flags |= FL_MONSTERCLIP;
			pev->solid = SOLID_BSP;
		}
		UTIL_SetOrigin( pev, pev->origin );
	}

	bool IsSolidForPlayer( void ) const { return ( pev->flags & FL_MONSTERCLIP ) == 0; }
};

//
// trigger_gladder_start
// Mapper brush trigger placed at Point A (staging area).
// Fires wave start on demand when the player crosses it.
// Blocks monsters and pushables permanently, and becomes solid for the player
// once traversed so the player cannot retreat after starting the wave.
//
class CTriggerGladderStart : public CBaseTrigger
{
  public:
	void Spawn( void );
	void EXPORT StartTouch( CBaseEntity *pOther );
	void EXPORT StartUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT CrossThink( void );

	void ResetTrigger( void );
	void SetActiveState( bool bActive );
	void SetSolidForPlayer( bool bSolid );
	bool IsPlayerCrossed( void ) const { return m_bPlayerCrossed; }
	CGladderTriggerClip *GetClip( void ) { return (CGladderTriggerClip *)( (CBaseEntity *)m_hClip ); }

  private:
	void CreateClipBrush( void );
	bool IsEntityInside( CBaseEntity *pEntity );

	EHANDLE m_hClip;
	EHANDLE m_hActivatingPlayer;
	Vector m_vecEntryPos;
	bool m_bPlayerCrossed;
};

//
// trigger_gladder_finish
// Mapper brush trigger placed at Point B (extraction / finish line).
// Completes current wave, plays reward acoustic cue, cleans up entities,
// and teleports the player back to Point A target destination preserving inventory.
// Blocks monsters and pushables permanently while allowing the player to enter.
//
class CTriggerGladderFinish : public CBaseTrigger
{
  public:
	CTriggerGladderFinish() : m_iszClearTarget( 0 ), m_flNextDenySoundTime( 0.0f ) {}
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT FinishTouch( CBaseEntity *pOther );
	void EXPORT FinishUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	CGladderTriggerClip *GetClip( void ) { return (CGladderTriggerClip *)( (CBaseEntity *)m_hClip ); }
	void SetUnlockedState( bool bUnlocked );
	void ResetTrigger( void );
	void FireClearTarget( CBaseEntity *pActivator );
	const char *GetClearTarget( void ) const { return STRING( m_iszClearTarget ); }

  private:
	void DoFinish( CBaseEntity *pActivator );
	void CreateClipBrush( void );

	EHANDLE m_hClip;
	string_t m_iszClearTarget;
	float m_flNextDenySoundTime;
};

void Gladder_FireClearTargets( CBaseEntity *pActivator );

//
// trigger_gladder_area
// Bounding volume brush entity specifying 3D bounds for spatial indexing.
// KeyValue: netname ("Area Name")
// Numeric area ID is assigned automatically and incrementally by the engine (SPEC §4.2, Issue #102).
//
class CTriggerGladderArea : public CBaseEntity
{
  public:
	CTriggerGladderArea()
	    : m_iAreaId( 0 )
	{
		m_szAreaName[0] = '\0';
	}

	void Spawn( void )
	{
		pev->solid    = SOLID_NOT;
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

		if ( m_szAreaName[0] == '\0' && !FStringNull( pev->netname ) )
		{
			strncpy( m_szAreaName, STRING( pev->netname ), sizeof( m_szAreaName ) - 1 );
			m_szAreaName[sizeof( m_szAreaName ) - 1] = '\0';
		}
	}

	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "netname" ) )
		{
			strncpy( m_szAreaName, pkvd->szValue, sizeof( m_szAreaName ) - 1 );
			m_szAreaName[sizeof( m_szAreaName ) - 1] = '\0';
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "areaname" ) || FStrEq( pkvd->szKeyName, "areaid" ) )
		{
			// Legacy keys ignored without errors (SPEC §4.2, Issue #102)
			pkvd->fHandled = TRUE;
		}
		else
		{
			CBaseEntity::KeyValue( pkvd );
		}
	}

	const char *GetAreaName( void ) const { return m_szAreaName; }
	int GetAreaId( void ) const { return m_iAreaId; }
	void SetAreaId( int id ) { m_iAreaId = id; }
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

//
// gladder_camera
// Mapper point entity (subclass of CTriggerCamera) providing
// dynamic panoramic camera vantage points for the post-match summary screen (SPEC §6, §9.2).
//
class CGladderCamera : public CTriggerCamera
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
};

#endif // GLADDER_ENTITIES_H
