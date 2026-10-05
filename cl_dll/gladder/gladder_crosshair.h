/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Universal Weapon Crosshair Fallback (SPEC §7.4, GitHub Issue #36)
 *
 ****/

#pragma once

#ifndef GLADDER_CROSSHAIR_H
#define GLADDER_CROSSHAIR_H

struct WEAPON;

// Fallback callback for WeaponsResource
void GladderCrosshairFallback( struct WEAPON *pWeapon, int iRes );

// Initializes and registers the fallback handler with WeaponsResource
void Gladder_InitCrosshairFallback( void );

#endif // GLADDER_CROSSHAIR_H
