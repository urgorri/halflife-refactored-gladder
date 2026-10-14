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
#include "core/player_input.h"
#include "weapons/weapon_spraycan.h"
#include "world/trains.h"
#include "ai/nodes.h"
#include "weapons/weapon_base.h"
#include "items/item_registry.h"
#include "ai/soundent.h"
#include "ai/monsters.h"
#include "shake.h"
#include "core/decals.h"
#include "gameplay/gamerules.h"
#include "core/game.h"
#include "pm_shared.h"

extern int gEvilImpulse101;
extern int gmsgLogo;
extern int giPrecacheGrunt;
extern int TrainSpeed( int iSpeed, int iMax );
extern CBaseEntity *FindEntityForward( CBaseEntity *pMe );

#define PLAYER_SEARCH_RADIUS (float)64
#define TRAIN_ACTIVE 0x80
#define TRAIN_NEW 0xc0
#define TRAIN_OFF 0x00
#define TRAIN_NEUTRAL 0x01
#define TRAIN_SLOW 0x02
#define TRAIN_MEDIUM 0x03
#define TRAIN_FAST 0x04
#define TRAIN_BACK 0x05

void CBasePlayer::PlayerUse( void )
{
	if ( IsObserver() )
		return;

	// Was use pressed or released?
	if ( !( ( pev->button | m_afButtonPressed | m_afButtonReleased ) & IN_USE ) )
		return;

	// Hit Use on a train?
	if ( m_afButtonPressed & IN_USE )
	{
		if ( m_pTank != NULL )
		{
			// Stop controlling the tank
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
			return;
		}
		else
		{
			if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain            = TRAIN_NEW | TRAIN_OFF;
				CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );
				if ( pTrain && ( pTrain->Classify() == CLASS_VEHICLE ) )
					( (CFuncVehicle *)pTrain )->m_pDriver = NULL;
				return;
			}
			else
			{ // Start controlling the train!
				CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );

				if ( pTrain && !( pev->button & IN_JUMP ) && FBitSet( pev->flags, FL_ONGROUND ) && ( pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE ) && pTrain->OnControls( pev ) )
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed( pTrain->pev->speed, pTrain->pev->impulse );
					m_iTrain |= TRAIN_NEW;
					if ( pTrain->Classify() == CLASS_VEHICLE )
					{
						EMIT_SOUND( ENT( pev ), CHAN_ITEM, "plats/vehicle_ignition.wav", 0.8, ATTN_NORM );
						( (CFuncVehicle *)pTrain )->m_pDriver = this;
					}
					else
						EMIT_SOUND( ENT( pev ), CHAN_ITEM, "plats/train_use1.wav", 0.8, ATTN_NORM );
					return;
				}
			}
		}
	}

	CBaseEntity *pObject  = NULL;
	CBaseEntity *pClosest = NULL;
	Vector vecLOS;
	float flMaxDot = VIEW_FIELD_NARROW;
	float flDot;

	UTIL_MakeVectors( pev->v_angle ); // so we know which way we are facing

	while ( ( pObject = UTIL_FindEntityInSphere( pObject, pev->origin, PLAYER_SEARCH_RADIUS ) ) != NULL )
	{

		if ( pObject->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE ) )
		{
			// !!!PERFORMANCE- should this check be done on a per case basis AFTER we've determined that
			// this object is actually usable? This dot is being done for every object within PLAYER_SEARCH_RADIUS
			// when player hits the use key. How many objects can be in that area, anyway? (sjb)
			vecLOS = ( VecBModelOrigin( pObject->pev ) - ( pev->origin + pev->view_ofs ) );

			// This essentially moves the origin of the target to the corner nearest the player to test to see
			// if it's "hull" is in the view cone
			vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );

			flDot = DotProduct( vecLOS, gpGlobals->v_forward );
			if ( flDot > flMaxDot )
			{ // only if the item is in front of the user
				pClosest = pObject;
				flMaxDot = flDot;
				//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
			//			ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
		}
	}
	pObject = pClosest;

	// Found an object
	if ( pObject )
	{
		int caps = pObject->ObjectCaps();

		if ( m_afButtonPressed & IN_USE )
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "common/wpn_select.wav", 0.4, ATTN_NORM );

		if ( ( ( pev->button & IN_USE ) && ( caps & FCAP_CONTINUOUS_USE ) ) ||
		     ( ( m_afButtonPressed & IN_USE ) && ( caps & ( FCAP_IMPULSE_USE | FCAP_ONOFF_USE ) ) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
				m_afPhysicsFlags |= PFLAG_USING;

			pObject->Use( this, this, USE_SET, 1 );
		}
		else if ( ( m_afButtonReleased & IN_USE ) && ( pObject->ObjectCaps() & FCAP_ONOFF_USE ) )
		{
			// Send off state on button release for toggleable objects
			pObject->Use( this, this, USE_SET, 0 );
		}
	}
	else
	{
		if ( m_afButtonPressed & IN_USE )
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4, ATTN_NORM );
	}
}

