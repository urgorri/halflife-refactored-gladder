/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/
/*

===== player.cpp ========================================================

  functions dealing with the player

*/

#include "core/extdll.h"
#include "core/util.h"

#include "core/cbase.h"
#include "core/player.h"
#include "core/player_physics.h"
#include "weapons/weapon_spraycan.h"
#include "world/trains.h"
#include "ai/nodes.h"
#include "weapons/weapon_base.h"
#include "ai/soundent.h"
#include "ai/monsters.h"
#include "shake.h"
#include "core/decals.h"
#include "gameplay/gamerules.h"
#include "core/game.h"
#include "pm_shared.h"
#include "hltv.h"

// #define DUCKFIX

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL g_fGameOver;
extern DLL_GLOBAL BOOL g_fDrawLines;
int gEvilImpulse101;
extern DLL_GLOBAL int g_iSkillLevel, gDisplayTitle;

BOOL gInitHUD = TRUE;

extern void CopyToBodyQue( entvars_t *pev );
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );
extern Vector VecBModelOrigin( entvars_t *pevBModel );
extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

extern bool IsBustingGame();

// the world node graph
extern CGraph WorldGraph;

#define TRAIN_ACTIVE 0x80
#define TRAIN_NEW 0xc0
#define TRAIN_OFF 0x00
#define TRAIN_NEUTRAL 0x01
#define TRAIN_SLOW 0x02
#define TRAIN_MEDIUM 0x03
#define TRAIN_FAST 0x04
#define TRAIN_BACK 0x05

