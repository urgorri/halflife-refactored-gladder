/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: Desert Eagle (weapon_eagle)
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
#include "weapons/projectile_rocket.h"
#include "gladder_weapons.h"
#include "weapon_eagle.h"

LINK_ENTITY_TO_CLASS( weapon_eagle, CEagle );
#ifndef CLIENT_DLL
REGISTER_WEAPON( weapon_eagle, 175 );
#endif

void CEagle::Spawn()
{
	Precache();
	m_iId = WEAPON_EAGLE;
	SET_MODEL( ENT( pev ), "models/w_desert_eagle.mdl" );

	m_iDefaultAmmo = EAGLE_DEFAULT_GIVE;
	m_bLaserActive = false;

	FallInit();
}

void CEagle::Precache( void )
{
	PRECACHE_MODEL( "models/v_desert_eagle.mdl" );
	PRECACHE_MODEL( "models/w_desert_eagle.mdl" );
	PRECACHE_MODEL( "models/p_desert_eagle.mdl" );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );

	PRECACHE_SOUND( "weapons/desert_eagle_fire.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_reload.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_sight.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_sight2.wav" );

	m_usFireEagle = PRECACHE_EVENT( 1, "events/eagle.sc" );
}

int CEagle::GetItemInfo( ItemInfo *p )
{
	p->pszName   = STRING( pev->classname );
	p->pszAmmo1  = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;
	p->pszAmmo2  = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip  = EAGLE_MAX_CLIP;
	p->iSlot     = 1;
	p->iPosition = 2;
	p->iFlags    = 0;
	p->iId       = m_iId = WEAPON_EAGLE;
	p->iWeight   = EAGLE_WEIGHT;
	return 1;
}

BOOL CEagle::Deploy()
{
	return DefaultDeploy( "models/v_desert_eagle.mdl", "models/p_desert_eagle.mdl", EAGLE_DRAW, "onehanded" );
}

void CEagle::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( EAGLE_HOLSTER );

#ifndef CLIENT_DLL
	if ( m_pLaserSpot )
	{
		m_pLaserSpot->Killed( nullptr, GIB_NEVER );
		m_pLaserSpot = nullptr;
	}
#endif
}

void CEagle::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.2;
		return;
	}

	if ( m_iClip <= 0 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.2;
		return;
	}

	m_iClip--;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc    = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecSpread = m_bLaserActive ? VECTOR_CONE_1DEGREES : VECTOR_CONE_3DEGREES;

	Vector vecDir = m_pPlayer->FireBulletsPlayer(
		1, vecSrc, vecAiming, vecSpread, 8192,
		BULLET_PLAYER_357, 0, (int)GLADDER_DMG_EAGLE,
		m_pPlayer->pev, m_pPlayer->random_seed
	);

	int flags = 0;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireEagle, 0.0,
	                     (float *)&g_vecZero, (float *)&g_vecZero,
	                     vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if ( m_iClip == 0 )
	{
		SendWeaponAnim( EAGLE_SHOOT_EMPTY );
	}
	else
	{
		SendWeaponAnim( EAGLE_SHOOT );
	}

	m_pPlayer->pev->punchangle.x -= 3.5;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + ( m_bLaserActive ? 0.35 : 0.25 );
	m_flTimeWeaponIdle    = UTIL_WeaponTimeBase() + 2.0;

	UpdateSpot();
}

void CEagle::SecondaryAttack()
{
	m_bLaserActive = !m_bLaserActive;

	if ( m_bLaserActive )
	{
		EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/desert_eagle_sight.wav", 1.0f, ATTN_NORM );
	}
	else
	{
		EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/desert_eagle_sight2.wav", 1.0f, ATTN_NORM );
#ifndef CLIENT_DLL
		if ( m_pLaserSpot )
		{
			m_pLaserSpot->Killed( nullptr, GIB_NEVER );
			m_pLaserSpot = nullptr;
		}
#endif
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3;
	UpdateSpot();
}

void CEagle::UpdateSpot()
{
#ifndef CLIENT_DLL
	if ( !m_bLaserActive )
	{
		if ( m_pLaserSpot )
		{
			m_pLaserSpot->Killed( nullptr, GIB_NEVER );
			m_pLaserSpot = nullptr;
		}
		return;
	}

	if ( !m_pLaserSpot )
	{
		m_pLaserSpot = CLaserSpot::CreateSpot();
		if ( m_pLaserSpot )
		{
			m_pLaserSpot->pev->scale = 0.5f;
		}
	}

	if ( m_pLaserSpot )
	{
		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition();
		Vector vecEnd = vecSrc + gpGlobals->v_forward * 4096;

		TraceResult tr;
		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );
		UTIL_SetOrigin( m_pLaserSpot->pev, tr.vecEndPos + tr.vecPlaneNormal * 0.1f );
	}
#endif
}

void CEagle::Reload( void )
{
	if ( m_pPlayer->ammo_357 <= 0 )
		return;

	int iAnim = ( m_iClip == 0 ) ? EAGLE_RELOAD : EAGLE_RELOAD_NOSHOT;
	if ( DefaultReload( EAGLE_MAX_CLIP, iAnim, 1.8 ) )
	{
		m_flNextPrimaryAttack   = UTIL_WeaponTimeBase() + 1.8;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.8;
		m_flTimeWeaponIdle      = UTIL_WeaponTimeBase() + 2.5;
	}
}

void CEagle::WeaponIdle( void )
{
	UpdateSpot();

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );
	if ( flRand <= 0.3 )
		iAnim = EAGLE_IDLE1;
	else if ( flRand <= 0.6 )
		iAnim = EAGLE_IDLE2;
	else
		iAnim = EAGLE_IDLE3;

	SendWeaponAnim( iAnim );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0;
}
