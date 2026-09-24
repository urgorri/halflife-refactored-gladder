#include "systems/chargers.h"
#include "weapons/weapon_base.h"

TYPEDESCRIPTION CWallHealth::m_SaveData[] =
{
	DEFINE_FIELD( CWallHealth, m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( CWallHealth, m_iReactivate, FIELD_INTEGER ),
	DEFINE_FIELD( CWallHealth, m_iJuice, FIELD_INTEGER ),
	DEFINE_FIELD( CWallHealth, m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( CWallHealth, m_flSoundTime, FIELD_TIME ),
};

int CWallHealth::Save( CSave &save )
{
	if ( !CBaseEntity::Save( save ) )
		return 0;
	return save.WriteFields( "CWallHealth", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CWallHealth::Restore( CRestore &restore )
{
	if ( !CBaseEntity::Restore( restore ) )
		return 0;
	if ( restore.ReadFields( "CWallHealth", this, m_SaveData, ARRAYSIZE( m_SaveData ) ) )
		return 1;
	return restore.ReadFields( "CBaseWallCharger", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

TYPEDESCRIPTION CWallRecharge::m_SaveData[] =
{
	DEFINE_FIELD( CWallRecharge, m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( CWallRecharge, m_iReactivate, FIELD_INTEGER ),
	DEFINE_FIELD( CWallRecharge, m_iJuice, FIELD_INTEGER ),
	DEFINE_FIELD( CWallRecharge, m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( CWallRecharge, m_flSoundTime, FIELD_TIME ),
};

int CWallRecharge::Save( CSave &save )
{
	if ( !CBaseEntity::Save( save ) )
		return 0;
	return save.WriteFields( "CRecharge", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CWallRecharge::Restore( CRestore &restore )
{
	if ( !CBaseEntity::Restore( restore ) )
		return 0;
	if ( restore.ReadFields( "CRecharge", this, m_SaveData, ARRAYSIZE( m_SaveData ) ) )
		return 1;
	return restore.ReadFields( "CBaseWallCharger", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CBaseWallCharger::Save( CSave &save )
{
	return CBaseToggle::Save( save );
}

int CBaseWallCharger::Restore( CRestore &restore )
{
	return CBaseToggle::Restore( restore );
}

LINK_ENTITY_TO_CLASS( func_healthcharger, CWallHealth );
LINK_ENTITY_TO_CLASS( func_recharge, CWallRecharge );

void CBaseWallCharger::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "style" ) ||
	     FStrEq( pkvd->szKeyName, "height" ) ||
	     FStrEq( pkvd->szKeyName, "value1" ) ||
	     FStrEq( pkvd->szKeyName, "value2" ) ||
	     FStrEq( pkvd->szKeyName, "value3" ) )
	{
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "dmdelay" ) )
	{
		m_iReactivate  = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseToggle::KeyValue( pkvd );
	}
}

void CBaseWallCharger::Spawn( void )
{
	Precache();

	pev->solid    = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	m_iJuice   = GetCapacity();
	pev->frame = 0;
}

void CBaseWallCharger::Precache( void )
{
	if ( GetStartSound() && *GetStartSound() )
		PRECACHE_SOUND( (char *)GetStartSound() );
	if ( GetLoopSound() && *GetLoopSound() )
		PRECACHE_SOUND( (char *)GetLoopSound() );
	if ( GetDenySound() && *GetDenySound() )
		PRECACHE_SOUND( (char *)GetDenySound() );
}

void CBaseWallCharger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	if ( m_iJuice <= 0 )
	{
		pev->frame = 1;
		Off();
	}

	// If player does not have suit, or there is no juice left, play deny sound
	if ( ( m_iJuice <= 0 ) || ( !( pActivator->pev->weapons & ( 1 << WEAPON_SUIT ) ) ) )
	{
		if ( m_flSoundTime <= gpGlobals->time )
		{
			m_flSoundTime = gpGlobals->time + 0.62;
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, (char *)GetDenySound(), GetSoundVolume(), ATTN_NORM );
		}
		return;
	}

	pev->nextthink = pev->ltime + 0.25;
	SetThink( &CBaseWallCharger::Off );

	if ( m_flNextCharge >= gpGlobals->time )
		return;

	if ( !m_iOn )
	{
		m_iOn = 1;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, (char *)GetStartSound(), GetSoundVolume(), ATTN_NORM );
		m_flSoundTime = 0.56 + gpGlobals->time;
	}

	if ( ( m_iOn == 1 ) && ( m_flSoundTime <= gpGlobals->time ) )
	{
		m_iOn = 2;
		EMIT_SOUND( ENT( pev ), CHAN_STATIC, (char *)GetLoopSound(), GetSoundVolume(), ATTN_NORM );
	}

	// charge the player
	if ( GiveResource( pActivator ) )
	{
		m_iJuice--;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + 0.1;
}

void CBaseWallCharger::Recharge( void )
{
	m_iJuice   = GetCapacity();
	pev->frame = 0;
	SetThink( &CBaseWallCharger::SUB_DoNothing );
}

void CBaseWallCharger::Off( void )
{
	// Stop looping sound.
	if ( m_iOn > 1 )
		STOP_SOUND( ENT( pev ), CHAN_STATIC, (char *)GetLoopSound() );

	m_iOn = 0;

	if ( ( !m_iJuice ) && ( ( m_iReactivate = GetRechargeTime() ) > 0 ) )
	{
		pev->nextthink = pev->ltime + m_iReactivate;
		SetThink( &CBaseWallCharger::Recharge );
	}
	else
		SetThink( &CBaseWallCharger::SUB_DoNothing );
}
