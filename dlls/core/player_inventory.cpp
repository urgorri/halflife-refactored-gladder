/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, and modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons/weapon_base.h"
#include "player_inventory.h"
#include "gameplay/gamerules.h"
#include <unordered_map>
#include <string>

extern int gmsgFlashlight;
extern int gmsgFlashBattery;
extern int gmsgResetHUD;
extern int gmsgInitHUD;
extern int gmsgShowGameTitle;
extern int gmsgCurWeapon;
extern int gmsgHealth;
extern int gmsgDamage;
extern int gmsgBattery;
extern int gmsgTrain;
extern int gmsgWeaponList;
extern int gmsgAmmoX;
extern int gmsgAmmoPickup;
extern int gmsgHideWeapon;
extern int gmsgSetFOV;

extern int gEvilImpulse101;
extern DLL_GLOBAL int gDisplayTitle;
extern bool IsBustingGame();
extern cvar_t weaponstay;

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL g_fGameOver;

//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
void CBasePlayer::PackDeadPlayerItems( void )
{
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon *rgpPackWeapons[MAX_WEAPONS];
	int iPackAmmo[MAX_AMMO_SLOTS + 1];
	int iPW = 0; // index into packweapons array
	int iPA = 0; // index into packammo array

	memset( rgpPackWeapons, 0, sizeof( rgpPackWeapons ) );
	memset( iPackAmmo, -1, sizeof( iPackAmmo ) );

	// get the game rules
	iWeaponRules = g_pGameRules->DeadPlayerWeapons( this );
	iAmmoRules   = g_pGameRules->DeadPlayerAmmo( this );

	if ( iWeaponRules == GR_PLR_DROP_GUN_NO && iAmmoRules == GR_PLR_DROP_AMMO_NO )
	{
		// nothing to pack. Remove the weapons and return. Don't call create on the box!
		RemoveAllItems( TRUE );
		return;
	}

	// go through all of the weapons and make a list of the ones to pack
	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

			while ( pPlayerItem )
			{
				switch ( iWeaponRules )
				{
				case GR_PLR_DROP_GUN_ACTIVE:
					if ( m_pActiveItem && pPlayerItem == m_pActiveItem )
					{
						CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)pPlayerItem;

						// this is the active item. Pack it.
						if ( iPW < MAX_WEAPONS )
						{
							rgpPackWeapons[iPW++] = pWeapon;
						}
						else
						{
							ALERT( at_error, "PackDeadPlayerItems: Exceeded MAX_WEAPONS capacity (%d)!\n", MAX_WEAPONS );
						}

						// Reload the weapon before dropping it if we have ammo
						int j = min( pWeapon->iMaxClip() - pWeapon->m_iClip, m_rgAmmo[pWeapon->m_iPrimaryAmmoType] );

						// Add them to the clip
						pWeapon->m_iClip += j;
						m_rgAmmo[pWeapon->m_iPrimaryAmmoType] -= j;

						TabulateAmmo();
					}
					break;

				case GR_PLR_DROP_GUN_ALL:
					if ( iPW < MAX_WEAPONS )
					{
						rgpPackWeapons[iPW++] = (CBasePlayerWeapon *)pPlayerItem;
					}
					else
					{
						ALERT( at_error, "PackDeadPlayerItems: Exceeded MAX_WEAPONS capacity (%d)!\n", MAX_WEAPONS );
					}
					break;

				default:
					break;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	// now go through ammo and make a list of which types to pack.
	if ( iAmmoRules != GR_PLR_DROP_AMMO_NO )
	{
		for ( i = 0; i < MAX_AMMO_SLOTS; i++ )
		{
			if ( m_rgAmmo[i] > 0 )
			{
				// player has some ammo of this type.
				switch ( iAmmoRules )
				{
				case GR_PLR_DROP_AMMO_ALL:
					if ( iPA < MAX_AMMO_SLOTS )
					{
						iPackAmmo[iPA++] = i;
					}
					break;

				case GR_PLR_DROP_AMMO_ACTIVE:
					if ( m_pActiveItem && i == m_pActiveItem->PrimaryAmmoIndex() )
					{
						// this is the primary ammo type for the active weapon
						if ( iPA < MAX_AMMO_SLOTS )
						{
							iPackAmmo[iPA++] = i;
						}
					}
					else if ( m_pActiveItem && i == m_pActiveItem->SecondaryAmmoIndex() )
					{
						// this is the secondary ammo type for the active weapon
						if ( iPA < MAX_AMMO_SLOTS )
						{
							iPackAmmo[iPA++] = i;
						}
					}
					break;

				default:
					break;
				}
			}
		}
	}

	// create a box to pack the stuff into.
	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create( "weaponbox", pev->origin, pev->angles, edict() );

	pWeaponBox->pev->angles.x = 0; // don't let weaponbox tilt.
	pWeaponBox->pev->angles.z = 0;

	pWeaponBox->SetThink( &CWeaponBox::Kill );
	pWeaponBox->pev->nextthink = gpGlobals->time + 120;

	// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

	if ( IsBustingGame() )
	{
		if ( HasNamedPlayerItem( "weapon_egon" ) )
		{
			for ( i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				CBasePlayerItem *pItem = m_rgpPlayerItems[i];

				if ( pItem )
				{
					if ( !strcmp( "weapon_egon", STRING( pItem->pev->classname ) ) )
					{
						pWeaponBox->PackWeapon( pItem );

						SET_MODEL( ENT( pWeaponBox->pev ), "models/w_egon.mdl" );

						pWeaponBox->pev->velocity    = vec3_t( 0, 0, 0 );
						pWeaponBox->pev->renderfx    = kRenderFxGlowShell;
						pWeaponBox->pev->renderamt   = 25;
						pWeaponBox->pev->rendercolor = Vector( 0, 75, 250 );

						break;
					}
				}
			}
		}
	}
	else
	{
		bool bPackItems = TRUE;

		if ( iAmmoRules == GR_PLR_DROP_AMMO_ACTIVE && iWeaponRules == GR_PLR_DROP_GUN_ACTIVE )
		{
			if ( rgpPackWeapons[0] == nullptr || ( FClassnameIs( rgpPackWeapons[0]->pev, "weapon_satchel" ) && ( iPackAmmo[0] == -1 || ( m_rgAmmo[iPackAmmo[0]] == 0 ) ) ) )
			{
				bPackItems = FALSE;
			}
		}

		if ( bPackItems )
		{
			// pack the ammo
			while ( iPA < MAX_AMMO_SLOTS && iPackAmmo[iPA] != -1 )
			{
				pWeaponBox->PackAmmo( MAKE_STRING( CBasePlayerItem::AmmoInfoArray[iPackAmmo[iPA]].pszName ), m_rgAmmo[iPackAmmo[iPA]] );
				iPA++;
			}

			// now pack all of the items in the lists
			while ( iPW < MAX_WEAPONS && rgpPackWeapons[iPW] )
			{
				// weapon unhooked from the player. Pack it into der box.
				pWeaponBox->PackWeapon( rgpPackWeapons[iPW] );

				iPW++;
			}
		}

		pWeaponBox->pev->velocity = pev->velocity * 1.2; // weaponbox has player's velocity, then some.
	}

	RemoveAllItems( TRUE ); // now strip off everything that wasn't handled by the code above.
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
	if ( m_pActiveItem )
	{
		ResetAutoaim();
		m_pActiveItem->Holster();
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	int i;
	CBasePlayerItem *pPendingItem;
	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while ( m_pActiveItem )
		{
			pPendingItem = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop();
			m_pActiveItem = pPendingItem;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;

	pev->viewmodel   = 0;
	pev->weaponmodel = 0;

	if ( removeSuit )
	{
		pev->weapons = 0;
		ClearCustomItems();
	}
	else
		pev->weapons &= ~WEAPON_ALLWEAPONS;

	for ( i = 0; i < MAX_AMMO_SLOTS; i++ )
		m_rgAmmo[i] = 0;

	UpdateClientData();
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	MESSAGE_END();
}
void CBasePlayer::TabulateAmmo()
{
	ammo_9mm      = AmmoInventory( GetAmmoIndex( "9mm" ) );
	ammo_357      = AmmoInventory( GetAmmoIndex( "357" ) );
	ammo_argrens  = AmmoInventory( GetAmmoIndex( "ARgrenades" ) );
	ammo_bolts    = AmmoInventory( GetAmmoIndex( "bolts" ) );
	ammo_buckshot = AmmoInventory( GetAmmoIndex( "buckshot" ) );
	ammo_rockets  = AmmoInventory( GetAmmoIndex( "rockets" ) );
	ammo_uranium  = AmmoInventory( GetAmmoIndex( "uranium" ) );
	ammo_hornets  = AmmoInventory( GetAmmoIndex( "Hornets" ) );
}

//
// Marks everything as new so the player will resend this to the hud.
//
void CBasePlayer::RenewItems( void )
{
}

void CBasePlayer::SelectNextItem( int iItem )
{
	CBasePlayerItem *pItem;

	pItem = m_rgpPlayerItems[iItem];

	if ( !pItem )
		return;

	if ( pItem == m_pActiveItem )
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext;
		if ( !pItem )
		{
			return;
		}

		CBasePlayerItem *pLast;
		pLast = pItem;
		while ( pLast->m_pNext )
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext          = m_pActiveItem;
		m_pActiveItem->m_pNext  = NULL;
		m_rgpPlayerItems[iItem] = pItem;
	}

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if ( m_pActiveItem )
	{
		m_pActiveItem->Holster();
	}

	m_pActiveItem = pItem;

	if ( m_pActiveItem )
	{
		m_pActiveItem->Deploy();
		m_pActiveItem->UpdateItemInfo();
	}
}

void CBasePlayer::SelectItem( const char *pstr )
{
	if ( !pstr )
		return;

	CBasePlayerItem *pItem = NULL;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			pItem = m_rgpPlayerItems[i];

			while ( pItem )
			{
				if ( FClassnameIs( pItem->pev, pstr ) )
					break;
				pItem = pItem->m_pNext;
			}
		}

		if ( pItem )
			break;
	}

	if ( !pItem )
		return;

	if ( pItem == m_pActiveItem )
		return;

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if ( m_pActiveItem )
		m_pActiveItem->Holster();

	m_pLastItem   = m_pActiveItem;
	m_pActiveItem = pItem;

	if ( m_pActiveItem )
	{
		m_pActiveItem->Deploy();
		m_pActiveItem->UpdateItemInfo();
	}
}