// Global Savedata for player
TYPEDESCRIPTION CBasePlayer::m_playerSaveData[] =
    {
        DEFINE_FIELD( CBasePlayer, m_flFlashLightTime, FIELD_TIME ),
        DEFINE_FIELD( CBasePlayer, m_iFlashBattery, FIELD_INTEGER ),

        DEFINE_FIELD( CBasePlayer, m_afButtonLast, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_afButtonPressed, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_afButtonReleased, FIELD_INTEGER ),

        DEFINE_ARRAY( CBasePlayer, m_rgItems, FIELD_INTEGER, MAX_ITEMS ),
        DEFINE_FIELD( CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER ),

        DEFINE_FIELD( CBasePlayer, m_flTimeStepSound, FIELD_TIME ),
        DEFINE_FIELD( CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME ),
        DEFINE_FIELD( CBasePlayer, m_flSwimTime, FIELD_TIME ),
        DEFINE_FIELD( CBasePlayer, m_flDuckTime, FIELD_TIME ),
        DEFINE_FIELD( CBasePlayer, m_flWallJumpTime, FIELD_TIME ),

        DEFINE_FIELD( CBasePlayer, m_flSuitUpdate, FIELD_TIME ),
        DEFINE_ARRAY( CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST ),
        DEFINE_FIELD( CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER ),
        DEFINE_ARRAY( CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT ),
        DEFINE_ARRAY( CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT ),
        DEFINE_FIELD( CBasePlayer, m_lastDamageAmount, FIELD_INTEGER ),

        DEFINE_ARRAY( CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
        DEFINE_FIELD( CBasePlayer, m_pActiveItem, FIELD_CLASSPTR ),
        DEFINE_FIELD( CBasePlayer, m_pLastItem, FIELD_CLASSPTR ),

        DEFINE_ARRAY( CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
        DEFINE_FIELD( CBasePlayer, m_idrowndmg, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_idrownrestored, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_tSneaking, FIELD_TIME ),

        DEFINE_FIELD( CBasePlayer, m_iTrain, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_flFallVelocity, FIELD_FLOAT ),
        DEFINE_FIELD( CBasePlayer, m_iTargetVolume, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_iWeaponVolume, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_iWeaponFlash, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_fLongJump, FIELD_BOOLEAN ),
        DEFINE_FIELD( CBasePlayer, m_fInitHUD, FIELD_BOOLEAN ),
        DEFINE_FIELD( CBasePlayer, m_tbdPrev, FIELD_TIME ),

        DEFINE_FIELD( CBasePlayer, m_pTank, FIELD_EHANDLE ),
        DEFINE_FIELD( CBasePlayer, m_iHideHUD, FIELD_INTEGER ),
        DEFINE_FIELD( CBasePlayer, m_iFOV, FIELD_INTEGER ),

        // DEFINE_FIELD( CBasePlayer, m_fDeadTime, FIELD_FLOAT ), // only used in multiplayer games
        // DEFINE_FIELD( CBasePlayer, m_fGameHUDInitialized, FIELD_INTEGER ), // only used in multiplayer games
        // DEFINE_FIELD( CBasePlayer, m_flStopExtraSoundTime, FIELD_TIME ),
        // DEFINE_FIELD( CBasePlayer, m_fKnownItem, FIELD_INTEGER ), // reset to zero on load
        // DEFINE_FIELD( CBasePlayer, m_iPlayerSound, FIELD_INTEGER ),	// Don't restore, set in Precache()
        // DEFINE_FIELD( CBasePlayer, m_pentSndLast, FIELD_EDICT ),	// Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_flSndRoomtype, FIELD_FLOAT ),	// Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_flSndRange, FIELD_FLOAT ),	// Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_fNewAmmo, FIELD_INTEGER ), // Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_flgeigerRange, FIELD_FLOAT ),	// Don't restore, reset in Precache()
        // DEFINE_FIELD( CBasePlayer, m_flgeigerDelay, FIELD_FLOAT ),	// Don't restore, reset in Precache()
        // DEFINE_FIELD( CBasePlayer, m_igeigerRangePrev, FIELD_FLOAT ),	// Don't restore, reset in Precache()
        // DEFINE_FIELD( CBasePlayer, m_iStepLeft, FIELD_INTEGER ), // Don't need to restore
        // DEFINE_ARRAY( CBasePlayer, m_szTextureName, FIELD_CHARACTER, CBTEXTURENAMEMAX ), // Don't need to restore
        // DEFINE_FIELD( CBasePlayer, m_chTextureType, FIELD_CHARACTER ), // Don't need to restore
        // DEFINE_FIELD( CBasePlayer, m_fNoPlayerSound, FIELD_BOOLEAN ), // Don't need to restore, debug
        // DEFINE_FIELD( CBasePlayer, m_iUpdateTime, FIELD_INTEGER ), // Don't need to restore
        // DEFINE_FIELD( CBasePlayer, m_iClientHealth, FIELD_INTEGER ), // Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_iClientBattery, FIELD_INTEGER ), // Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_iClientHideHUD, FIELD_INTEGER ), // Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_fWeapon, FIELD_BOOLEAN ),  // Don't restore, client needs reset
        // DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't restore, depends on server message after spawning and only matters in multiplayer
        // DEFINE_FIELD( CBasePlayer, m_vecAutoAim, FIELD_VECTOR ), // Don't save/restore - this is recomputed
        // DEFINE_ARRAY( CBasePlayer, m_rgAmmoLast, FIELD_INTEGER, MAX_AMMO_SLOTS ), // Don't need to restore
        // DEFINE_FIELD( CBasePlayer, m_fOnTarget, FIELD_BOOLEAN ), // Don't need to restore
        // DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't need to restore

};

LINK_ENTITY_TO_CLASS( player, CBasePlayer );

#include "core/player_network.h"

Vector CBasePlayer ::GetGunPosition()
{
	//	UTIL_MakeVectors(pev->v_angle);
	//	m_HackedGunPos = pev->view_ofs;
	Vector origin;

	origin = pev->origin + pev->view_ofs;

	return origin;
}

// Set the activity based on an event or current state
void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	int animDesired;
	float speed;
	char szAnim[64];

	speed = pev->velocity.Length2D();

	if ( pev->flags & FL_FROZEN )
	{
		speed      = 0;
		playerAnim = PLAYER_IDLE;
	}

	switch ( playerAnim )
	{
	case PLAYER_JUMP:
		m_IdealActivity = ACT_HOP;
		break;

	case PLAYER_SUPERJUMP:
		m_IdealActivity = ACT_LEAP;
		break;

	case PLAYER_DIE:
		m_IdealActivity = ACT_DIESIMPLE;
		m_IdealActivity = GetDeathActivity();
		break;

	case PLAYER_ATTACK1:
		switch ( m_Activity )
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_RANGE_ATTACK1;
			break;
		}
		break;
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if ( !FBitSet( pev->flags, FL_ONGROUND ) && ( m_Activity == ACT_HOP || m_Activity == ACT_LEAP ) ) // Still jumping
		{
			m_IdealActivity = m_Activity;
		}
		else if ( pev->waterlevel > 1 )
		{
			if ( speed == 0 )
				m_IdealActivity = ACT_HOVER;
			else
				m_IdealActivity = ACT_SWIM;
		}
		else
		{
			m_IdealActivity = ACT_WALK;
		}
		break;
	}

	switch ( m_IdealActivity )
	{
	case ACT_HOVER:
	case ACT_LEAP:
	case ACT_SWIM:
	case ACT_HOP:
	case ACT_DIESIMPLE:
	default:
		if ( m_Activity == m_IdealActivity )
			return;
		m_Activity = m_IdealActivity;

		animDesired = LookupActivity( m_Activity );
		// Already using the desired animation?
		if ( pev->sequence == animDesired )
			return;

		// ALERT(at_console, "Set die animation to %d\n", animDesired);

		pev->gaitsequence = 0;
		pev->sequence     = animDesired;
		pev->frame        = 0;
		ResetSequenceInfo();
		return;

	case ACT_RANGE_ATTACK1:
		if ( FBitSet( pev->flags, FL_DUCKING ) ) // crouching
			strcpy( szAnim, "crouch_shoot_" );
		else
			strcpy( szAnim, "ref_shoot_" );
		strcat( szAnim, m_szAnimExtention );
		animDesired = LookupSequence( szAnim );
		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		if ( !m_fSequenceLoops )
		{
			pev->effects |= EF_NOINTERP;
		}

		m_Activity = m_IdealActivity;

		pev->sequence = animDesired;
		ResetSequenceInfo();
		break;

	case ACT_WALK:
		if ( m_Activity != ACT_RANGE_ATTACK1 || m_fSequenceFinished )
		{
			if ( FBitSet( pev->flags, FL_DUCKING ) ) // crouching
				strcpy( szAnim, "crouch_aim_" );
			else
				strcpy( szAnim, "ref_aim_" );
			strcat( szAnim, m_szAnimExtention );
			animDesired = LookupSequence( szAnim );
			if ( animDesired == -1 )
				animDesired = 0;
			m_Activity = ACT_WALK;
		}
		else
		{
			animDesired = pev->sequence;
		}
	}

	if ( FBitSet( pev->flags, FL_DUCKING ) )
	{
		if ( speed == 0 )
		{
			pev->gaitsequence = LookupActivity( ACT_CROUCHIDLE );
			// pev->gaitsequence	= LookupActivity( ACT_CROUCH );
		}
		else
		{
			pev->gaitsequence = LookupActivity( ACT_CROUCH );
		}
	}
	else if ( speed > 220 )
	{
		pev->gaitsequence = LookupActivity( ACT_RUN );
	}
	else if ( speed > 0 )
	{
		pev->gaitsequence = LookupActivity( ACT_WALK );
	}
	else
	{
		// pev->gaitsequence	= LookupActivity( ACT_WALK );
		pev->gaitsequence = LookupSequence( "deep_idle" );
	}

	// Already using the desired animation?
	if ( pev->sequence == animDesired )
		return;

	// ALERT( at_console, "Set animation to %d\n", animDesired );

	// Reset to first frame of desired animation
	pev->sequence = animDesired;
	pev->frame    = 0;
	ResetSequenceInfo();
}

