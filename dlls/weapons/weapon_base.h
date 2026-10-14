#ifndef WEAPON_BASE_H
#define WEAPON_BASE_H

#include "weapons/weapon_defs.h"
#include "weapons/weapon_damage.h"
#include "weapons/projectile_grenade.h"
#include "weapons/weapon_registry.h"

class CBasePlayer;

class CBasePlayerItem : public CBaseAnimating
{
  public:
	virtual void SetObjectCollisionBox( void );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	virtual int AddToPlayer( CBasePlayer *pPlayer );                     // return TRUE if the item you want the item added to the player inventory
	virtual int AddDuplicate( CBasePlayerItem *pItem ) { return FALSE; } // return TRUE if you want your duplicate removed from world
	void EXPORT DestroyItem( void );
	void EXPORT DefaultTouch( CBaseEntity *pOther ); // default weapon touch
	void EXPORT FallThink( void );                   // when an item is first spawned, this think is run to determine when the object has hit the ground.
	void EXPORT Materialize( void );                 // make a weapon visible and tangible
	void EXPORT AttemptToMaterialize( void );        // the weapon desires to become visible and tangible, if the game rules allow for it
	CBaseEntity *Respawn( void );                    // copy a weapon
	void FallInit( void );
	void CheckRespawn( void );
	virtual int GetItemInfo( ItemInfo *p ) { return 0; }; // returns 0 if struct not filled out
	virtual BOOL CanDeploy( void ) { return TRUE; };
	virtual BOOL Deploy() // returns is deploy was successful
	{
		return TRUE;
	};

	virtual BOOL CanHolster( void ) { return TRUE; }; // can this weapon be put away right now?
	virtual void Holster( int skiplocal = 0 );
	virtual void UpdateItemInfo( void ) { return; };

	virtual void ItemPreFrame( void ) { return; }  // called each frame by the player PreThink
	virtual void ItemPostFrame( void ) { return; } // called each frame by the player PostThink

	virtual void Drop( void );
	virtual void Kill( void );
	virtual void AttachToPlayer( CBasePlayer *pPlayer );

	virtual int PrimaryAmmoIndex() { return -1; };
	virtual int SecondaryAmmoIndex() { return -1; };

	virtual int UpdateClientData( CBasePlayer *pPlayer ) { return 0; }

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return NULL; };

	static ItemInfo ItemInfoArray[MAX_WEAPONS];
	static AmmoInfo AmmoInfoArray[MAX_AMMO_SLOTS];

	CBasePlayer *m_pPlayer;
	CBasePlayerItem *m_pNext;
	int m_iId; // WEAPON_???

	virtual int iItemSlot( void ) { return 0; } // return 0 to MAX_ITEMS_SLOTS, used in hud

	int iItemPosition( void ) { return ItemInfoArray[m_iId].iPosition; }
	const char *pszAmmo1( void ) { return ItemInfoArray[m_iId].pszAmmo1; }
	int iMaxAmmo1( void ) { return ItemInfoArray[m_iId].iMaxAmmo1; }
	const char *pszAmmo2( void ) { return ItemInfoArray[m_iId].pszAmmo2; }
	int iMaxAmmo2( void ) { return ItemInfoArray[m_iId].iMaxAmmo2; }
	const char *pszName( void ) { return ItemInfoArray[m_iId].pszName; }
	int iMaxClip( void ) { return ItemInfoArray[m_iId].iMaxClip; }
	int iWeight( void ) { return ItemInfoArray[m_iId].iWeight; }
	int iFlags( void ) { return ItemInfoArray[m_iId].iFlags; }

	// int		m_iIdPrimary;										// Unique Id for primary ammo
	// int		m_iIdSecondary;										// Unique Id for secondary ammo
};

class CBasePlayerWeapon : public CBasePlayerItem
{
  public:
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	// generic weapon versions of CBasePlayerItem calls
	virtual int AddToPlayer( CBasePlayer *pPlayer );
	virtual int AddDuplicate( CBasePlayerItem *pItem );

	virtual int ExtractAmmo( CBasePlayerWeapon *pWeapon );     //{ return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up
	virtual int ExtractClipAmmo( CBasePlayerWeapon *pWeapon ); // { return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up

	virtual int AddWeapon( void )
	{
		ExtractAmmo( this );
		return TRUE;
	}; // Return TRUE if you want to add yourself to the player