void CBasePlayer::ImpulseCommands()
{
	// Handle use events
	PlayerUse();

	int iImpulse = (int)pev->impulse;
	switch ( iImpulse )
	{
	case 99:
	{

		int iOn;

		if ( !gmsgLogo )
		{
			iOn      = 1;
			gmsgLogo = REG_USER_MSG( "Logo", 1 );
		}
		else
		{
			iOn = 0;
		}

		ASSERT( gmsgLogo > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgLogo, NULL, pev );
		WRITE_BYTE( iOn );
		MESSAGE_END();

		if ( !iOn )
			gmsgLogo = 0;
		break;
	}
	case 100:
		// temporary flashlight for level designers
		if ( FlashlightIsOn() )
		{
			FlashlightTurnOff();
		}
		else
		{
			FlashlightTurnOn();
		}
		break;

	case 201: // paint decal
	{
		if ( gpGlobals->time < m_flNextDecalTime )
		{
			// too early!
			break;
		}

		TraceResult tr;
		UTIL_MakeVectors( pev->v_angle );
		UTIL_TraceLine( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT( pev ), &tr );

		if ( tr.flFraction != 1.0 )
		{ // line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + decalfrequency.value;
			CSprayCan *pCan   = GetClassPtr( (CSprayCan *)NULL );
			pCan->Spawn( pev );
		}

		break;
	}

	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands( iImpulse );
		break;
	}

	pev->impulse = 0;
}

