/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal: M40A1 Sniper Rifle (weapon_sniper)
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
#include "weapon_sniper.h"

LINK_ENTITY_TO_CLASS( weapon_sniper, CSniperRifle );
#ifndef CLIENT_DLL
REGISTER_WEAPON( weapon_sniper, 185 );
#endif

void CSniperRifle::Spawn()
{
	Precache();
	m_iId = WEAPON_SNIPER;
	SET_MODEL( ENT( pev ), "models/w_m40a1.mdl" );

	m_iDefaultAmmo = SNIPER_DEFAULT_GIVE;
	m_bInZoom      = false;

	FallInit();
}

void CSniperRifle::Precache( void )
{
	PRECACHE_MODEL( "models/v_m40a1.mdl" );
	PRECACHE_MODEL( "models/w_m40a1.mdl" );
	PRECACHE_MODEL( "models/p_m40a1.mdl" );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );

	PRECACHE_SOUND( "weapons/sniper_fire.wav" );
	PRECACHE_SOUND( "weapons/sniper_fire_last_round.wav" );
	PRECACHE_SOUND( "weapons/sniper_reload_first_seq.wav" );
	PRECACHE_SOUND( "weapons/sniper_reload_second_seq.wav" );
	PRECACHE_SOUND( "weapons/sniper_reload3.wav" );
	PRECACHE_SOUND( "weapons/sniper_zoom.wav" );
	PRECACHE_SOUND( "weapons/sniper_bolt1.wav" );
	PRECACHE_SOUND( "weapons/sniper_bolt2.wav" );

	m_usSniper = PRECACHE_EVENT( 1, "events/sniper.sc" );
}

int CSniperRifle::GetItemInfo( ItemInfo *p )
{
	p->pszName   = STRING( pev->classname );
	p->pszAmmo1  = "762";
	p->iMaxAmmo1 = _762_MAX_CARRY;
	p->pszAmmo2  = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip  = SNIPER_MAX_CLIP;
	p->iSlot     = 2;
	p->iPosition = 3;
	p->iFlags    = 0;
	p->iId       = m_iId = WEAPON_SNIPER;
	p->iWeight   = SNIPER_WEIGHT;
	return 1;
}

BOOL CSniperRifle::Deploy()
{
	return DefaultDeploy( "models/v_m40a1.mdl", "models/p_m40a1.mdl", SNIPER_DRAW, "sniper" );
}

void CSniperRifle::Holster( int skiplocal /* = 0 */ )
{
	if ( m_bInZoom )
	{
		ToggleZoom();
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( SNIPER_HOLSTER );
}

void CSniperRifle::ToggleZoom( void )
{
	m_bInZoom = !m_bInZoom;

	EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/sniper_zoom.wav", 1.0f, ATTN_NORM );

	if ( m_bInZoom )
	{
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 18;
	}
	else
	{
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	}
}

void CSniperRifle::SecondaryAttack( void )
{
	ToggleZoom();
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.4;
}

void CSniperRifle::PrimaryAttack( void )
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
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );

	// Pinpoint accurate when scoped, wide spread when unscoped
	Vector vecSpread = m_bInZoom ? g_vecZero : VECTOR_CONE_6DEGREES;

	Vector vecDir = m_pPlayer->FireBulletsPlayer(
		1, vecSrc, vecAiming, vecSpread, 8192,
		BULLET_PLAYER_357, 0, (int)GLADDER_DMG_SNIPER,
		m_pPlayer->pev, m_pPlayer->random_seed
	);

	int flags = 0;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSniper, 0.0,
	                     (float *)&g_vecZero, (float *)&g_vecZero,
	                     vecDir.x, vecDir.y, m_iClip, 0, 0, 0 );

	if ( m_iClip == 0 )
	{
		SendWeaponAnim( SNIPER_FIRELAST );
	}
	else
	{
		SendWeaponAnim( SNIPER_FIRE );
	}

	m_pPlayer->pev->punchangle.x -= 4.5;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.2;
	m_flTimeWeaponIdle    = UTIL_WeaponTimeBase() + 1.5;
}

void CSniperRifle::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( m_bInZoom )
	{
		ToggleZoom();
	}

	if ( DefaultReload( SNIPER_MAX_CLIP, SNIPER_RELOAD1, 2.5 ) )
	{
		m_flNextPrimaryAttack   = UTIL_WeaponTimeBase() + 2.5;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.5;
		m_flTimeWeaponIdle      = UTIL_WeaponTimeBase() + 3.0;
	}
}

void CSniperRifle::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( SNIPER_IDLE1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.0;
}

// ==============================================================================
// 7.62mm Sniper Ammo Pickup (ammo_762)
// ==============================================================================
LINK_ENTITY_TO_CLASS( ammo_762, CSniperAmmo );

void CSniperAmmo::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_m40a1clip.mdl" );
	CBasePlayerAmmo::Spawn();
}

void CSniperAmmo::Precache( void )
{
	PRECACHE_MODEL( "models/w_m40a1clip.mdl" );
	PRECACHE_SOUND( "items/9mmclip1.wav" );
}

BOOL CSniperAmmo::AddAmmo( CBaseEntity *pOther )
{
	if ( pOther->GiveAmmo( AMMO_SNIPER_GIVE, "762", _762_MAX_CARRY ) != -1 )
	{
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1.0f, ATTN_NORM );
		return TRUE;
	}
	return FALSE;
}