	// generic "shared" ammo handlers
	BOOL AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry );
	BOOL AddSecondaryAmmo( int iCount, char *szName, int iMaxCarry );

	virtual void UpdateItemInfo( void ) {}; // updates HUD state

	int m_iPlayEmptySound;
	int m_fFireOnEmpty; // True when the gun is empty and the player is still holding down the
	                    // attack key(s)
	virtual BOOL PlayEmptySound( void );
	virtual void ResetEmptySound( void );

	virtual void SendWeaponAnim( int iAnim, int skiplocal = 1, int body = 0 ); // skiplocal is 1 if client is predicting weapon animations

	virtual BOOL CanDeploy( void );
	virtual BOOL IsUseable( void );
	BOOL DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal = 0, int body = 0 );
	int DefaultReload( int iClipSize, int iAnim, float fDelay, int body = 0 );

	virtual void ItemPostFrame( void ); // called each frame by the player PostThink
	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack( void ) { return; }        // do "+ATTACK"
	virtual void SecondaryAttack( void ) { return; }      // do "+ATTACK2"
	virtual void Reload( void ) { return; }               // do "+RELOAD"
	virtual void WeaponIdle( void ) { return; }           // called when no buttons pressed
	virtual int UpdateClientData( CBasePlayer *pPlayer ); // sends hud info to client dll, if things have changed
	virtual void RetireWeapon( void );
	virtual BOOL ShouldWeaponIdle( void ) { return FALSE; };
	virtual void Holster( int skiplocal = 0 );
	virtual BOOL UseDecrement( void ) { return FALSE; };

	int PrimaryAmmoIndex();
	int SecondaryAmmoIndex();

	void PrintState( void );

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return (CBasePlayerItem *)this; };
	float GetNextAttackDelay( float delay );

	float m_flPumpTime;
	int m_fInSpecialReload;        // Are we in the middle of a reload for the shotguns
	float m_flNextPrimaryAttack;   // soonest time ItemPostFrame will call PrimaryAttack
	float m_flNextSecondaryAttack; // soonest time ItemPostFrame will call SecondaryAttack
	float m_flTimeWeaponIdle;      // soonest time ItemPostFrame will call WeaponIdle
	int m_iPrimaryAmmoType;        // "primary" ammo index into players m_rgAmmo[]
	int m_iSecondaryAmmoType;      // "secondary" ammo index into players m_rgAmmo[]
	int m_iClip;                   // number of shots left in the primary weapon clip, -1 it not used
	int m_iClientClip;             // the last version of m_iClip sent to hud dll
	int m_iClientWeaponState;      // the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int m_fInReload;               // Are we in the middle of a reload;

	int m_iDefaultAmmo; // how much ammo you get when you pick up this weapon as placed by a level designer.

	// hle time creep vars
	float m_flPrevPrimaryAttack;
	float m_flLastFireTime;
};

class CBasePlayerAmmo : public CBaseEntity
{
  public:
	virtual void Spawn( void );
	void EXPORT DefaultTouch( CBaseEntity *pOther ); // default weapon touch
	virtual BOOL AddAmmo( CBaseEntity *pOther ) { return TRUE; };

	CBaseEntity *Respawn( void );
	void EXPORT Materialize( void );
};

#define INITIALIZE_WORLD_WEAPON( weaponId, worldModel, defaultAmmo ) \
	do                                                               \
	{                                                                \
		Precache();                                                  \
		m_iId = ( weaponId );                                        \
		SET_MODEL( ENT( pev ), ( worldModel ) );                     \
		m_iDefaultAmmo = ( defaultAmmo );                            \
		FallInit();                                                  \
	} while ( 0 )

#include "weapons/weapon_box.h"

extern int gmsgWeapPickup;

extern DLL_GLOBAL short g_sModelIndexLaser; // holds the index for the laser beam
extern DLL_GLOBAL const char *g_pModelNameLaser;

extern DLL_GLOBAL short g_sModelIndexLaserDot;   // holds the index for the laser beam dot
extern DLL_GLOBAL short g_sModelIndexFireball;   // holds the index for the fireball
extern DLL_GLOBAL short g_sModelIndexSmoke;      // holds the index for the smoke cloud
extern DLL_GLOBAL short g_sModelIndexWExplosion; // holds the index for the underwater explosion
extern DLL_GLOBAL short g_sModelIndexBubbles;    // holds the index for the bubbles model
extern DLL_GLOBAL short g_sModelIndexBloodDrop;  // holds the sprite index for blood drops
extern DLL_GLOBAL short g_sModelIndexBloodSpray; // holds the sprite index for blood spray (bigger)

int MaxAmmoCarry( int iszName );
void EjectBrass( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype );
void AddAmmoNameToAmmoRegistry( const char *szAmmoname );
void UTIL_PrecacheOtherWeapon( const char *szClassname );
void W_Precache( void );

#ifdef CLIENT_DLL
bool bIsMultiplayer( void );
void LoadVModel( char *szViewModel, CBasePlayer *m_pPlayer );
#define WEAPON_GET_MAX_AMMO( pszAmmoName, iDefaultCarry ) ( iDefaultCarry )
#else
#include "gameplay/gamerules.h"
#define WEAPON_GET_MAX_AMMO( pszAmmoName, iDefaultCarry ) ( g_pGameRules ? g_pGameRules->GetMaxAmmo( ( pszAmmoName ), ( iDefaultCarry ) ) : ( iDefaultCarry ) )
#endif

#endif // WEAPON_BASE_H