void CBasePlayer::CheatImpulseCommands( int iImpulse )
{
#if !defined( HLDEMO_BUILD )
	if ( CVAR_GET_FLOAT( "sv_cheats" ) == 0.0 )
	{
		return;
	}

	CBaseEntity *pEntity;
	TraceResult tr;

	switch ( iImpulse )
	{
	case 76:
	{
		if ( !giPrecacheGrunt )
		{
			giPrecacheGrunt = 1;
			ALERT( at_console, "You must now restart to use Grunt-o-matic.\n" );
		}
		else
		{
			UTIL_MakeVectors( Vector( 0, pev->v_angle.y, 0 ) );
			Create( "monster_human_grunt", pev->origin + gpGlobals->v_forward * 128, pev->angles );
		}
		break;
	}

	case 101:
	{
		static const char *const s_szImpulse101Items[] = {
		    "item_suit",
		    "item_battery",
		    "weapon_crowbar",
		    "weapon_9mmhandgun",
		    "ammo_9mmclip",
		    "weapon_shotgun",
		    "ammo_buckshot",
		    "weapon_9mmAR",
		    "ammo_9mmAR",
		    "ammo_ARgrenades",
		    "weapon_handgrenade",
		    "weapon_tripmine",
#ifndef OEM_BUILD
		    "weapon_357",
		    "ammo_357",
		    "weapon_crossbow",
		    "ammo_crossbow",
		    "weapon_egon",
		    "weapon_gauss",
		    "ammo_gaussclip",
		    "weapon_rpg",
		    "ammo_rpgclip",
		    "weapon_satchel",
		    "weapon_snark",
		    "weapon_hornetgun",
#endif
		};

		gEvilImpulse101 = TRUE;
		for ( size_t i = 0; i < ARRAYSIZE( s_szImpulse101Items ); ++i )
		{
			GiveNamedItem( s_szImpulse101Items[i] );
		}
		// Extra items registered by mods; empty (zero iterations) in vanilla Half-Life.
		for ( const char *pszExtraItem : ItemRegistry::GetImpulse101Items() )
		{
			GiveNamedItem( pszExtraItem );
		}
		gEvilImpulse101 = FALSE;
		break;
	}

	case 102:
		// Gibbage!!!
		CGib::SpawnRandomGibs( pev, 1, 1 );
		break;

	case 103:
		// What the hell are you doing?
		pEntity = FindEntityForward( this );
		if ( pEntity )
		{
			CBaseMonster *pMonster = pEntity->MyMonsterPointer();
			if ( pMonster )
				pMonster->ReportAIState();
		}
		break;

	case 104:
		// Dump all of the global state varaibles (and global entity names)
		gGlobalState.DumpGlobals();
		break;

	case 105: // player makes no sound for monsters to hear.
	{
		if ( m_fNoPlayerSound )
		{
			ALERT( at_console, "Player is audible\n" );
			m_fNoPlayerSound = FALSE;
		}
		else
		{
			ALERT( at_console, "Player is silent\n" );
			m_fNoPlayerSound = TRUE;
		}
		break;
	}

	case 106:
		// Give me the classname and targetname of this entity.
		pEntity = FindEntityForward( this );
		if ( pEntity )
		{
			ALERT( at_console, "Classname: %s", STRING( pEntity->pev->classname ) );

			if ( !FStringNull( pEntity->pev->targetname ) )
			{
				ALERT( at_console, " - Targetname: %s\n", STRING( pEntity->pev->targetname ) );
			}
			else
			{
				ALERT( at_console, " - TargetName: No Targetname\n" );
			}

			ALERT( at_console, "Model: %s\n", STRING( pEntity->pev->model ) );
			if ( pEntity->pev->globalname )
				ALERT( at_console, "Globalname: %s\n", STRING( pEntity->pev->globalname ) );
		}
		break;

	case 107:
	{
		TraceResult tr;

		edict_t *pWorld = g_engfuncs.pfnPEntityOfEntIndex( 0 );

		Vector start = pev->origin + pev->view_ofs;
		Vector end   = start + gpGlobals->v_forward * 1024;
		UTIL_TraceLine( start, end, ignore_monsters, edict(), &tr );
		if ( tr.pHit )
			pWorld = tr.pHit;
		const char *pTextureName = TRACE_TEXTURE( pWorld, start, end );
		if ( pTextureName )
			ALERT( at_console, "Texture: %s\n", pTextureName );
	}
	break;
	case 195: // show shortest paths for entire level to nearest node
	{
		Create( "node_viewer_fly", pev->origin, pev->angles );
	}
	break;
	case 196: // show shortest paths for entire level to nearest node
	{
		Create( "node_viewer_large", pev->origin, pev->angles );
	}
	break;
	case 197: // show shortest paths for entire level to nearest node
	{
		Create( "node_viewer_human", pev->origin, pev->angles );
	}
	break;
	case 199: // show nearest node and all connections
	{
		ALERT( at_console, "%d\n", WorldGraph.FindNearestNode( pev->origin, bits_NODE_GROUP_REALM ) );
		WorldGraph.ShowNodeConnections( WorldGraph.FindNearestNode( pev->origin, bits_NODE_GROUP_REALM ) );
	}
	break;
	case 202: // Random blood splatter
		UTIL_MakeVectors( pev->v_angle );
		UTIL_TraceLine( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT( pev ), &tr );

		if ( tr.flFraction != 1.0 )
		{ // line hit something, so paint a decal
			CBloodSplat *pBlood = GetClassPtr( (CBloodSplat *)NULL );
			pBlood->Spawn( pev );
		}
		break;
	case 203: // remove creature.
		pEntity = FindEntityForward( this );
		if ( pEntity )
		{
			if ( pEntity->pev->takedamage )
				pEntity->SetThink( &CBaseEntity::SUB_Remove );
		}
		break;
	}
#endif // HLDEMO_BUILD
}
