/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Extended Arsenal Definitions
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#pragma once

#ifndef GLADDER_WEAPONS_H
#define GLADDER_WEAPONS_H

#include "weapons/weapon_defs.h"

// Gladder / OpFor Weapon Slot IDs (allocated in free range 16..24, MAX_WEAPONS = 32)
#define WEAPON_PIPEWRENCH 16
#define WEAPON_KNIFE      17
#define WEAPON_EAGLE      18
#define WEAPON_SNIPER     19
#define WEAPON_M249       20

// Weapon Weights for autoselection priority
#define PIPEWRENCH_WEIGHT 10
#define KNIFE_WEIGHT      5
#define EAGLE_WEIGHT      15
#define SNIPER_WEIGHT     20
#define M249_WEIGHT       25

// Ammo capacities & quantities
#define _762_MAX_CARRY    30
#define _556_MAX_CARRY    200

#define EAGLE_MAX_CLIP    7
#define EAGLE_DEFAULT_GIVE 7

#define SNIPER_MAX_CLIP   5
#define SNIPER_DEFAULT_GIVE 5
#define AMMO_SNIPER_GIVE  15

#define M249_MAX_CLIP     50
#define M249_DEFAULT_GIVE 50
#define AMMO_M249_GIVE    50

// Damage values (autonomous constants avoiding core skill.h pollution)
#define GLADDER_DMG_PIPEWRENCH 35.0f
#define GLADDER_DMG_KNIFE      20.0f
#define GLADDER_DMG_KNIFE_BACK 60.0f
#define GLADDER_DMG_EAGLE      65.0f
#define GLADDER_DMG_SNIPER     100.0f
#define GLADDER_DMG_M249       16.0f

#endif // GLADDER_WEAPONS_H