/*
===========
TabulateAmmo
This function is used to find and store
all the ammo we have into the ammo vars.
============
*/

/*
===========
WaterMove
============
*/
#define AIRTIME 12 // lung full of air lasts this many seconds

// TRUE if the player is attached to a ladder
BOOL CBasePlayer::IsOnLadder( void )
{
	return ( pev->movetype == MOVETYPE_FLY );
}

void CBasePlayer::PlayerDeathThink( void )
{
	float flForward;

	if ( FBitSet( pev->flags, FL_ONGROUND ) )
	{
		flForward = pev->velocity.Length() - 20;
		if ( flForward <= 0 )
			pev->velocity = g_vecZero;
		else
			pev->velocity = flForward * pev->velocity.Normalize();
	}

	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	if ( pev->modelindex && ( !m_fSequenceFinished ) && ( pev->deadflag == DEAD_DYING ) )
	{
		StudioFrameAdvance();

		m_flRespawnTimer += gpGlobals->frametime;
		if ( m_flRespawnTimer < 4.0f ) // 120 frames at 30fps -- animations should be no longer than this
			return;
	}

	if ( pev->deadflag == DEAD_DYING )
	{
		// Once we finish animating, if we're in multiplayer just make a copy of our body right away.
		if ( m_fSequenceFinished && g_pGameRules->IsMultiplayer() && pev->movetype == MOVETYPE_NONE )
		{
			CopyToBodyQue( pev );
			pev->modelindex = 0;
		}

		pev->deadflag = DEAD_DEAD;
	}

	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// Prevent corpse origin desync during concurrent respawn interaction
	if ( pev->movetype != MOVETYPE_NONE && FBitSet( pev->flags, FL_ONGROUND ) )
		pev->movetype = MOVETYPE_NONE;

	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->framerate = 0.0;

	BOOL fAnyButtonDown = ( pev->button & ~IN_SCORE );

	// wait for all buttons released
	if ( pev->deadflag == DEAD_DEAD )
	{
		if ( fAnyButtonDown )
			return;

		if ( g_pGameRules->FPlayerCanRespawn( this ) )
		{
			m_fDeadTime   = gpGlobals->time;
			pev->deadflag = DEAD_RESPAWNABLE;
		}

		return;
	}

	// if the player has been dead for one second longer than allowed by forcerespawn,
	// forcerespawn isn't on. Send the player off to an intermission camera until they
	// choose to respawn.
	if ( g_pGameRules->IsMultiplayer() && ( gpGlobals->time > ( m_fDeadTime + 6 ) ) && !( m_afPhysicsFlags & PFLAG_OBSERVER ) )
	{
		// go to dead camera.
		StartDeathCam();
	}

	if ( pev->iuser1 ) // player is in spectator mode
		return;

	// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if ( !fAnyButtonDown && !( g_pGameRules->IsMultiplayer() && forcerespawn.value > 0 && ( gpGlobals->time > ( m_fDeadTime + 5 ) ) ) )
		return;

	pev->button      = 0;
	m_flRespawnTimer = 0.0f;

	// ALERT(at_console, "Respawn\n");

	respawn( pev, !( m_afPhysicsFlags & PFLAG_OBSERVER ) ); // don't copy a corpse if we're in deathcam.
	pev->nextthink = -1;
}

