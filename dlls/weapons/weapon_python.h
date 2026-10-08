#pragma once

#ifndef WEAPON_PYTHON_H
#define WEAPON_PYTHON_H

class CPython : public CBasePlayerWeapon
{
  public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	int GetItemInfo( ItemInfo *p );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	BOOL m_fInZoom; // don't save this.

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

  private:
	unsigned short m_usFirePython;
};

#endif // WEAPON_PYTHON_H