void CBasePlayer::SelectLastItem( void )
{
	if ( !m_pLastItem )
	{
		return;
	}

	if ( m_pActiveItem && !m_pActiveItem->CanHolster() )
	{
		return;
	}

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if ( m_pActiveItem )
		m_pActiveItem->Holster();

	CBasePlayerItem *pTemp = m_pActiveItem;
	m_pActiveItem          = m_pLastItem;
	m_pLastItem            = pTemp;
	m_pActiveItem->Deploy();
	m_pActiveItem->UpdateItemInfo();
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
BOOL CBasePlayer::HasWeapons( void )
{
	int i;

	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			return TRUE;
		}
	}

	return FALSE;
}

void CBasePlayer::SelectPrevItem( int iItem )
{
}

//==============================================
// Spray decal entity creation

void CBasePlayer::GiveNamedItem( const char *pszName )
{
	edict_t *pent;

	int istr = MAKE_STRING( pszName );

	pent = CREATE_NAMED_ENTITY( istr );
	if ( FNullEnt( pent ) )
	{
		ALERT( at_console, "NULL Ent in GiveNamedItem!\n" );
		return;
	}
	VARS( pent )->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	DispatchSpawn( pent );
	DispatchTouch( pent, ENT( pev ) );
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
int CBasePlayer::AddPlayerItem( CBasePlayerItem *pItem )
{
	CBasePlayerItem *pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];

	while ( pInsert )
	{
		if ( FClassnameIs( pInsert->pev, STRING( pItem->pev->classname ) ) )
		{
			if ( pItem->AddDuplicate( pInsert ) )
			{
				g_pGameRules->PlayerGotWeapon( this, pItem );
				pItem->CheckRespawn();

				// Update weapon clip state without broadcasting extra message
				pInsert->UpdateItemInfo();
				if ( m_pActiveItem )
					m_pActiveItem->UpdateItemInfo();

				pItem->Kill();
			}
			else if ( gEvilImpulse101 )
			{
				// Deathmatch item provisioning configuration
				pItem->Kill();
			}
			return FALSE;
		}
		pInsert = pInsert->m_pNext;
	}

	if ( pItem->AddToPlayer( this ) )
	{
		g_pGameRules->PlayerGotWeapon( this, pItem );
		pItem->CheckRespawn();

		pItem->m_pNext                       = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		// should we switch to this item?
		if ( g_pGameRules->FShouldSwitchWeapon( this, pItem ) )
		{
			SwitchWeapon( pItem );
		}

		return TRUE;
	}
	else if ( gEvilImpulse101 )
	{
		// Deathmatch item provisioning configuration
		pItem->Kill();
	}
	return FALSE;
}