//=========================================================
// StartDeathCam - find an intermission spot and send the
// player off into observer mode
//=========================================================
void CBasePlayer::StartDeathCam( void )
{
	edict_t *pSpot, *pNewSpot;
	int iRand;

	if ( pev->view_ofs == g_vecZero )
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	pSpot = FIND_ENTITY_BY_CLASSNAME( NULL, "info_intermission" );

	if ( !FNullEnt( pSpot ) )
	{
		// at least one intermission spot in the world.
		iRand = RANDOM_LONG( 0, 3 );

		while ( iRand > 0 )
		{
			pNewSpot = FIND_ENTITY_BY_CLASSNAME( pSpot, "info_intermission" );

			if ( pNewSpot )
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

		CopyToBodyQue( pev );

		UTIL_SetOrigin( pev, pSpot->v.origin );
		pev->angles = pev->v_angle = pSpot->v.v_angle;
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
		CopyToBodyQue( pev );
		UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, 128 ), ignore_monsters, edict(), &tr );

		UTIL_SetOrigin( pev, tr.vecEndPos );
		pev->angles = pev->v_angle = UTIL_VecToAngles( tr.vecEndPos - pev->origin );
	}

	// start death cam

	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->view_ofs   = g_vecZero;
	pev->fixangle   = TRUE;
	pev->solid      = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype   = MOVETYPE_NONE;
	pev->modelindex = 0;
}

void CBasePlayer::StartObserver( Vector vecPosition, Vector vecViewAngle )
{
	// clear any clientside entities attached to this player
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_KILLPLAYERATTACHMENTS );
	WRITE_BYTE( (BYTE)entindex() );
	MESSAGE_END();

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster();

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate( NULL, FALSE, 0 );

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0XFF );
	WRITE_BYTE( 0xFF );
	MESSAGE_END();

	// reset FOV
	m_iFOV = m_iClientFOV = 0;
	pev->fov              = m_iFOV;
	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
	WRITE_BYTE( 0 );
	MESSAGE_END();

	// Setup flags
	m_iHideHUD = ( HIDEHUD_HEALTH | HIDEHUD_WEAPONS );
	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->effects  = EF_NODRAW;
	pev->view_ofs = g_vecZero;
	pev->angles = pev->v_angle = vecViewAngle;
	pev->fixangle              = TRUE;
	pev->solid                 = SOLID_NOT;
	pev->takedamage            = DAMAGE_NO;
	pev->movetype              = MOVETYPE_NONE;
	ClearBits( m_afPhysicsFlags, PFLAG_DUCKING );
	ClearBits( pev->flags, FL_DUCKING );
	pev->deadflag = DEAD_RESPAWNABLE;
	pev->health   = 1;

	// Clear out the status bar
	m_fInitHUD = TRUE;

	pev->team = 0;
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
	WRITE_BYTE( ENTINDEX( edict() ) );
	WRITE_STRING( "" );
	MESSAGE_END();

	// Remove all the player's stuff
	RemoveAllItems( FALSE );

	// Move them to the new position
	UTIL_SetOrigin( pev, vecPosition );

	// Find a player to watch
	m_flNextObserverInput = 0;
	Observer_SetMode( m_iObserverLastMode );
}

//
// PlayerUse - handles USE keypress
//
#define PLAYER_SEARCH_RADIUS (float)64

//
// ID's player as such.
//
int CBasePlayer::Classify( void )
{
	return CLASS_PLAYER;
}

