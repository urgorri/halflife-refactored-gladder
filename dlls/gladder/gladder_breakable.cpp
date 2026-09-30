/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	gladder_breakable - Wave-Resettable Destructible Entity (SPEC §11.3)
 *
 ****/

#include "gladder_breakable.h"

LINK_ENTITY_TO_CLASS( gladder_breakable, CGladderBreakable );

CGladderBreakable::CGladderBreakable()
    : m_flOriginalHealth( 0.0f ),
      m_iszOriginalModel( 0 ),
      m_iszOriginalTargetname( 0 ),
      m_vecOriginalMins( 0, 0, 0 ),
      m_vecOriginalMaxs( 0, 0, 0 ),
      m_vecOriginalOrigin( 0, 0, 0 ),
      m_vecOriginalAngles( 0, 0, 0 ),
      m_iOriginalSolid( SOLID_BSP ),
      m_bBroken( false )
{
	m_Material       = matWood;
	m_Explosion      = expRandom;
	m_idShard        = 0;
	m_angle          = 0.0f;
	m_iszGibModel    = 0;
	m_iszSpawnObject = 0;
}

void CGladderBreakable::Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;
	// Do NOT call UTIL_Remove( this ). Entity is preserved in memory across wave resets.
}

void CGladderBreakable::Spawn( void )
{
	CBreakable::Spawn();

	// Cache initial state for wave resets
	m_flOriginalHealth     = pev->health;
	m_iszOriginalModel      = pev->model;
	m_iszOriginalTargetname = pev->targetname;
	m_vecOriginalMins       = pev->mins;
	m_vecOriginalMaxs       = pev->maxs;
	m_vecOriginalOrigin     = pev->origin;
	m_vecOriginalAngles     = pev->angles;
	m_iOriginalSolid        = pev->solid;
	m_bBroken               = false;
}

void CGladderBreakable::Die( void )
{
	if ( m_bBroken )
		return;

	// Execute full parent break sequence: gib/shard spawning, acoustic cues, explosions, target firing
	CBreakable::Die();

	// Cancel SUB_Remove destruction scheduled by CBreakable::Die()
	SetThink( nullptr );
	pev->nextthink = 0;

	// Enter hidden, non-solid state in memory
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	m_bBroken  = true;
}

int CGladderBreakable::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_bBroken || ( pev->effects & EF_NODRAW ) )
		return 0;

	if ( !pevInflictor )
		pevInflictor = pev;
	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	return CBreakable::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CGladderBreakable::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( m_bBroken || ( pev->effects & EF_NODRAW ) )
		return;

	CBreakable::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

void CGladderBreakable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_bBroken || ( pev->effects & EF_NODRAW ) )
		return;

	CBreakable::Use( pActivator, pCaller, useType, value );
}

void CGladderBreakable::Reset( void )
{
	// Restore visibility and solid collision
	pev->effects &= ~EF_NODRAW;
	pev->solid = m_iOriginalSolid;

	// Restore hit points and damage susceptibility
	pev->health = m_flOriginalHealth;
	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )
	{
		pev->takedamage = DAMAGE_NO;
	}
	else
	{
		pev->takedamage = DAMAGE_YES;
	}

	// Restore targetname and model
	pev->targetname = m_iszOriginalTargetname;
	pev->model      = m_iszOriginalModel;

	if ( !FStringNull( pev->model ) )
	{
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	}

	// Restore cached bounds and origin
	pev->mins   = m_vecOriginalMins;
	pev->maxs   = m_vecOriginalMaxs;
	UTIL_SetSize( pev, m_vecOriginalMins, m_vecOriginalMaxs );
	pev->origin = m_vecOriginalOrigin;
	pev->angles = m_vecOriginalAngles;

	// Restore touch handler
	SetTouch( &CBreakable::BreakTouch );
	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )
	{
		SetTouch( nullptr );
	}

	// Cancel any pending think routines
	SetThink( nullptr );
	pev->nextthink = 0;

	m_bBroken = false;
}
