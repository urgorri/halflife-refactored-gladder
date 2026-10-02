/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client-Side Arcade Visuals: Pickups Floating, Bobbing & Dynamic Lights
 *	(SPEC §7.2, GitHub Issue #12)
 *
 ****/

#pragma once

#ifndef GLADDER_VISUALS_H
#define GLADDER_VISUALS_H

enum EGladderPickupCategory
{
	GLADDER_PICKUP_NONE = 0,
	GLADDER_PICKUP_HEALTH,
	GLADDER_PICKUP_HEV,
	GLADDER_PICKUP_AMMO,
	GLADDER_PICKUP_LAMBDA,
	GLADDER_PICKUP_WEAPON
};

// Returns the glow color category for a given model path, or GLADDER_PICKUP_NONE.
EGladderPickupCategory ClassifyPickupModel( const char *pszModelName );

struct cl_entity_s;

// Modifier function registered with EntityVisualRegistry
void GladderPickupVisualModifier( int type, struct cl_entity_s *ent, const char *modelname );

#endif // GLADDER_VISUALS_H
