#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "weapons/weapon_base.h"
#include "gameplay/gamerules.h"
#include "core/game.h"
#include "player_network.h"

#include "pm_shared.h"

extern bool IsBustingGame();
#define FLASH_DRAIN_TIME 1.2
#define FLASH_CHARGE_TIME 0.2
#define TRAIN_NEW 0xc0

extern int gmsgAmmoX;
extern int gmsgResetHUD;
extern int gmsgInitHUD;
extern int gmsgFlashlight;
extern int gmsgHideWeapon;
extern int gmsgSetFOV;
extern int gmsgShowGameTitle;
extern int gmsgHealth;
extern int gmsgBattery;
extern int gmsgDamage;
extern int gmsgFlashBattery;
extern int gmsgTrain;
extern int gmsgWeaponList;
extern int gmsgCurWeapon;
extern int gmsgAmmoPickup;
extern int gmsgLogo;

// these globals are accessed:
extern int gInitHUD;
extern int gDisplayTitle;

/*
=========================================================
    UpdateClientData

resends any changed player HUD info to the client.
Called every frame by PlayerPreThink
Also called at start of demo recording and playback by
ForceClientDllUpdate to ensure the demo gets messages
reflecting all of the HUD state info.
=========================================================
*/
void CBasePlayer ::UpdateClientData( void )
{
	if ( m_fInitHUD )
	{
		m_fInitHUD = FALSE;
		gInitHUD   = FALSE;

		MESSAGE_BEGIN( MSG_ONE, gmsgResetHUD, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();

		if ( !m_fGameHUDInitialized )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, pev );
			MESSAGE_END();

			g_pGameRules->InitHUD( this );
			m_fGameHUDInitialized = TRUE;

			m_iObserverLastMode = OBS_ROAMING;

			if ( g_pGameRules->IsMultiplayer() )
			{
				FireTargets( "game_playerjoin", this, this, USE_TOGGLE, 0 );
			}
		}

		FireTargets( "game_playerspawn", this, this, USE_TOGGLE, 0 );

		InitStatusBar();

		// Update initial flashlight state
		MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
		WRITE_BYTE( FlashlightIsOn() ? 1 : 0 );
		WRITE_BYTE( m_iFlashBattery );
		MESSAGE_END();
	}

	if ( m_iHideHUD != m_iClientHideHUD )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHideWeapon, NULL, pev );
		WRITE_BYTE( m_iHideHUD );
		MESSAGE_END();

		m_iClientHideHUD = m_iHideHUD;
	}

	if ( m_iFOV != m_iClientFOV )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE( m_iFOV );
		MESSAGE_END();

		// cache FOV change at end of function, so weapon updates can see that FOV has changed
	}

	// HACKHACK -- send the message to display the game title
	if ( gDisplayTitle )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgShowGameTitle, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		gDisplayTitle = 0;
	}

	if ( pev->health != m_iClientHealth )
	{
#define clamp( val, min, max ) ( ( ( val ) > ( max ) ) ? ( max ) : ( ( ( val ) < ( min ) ) ? ( min ) : ( val ) ) )
		int iHealth = clamp( pev->health, 0, 255 ); // make sure that no negative health values are sent
		if ( pev->health > 0.0f && pev->health <= 1.0f )
			iHealth = 1;

		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
		WRITE_BYTE( iHealth );
		MESSAGE_END();

		m_iClientHealth = pev->health;
	}

	if ( pev->armorvalue != m_iClientBattery )
	{
		m_iClientBattery = pev->armorvalue;

		ASSERT( gmsgBattery > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgBattery, NULL, pev );
		WRITE_SHORT( (int)pev->armorvalue );
		MESSAGE_END();
	}

	if ( pev->dmg_take || pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType )
	{
		// Comes from inside me if not set
		Vector damageOrigin = pev->origin;
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		edict_t *other = pev->dmg_inflictor;
		if ( other )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( other );
			if ( pEntity )
				damageOrigin = pEntity->Center();
		}

		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;

		MESSAGE_BEGIN( MSG_ONE, gmsgDamage, NULL, pev );
		WRITE_BYTE( pev->dmg_save );
		WRITE_BYTE( pev->dmg_take );
		WRITE_LONG( visibleDamageBits );
		WRITE_COORD( damageOrigin.x );
		WRITE_COORD( damageOrigin.y );
		WRITE_COORD( damageOrigin.z );
		MESSAGE_END();

		pev->dmg_take   = 0;
		pev->dmg_save   = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}

	// Update Flashlight
	if ( ( m_flFlashLightTime ) && ( m_flFlashLightTime <= gpGlobals->time ) )
	{
		if ( FlashlightIsOn() )
		{
			if ( m_iFlashBattery )
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				m_iFlashBattery--;

				if ( !m_iFlashBattery )
					FlashlightTurnOff();
			}
		}
		else
		{
			if ( m_iFlashBattery < 100 )
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
				m_iFlashBattery++;
			}
			else
				m_flFlashLightTime = 0;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgFlashBattery, NULL, pev );
		WRITE_BYTE( m_iFlashBattery );
		MESSAGE_END();
	}

	if ( m_iTrain & TRAIN_NEW )
	{
		ASSERT( gmsgTrain > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgTrain, NULL, pev );
		WRITE_BYTE( m_iTrain & 0xF );
		MESSAGE_END();

		m_iTrain &= ~TRAIN_NEW;
	}

	//
	// New Weapon?
	//
	if ( !m_fKnownItem )
	{
		m_fKnownItem = TRUE;

		// WeaponInit Message
		// byte  = # of weapons
		//
		// for each weapon:
		// byte		name str length (not including null)
		// bytes... name
		// byte		Ammo Type
		// byte		Ammo2 Type
		// byte		bucket
		// byte		bucket pos
		// byte		flags
		// ????		Icons

		// Send ALL the weapon info now
		int i;

		for ( i = 0; i < MAX_WEAPONS; i++ )
		{
			ItemInfo &II = CBasePlayerItem::ItemInfoArray[i];

			if ( !II.iId )
				continue;

			const char *pszName;
			if ( !II.pszName )
				pszName = "Empty";
			else
				pszName = II.pszName;

			MESSAGE_BEGIN( MSG_ONE, gmsgWeaponList, NULL, pev );
			WRITE_STRING( pszName );                   // string	weapon name
			WRITE_BYTE( GetAmmoIndex( II.pszAmmo1 ) ); // byte		Ammo Type
			WRITE_BYTE( ( II.iMaxAmmo1 < 0 ) ? 255 : min( II.iMaxAmmo1, 254 ) ); // byte     Max Ammo 1 (legacy clamp)
			WRITE_BYTE( GetAmmoIndex( II.pszAmmo2 ) ); // byte		Ammo2 Type
			WRITE_BYTE( ( II.iMaxAmmo2 < 0 ) ? 255 : min( II.iMaxAmmo2, 254 ) ); // byte     Max Ammo 2 (legacy clamp)
			WRITE_BYTE( II.iSlot );                    // byte		bucket
			WRITE_BYTE( II.iPosition );                // byte		bucket pos
			WRITE_BYTE( II.iId );                      // byte		id (bit index into pev->weapons)
			WRITE_BYTE( II.iFlags );                   // byte		Flags
			WRITE_SHORT( II.iMaxAmmo1 );               // short    Extended Max Ammo 1
			WRITE_SHORT( II.iMaxAmmo2 );               // short    Extended Max Ammo 2
			MESSAGE_END();
		}
	}

	SendAmmoUpdate();

	// Update all the items
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] ) // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData( this );
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	m_iClientFOV        = m_iFOV;

	// Update Status Bar
	if ( m_flNextSBarUpdateTime < gpGlobals->time )
	{
		UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.2;
	}
}