void CBasePlayer::AddPoints( int score, BOOL bAllowNegativeScore )
{
	// Positive score always adds
	if ( score < 0 )
	{
		if ( !bAllowNegativeScore )
		{
			if ( pev->frags < 0 ) // Can't go more negative
				return;

			if ( -score > pev->frags ) // Will this go negative?
			{
				score = -pev->frags; // Sum will be 0
			}
		}
	}

	pev->frags += score;

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
	WRITE_BYTE( ENTINDEX( edict() ) );
	WRITE_SHORT( pev->frags );
	WRITE_SHORT( m_iDeaths );
	WRITE_SHORT( 0 );
	WRITE_SHORT( g_pGameRules->GetTeamIndex( m_szTeamName ) + 1 );
	MESSAGE_END();
}

void CBasePlayer::AddPointsToTeam( int score, BOOL bAllowNegativeScore )
{
	int index = entindex();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && i != index )
		{
			if ( g_pGameRules->PlayerRelationship( this, pPlayer ) == GR_TEAMMATE )
			{
				pPlayer->AddPoints( score, bAllowNegativeScore );
			}
		}
	}
}

// Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0;
}

void CBasePlayer::UpdateStatusBar()
{
	int newSBarState[SBAR_END];
	char sbuf0[SBAR_STRING_SIZE];
	char sbuf1[SBAR_STRING_SIZE];

	memset( newSBarState, 0, sizeof( newSBarState ) );
	strcpy( sbuf0, m_SbarString0 );
	strcpy( sbuf1, m_SbarString1 );

	// Find an ID Target
	TraceResult tr;
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector vecSrc = EyePosition();
	Vector vecEnd = vecSrc + ( gpGlobals->v_forward * MAX_ID_RANGE );
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr );

	if ( tr.flFraction != 1.0 )
	{
		if ( !FNullEnt( tr.pHit ) )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

			if ( pEntity->Classify() == CLASS_PLAYER )
			{
				newSBarState[SBAR_ID_TARGETNAME] = ENTINDEX( pEntity->edict() );
				strcpy( sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%" );

				// allies and medics get to see the targets health
				if ( g_pGameRules->PlayerRelationship( this, pEntity ) == GR_TEAMMATE )
				{
					newSBarState[SBAR_ID_TARGETHEALTH] = 100 * ( pEntity->pev->health / pEntity->pev->max_health );
					newSBarState[SBAR_ID_TARGETARMOR]  = pEntity->pev->armorvalue; // No need to get it % based since 100 it's the max.
				}

				m_flStatusBarDisappearDelay = gpGlobals->time + 1.0;
			}
		}
		else if ( m_flStatusBarDisappearDelay > gpGlobals->time )
		{
			// hold the values for a short amount of time after viewing the object
			newSBarState[SBAR_ID_TARGETNAME]   = m_izSBarState[SBAR_ID_TARGETNAME];
			newSBarState[SBAR_ID_TARGETHEALTH] = m_izSBarState[SBAR_ID_TARGETHEALTH];
			newSBarState[SBAR_ID_TARGETARMOR]  = m_izSBarState[SBAR_ID_TARGETARMOR];
		}
	}

	BOOL bForceResend = FALSE;

	if ( strcmp( sbuf0, m_SbarString0 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( sbuf0 );
		MESSAGE_END();

		strcpy( m_SbarString0, sbuf0 );

		// make sure everything's resent
		bForceResend = TRUE;
	}

	if ( strcmp( sbuf1, m_SbarString1 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_STRING( sbuf1 );
		MESSAGE_END();

		strcpy( m_SbarString1, sbuf1 );

		// make sure everything's resent
		bForceResend = TRUE;
	}

	// Check values and send if they don't match
	for ( int i = 1; i < SBAR_END; i++ )
	{
		if ( newSBarState[i] != m_izSBarState[i] || bForceResend )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusValue, NULL, pev );
			WRITE_BYTE( i );
			WRITE_SHORT( newSBarState[i] );
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
}

#define CLIMB_SHAKE_FREQUENCY 22 // how many frames in between screen shakes when climbing
#define MAX_CLIMB_SPEED 200      // fastest vertical climbing speed possible
#define CLIMB_SPEED_DEC 15       // climbing deceleration rate
#define CLIMB_PUNCH_X -7         // how far to 'punch' client X axis when climbing
#define CLIMB_PUNCH_Z 7          // how far to 'punch' client Z axis when climbing

/* Time based Damage works as follows:
    1) There are several types of timebased damage:

        #define DMG_PARALYZE		(1 << 14)	// slows affected creature down
        #define DMG_NERVEGAS		(1 << 15)	// nerve toxins, very bad
        #define DMG_POISON			(1 << 16)	// blood poisioning
        #define DMG_RADIATION		(1 << 17)	// radiation exposure
        #define DMG_DROWNRECOVER	(1 << 18)	// drown recovery
        #define DMG_ACID			(1 << 19)	// toxic chemicals or acid burns
        #define DMG_SLOWBURN		(1 << 20)	// in an oven
        #define DMG_SLOWFREEZE		(1 << 21)	// in a subzero freezer

    2) A new hit inflicting tbd restarts the tbd counter - each monster has an 8bit counter,
        per damage type. The counter is decremented every second, so the maximum time
        an effect will last is 255/60 = 4.25 minutes.  Of course, staying within the radius
        of a damaging effect like fire, nervegas, radiation will continually reset the counter to max.

    3) Every second that a tbd counter is running, the player takes damage.  The damage
        is determined by the type of tdb.
            Paralyze		- 1/2 movement rate, 30 second duration.
            Nervegas		- 5 points per second, 16 second duration = 80 points max dose.
            Poison			- 2 points per second, 25 second duration = 50 points max dose.
            Radiation		- 1 point per second, 50 second duration = 50 points max dose.
            Drown			- 5 points per second, 2 second duration.
            Acid/Chemical	- 5 points per second, 10 second duration = 50 points max.
            Burn			- 10 points per second, 2 second duration.
            Freeze			- 3 points per second, 10 second duration = 30 points max.

    4) Certain actions or countermeasures counteract the damaging effects of tbds:

        Armor/Heater/Cooler - Chemical(acid),burn, freeze all do damage to armor power, then to body
        - recharged by suit recharger
        Air In Lungs		- drowning damage is done to air in lungs first, then to body
        - recharged by poking head out of water
        - 10 seconds if swiming fast
        Air In SCUBA		- drowning damage is done to air in tanks first, then to body
        - 2 minutes in tanks. Need new tank once empty.
        Radiation Syringe	- Each syringe full provides protection vs one radiation dosage
        Antitoxin Syringe	- Each syringe full provides protection vs one poisoning (nervegas or poison).
        Health kit			- Immediate stop to acid/chemical, fire or freeze damage.
        Radiation Shower	- Immediate stop to radiation damage, acid/chemical or fire damage.


*/

// If player is taking time based damage, continue doing damage to player -
// this simulates the effect of being poisoned, gassed, dosed with radiation etc -
// anything that continues to do damage even after the initial contact stops.
// Update all time based damage counters, and shut off any that are done.

// The m_bitsDamageType bit MUST be set if any damage is to be taken.
// This routine will detect the initial on value of the m_bitsDamageType
// and init the appropriate counter.  Only processes damage every second.

// #define PARALYZE_DURATION	30		// number of 2 second intervals to take damage
// #define PARALYZE_DAMAGE		0.0		// damage to take each 2 second interval

// #define NERVEGAS_DURATION	16
// #define NERVEGAS_DAMAGE		5.0

// #define POISON_DURATION		25
// #define POISON_DAMAGE		2.0

// #define RADIATION_DURATION	50
// #define RADIATION_DAMAGE	1.0

// #define ACID_DURATION		10
// #define ACID_DAMAGE			5.0

// #define SLOWBURN_DURATION	2
// #define SLOWBURN_DAMAGE		1.0

// #define SLOWFREEZE_DURATION	1.0
// #define SLOWFREEZE_DAMAGE	3.0

// checks if the spot is clear of players
BOOL IsSpawnPointValid( CBaseEntity *pPlayer, CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	if ( !pSpot->IsTriggered( pPlayer ) )
	{
		return FALSE;
	}

	while ( ( ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 128 ) ) != NULL )
	{
		// if ent is a client, don't spawn on 'em
		if ( ent->IsPlayer() && ent != pPlayer )
			return FALSE;
	}

	return TRUE;
}

