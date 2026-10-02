/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Iconic Collectible Item (item_gladder_lambda) (SPEC §7.3, GitHub Issue #11)
 *
 ****/

#pragma once

#ifndef GLADDER_ITEMS_H
#define GLADDER_ITEMS_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "items/item_base.h"

class CGladderItemLambda : public CItem
{
  public:
	void Spawn( void ) override;
	void Precache( void ) override;
	void EXPORT ItemTouch( CBaseEntity *pOther );

	static void PrecacheItem( void );
	static void RegisterItems( void );
};

#endif // GLADDER_ITEMS_H
