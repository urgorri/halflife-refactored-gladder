/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: Pipe Wrench (weapon_pipewrench)
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
#include "weapon_pipewrench.h"

#define PIPEWRENCH_BODYHIT_VOLUME 128
#define PIPEWRENCH_WALLHIT_VOLUME 512

LINK_ENTITY_TO_CLASS( weapon_pipewrench, CPipewrench );
#ifndef CLIENT_DLL
REGISTER_WEAPON( weapon_pipewrench, 160 );
#endif

void CPipewrench::Spawn()
{
	Precache();
	m_iId = WEAPON_PIPEWRENCH;
	SET_MODEL( ENT( pev ), "models/w_pipe_wrench.mdl" );
	m_iClip = WEAPON_NOCLIP;
	m_iSwingMode = WRENCH_SWING_NONE;

	FallInit();
}

void CPipewrench::Precache( void )
{
	PRECACHE_MODEL( "models/v_pipe_wrench.mdl" );
	PRECACHE_MODEL( "models/w_pipe_wrench.mdl" );
	PRECACHE_MODEL( "models/p_pipe_wrench.mdl" );

	PRECACHE_SOUND( "weapons/pwrench_big_hitbod1.wav" );
	PRECACHE_SOUND( "weapons/pwrench_big_hitbod2.wav" );
	PRECACHE_SOUND( "weapons/pwrench_big_miss.wav" );
	PRECACHE_SOUND( "weapons/pwrench_hit1.wav" );
	PRECACHE_SOUND( "weapons/pwrench_hit2.wav" );
	PRECACHE_SOUND( "weapons/pwrench_hitbod1.wav" );
	PRECACHE_SOUND( "weapons/pwrench_hitbod2.wav" );
	PRECACHE_SOUND( "weapons/pwrench_hitbod3.wav" );
	PRECACHE_SOUND( "weapons/pwrench_miss1.wav" );
	PRECACHE_SOUND( "weapons/pwrench_miss2.wav" );

	m_usPipewrench = PRECACHE_EVENT( 1, "events/pipewrench.sc" );
}

int CPipewrench::GetItemInfo( ItemInfo *p )
{
	p->pszName   = STRING( pev->classname );
	p->pszAmmo1  = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2  = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip  = WEAPON_NOCLIP;
	p->iSlot     = 0;
	p->iPosition = 3;
	p->iId       = WEAPON_PIPEWRENCH;
	p->iWeight   = PIPEWRENCH_WEIGHT;
	return 1;
}

BOOL CPipewrench::Deploy()
{
	return DefaultDeploy( "models/v_pipe_wrench.mdl", "models/p_pipe_wrench.mdl", PIPEWRENCH_DRAW, "crowbar" );
}

void CPipewrench::Holster( int skiplocal /* = 0 */ )
{
	m_iSwingMode = WRENCH_SWING_NONE;
	SetThink( nullptr );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( PIPEWRENCH_HOLSTER );
}

void CPipewrench::PrimaryAttack()
{
	if ( m_iSwingMode == WRENCH_SWING_NONE && !Swing( 1 ) )
	{
#ifndef CLIENT_DLL
		SetThink( &CPipewrench::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
#endif
	}
}

void CPipewrench::SecondaryAttack()
{
	if ( m_iSwingMode == WRENCH_SWING_NONE )
	{
		m_iSwingMode = WRENCH_SWING_START;
		m_flBigSwingStart = gpGlobals->time;
		SendWeaponAnim( PIPEWRENCH_BIG_SWING_START );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
	}
}

void CPipewrench::WeaponIdle( void )
{
	if ( m_iSwingMode == WRENCH_SWING_START )
	{
		m_iSwingMode = WRENCH_SWING_HOLD;
		SendWeaponAnim( PIPEWRENCH_BIG_SWING_IDLE );
	}
	else if ( m_iSwingMode == WRENCH_SWING_HOLD )
	{
		// If player released secondary attack button or max charge reached (1.5s), unleash heavy smash!
		if ( !( m_pPlayer->pev->button & IN_ATTACK2 ) || ( gpGlobals->time - m_flBigSwingStart >= 1.5f ) )
		{
			BigSwing();
		}
	}
}

void CPipewrench::Smack()
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

void CPipewrench::SwingAgain( void )
{
	Swing( 0 );
}

static void WrenchFindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
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

int CPipewrench::Swing( int fFirst )
{
	int fDidHit = FALSE;
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
				WrenchFindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;
		}
	}
