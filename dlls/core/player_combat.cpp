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
#include "ai/basemonster.h"
#include "ai/monsters.h"
#include "ai/soundent.h"
#include "weapons/weapon_base.h"
#include "gameplay/gamerules.h"
#include "gameplay/spectator.h"
#include "core/game.h"
#include "core/decals.h"
#include "shake.h"
#include "hltv.h"
#include "core/player_network.h"

extern void CopyToBodyQue( entvars_t *pev );
extern DLL_GLOBAL int g_iSkillLevel;
extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;

void CBasePlayer ::Pain( void )
{
	float flRndSound; // sound randomizer

	flRndSound = RANDOM_FLOAT( 0, 1 );

	if ( flRndSound <= 0.33 )
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM );
	else if ( flRndSound <= 0.66 )
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM );
	else
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM );
}

/*
 *
 */
Vector VecVelocityForDamage( float flDamage )
{
	Vector vec( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );

	if ( flDamage > -50 )
		vec = vec * 0.7;
	else if ( flDamage > -200 )
		vec = vec * 2;
	else
		vec = vec * 10;

	return vec;
}

void CBasePlayer ::DeathSound( void )
{
	// temporarily using pain sounds for death sounds
	switch ( RANDOM_LONG( 1, 5 ) )
	{
	case 1:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM );
		break;
	case 3:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM );
		break;
	}

	// play one of the suit death alarms
	EMIT_GROUPNAME_SUIT( ENT( pev ), "HEV_DEAD" );
}

// override takehealth
// bitsDamageType indicates type of damage healed.

int CBasePlayer ::TakeHealth( float flHealth, int bitsDamageType )
{
	return CBaseMonster ::TakeHealth( flHealth, bitsDamageType );
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer ::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			flDamage *= gSkillData.plrHead;
			break;
		case HITGROUP_CHEST:
			flDamage *= gSkillData.plrChest;
			break;
		case HITGROUP_STOMACH:
			flDamage *= gSkillData.plrStomach;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			flDamage *= gSkillData.plrArm;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= gSkillData.plrLeg;
			break;
		default:
			break;
		}

		SpawnBlood( ptr->vecEndPos, BloodColor(), flDamage ); // a little surface blood.
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}
}

