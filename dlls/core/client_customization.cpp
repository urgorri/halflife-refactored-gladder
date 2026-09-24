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

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#include "core/client_customization.h"

#include <string.h>

static int g_iPendingDecalFrames[MAX_CLIENTS + 1];

int ValidateCustomDecalFrames( int nFrames )
{
	if ( nFrames > 0 && nFrames < 8 )
		return nFrames;
	return -1;
}

void CBasePlayer::SetCustomDecalFrames( int nFrames )
{
	m_nCustomSprayFrames = ValidateCustomDecalFrames( nFrames );
}

int CBasePlayer::GetCustomDecalFrames( void )
{
	return m_nCustomSprayFrames;
}

void CPlayerCustomizationBuffer::Init( void )
{
	ClearAll();
}

void CPlayerCustomizationBuffer::Reset( void )
{
	ClearAll();
}

int CPlayerCustomizationBuffer::GetPendingDecalFrames( int clientIndex )
{
	if ( clientIndex >= 1 && clientIndex <= MAX_CLIENTS )
		return g_iPendingDecalFrames[clientIndex];
	return 0;
}

void CPlayerCustomizationBuffer::SetPendingDecalFrames( int clientIndex, int frames )
{
	if ( clientIndex >= 1 && clientIndex <= MAX_CLIENTS )
		g_iPendingDecalFrames[clientIndex] = frames;
}

void CPlayerCustomizationBuffer::ClearPendingDecalFrames( int clientIndex )
{
	if ( clientIndex >= 1 && clientIndex <= MAX_CLIENTS )
		g_iPendingDecalFrames[clientIndex] = 0;
}

void CPlayerCustomizationBuffer::ClearAll( void )
{
	memset( g_iPendingDecalFrames, 0, sizeof( g_iPendingDecalFrames ) );
}

void CPlayerCustomizationBuffer::ApplyPendingDecalFrames( int clientIndex, CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	int frames = GetPendingDecalFrames( clientIndex );
	if ( frames > 0 )
	{
		pPlayer->SetCustomDecalFrames( frames );
		ClearPendingDecalFrames( clientIndex );
	}
	else
	{
		pPlayer->SetCustomDecalFrames( -1 );
	}
}

void CPlayerCustomizationBuffer::HandlePlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
	CBasePlayer *pPlayer = pEntity ? (CBasePlayer *)GET_PRIVATE( pEntity ) : nullptr;
	int clientIndex      = pEntity ? ENTINDEX( pEntity ) : 0;

	if ( !pPlayer )
	{
		// On Build 10210+ (Anniversary update), PlayerCustomization may be called during
		// client connection / resource negotiation before ClientPutInServer() instantiates CBasePlayer.
		// Buffer the pending customization for when the player entity spawns.
		if ( clientIndex >= 1 && clientIndex <= MAX_CLIENTS )
		{
			if ( !pCust )
			{
				ALERT( at_console, "PlayerCustomization:  NULL customization!\n" );
				return;
			}

			switch ( pCust->resource.type )
			{
			case t_decal:
				SetPendingDecalFrames( clientIndex, pCust->nUserData2 );
				break;
			case t_sound:
			case t_skin:
			case t_model:
				// Ignore for now.
				break;
			default:
				ALERT( at_console, "PlayerCustomization:  Unknown customization type!\n" );
				break;
			}
			return;
		}

		ALERT( at_console, "PlayerCustomization:  Couldn't get player!\n" );
		return;
	}

	if ( !pCust )
	{
		ALERT( at_console, "PlayerCustomization:  NULL customization!\n" );
		return;
	}

	switch ( pCust->resource.type )
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames( pCust->nUserData2 );
		if ( clientIndex >= 1 && clientIndex <= MAX_CLIENTS )
		{
			ClearPendingDecalFrames( clientIndex );
		}
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT( at_console, "PlayerCustomization:  Unknown customization type!\n" );
		break;
	}
}
