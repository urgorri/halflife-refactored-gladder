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
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "ai/soundent.h"
#include "ai/basemonster.h"
#include "gameplay/gamerules.h"
#include "core/game.h"
#include "core/player_network.h"

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;

/*
THE POWER SUIT

The Suit provides 3 main functions: Protection, Notification and Augmentation.
Some functions are automatic, some require power.
The player gets the suit shortly after getting off the train in C1A0 and it stays
with him for the entire game.

Protection

    Heat/Cold
        When the player enters a hot/cold area, the heating/cooling indicator on the suit
        will come on and the battery will drain while the player stays in the area.
        After the battery is dead, the player starts to take damage.
        This feature is built into the suit and is automatically engaged.
    Radiation Syringe
        This will cause the player to be immune from the effects of radiation for N seconds. Single use item.
    Anti-Toxin Syringe
        This will cure the player from being poisoned. Single use item.
    Health
        Small (1st aid kits, food, etc.)
        Large (boxes on walls)
    Armor
        The armor works using energy to create a protective field that deflects a
        percentage of damage projectile and explosive attacks. After the armor has been deployed,
        it will attempt to recharge itself to full capacity with the energy reserves from the battery.
        It takes the armor N seconds to fully charge.

Notification (via the HUD)

x	Health
x	Ammo
x	Automatic Health Care
        Notifies the player when automatic healing has been engaged.
x	Geiger counter
        Classic Geiger counter sound and status bar at top of HUD
        alerts player to dangerous levels of radiation. This is not visible when radiation levels are normal.
x	Poison
    Armor
        Displays the current level of armor.

Augmentation

    Reanimation (w/adrenaline)
        Causes the player to come back to life after he has been dead for 3 seconds.
        Will not work if player was gibbed. Single use.
    Long Jump
        Used by hitting the ??? key(s). Caused the player to further than normal.
    SCUBA
        Used automatically after picked up and after player enters the water.
        Works for N seconds. Single use.

Things powered by the battery

    Armor
        Uses N watts for every M units of damage.
    Heat/Cool
        Uses N watts for every second in hot/cold area.
    Long Jump
        Uses N watts for every jump.
    Alien Cloak
        Uses N watts for each use. Each use lasts M seconds.
    Alien Shield
        Augments armor. Reduces Armor drain by one half

*/

// if in range of radiation source, ping geiger counter

#define GEIGERDELAY 0.25

void CBasePlayer ::UpdateGeigerCounter( void )
{
	BYTE range;

	// delay per update ie: don't flood net with these msgs
	if ( gpGlobals->time < m_flgeigerDelay )
		return;

	m_flgeigerDelay = gpGlobals->time + GEIGERDELAY;

	// send range to radition source to client

	range = (BYTE)( m_flgeigerRange / 4 );

	if ( range != m_igeigerRangePrev )
	{
		m_igeigerRangePrev = range;

		MESSAGE_BEGIN( MSG_ONE, gmsgGeigerRange, NULL, pev );
		WRITE_BYTE( range );
		MESSAGE_END();
	}

	// reset counter and semaphore
	if ( !RANDOM_LONG( 0, 3 ) )
		m_flgeigerRange = 1000;
}

/*
================
CheckSuitUpdate

Play suit update if it's time
================
*/

#define SUITUPDATETIME 3.5
#define SUITFIRSTUPDATETIME 0.1

void CBasePlayer::CheckSuitUpdate()
{
	int i;
	int isentence = 0;
	int isearch   = m_iSuitPlayNext;

	// Ignore suit updates if no suit
	if ( !( pev->weapons & ( 1 << WEAPON_SUIT ) ) )
		return;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();

	if ( g_pGameRules->IsMultiplayer() )
	{
		// don't bother updating HEV voice in multiplayer.
		return;
	}

	if ( gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0 )
	{
		// play a sentence off of the end of the queue
		for ( i = 0; i < CSUITPLAYLIST; i++ )
		{
			if ( isentence = m_rgSuitPlayList[isearch] )
				break;

			if ( ++isearch == CSUITPLAYLIST )
				isearch = 0;
		}

		if ( isentence )
		{
			m_rgSuitPlayList[isearch] = 0;
			if ( isentence > 0 )
			{
				// play sentence number

				char sentence[CBSENTENCENAME_MAX + 1];
				snprintf( sentence, sizeof( sentence ), "!%s", gszallsentencenames[isentence] );
				EMIT_SOUND_SUIT( ENT( pev ), sentence );
			}
			else
			{
				// play sentence group
				EMIT_GROUPID_SUIT( ENT( pev ), -isentence );
			}
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
		}
		else
			// queue is empty, don't check
			m_flSuitUpdate = 0;
	}
}

// add sentence to suit playlist queue. if fgroup is true, then
// name is a sentence group (HEV_AA), otherwise name is a specific
// sentence name ie: !HEV_AA0.  If iNoRepeat is specified in
// seconds, then we won't repeat playback of this word or sentence
// for at least that number of seconds.