/*
    Take some damage.
    NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
    type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
    etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define ARMOR_RATIO 0.2 // Armor Takes 80% of the damage
#define ARMOR_BONUS 0.5 // Each Point of Armor is work 1/x points of health

static void CalculateArmorAbsorption( float &flDamage, float &flArmorValue, int bitsDamageType, BOOL bIsMultiplayer )
{
	// Armor doesn't protect against fall or drown damage, or if depleted
	if ( flArmorValue <= 0.0f || ( bitsDamageType & ( DMG_FALL | DMG_DROWN ) ) )
		return;

	float flBonus = ARMOR_BONUS;
	float flRatio = ARMOR_RATIO;

	if ( ( bitsDamageType & DMG_BLAST ) && bIsMultiplayer )
	{
		// Blasts damage armor more in multiplayer
		flBonus *= 2;
	}

	float flNew   = flDamage * flRatio;
	float flArmor = ( flDamage - flNew ) * flBonus;

	// Does this use more armor than we have?
	if ( flArmor > flArmorValue )
	{
		flArmor = flArmorValue;
		flArmor *= ( 1.0f / flBonus );
		flNew        = flDamage - flArmor;
		flArmorValue = 0;
	}
	else
	{
		flArmorValue -= flArmor;
	}

	flDamage = flNew;
}

static void UpdateSuitDamageDiagnosis( CBasePlayer *pPlayer, int bitsDamage, BOOL fTookDamage, BOOL ftrivial, BOOL fmajor, float flLastDamage )
{
	if ( !fTookDamage || ( ftrivial && !( bitsDamage & DMG_TIMEBASED ) ) )
		return;

	if ( bitsDamage & DMG_CLUB )
	{
		if ( fmajor )
			pPlayer->SetSuitUpdate( "!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC ); // minor fracture
	}

	if ( bitsDamage & ( DMG_FALL | DMG_CRUSH ) )
	{
		if ( fmajor )
			pPlayer->SetSuitUpdate( "!HEV_DMG5", FALSE, SUIT_NEXT_IN_30SEC ); // major fracture
		else
			pPlayer->SetSuitUpdate( "!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC ); // minor fracture
	}

	if ( bitsDamage & DMG_BULLET )
	{
		if ( flLastDamage > 5 )
			pPlayer->SetSuitUpdate( "!HEV_DMG6", FALSE, SUIT_NEXT_IN_30SEC ); // blood loss detected
	}

	if ( bitsDamage & DMG_SLASH )
	{
		if ( fmajor )
			pPlayer->SetSuitUpdate( "!HEV_DMG1", FALSE, SUIT_NEXT_IN_30SEC ); // major laceration
		else
			pPlayer->SetSuitUpdate( "!HEV_DMG0", FALSE, SUIT_NEXT_IN_30SEC ); // minor laceration
	}

	if ( bitsDamage & DMG_SONIC )
	{
		if ( fmajor )
			pPlayer->SetSuitUpdate( "!HEV_DMG2", FALSE, SUIT_NEXT_IN_1MIN ); // internal bleeding
	}

	if ( bitsDamage & ( DMG_POISON | DMG_PARALYZE ) )
	{
		pPlayer->SetSuitUpdate( "!HEV_DMG3", FALSE, SUIT_NEXT_IN_1MIN ); // blood toxins detected
	}

	if ( bitsDamage & DMG_ACID )
	{
		pPlayer->SetSuitUpdate( "!HEV_DET1", FALSE, SUIT_NEXT_IN_1MIN ); // hazardous chemicals detected
	}

	if ( bitsDamage & DMG_NERVEGAS )
	{
		pPlayer->SetSuitUpdate( "!HEV_DET0", FALSE, SUIT_NEXT_IN_1MIN ); // biohazard detected
	}

	if ( bitsDamage & DMG_RADIATION )
	{
		pPlayer->SetSuitUpdate( "!HEV_DET2", FALSE, SUIT_NEXT_IN_1MIN ); // radiation detected
	}
}

int CBasePlayer ::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Already dead
	if ( !IsAlive() )
		return 0;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	if ( !g_pGameRules->FPlayerCanTakeDamage( this, pAttacker ) )
	{
		// Refuse the damage
		return 0;
	}

	float flHealthPrev = pev->health;

	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// Calculate and drain armor absorption
	CalculateArmorAbsorption( flDamage, pev->armorvalue, bitsDamageType, g_pGameRules->IsMultiplayer() );

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	int fTookDamage = CBaseMonster::TakeDamage( pevInflictor, pevAttacker, (int)flDamage, bitsDamageType );

	// reset damage time countdown for each type of time based damage player just sustained
	for ( int i = 0; i < CDMG_TIMEBASED; i++ )
	{
		if ( bitsDamageType & ( DMG_PARALYZE << i ) )
			m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
	WRITE_BYTE( 9 );                                // command length in bytes
	WRITE_BYTE( DRC_CMD_EVENT );                    // take damage event
	WRITE_SHORT( ENTINDEX( this->edict() ) );       // index number of primary entity
	WRITE_SHORT( ENTINDEX( ENT( pevInflictor ) ) ); // index number of secondary entity
	WRITE_LONG( 5 );                                // eventflags (priority and flags)
	MESSAGE_END();

	// how bad is it, doc?
	BOOL ftrivial  = ( pev->health > 75 || m_lastDamageAmount < 5 );
	BOOL fmajor    = ( m_lastDamageAmount > 25 );
	BOOL fcritical = ( pev->health < 30 );

	m_bitsDamageType |= bitsDamageType; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;               // make sure the damage bits get resent

	UpdateSuitDamageDiagnosis( this, bitsDamageType, fTookDamage, ftrivial, fmajor, m_lastDamageAmount );

	pev->punchangle.x = -2;

	if ( fTookDamage && !ftrivial && fmajor && flHealthPrev >= 75 )
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate( "!HEV_MED1", FALSE, SUIT_NEXT_IN_30MIN ); // automedic on

		// give morphine shot if not given recently
		SetSuitUpdate( "!HEV_HEAL7", FALSE, SUIT_NEXT_IN_30MIN ); // morphine shot
	}

	if ( fTookDamage && !ftrivial && fcritical && flHealthPrev < 75 )
	{

		// already took major damage, now it's critical...
		if ( pev->health < 6 )
			SetSuitUpdate( "!HEV_HLTH3", FALSE, SUIT_NEXT_IN_10MIN ); // near death
		else if ( pev->health < 20 )
			SetSuitUpdate( "!HEV_HLTH2", FALSE, SUIT_NEXT_IN_10MIN ); // health critical

		// give critical health warnings
		if ( !RANDOM_LONG( 0, 3 ) && flHealthPrev < 50 )
			SetSuitUpdate( "!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN ); // seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if ( fTookDamage && ( bitsDamageType & DMG_TIMEBASED ) && flHealthPrev < 75 )
	{
		if ( flHealthPrev < 50 )
		{
			if ( !RANDOM_LONG( 0, 3 ) )
				SetSuitUpdate( "!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN ); // seek medical attention
		}
		else
			SetSuitUpdate( "!HEV_HLTH1", FALSE, SUIT_NEXT_IN_10MIN ); // health dropping
	}

	return fTookDamage;
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
// Set in combat.cpp. Used to pass the damage inflictor for death messages.
entvars_t *g_pevLastInflictor;

void CBasePlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	CSound *pSound;

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster();

	if ( g_pGameRules && g_pGameRules->FSilenceLoopingWeaponSoundsOnDeath() )
	{
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM );
	}

	g_pGameRules->PlayerKilled( this, pevAttacker, g_pevLastInflictor );

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	{
		if ( pSound )
		{
			pSound->Reset();
		}
	}

	SetAnimation( PLAYER_DIE );

	m_flRespawnTimer = 0.0f;

	pev->modelindex = g_ulModelIndexPlayer; // don't use eyes

	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	ClearBits( pev->flags, FL_ONGROUND );
	if ( pev->velocity.z < 10 )
		pev->velocity.z += RANDOM_FLOAT( 0, 300 );

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate( NULL, FALSE, 0 );

	// send "health" update message to zero
	m_iClientHealth = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
	WRITE_BYTE( m_iClientHealth );
	MESSAGE_END();

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0XFF );
	WRITE_BYTE( 0xFF );
	MESSAGE_END();

	// reset FOV
	pev->fov = m_iFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
	WRITE_BYTE( 0 );
	MESSAGE_END();

	// Adrian: always make the players non-solid in multiplayer when they die
	if ( g_pGameRules->IsMultiplayer() )
	{
		pev->solid = SOLID_NOT;
	}

	if ( ( pev->health < -40 && iGib != GIB_NEVER ) || iGib == GIB_ALWAYS )
	{
		pev->solid = SOLID_NOT;

		GibMonster(); // This clears pev->model
		pev->effects |= EF_NODRAW;
		return;
	}

	DeathSound();

	pev->angles.x = 0;
	pev->angles.z = 0;
	m_vecDeathAngles = pev->angles;

	SetThink( &CBasePlayer::PlayerDeathThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

/* */