int CBasePlayer::RemovePlayerItem( CBasePlayerItem *pItem )
{
	if ( m_pActiveItem == pItem )
	{
		ResetAutoaim();
		pItem->Holster();
		pItem->pev->nextthink = 0; // crowbar may be trying to swing again, etc.
		pItem->SetThink( NULL );
		m_pActiveItem    = NULL;
		pev->viewmodel   = 0;
		pev->weaponmodel = 0;
	}
	if ( m_pLastItem == pItem )
		m_pLastItem = NULL;

	CBasePlayerItem *pPrev = m_rgpPlayerItems[pItem->iItemSlot()];

	if ( pPrev == pItem )
	{
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem->m_pNext;
		return TRUE;
	}
	else
	{
		while ( pPrev && pPrev->m_pNext != pItem )
		{
			pPrev = pPrev->m_pNext;
		}
		if ( pPrev )
		{
			pPrev->m_pNext = pItem->m_pNext;
			return TRUE;
		}
	}
	return FALSE;
}

//
// Returns the unique ID for the ammo, or -1 if error
//
int CBasePlayer ::GiveAmmo( int iCount, char *szName, int iMax )
{
	if ( !szName )
	{
		// no ammo.
		return -1;
	}

	if ( !g_pGameRules->CanHaveAmmo( this, szName, iMax ) )
	{
		// game rules say I can't have any more of this ammo type.
		return -1;
	}

	int i = 0;

	i = GetAmmoIndex( szName );

	if ( i < 0 || i >= MAX_AMMO_SLOTS )
		return -1;

	int iAdd = min( iCount, iMax - m_rgAmmo[i] );
	if ( iAdd < 1 )
		return i;

	m_rgAmmo[i] += iAdd;

	if ( gmsgAmmoPickup ) // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN( MSG_ONE, gmsgAmmoPickup, NULL, pev );
		WRITE_BYTE( GetAmmoIndex( szName ) ); // ammo ID
		WRITE_BYTE( iAdd );                   // amount
		MESSAGE_END();
	}

	TabulateAmmo();

	return i;
}
void CBasePlayer::ItemPreFrame()
{
#if defined( CLIENT_WEAPONS )
	if ( m_flNextAttack > 0 )
#else
	if ( gpGlobals->time < m_flNextAttack )
#endif
	{
		return;
	}

	if ( !m_pActiveItem )
		return;

	m_pActiveItem->ItemPreFrame();
}
void CBasePlayer::ItemPostFrame()
{
	static int fInSelect = FALSE;

	// check if the player is using a tank
	if ( m_pTank != NULL )
		return;

#if defined( CLIENT_WEAPONS )
	if ( m_flNextAttack > 0 )
#else
	if ( gpGlobals->time < m_flNextAttack )
#endif
	{
		return;
	}

	ImpulseCommands();

	if ( !m_pActiveItem )
		return;

	m_pActiveItem->ItemPostFrame();
}