DLL_GLOBAL CBaseEntity *g_pLastSpawn;
inline int FNullEnt( CBaseEntity *ent )
{
	return ( ent == NULL ) || FNullEnt( ent->edict() );
}

/*
============
EntSelectSpawnPoint

Returns the entity to spawn at

USES AND SETS GLOBAL g_pLastSpawn
============
*/
edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer )
{
	CBaseEntity *pSpot;
	edict_t *player;

	int nNumRandomSpawnsToTry = 10;

	player = pPlayer->edict();

	// choose a info_player_deathmatch point
	if ( g_pGameRules->IsCoOp() )
	{
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_coop" );
		if ( !FNullEnt( pSpot ) )
			goto ReturnSpot;
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_start" );
		if ( !FNullEnt( pSpot ) )
			goto ReturnSpot;
	}
	else if ( g_pGameRules->IsDeathmatch() )
	{
		if ( NULL == g_pLastSpawn )
		{
			int nNumSpawnPoints = 0;
			CBaseEntity *pEnt   = UTIL_FindEntityByClassname( NULL, "info_player_deathmatch" );
			while ( NULL != pEnt )
			{
				nNumSpawnPoints++;
				pEnt = UTIL_FindEntityByClassname( pEnt, "info_player_deathmatch" );
			}
			nNumRandomSpawnsToTry = nNumSpawnPoints;
		}

		pSpot = g_pLastSpawn;
		// Randomize the start spot
		for ( int i = RANDOM_LONG( 1, nNumRandomSpawnsToTry - 1 ); i > 0; i-- )
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		if ( FNullEnt( pSpot ) ) // skip over the null point
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );

		CBaseEntity *pFirstSpot = pSpot;

		do
		{
			if ( pSpot )
			{
				// check if pSpot is valid
				if ( IsSpawnPointValid( pPlayer, pSpot ) )
				{
					if ( pSpot->pev->origin == Vector( 0, 0, 0 ) )
					{
						pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
						continue;
					}

					// if so, go to pSpot
					goto ReturnSpot;
				}
			}
			// increment pSpot
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

		// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
		if ( !FNullEnt( pSpot ) )
		{
			CBaseEntity *ent = NULL;
			while ( ( ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 128 ) ) != NULL )
			{
				// if ent is a client, kill em (unless they are ourselves)
				if ( ent->IsPlayer() && !( ent->edict() == player ) )
					ent->TakeDamage( VARS( INDEXENT( 0 ) ), VARS( INDEXENT( 0 ) ), 300, DMG_GENERIC );
			}
			goto ReturnSpot;
		}
	}

	// If startspot is set, (re)spawn there.
	if ( FStringNull( gpGlobals->startspot ) || !strlen( STRING( gpGlobals->startspot ) ) )
	{
		pSpot = UTIL_FindEntityByClassname( NULL, "info_player_start" );
		if ( !FNullEnt( pSpot ) )
			goto ReturnSpot;
	}
	else
	{
		pSpot = UTIL_FindEntityByTargetname( NULL, STRING( gpGlobals->startspot ) );
		if ( !FNullEnt( pSpot ) )
			goto ReturnSpot;
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT( at_error, "PutClientInServer: no info_player_start on level" );
		return INDEXENT( 0 );
	}

	g_pLastSpawn = pSpot;
	return pSpot->edict();
}