void CBasePlayer::CheckTimeBasedDamage()
{
	int i;
	BYTE bDuration = 0;

	static float gtbdPrev = 0.0;

	if ( !( m_bitsDamageType & DMG_TIMEBASED ) )
		return;

	// only check for time based damage approx. every 2 seconds
	if ( abs( gpGlobals->time - m_tbdPrev ) < 2.0 )
		return;

	m_tbdPrev = gpGlobals->time;

	for ( i = 0; i < CDMG_TIMEBASED; i++ )
	{
		// make sure bit is set for damage type
		if ( m_bitsDamageType & ( DMG_PARALYZE << i ) )
		{
			switch ( i )
			{
			case itbd_Paralyze:
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_Poison:
				TakeDamage( pev, pev, POISON_DAMAGE, DMG_GENERIC );
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// DrownRecover gradually restores health after player surfaces and takes a breath
				if ( m_idrowndmg > m_idrownrestored )
				{
					int idif = min( m_idrowndmg - m_idrownrestored, 10 );

					TakeHealth( idif, DMG_GENERIC );
					m_idrownrestored += idif;
				}
				bDuration = 4; // get up to 5*10 = 50 points back
				break;
			case itbd_Acid:
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if ( m_rgbTimeBasedDamage[i] )
			{
				// use up an antitoxin on poison or nervegas after a few seconds of damage
				if ( ( ( i == itbd_NerveGas ) && ( m_rgbTimeBasedDamage[i] < NERVEGAS_DURATION ) ) ||
				     ( ( i == itbd_Poison ) && ( m_rgbTimeBasedDamage[i] < POISON_DURATION ) ) )
				{
					if ( m_rgItems[ITEM_ANTIDOTE] )
					{
						m_rgbTimeBasedDamage[i] = 0;
						m_rgItems[ITEM_ANTIDOTE]--;
						SetSuitUpdate( "!HEV_HEAL4", FALSE, SUIT_REPEAT_OK );
					}
				}

				// decrement damage duration, detect when done.
				if ( !m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0 )
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~( DMG_PARALYZE << i );
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

//=========================================================
Vector CBasePlayer ::GetAutoaimVector( float flDelta )
{
	if ( g_iSkillLevel == SKILL_HARD )
	{
		UTIL_MakeVectors( pev->v_angle + pev->punchangle );
		return gpGlobals->v_forward;
	}

	Vector vecSrc = GetGunPosition();
	float flDist  = 8192;

	// Always use non-sticky autoaim for standard skill levels
	m_vecAutoAim = Vector( 0, 0, 0 );

	BOOL m_fOldTargeting = m_fOnTarget;
	Vector angles        = AutoaimDeflection( vecSrc, flDist, flDelta );

	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
		m_fOnTarget = 0;
	else if ( m_fOldTargeting != m_fOnTarget )
	{
		m_pActiveItem->UpdateItemInfo();
	}

	if ( angles.x > 180 )
		angles.x -= 360;
	if ( angles.x < -180 )
		angles.x += 360;
	if ( angles.y > 180 )
		angles.y -= 360;
	if ( angles.y < -180 )
		angles.y += 360;

	if ( angles.x > 25 )
		angles.x = 25;
	if ( angles.x < -25 )
		angles.x = -25;
	if ( angles.y > 12 )
		angles.y = 12;
	if ( angles.y < -12 )
		angles.y = -12;

	// Deflection weighting based on skill level
	if ( g_iSkillLevel == SKILL_EASY )
	{
		m_vecAutoAim = m_vecAutoAim * 0.67 + angles * 0.33;
	}
	else
	{
		m_vecAutoAim = angles * 0.9;
	}

	// m_vecAutoAim = m_vecAutoAim * 0.99;

	// Don't send across network if sv_aim is 0
	if ( g_psv_aim->value != 0 && g_psv_allow_autoaim->value != 0 )
	{
		if ( m_vecAutoAim.x != m_lastx ||
		     m_vecAutoAim.y != m_lasty )
		{
			SET_CROSSHAIRANGLE( edict(), -m_vecAutoAim.x, m_vecAutoAim.y );

			m_lastx = m_vecAutoAim.x;
			m_lasty = m_vecAutoAim.y;
		}
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors( pev->v_angle + pev->punchangle + m_vecAutoAim );
	return gpGlobals->v_forward;
}

Vector CBasePlayer ::AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta )
{
	edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity *pEntity;
	float bestdot;
	Vector bestdir;
	edict_t *bestent;
	TraceResult tr;

	if ( g_psv_aim->value == 0 || g_psv_allow_autoaim->value == 0 )
	{
		m_fOnTarget = FALSE;
		return g_vecZero;
	}

	UTIL_MakeVectors( pev->v_angle + pev->punchangle + m_vecAutoAim );

	// try all possible entities
	bestdir = gpGlobals->v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	m_fOnTarget = FALSE;

	UTIL_TraceLine( vecSrc, vecSrc + bestdir * flDist, dont_ignore_monsters, edict(), &tr );

	if ( tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO )
	{
		// don't look through water
		if ( !( ( pev->waterlevel != 3 && tr.pHit->v.waterlevel == 3 ) || ( pev->waterlevel == 3 && tr.pHit->v.waterlevel == 0 ) ) )
		{
			if ( tr.pHit->v.takedamage == DAMAGE_AIM )
				m_fOnTarget = TRUE;

			return m_vecAutoAim;
		}
	}

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		Vector center;
		Vector dir;
		float dot;

		if ( pEdict->free ) // Not in use
			continue;

		if ( pEdict->v.takedamage != DAMAGE_AIM )
			continue;
		if ( pEdict == edict() )
			continue;
		//		if (pev->team > 0 && pEdict->v.team == pev->team)
		//			continue;	// don't aim at teammate
		if ( !g_pGameRules->ShouldAutoAim( this, pEdict ) )
			continue;

		pEntity = Instance( pEdict );
		if ( pEntity == NULL )
			continue;

		if ( !pEntity->IsAlive() )
			continue;

		// don't look through water
		if ( ( pev->waterlevel != 3 && pEntity->pev->waterlevel == 3 ) || ( pev->waterlevel == 3 && pEntity->pev->waterlevel == 0 ) )
			continue;

		center = pEntity->BodyTarget( vecSrc );

		dir = ( center - vecSrc ).Normalize();

		// make sure it's in front of the player
		if ( DotProduct( dir, gpGlobals->v_forward ) < 0 )
			continue;

		dot = fabs( DotProduct( dir, gpGlobals->v_right ) ) + fabs( DotProduct( dir, gpGlobals->v_up ) ) * 0.5;

		// tweek for distance
		dot *= 1.0 + 0.2 * ( ( center - vecSrc ).Length() / flDist );

		if ( dot > bestdot )
			continue; // to far to turn

		UTIL_TraceLine( vecSrc, center, dont_ignore_monsters, edict(), &tr );
		if ( tr.flFraction != 1.0 && tr.pHit != pEdict )
		{
			// ALERT( at_console, "hit %s, can't see %s\n", STRING( tr.pHit->v.classname ), STRING( pEdict->v.classname ) );
			continue;
		}

		// don't shoot at friends
		if ( IRelationship( pEntity ) < 0 )
		{
			if ( !pEntity->IsPlayer() && !g_pGameRules->IsDeathmatch() )
				// ALERT( at_console, "friend\n");
				continue;
		}

		// can shoot at this one
		bestdot = dot;
		bestent = pEdict;
		bestdir = dir;
	}

	if ( bestent )
	{
		bestdir   = UTIL_VecToAngles( bestdir );
		bestdir.x = -bestdir.x;
		bestdir   = bestdir - pev->v_angle - pev->punchangle;

		if ( bestent->v.takedamage == DAMAGE_AIM )
			m_fOnTarget = TRUE;

		return bestdir;
	}

	return Vector( 0, 0, 0 );
}

void CBasePlayer ::ResetAutoaim()
{
	if ( m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0 )
	{
		m_vecAutoAim = Vector( 0, 0, 0 );
		SET_CROSSHAIRANGLE( edict(), 0, 0 );
	}
	m_fOnTarget = FALSE;
}
