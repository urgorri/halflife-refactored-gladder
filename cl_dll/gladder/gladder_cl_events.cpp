/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Arsenal Client Event Handlers (m249.sc, sniper.sc, eagle.sc, knife.sc, pipewrench.sc)
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#include "hud/hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"
#include "event_registry.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "studio/studio_util.h"

// OpFor Weapon Viewmodel Sequences for Event Callbacks
enum M249Anim
{
	M249_SLOWIDLE = 0,
	M249_IDLE2,
	M249_LAUNCH,
	M249_RELOAD1,
	M249_HOLSTER,
	M249_DRAW,
	M249_SHOOT1,
	M249_SHOOT2
};

enum SniperAnim
{
	SNIPER_IDLE1 = 0,
	SNIPER_FIRE,
	SNIPER_FIRELAST,
	SNIPER_RELOAD1,
	SNIPER_RELOAD2,
	SNIPER_RELOAD3,
	SNIPER_SLOWIDLE,
	SNIPER_HOLSTER,
	SNIPER_DRAW
};

enum EagleAnim
{
	EAGLE_IDLE1 = 0,
	EAGLE_IDLE2,
	EAGLE_IDLE3,
	EAGLE_IDLE4,
	EAGLE_IDLE5,
	EAGLE_SHOOT,
	EAGLE_SHOOT_EMPTY,
	EAGLE_RELOAD_NOSHOT,
	EAGLE_RELOAD,
	EAGLE_DRAW,
	EAGLE_HOLSTER
};

enum KnifeAnim
{
	KNIFE_IDLE1 = 0,
	KNIFE_DRAW,
	KNIFE_HOLSTER,
	KNIFE_ATTACK1,
	KNIFE_ATTACK1MISS,
	KNIFE_ATTACK2,
	KNIFE_ATTACK2HIT,
	KNIFE_ATTACK3,
	KNIFE_ATTACK3HIT,
	KNIFE_IDLE2,
	KNIFE_IDLE3,
	KNIFE_CHARGE,
	KNIFE_STAB
};

enum PipewrenchAnim
{
	PIPEWRENCH_IDLE1 = 0,
	PIPEWRENCH_IDLE2,
	PIPEWRENCH_IDLE3,
	PIPEWRENCH_DRAW,
	PIPEWRENCH_HOLSTER,
	PIPEWRENCH_ATTACK1HIT,
	PIPEWRENCH_ATTACK1MISS,
	PIPEWRENCH_ATTACK2HIT,
	PIPEWRENCH_ATTACK2MISS,
	PIPEWRENCH_ATTACK3HIT,
	PIPEWRENCH_ATTACK3MISS,
	PIPEWRENCH_BIG_SWING_START,
	PIPEWRENCH_BIG_SWING_HIT,
	PIPEWRENCH_BIG_SWING_MISS,
	PIPEWRENCH_BIG_SWING_IDLE
};

extern void V_PunchAxis( int axis, float punch );
extern int tracerCount[32];

static void EV_FireM249( event_args_t *args )
{
	int idx = args->entindex;
	vec3_t origin, angles, velocity;
	vec3_t ShellVelocity, ShellOrigin;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/saw_shell.mdl" );
	if ( shell <= 0 )
	{
		shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );
	}

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( gEngfuncs.pfnRandomLong( 0, 1 ) ? M249_SHOOT1 : M249_SHOOT2, 2 );
		V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -2.2f, -1.6f ) );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20.0f, -12.0f, 4.0f );
	EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL );

	switch ( gEngfuncs.pfnRandomLong( 0, 2 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/saw_fire1.wav", 1.0f, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/saw_fire2.wav", 1.0f, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 2:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/saw_fire3.wav", 1.0f, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192.0f, BULLET_PLAYER_MP5, 2, &tracerCount[idx - 1], args->fparam1, args->fparam2 );
}

static void EV_FireSniper( event_args_t *args )
{
	int idx = args->entindex;
	vec3_t origin, angles, velocity;
	vec3_t ShellVelocity, ShellOrigin;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( args->iparam1 == 0 ? SNIPER_FIRELAST : SNIPER_FIRE, 2 );
		V_PunchAxis( 0, -4.0f );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 24.0f, -10.0f, 4.0f );
	EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL );

	if ( args->iparam1 == 0 )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sniper_fire_last_round.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sniper_fire.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
	}

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192.0f, BULLET_PLAYER_357, 0, &tracerCount[idx - 1], args->fparam1, args->fparam2 );
}

static void EV_FireEagle( event_args_t *args )
{
	int idx = args->entindex;
	vec3_t origin, angles, velocity;
	vec3_t ShellVelocity, ShellOrigin;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell.mdl" );

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( EAGLE_SHOOT, 2 );
		V_PunchAxis( 0, -3.5f );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20.0f, -12.0f, 4.0f );
	EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/desert_eagle_fire.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192.0f, BULLET_PLAYER_357, 0, &tracerCount[idx - 1], args->fparam1, args->fparam2 );
}

static void EV_Knife( event_args_t *args )
{
	int idx = args->entindex;
	vec3_t origin;
	VectorCopy( args->origin, origin );

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( KNIFE_ATTACK1MISS, 2 );
	}

	switch ( gEngfuncs.pfnRandomLong( 0, 2 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/knife1.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/knife2.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 2:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/knife3.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
		break;
	}
}

static void EV_Pipewrench( event_args_t *args )
{
	int idx = args->entindex;
	vec3_t origin;
	VectorCopy( args->origin, origin );

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( PIPEWRENCH_ATTACK1MISS, 2 );
	}

	switch ( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/pwrench_miss1.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/pwrench_miss2.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM );
		break;
	}
}

REGISTER_CLIENT_EVENT( "events/m249.sc", EV_FireM249 );
REGISTER_CLIENT_EVENT( "events/sniper.sc", EV_FireSniper );
REGISTER_CLIENT_EVENT( "events/eagle.sc", EV_FireEagle );
REGISTER_CLIENT_EVENT( "events/knife.sc", EV_Knife );
REGISTER_CLIENT_EVENT( "events/pipewrench.sc", EV_Pipewrench );
