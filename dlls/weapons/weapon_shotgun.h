#ifndef WEAPON_SHOTGUN_H
#define WEAPON_SHOTGUN_H

class CShotgun : public CBasePlayerWeapon
{
  public:
#ifndef CLIENT_DLL
	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn( void );
	void Precache( void );
	int iItemSlot() { return 3; }
	int GetItemInfo( ItemInfo *p );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy();
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	void ItemPostFrame( void );
	int m_fInReload;
	float m_flNextReload;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

  private:
	unsigned short m_usDoubleFire;
	unsigned short m_usSingleFire;
};

#endif // WEAPON_SHOTGUN_H