void CBasePlayer::SetSuitUpdate( char *name, int fgroup, int iNoRepeatTime )
{
	int i;
	int isentence;
	int iempty = -1;

	// Ignore suit updates if no suit
	if ( !( pev->weapons & ( 1 << WEAPON_SUIT ) ) )
		return;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// due to static channel design, etc. We don't play HEV sounds in multiplayer right now.
		return;
	}

	// if name == NULL, then clear out the queue

	if ( !name )
	{
		for ( i = 0; i < CSUITPLAYLIST; i++ )
			m_rgSuitPlayList[i] = 0;
		return;
	}
	// get sentence or group number
	if ( !fgroup )
	{
		isentence = SENTENCEG_Lookup( name, NULL );
		if ( isentence < 0 )
			return;
	}
	else
		// mark group number as negative
		isentence = -SENTENCEG_GetIndex( name );

	// check norepeat list - this list lets us cancel
	// the playback of words or sentences that have already
	// been played within a certain time.

	for ( i = 0; i < CSUITNOREPEAT; i++ )
	{
		if ( isentence == m_rgiSuitNoRepeat[i] )
		{
			// this sentence or group is already in
			// the norepeat list

			if ( m_rgflSuitNoRepeatTime[i] < gpGlobals->time )
			{
				// norepeat time has expired, clear it out
				m_rgiSuitNoRepeat[i]      = 0;
				m_rgflSuitNoRepeatTime[i] = 0.0;
				iempty                    = i;
				break;
			}
			else
			{
				// don't play, still marked as norepeat
				return;
			}
		}
		// keep track of empty slot
		if ( !m_rgiSuitNoRepeat[i] )
			iempty = i;
	}

	// sentence is not in norepeat list, save if norepeat time was given

	if ( iNoRepeatTime )
	{
		if ( iempty < 0 )
			iempty = RANDOM_LONG( 0, CSUITNOREPEAT - 1 ); // pick random slot to take over
		m_rgiSuitNoRepeat[iempty]      = isentence;
		m_rgflSuitNoRepeatTime[iempty] = iNoRepeatTime + gpGlobals->time;
	}

	// find empty spot in queue, or overwrite last spot

	m_rgSuitPlayList[m_iSuitPlayNext++] = isentence;
	if ( m_iSuitPlayNext == CSUITPLAYLIST )
		m_iSuitPlayNext = 0;

	if ( m_flSuitUpdate <= gpGlobals->time )
	{
		if ( m_flSuitUpdate == 0 )
			// play queue is empty, don't delay too long before playback
			m_flSuitUpdate = gpGlobals->time + SUITFIRSTUPDATETIME;
		else
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
	}
}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
void CheckPowerups( entvars_t *pev )
{
	if ( pev->health <= 0 )
		return;

	pev->modelindex = g_ulModelIndexPlayer; // don't use eyes
}

//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
void CBasePlayer ::UpdatePlayerSound( void )
{
	int iBodyVolume;
	int iVolume;
	CSound *pSound;

	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt ::ClientSoundIndex( edict() ) );

	if ( !pSound )
	{
		ALERT( at_console, "Client lost reserved sound!\n" );
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.

	if ( FBitSet( pev->flags, FL_ONGROUND ) )
	{
		iBodyVolume = pev->velocity.Length();

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast.
		if ( iBodyVolume > 512 )
		{
			iBodyVolume = 512;
		}
	}
	else
	{
		iBodyVolume = 0;
	}

	if ( pev->button & IN_JUMP )
	{
		iBodyVolume += 100;
	}

	// convert player move speed and actions into sound audible by monsters.
	if ( m_iWeaponVolume > iBodyVolume )
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player.
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if ( m_iWeaponVolume < 0 )
	{
		iVolume = 0;
	}

	// if target volume is greater than the player sound's current volume, we paste the new volume in
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if ( m_iTargetVolume > iVolume )
	{
		iVolume = m_iTargetVolume;
	}
	else if ( iVolume > m_iTargetVolume )
	{
		iVolume -= 250 * gpGlobals->frametime;

		if ( iVolume < m_iTargetVolume )
		{
			iVolume = 0;
		}
	}

	if ( m_fNoPlayerSound )
	{
		// debugging flag, lets players move around and shoot without monsters hearing.
		iVolume = 0;
	}

	if ( gpGlobals->time > m_flStopExtraSoundTime )
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if ( pSound )
	{
		pSound->m_vecOrigin = pev->origin;
		pSound->m_iType |= ( bits_SOUND_PLAYER | m_iExtraSoundTypes );
		pSound->m_iVolume = iVolume;
	}

	// keep track of virtual muzzle flash
	m_iWeaponFlash -= 256 * gpGlobals->frametime;
	if ( m_iWeaponFlash < 0 )
		m_iWeaponFlash = 0;

	// UTIL_MakeVectors ( pev->angles );
	// gpGlobals->v_forward.z = 0;

	// Below are a couple of useful little bits that make it easier to determine just how much noise the
	// player is making.
	// UTIL_ParticleEffect ( pev->origin + gpGlobals->v_forward * iVolume, g_vecZero, 255, 25 );
	// ALERT ( at_console, "%d/%d\n", iVolume, m_iTargetVolume );
}

BOOL CBasePlayer ::FlashlightIsOn( void )
{
	return FBitSet( pev->effects, EF_DIMLIGHT );
}

void CBasePlayer ::FlashlightTurnOn( void )
{
	if ( !g_pGameRules->FAllowFlashlight() )
	{
		return;
	}

	if ( ( pev->weapons & ( 1 << WEAPON_SUIT ) ) )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM );
		SetBits( pev->effects, EF_DIMLIGHT );
		MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_BYTE( m_iFlashBattery );
		MESSAGE_END();

		m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
	}
}

void CBasePlayer ::FlashlightTurnOff( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM );
	ClearBits( pev->effects, EF_DIMLIGHT );
	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( m_iFlashBattery );
	MESSAGE_END();

	m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
}