#endif

	if ( fFirst )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usPipewrench, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0.0, 0, 0.0 );
	}

	if ( tr.flFraction >= 1.0 )
	{
		if ( fFirst )
		{
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.6 );
			m_flNextSecondaryAttack = GetNextAttackDelay( 0.6 );
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
			switch ( ( m_iSwing++ ) % 3 )
			{
			case 0: SendWeaponAnim( PIPEWRENCH_ATTACK1MISS ); break;
			case 1: SendWeaponAnim( PIPEWRENCH_ATTACK2MISS ); break;
			case 2: SendWeaponAnim( PIPEWRENCH_ATTACK3MISS ); break;
			}
			switch ( RANDOM_LONG( 0, 1 ) )
			{
			case 0: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_miss1.wav", 1.0f, ATTN_NORM ); break;
			case 1: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_miss2.wav", 1.0f, ATTN_NORM ); break;
			}
		}
	}
	else
	{
		switch ( ( m_iSwing++ ) % 3 )
		{
		case 0: SendWeaponAnim( PIPEWRENCH_ATTACK1HIT ); break;
		case 1: SendWeaponAnim( PIPEWRENCH_ATTACK2HIT ); break;
		case 2: SendWeaponAnim( PIPEWRENCH_ATTACK3HIT ); break;
		}

		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage();
			pEntity->TraceAttack( m_pPlayer->pev, GLADDER_DMG_PIPEWRENCH, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
		}

		float flVol = 1.0f;
		BOOL fHitWorld = TRUE;

		if ( pEntity )
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				switch ( RANDOM_LONG( 0, 2 ) )
				{
				case 0: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod1.wav", 1.0f, ATTN_NORM ); break;
				case 1: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod2.wav", 1.0f, ATTN_NORM ); break;
				case 2: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod3.wav", 1.0f, ATTN_NORM ); break;
				}
				m_pPlayer->m_iWeaponVolume = PIPEWRENCH_BODYHIT_VOLUME;
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
			case 0: EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) ); break;
			case 1: EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) ); break;
			}
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * PIPEWRENCH_WALLHIT_VOLUME;

		SetThink( &CPipewrench::Smack );
		pev->nextthink = gpGlobals->time + 0.2;
#endif

		m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
		m_flNextSecondaryAttack = GetNextAttackDelay( 0.6 );
	}

	return fDidHit;
}

void CPipewrench::BigSwing( void )
{
	m_iSwingMode = WRENCH_SWING_NONE;

	TraceResult tr;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 40;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				WrenchFindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		SendWeaponAnim( PIPEWRENCH_BIG_SWING_HIT );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if ( pEntity )
		{
			ClearMultiDamage();
			float chargeRatio = (std::min)( 1.5f, (std::max)( 0.2f, gpGlobals->time - m_flBigSwingStart ) ) / 1.5f;
			float flDamage = GLADDER_DMG_PIPEWRENCH + chargeRatio * 50.0f; // up to 85 damage!

			pEntity->TraceAttack( m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				switch ( RANDOM_LONG( 0, 1 ) )
				{
				case 0: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_big_hitbod1.wav", 1.0f, ATTN_NORM ); break;
				case 1: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_big_hitbod2.wav", 1.0f, ATTN_NORM ); break;
				}
			}
			else
			{
				EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit1.wav", 1.0f, ATTN_NORM );
			}
		}
	}
	else
	{
		SendWeaponAnim( PIPEWRENCH_BIG_SWING_MISS );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_big_miss.wav", 1.0f, ATTN_NORM );
	}
#endif

	m_flNextPrimaryAttack = GetNextAttackDelay( 1.0 );
	m_flNextSecondaryAttack = GetNextAttackDelay( 1.0 );
}
