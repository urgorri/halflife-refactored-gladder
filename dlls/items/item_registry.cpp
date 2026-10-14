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

#include "item_registry.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <string>

#ifndef CLIENT_DLL
#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "core/player.h"
#endif

static std::vector<ItemDescriptor> &GetRegistryStorage()
{
	static std::vector<ItemDescriptor> s_items;
	return s_items;
}

static std::vector<const char *> &GetImpulse101Storage()
{
	static std::vector<const char *> s_impulse101Items;
	return s_impulse101Items;
}

void ItemRegistry::Register( const ItemDescriptor &desc )
{
	auto &storage = GetRegistryStorage();
	for ( const auto &existing : storage )
	{
		if ( existing.pszClassname && desc.pszClassname &&
		     strcmp( existing.pszClassname, desc.pszClassname ) == 0 )
		{
			return; // Avoid duplicate registration
		}
	}
	storage.push_back( desc );
}

void ItemRegistry::PrecacheAll( void )
{
	auto &storage = GetRegistryStorage();
	std::sort( storage.begin(), storage.end(),
	           []( const ItemDescriptor &a, const ItemDescriptor &b ) {
		           return a.iPriorityOrder < b.iPriorityOrder;
	           } );

	for ( const auto &desc : storage )
	{
#ifndef CLIENT_DLL
		if ( desc.pszClassname && *desc.pszClassname )
		{
			UTIL_PrecacheOther( desc.pszClassname );
		}
#endif
		if ( desc.pfnPrecache )
		{
			desc.pfnPrecache();
		}
	}
}

const std::vector<ItemDescriptor> &ItemRegistry::GetDescriptors( void )
{
	return GetRegistryStorage();
}

const ItemDescriptor *ItemRegistry::FindByClassname( const char *szClassname )
{
	if ( !szClassname )
		return nullptr;

	const auto &storage = GetRegistryStorage();
	for ( const auto &desc : storage )
	{
		if ( desc.pszClassname && strcmp( desc.pszClassname, szClassname ) == 0 )
		{
			return &desc;
		}
	}
	return nullptr;
}

void ItemRegistry::Clear( void )
{
	GetRegistryStorage().clear();
	GetImpulse101Storage().clear();
}

void ItemRegistry::RegisterImpulse101Item( const char *pszClassname )
{
	if ( !pszClassname || !*pszClassname )
		return;

	auto &storage = GetImpulse101Storage();
	for ( const char *existing : storage )
	{
		if ( strcmp( existing, pszClassname ) == 0 )
		{
			return; // Avoid duplicate registration
		}
	}
	storage.push_back( pszClassname );
}

const std::vector<const char *> &ItemRegistry::GetImpulse101Items( void )
{
	return GetImpulse101Storage();
}

static std::unordered_map<const CBasePlayer *, std::unordered_map<std::string, int>> &GetPlayerCustomItemsMap()
{
	static std::unordered_map<const CBasePlayer *, std::unordered_map<std::string, int>> s_playerCustomItems;
	return s_playerCustomItems;
}

int ItemRegistry::GetPlayerCustomItemCount( const CBasePlayer *pPlayer, const char *pszItemName )
{
	if ( !pPlayer || !pszItemName || !*pszItemName )
		return 0;

	auto &map = GetPlayerCustomItemsMap();
	auto playerIt = map.find( pPlayer );
	if ( playerIt == map.end() )
		return 0;

	auto itemIt = playerIt->second.find( pszItemName );
	if ( itemIt == playerIt->second.end() )
		return 0;

	return itemIt->second;
}

void ItemRegistry::AddPlayerCustomItem( const CBasePlayer *pPlayer, const char *pszItemName, int count )
{
	if ( !pPlayer || !pszItemName || !*pszItemName || count == 0 )
		return;

	auto &map = GetPlayerCustomItemsMap();
	auto &items = map[pPlayer];
	items[pszItemName] += count;
	if ( items[pszItemName] <= 0 )
	{
		items.erase( pszItemName );
	}
}

void ItemRegistry::SetPlayerCustomItemCount( const CBasePlayer *pPlayer, const char *pszItemName, int count )
{
	if ( !pPlayer || !pszItemName || !*pszItemName )
		return;

	auto &map = GetPlayerCustomItemsMap();
	if ( count <= 0 )
	{
		auto playerIt = map.find( pPlayer );
		if ( playerIt != map.end() )
		{
			playerIt->second.erase( pszItemName );
		}
		return;
	}

	map[pPlayer][pszItemName] = count;
}

bool ItemRegistry::HasPlayerCustomItem( const CBasePlayer *pPlayer, const char *pszItemName )
{
	return GetPlayerCustomItemCount( pPlayer, pszItemName ) > 0;
}

void ItemRegistry::ClearPlayerCustomItems( const CBasePlayer *pPlayer )
{
	if ( pPlayer )
	{
		GetPlayerCustomItemsMap().erase( pPlayer );
	}
}

void ItemRegistry::ClearAllPlayerCustomItems( void )
{
	GetPlayerCustomItemsMap().clear();
}

#ifndef CLIENT_DLL
int CBasePlayer::GetCustomItemCount( const char *pszItemName )
{
	return ItemRegistry::GetPlayerCustomItemCount( this, pszItemName );
}

void CBasePlayer::AddCustomItem( const char *pszItemName, int count )
{
	ItemRegistry::AddPlayerCustomItem( this, pszItemName, count );
}

void CBasePlayer::SetCustomItemCount( const char *pszItemName, int count )
{
	ItemRegistry::SetPlayerCustomItemCount( this, pszItemName, count );
}

BOOL CBasePlayer::HasCustomItem( const char *pszItemName )
{
	return ItemRegistry::HasPlayerCustomItem( this, pszItemName ) ? TRUE : FALSE;
}

void CBasePlayer::ClearCustomItems( void )
{
	ItemRegistry::ClearPlayerCustomItems( this );
}
#endif

