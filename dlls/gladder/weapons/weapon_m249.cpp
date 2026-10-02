/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: M249 SAW (weapon_m249)
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
#include "weapon_m249.h"

LINK_ENTITY_TO_CLASS( weapon_m249, CM249 );
#ifndef CLIENT_DLL
REGISTER_WEAPON( weapon_m249, 195 );
#endif

void CM249::Spawn()
{
	Precache();
	m_iId = WEAPON_M249;
	SET_MODEL( ENT( pev ), "models/w_saw.mdl" );

	m_iDefaultAmmo = M249_DEFAULT_GIVE;

	FallInit();
}

void CM249::Precache( void )
{
	PRECACHE_MODEL( "models/v_saw.mdl" );
	PRECACHE_MODEL( "models/w_saw.mdl" );
	PRECACHE_MODEL( "models/p_saw.mdl" );

	m_iShell = PRECACHE_MODEL( "models/saw_shell.mdl" );
	m_iLink  = PRECACHE_MODEL( "models/saw_link.mdl" );

	PRECACHE_SOUND( "weapons/saw_fire1.wav" );
	PRECACHE_SOUND( "weapons/saw_fire2.wav" );
	PRECACHE_SOUND( "weapons/saw_fire3.wav" );
	PRECACHE_SOUND( "weapons/saw_reload.wav" );
	PRECACHE_SOUND( "weapons/saw_reload2.wav" );

	m_usFireM249 = PRECACHE_EVENT( 1, "events/m249.sc" );
}

int CM249::GetItemInfo( ItemInfo *p )
{
	p->pszName   = STRING( pev->classname );
	p->pszAmmo1  = "556";
	p->iMaxAmmo1 = _556_MAX_CARRY;
	p->pszAmmo2  = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip  = M249_MAX_CLIP;
	p->iSlot     = 3;
	p->iPosition = 2;
	p->iFlags    = 0;
	p->iId       = m_iId = WEAPON_M249;
	p->iWeight   = M249_WEIGHT;
	return 1;
}

BOOL CM249::Deploy()
{
	return DefaultDeploy( "models/v_saw.mdl", "models/p_saw.mdl", M249_DRAW, "saw" );
}

void CM249::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( M249_HOLSTER );
}

void CM249::PrimaryAttack( void )
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if ( m_iClip <= 0 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_iClip--;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc    = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	Vector vecDir = m_pPlayer->FireBulletsPlayer(
		1, vecSrc, vecAiming, VECTOR_CONE_4DEGREES, 8192,
		BULLET_PLAYER_MP5, 0, (int)GLADDER_DMG_M249,
		m_pPlayer->pev, m_pPlayer->random_seed
	);

	int flags = 0;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireM249, 0.0,
	                     (float *)&g_vecZero, (float *)&g_vecZero,
	                     vecDir.x, vecDir.y, m_iClip, 0, 0, 0 );

	if ( RANDOM_LONG( 0, 1 ) )
	{
		SendWeaponAnim( M249_SHOOT1 );
	}
	else
	{
		SendWeaponAnim( M249_SHOOT2 );
	}

	m_pPlayer->pev->punchangle.x -= UTIL_SharedRandomFloat( m_pPlayer->random_seed, 1.6f, 2.2f );
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.085; // ~700 RPM
	m_flTimeWeaponIdle    = UTIL_WeaponTimeBase() + 1.0;
}

void CM249::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( DefaultReload( M249_MAX_CLIP, M249_RELOAD1, 3.5 ) )
	{
		m_flNextPrimaryAttack   = UTIL_WeaponTimeBase() + 3.5;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 3.5;
		m_flTimeWeaponIdle      = UTIL_WeaponTimeBase() + 4.0;
	}
}

void CM249::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( M249_SLOWIDLE );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
}

// ==============================================================================
// 5.56mm SAW Ammo Pickup (ammo_556)
// ==============================================================================
LINK_ENTITY_TO_CLASS( ammo_556, CAmmo556 );

void CAmmo556::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_saw_clip.mdl" );
	CBasePlayerAmmo::Spawn();
}

void CAmmo556::Precache( void )
{
	PRECACHE_MODEL( "models/w_saw_clip.mdl" );
	PRECACHE_SOUND( "items/9mmclip1.wav" );
}

BOOL CAmmo556::AddAmmo( CBaseEntity *pOther )
{
	if ( pOther->GiveAmmo( AMMO_M249_GIVE, "556", _556_MAX_CARRY ) != -1 )
	{
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1.0f, ATTN_NORM );
		return TRUE;
	}
	return FALSE;
}
