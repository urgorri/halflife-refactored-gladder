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

#include <cstddef>

#ifdef CLIENT_DLL
#include "core/util_vector.h"
#else
#include "cl_dll/core/util_vector.h"
#endif

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

// Checks whether an entity and model qualify for Gladder pickup visual effects (floating, bobbing, glow).
// Excludes in-flight or active combat ordnance (thrown grenades, active satchels, planted tripmines,
// and live snarks), while allowing collectible weapon pickups on the ground (SPEC §7.2, Issue #40).
bool ShouldApplyPickupVisuals( const struct cl_entity_s *ent, const char *pszModelName );

// Active wall charger descriptor tracked for TriAPI glow shell rendering (SPEC §3, §7.2, Issue #48)
struct GladderActiveChargerInfo
{
	int entityIndex;
	Vector origin;
	Vector mins;
	Vector maxs;
	unsigned char r;
	unsigned char g;
	unsigned char b;
	float lastSeenTime;
};

// Checks whether an entity and model qualify as an active wall charger needing a glow shell (SPEC §3, §7.2, Issue #48).
// Requires brush model (modelname[0] == '*'), renderfx == kRenderFxGlowShell, frame == 0,
// and matching Health (32, 255, 32) or HEV (255, 140, 20) color.
bool ShouldApplyChargerGlowShell( const struct cl_entity_s *ent, const char *pszModelName );

// Computes the world-space bounding box expanded by flDelta for a charger brush
void GladderComputeChargerGlowBounds( const Vector &origin, const Vector &mins, const Vector &maxs, float flDelta, Vector &outMins, Vector &outMaxs );

// Modifier functions registered with EntityVisualRegistry
void GladderPickupVisualModifier( int type, struct cl_entity_s *ent, const char *modelname );
void GladderChargerVisualModifier( int type, struct cl_entity_s *ent, const char *modelname );

// Transparent triangle renderer registered with TriangleRenderRegistry (SPEC §3, §7.2, Issue #48)
void GladderDrawChargerGlowShells( void );

// State management helpers (e.g. for wave transitions and unit testing)
void GladderClearActiveChargers( void );
size_t GladderGetActiveChargerCount( void );
bool GladderGetActiveChargerInfo( size_t index, GladderActiveChargerInfo &outInfo );

#endif // GLADDER_VISUALS_H