void CBasePlayer::Spawn( void )
{
	m_flStartCharge = gpGlobals->time;

	pev->classname  = MAKE_STRING( "player" );
	pev->health     = 100;
	pev->armorvalue = 0;
	pev->takedamage = DAMAGE_AIM;
	pev->solid      = SOLID_SLIDEBOX;
	pev->movetype   = MOVETYPE_WALK;
	pev->max_health = pev->health;
	pev->flags &= FL_PROXY; // keep proxy flag sey by engine
	pev->flags |= FL_CLIENT;
	pev->air_finished = gpGlobals->time + 12;
	pev->dmg          = 2; // initial water damage
	pev->effects      = 0;
	pev->deadflag     = DEAD_NO;
	pev->dmg_take     = 0;
	pev->dmg_save     = 0;
	pev->friction     = 1.0;
	pev->gravity      = 1.0;
	m_bitsHUDDamage   = -1;
	m_bitsDamageType  = 0;
	m_afPhysicsFlags  = 0;
	m_fLongJump       = FALSE; // no longjump module.

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	pev->fov = m_iFOV = 0;  // init field of view.
	m_iClientFOV      = -1; // make sure fov reset is sent

	m_flNextDecalTime = 0; // let this player decal as soon as he spawns.

	m_flgeigerDelay = gpGlobals->time + 2.0; // wait a few seconds until user-defined message registrations
	                                         // are recieved by all clients

	m_flTimeStepSound = 0;
	m_iStepLeft       = 0;
	m_flFieldOfView   = 0.5; // some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor   = BLOOD_COLOR_RED;
	m_flNextAttack = UTIL_WeaponTimeBase();
	StartSneaking();

	m_iFlashBattery    = 99;
	m_flFlashLightTime = 1; // force first message

	// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam( this );
	g_pGameRules->GetPlayerSpawnSpot( this );

	SET_MODEL( ENT( pev ), "models/player.mdl" );
	g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence        = LookupActivity( ACT_IDLE );

	if ( FBitSet( pev->flags, FL_DUCKING ) )
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );

	pev->view_ofs = VEC_VIEW;
	Precache();
	m_HackedGunPos = Vector( 0, 32, 0 );

	if ( m_iPlayerSound == SOUNDLIST_EMPTY )
	{
		ALERT( at_console, "Couldn't alloc player sound slot!\n" );
	}

	m_fNoPlayerSound = FALSE; // normal sound behavior.

	m_pLastItem         = NULL;
	m_fInitHUD          = TRUE;
	m_iClientHideHUD    = -1; // force this to be recalculated
	m_fWeapon           = FALSE;
	m_pClientActiveItem = NULL;
	m_iClientBattery    = -1;

	// reset all ammo values to 0
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_rgAmmo[i]     = 0;
		m_rgAmmoLast[i] = 0; // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	m_lastx = m_lasty = 0;

	m_flNextChatTime = gpGlobals->time;

	g_pGameRules->PlayerSpawn( this );
}