int CBasePlayer::AmmoInventory( int iAmmoIndex )
{
	if ( iAmmoIndex == -1 )
	{
		return -1;
	}

	return m_rgAmmo[iAmmoIndex];
}

int CBasePlayer::GetAmmoIndex( const char *psz )
{
	int i;

	if ( !psz )
		return -1;

	for ( i = 1; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( !CBasePlayerItem::AmmoInfoArray[i].pszName )
			continue;

		if ( stricmp( psz, CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
			return i;
	}

	return -1;
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
void CBasePlayer::SendAmmoUpdate( void )
{
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( m_rgAmmo[i] != m_rgAmmoLast[i] )
		{
			m_rgAmmoLast[i] = m_rgAmmo[i];

			ASSERT( m_rgAmmo[i] >= 0 );
			ASSERT( m_rgAmmo[i] < 255 );

			// send "Ammo" update message
			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
			WRITE_BYTE( i );
			WRITE_BYTE( max( min( m_rgAmmo[i], 254 ), 0 ) ); // clamp the value to one byte
			MESSAGE_END();
		}
	}
}

//=========================================================
// DropPlayerItem - drop the named item, or if no name,
// the active item.
//=========================================================
void CBasePlayer::DropPlayerItem( char *pszItemName )
{
	if ( !g_pGameRules->IsMultiplayer() || ( weaponstay.value > 0 ) )
	{
		// no dropping in single player.
		return;
	}

	if ( !strlen( pszItemName ) )
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		// make the string null to make future operations in this function easier
		pszItemName = NULL;
	}

	CBasePlayerItem *pWeapon;
	int i;

	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		pWeapon = m_rgpPlayerItems[i];

		while ( pWeapon )
		{
			if ( pszItemName )
			{
				// try to match by name.
				if ( !strcmp( pszItemName, STRING( pWeapon->pev->classname ) ) )
				{
					// match!
					break;
				}
			}
			else
			{
				// trying to drop active item
				if ( pWeapon == m_pActiveItem )
				{
					// active item!
					break;
				}
			}

			pWeapon = pWeapon->m_pNext;
		}

		// if we land here with a valid pWeapon pointer, that's because we found the
		// item we want to drop and hit a BREAK;  pWeapon is the item.
		if ( pWeapon )
		{
			if ( !g_pGameRules->GetNextBestWeapon( this, pWeapon ) )
				return; // can't drop the item they asked for, may be our last item or something we can't holster

			UTIL_MakeVectors( pev->angles );

			pev->weapons &= ~( 1 << pWeapon->m_iId ); // take item off hud

			CWeaponBox *pWeaponBox    = (CWeaponBox *)CBaseEntity::Create( "weaponbox", pev->origin + gpGlobals->v_forward * 10, pev->angles, edict() );
			pWeaponBox->pev->angles.x = 0;
			pWeaponBox->pev->angles.z = 0;
			pWeaponBox->PackWeapon( pWeapon );
			pWeaponBox->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;

			// drop half of the ammo for this weapon.
			int iAmmoIndex;

			iAmmoIndex = GetAmmoIndex( pWeapon->pszAmmo1() ); // ???

			if ( iAmmoIndex != -1 )
			{
				// this weapon weapon uses ammo, so pack an appropriate amount.
				if ( pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE )
				{
					// pack up all the ammo, this weapon is its own ammo type
					pWeaponBox->PackAmmo( MAKE_STRING( pWeapon->pszAmmo1() ), m_rgAmmo[iAmmoIndex] );
					m_rgAmmo[iAmmoIndex] = 0;
				}
				else
				{
					// pack half of the ammo
					pWeaponBox->PackAmmo( MAKE_STRING( pWeapon->pszAmmo1() ), m_rgAmmo[iAmmoIndex] / 2 );
					m_rgAmmo[iAmmoIndex] /= 2;
				}
			}

			return; // we're done, so stop searching with the FOR loop.
		}
	}
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while ( pItem )
	{
		if ( FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname ) ) )
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasNamedPlayerItem( const char *pszItemName )
{
	CBasePlayerItem *pItem;
	int i;

	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		pItem = m_rgpPlayerItems[i];

		while ( pItem )
		{
			if ( !strcmp( pszItemName, STRING( pItem->pev->classname ) ) )
			{
				return TRUE;
			}
			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}

//=========================================================
// HasPlayerItemFromID
// Just compare IDs, rather than classnames
//=========================================================
BOOL CBasePlayer::HasPlayerItemFromID( int nID )
{
	CBasePlayerItem *pItem;
	int i;

	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		pItem = m_rgpPlayerItems[i];

		while ( pItem )
		{
			if ( pItem->m_iId == nID )
			{
				return TRUE;
			}

			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}

//=========================================================
//
//=========================================================
BOOL CBasePlayer ::SwitchWeapon( CBasePlayerItem *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
	{
		return FALSE;
	}

	ResetAutoaim();

	if ( m_pActiveItem )
	{
		m_pActiveItem->Holster();
	}

	m_pActiveItem = pWeapon;
	pWeapon->Deploy();

	return TRUE;
}

