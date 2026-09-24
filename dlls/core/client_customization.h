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
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#ifndef CLIENT_CUSTOMIZATION_H
#define CLIENT_CUSTOMIZATION_H

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 32
#endif

class CBasePlayer;
struct edict_s;
typedef struct edict_s edict_t;
struct customization_s;
typedef struct customization_s customization_t;

// Validates and clamps custom decal animation frame counts (1 to 7 inclusive).
// Returns -1 if invalid or not present.
int ValidateCustomDecalFrames( int nFrames );

// Client customization buffer for managing pre-spawn resource registrations (Build 10210+)
class CPlayerCustomizationBuffer
{
  public:
	static void Init( void );
	static void Reset( void );

	static int GetPendingDecalFrames( int clientIndex );
	static void SetPendingDecalFrames( int clientIndex, int frames );
	static void ClearPendingDecalFrames( int clientIndex );
	static void ClearAll( void );

	// Applies any pending decal frames to the spawning player.
	// If no pending frames exist, defaults to -1 (none).
	static void ApplyPendingDecalFrames( int clientIndex, CBasePlayer *pPlayer );

	// Handles engine PlayerCustomization callback, buffering if pPlayer is NULL
	// on a connecting client edict, or updating pPlayer directly if spawned.
	static void HandlePlayerCustomization( edict_t *pEntity, customization_t *pCust );
};

#endif // CLIENT_CUSTOMIZATION_H