void CBasePlayer ::Precache( void )
{
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if ( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if ( !WorldGraph.FSetGraphPointers() )
		{
			ALERT( at_console, "**Graph pointers were not set!\n" );
		}
		else
		{
			ALERT( at_console, "**Graph Pointers Set!\n" );
		}
	}

	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	// init geiger counter vars during spawn and each time
	// we cross a level transition

	m_flgeigerRange    = 1000;
	m_igeigerRangePrev = 1000;

	m_bitsDamageType = 0;
	m_bitsHUDDamage  = -1;

	m_iClientBattery = -1;

	m_iTrain = TRAIN_NEW;

	// Make sure any necessary user messages have been registered
	LinkUserMessages();

	m_iUpdateTime = 5; // won't update for 1/2 a second

	if ( gInitHUD )
		m_fInitHUD = TRUE;
}

int CBasePlayer::Save( CSave &save )
{
	if ( !CBaseMonster::Save( save ) )
		return 0;

	return save.WriteFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE( m_playerSaveData ) );
}

int CBasePlayer::Restore( CRestore &restore )
{
	if ( !CBaseMonster::Restore( restore ) )
		return 0;

	int status = restore.ReadFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE( m_playerSaveData ) );

	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;
	// landmark isn't present.
	if ( !pSaveData->fUseLandmark )
	{
		ALERT( at_console, "No Landmark:%s\n", pSaveData->szLandmarkName );

		// default to normal spawn
		edict_t *pentSpawnSpot = EntSelectSpawnPoint( this );
		pev->origin            = VARS( pentSpawnSpot )->origin + Vector( 0, 0, 1 );
		pev->angles            = VARS( pentSpawnSpot )->angles;
	}
	pev->v_angle.z = 0; // Clear out roll
	pev->angles    = pev->v_angle;

	pev->fixangle = TRUE; // turn this way immediately

	// Copied from spawn() for now
	m_bloodColor = BLOOD_COLOR_RED;

	g_ulModelIndexPlayer = pev->modelindex;

	if ( FBitSet( pev->flags, FL_DUCKING ) )
	{
		// Apply duck hull collision adjustment
		// FixPlayerCrouchStuck( edict() );
		// Don't need to do this with new player prediction code.
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	}
	else
	{
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	}

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	if ( m_fLongJump )
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "1" );
	}
	else
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	}

	RenewItems();

	// Resync ammo data so you can reload - Solokiller
	TabulateAmmo();

#if defined( CLIENT_WEAPONS )
	// Monster time tracking variable reused for player interval calculation
	//			as just a counter.  Ideally, this needs its own variable that's saved as a plain float.
	//			Barring that, we clear it out here instead of using the incorrect restored time value.
	m_flNextAttack = UTIL_WeaponTimeBase();
#endif

	return status;
}

const char *CBasePlayer::TeamID( void )
{
	if ( pev == NULL ) // Not fully connected yet
		return "";

	// return their team name
	return m_szTeamName;
}

CBaseEntity *FindEntityForward( CBaseEntity *pMe )
{
	TraceResult tr;

	UTIL_MakeVectors( pMe->pev->v_angle );
	UTIL_TraceLine( pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr );
	if ( tr.flFraction != 1.0 && !FNullEnt( tr.pHit ) )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		return pHit;
	}
	return NULL;
}

/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
void CBasePlayer ::ForceClientDllUpdate( void )
{
	m_iClientHealth  = -1;
	m_iClientBattery = -1;
	m_iTrain |= TRAIN_NEW; // Force new train message.
	m_fWeapon    = FALSE;  // Force weapon send
	m_fKnownItem = FALSE;  // Force weaponinit messages.
	m_fInitHUD   = TRUE;   // Force HUD gmsgResetHUD message

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();
}

/*
============
ImpulseCommands
============
*/

//=========================================================
//=========================================================

/*
============
ItemPreFrame

Called every frame by the player PreThink
============
*/

/*
============
ItemPostFrame

Called every frame by the player PostThink
============
*/

void CBasePlayer ::SetPrefsFromUserinfo( char *infobuffer )
{
	const char *pszKeyVal;

	// Set autoswitch preference
	pszKeyVal = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_autowepswitch" );
	if ( FStrEq( pszKeyVal, "" ) )
	{
		m_iAutoWepSwitch = 1;
	}
	else
	{
		m_iAutoWepSwitch = atoi( pszKeyVal );
	}
}

//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
