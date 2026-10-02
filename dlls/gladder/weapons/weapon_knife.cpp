/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: Combat Knife (weapon_knife)
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "weapons/weapon_base.h"
#include "core/player.h"
#include "gameplay/gamerules.h"
#include "weapons/weapon_registry.h"
#include "gladder_weapons.h"
#include "weapon_knife.h"

#define KNIFE_BODYHIT_VOLUME 128
#define KNIFE_WALLHIT_VOLUME 512

LINK_ENTITY_TO_CLASS( weapon_knife, CKnife );
#ifndef CLIENT_DLL
REGISTER_WEAPON( weapon_knife, 150 );
#endif

void CKnife::Spawn()
{
	Precache();
	m_iId = WEAPON_KNIFE;
	SET_MODEL( ENT( pev ), "models/w_knife.mdl" );
	m_iClip = WEAPON_NOCLIP;

	FallInit();
}

void CKnife::Precache( void )
{
	PRECACHE_MODEL( "models/v_knife.mdl" );
	PRECACHE_MODEL( "models/w_knife.mdl" );
	PRECACHE_MODEL( "models/p_knife.mdl" );

	PRECACHE_SOUND( "weapons/knife1.wav" );
	PRECACHE_SOUND( "weapons/knife2.wav" );
	PRECACHE_SOUND( "weapons/knife3.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_flesh1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_flesh2.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_wall1.wav" );
	PRECACHE_SOUND( "weapons/knife_hit_wall2.wav" );

	m_usKnife = PRECACHE_EVENT( 1, "events/knife.sc" );
}

int CKnife::GetItemInfo( ItemInfo *p )
{
	p->pszName   = STRING( pev->classname );
	p->pszAmmo1  = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2  = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip  = WEAPON_NOCLIP;
	p->iSlot     = 0;
	p->iPosition = 2;
	p->iId       = WEAPON_KNIFE;
	p->iWeight   = KNIFE_WEIGHT;
	return 1;
}

BOOL CKnife::Deploy()
{
	return DefaultDeploy( "models/v_knife.mdl", "models/p_knife.mdl", KNIFE_DRAW, "crowbar" );
}

void CKnife::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( KNIFE_HOLSTER );
}

void CKnife::PrimaryAttack()
{
	if ( !Swing( 1 ) )
	{
#ifndef CLIENT_DLL
		SetThink( &CKnife::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
#endif
	}
}

void CKnife::SecondaryAttack()
{
	Stab();
}

void CKnife::Smack()
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

void CKnife::SwingAgain( void )
{
	Swing( 0 );
}

static void KnifeFindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	TraceResult tmpTrace;
	Vector vecHullEnd = tr.vecEndPos;
	Vector vecEnd;
	float distance = 1e6f;

	vecHullEnd = vecSrc + ( ( vecHullEnd - vecSrc ) * 2 );
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	float *minmaxs[2] = { mins, maxs };
	for ( int i = 0; i < 2; i++ )
	{
		for ( int j = 0; j < 2; j++ )
		{
			for ( int k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = ( tmpTrace.vecEndPos - vecSrc ).Length();
					if ( thisDistance < distance )
					{
						tr       = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

int CKnife::Swing( int fFirst )
{
	int fDidHit = FALSE;
	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				KnifeFindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;
		}
	}
#endif

	if ( fFirst )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usKnife, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0.0, 0, 0.0 );
	}

	if ( tr.flFraction >= 1.0 )
	{
		if ( fFirst )
		{
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.4 );
			m_flNextSecondaryAttack = GetNextAttackDelay( 0.5 );
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
			SendWeaponAnim( KNIFE_ATTACK1MISS );
		}
	}
	else
	{
		switch ( ( ( m_iSwing++ ) % 2 ) + 1 )
		{
		case 0:
			SendWeaponAnim( KNIFE_ATTACK1 );
			break;
		case 1:
			SendWeaponAnim( KNIFE_ATTACK2HIT );
			break;
		case 2:
			SendWeaponAnim( KNIFE_ATTACK3HIT );
			break;
		}

		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage();

			float flDamage = GLADDER_DMG_KNIFE;

			// Backstab bonus calculation
			UTIL_MakeVectors( pEntity->pev->v_angle );
			Vector targetForward = gpGlobals->v_forward;
			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			Vector ownerForward = gpGlobals->v_forward;

			if ( DotProduct( targetForward, ownerForward ) > 0.6f )
			{
				flDamage = GLADDER_DMG_KNIFE_BACK; // 60 dmg backstab
			}

			pEntity->TraceAttack( m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
		}

		float flVol = 1.0f;
		BOOL fHitWorld = TRUE;

		if ( pEntity )
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				switch ( RANDOM_LONG( 0, 1 ) )
				{
				case 0:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hit_flesh1.wav", 1.0f, ATTN_NORM );
					break;
				case 1:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hit_flesh2.wav", 1.0f, ATTN_NORM );
					break;
				}
				m_pPlayer->m_iWeaponVolume = KNIFE_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					return TRUE;
				flVol = 0.1f;
				fHitWorld = FALSE;
			}
		}

		if ( fHitWorld )
		{
			float fvolbar = TEXTURETYPE_PlaySound( &tr, vecSrc, vecSrc + ( vecEnd - vecSrc ) * 2, BULLET_PLAYER_CROWBAR );
			switch ( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hit_wall1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			case 1:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hit_wall2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			}
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * KNIFE_WALLHIT_VOLUME;

		SetThink( &CKnife::Smack );
		pev->nextthink = gpGlobals->time + 0.15;
#endif

		m_flNextPrimaryAttack = GetNextAttackDelay( 0.35 );
		m_flNextSecondaryAttack = GetNextAttackDelay( 0.45 );
	}

	return fDidHit;
}

int CKnife::Stab( void )
{
	SendWeaponAnim( KNIFE_STAB );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	TraceResult tr;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 36;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				KnifeFindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if ( pEntity )
		{
			ClearMultiDamage();
			float flDamage = GLADDER_DMG_KNIFE_BACK; // heavy stab (60 dmg)
			pEntity->TraceAttack( m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hit_flesh2.wav", 1.0f, ATTN_NORM );
			}
			else
			{
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife_hit_wall2.wav", 1.0f, ATTN_NORM, 0, 100 );
			}
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife1.wav", 1.0f, ATTN_NORM ); break;
		case 1: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife2.wav", 1.0f, ATTN_NORM ); break;
		case 2: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/knife3.wav", 1.0f, ATTN_NORM ); break;
		}
	}
#endif

	m_flNextPrimaryAttack = GetNextAttackDelay( 0.8 );
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.8 );
	return 1;
}
