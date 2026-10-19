/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Toxic Spit Smoke Cloud Entity (SPEC §3, GitHub Issue #116)
 *
 ****/

#include "squidspit_cloud.h"
#include "core/player.h"
#include "core/skill_manager.h"
#include "gladder_palette.h"
#include "gladder_monster_modifiers.h"

extern DLL_GLOBAL int g_iSkillLevel;

LINK_ENTITY_TO_CLASS( squidspit_cloud, CSquidSpitCloud );

void CSquidSpitCloud::Precache( void )
{
	m_iSprite = PRECACHE_MODEL( "sprites/steam1.spr" );
}

void CSquidSpitCloud::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid    = SOLID_NOT;
	pev->effects  = 0;

	SetThink( &CSquidSpitCloud::CloudThink );
	pev->nextthink = gpGlobals->time + 0.1f;
	m_flNextDmgTime = gpGlobals->time;
}

void CSquidSpitCloud::CloudThink( void )
{
	if ( gpGlobals->time >= m_flDieTime )
	{
		UTIL_Remove( this );
		return;
	}

	// 1. Emit green billowing smoke particles (SPEC §3, Issue #116)
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -16, 16 ) );
		WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -16, 16 ) );
		WRITE_COORD( pev->origin.z + RANDOM_FLOAT( 0, 16 ) );
		WRITE_SHORT( m_iSprite );
		WRITE_BYTE( RANDOM_LONG( 15, 25 ) ); // scale
		WRITE_BYTE( 10 );                   // framerate
	MESSAGE_END();

	// 2. Localized toxic green dynamic light
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 16 );
		WRITE_BYTE( 12 ); // radius
		WRITE_BYTE( GladderPalette::Health.r );
		WRITE_BYTE( GladderPalette::Health.g );
		WRITE_BYTE( GladderPalette::Health.b );
		WRITE_BYTE( 3 ); // life
		WRITE_BYTE( 10 ); // decay
	MESSAGE_END();

	// 3. Inflict periodic acid damage (DMG_ACID) to players within 64 units
	if ( gpGlobals->time >= m_flNextDmgTime )
	{
		CBaseEntity *pTarget = nullptr;
		while ( ( pTarget = UTIL_FindEntityInSphere( pTarget, pev->origin, 64.0f ) ) != nullptr )
		{
			if ( pTarget->IsPlayer() && pTarget->IsAlive() )
			{
				pTarget->TakeDamage( pev, pev, 4.0f, DMG_ACID );
			}
		}
		m_flNextDmgTime = gpGlobals->time + 0.5f;
	}

	pev->nextthink = gpGlobals->time + 0.25f;
}

CSquidSpitCloud *CSquidSpitCloud::Create( const Vector &vecOrigin, float flDuration )
{
	CSquidSpitCloud *pCloud = GetClassPtr( (CSquidSpitCloud *)nullptr );
	if ( !pCloud )
		return nullptr;

	if ( flDuration <= 0.0f )
	{
		flDuration = GladderMonsterModifiers::GetToxicCloudDuration( g_iSkillLevel );
	}

	pCloud->pev->classname = MAKE_STRING( "squidspit_cloud" );
	pCloud->pev->origin = vecOrigin;
	pCloud->m_flDieTime = gpGlobals->time + flDuration;
	pCloud->Spawn();
	return pCloud;
}
