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

#include "render/triangle_render_registry.h"
#include <algorithm>
#include <cstring>

bool TriangleRenderRegistry::s_hasTransparentHooks = false;
bool TriangleRenderRegistry::s_hasNormalHooks      = false;

static std::vector<TransparentTriangleDescriptor> &GetTransparentStorage()
{
	static std::vector<TransparentTriangleDescriptor> s_transparentRenderers;
	return s_transparentRenderers;
}

static std::vector<NormalTriangleDescriptor> &GetNormalStorage()
{
	static std::vector<NormalTriangleDescriptor> s_normalRenderers;
	return s_normalRenderers;
}

// ---------------------------------------------------------------------------
// Transparent Triangles
// ---------------------------------------------------------------------------

void TriangleRenderRegistry::RegisterTransparentCallback( TransparentTriangleCallback pfnCallback, int iDrawOrder, const char *pszName )
{
	if ( !pfnCallback )
		return;

	auto &storage = GetTransparentStorage();
	for ( auto &desc : storage )
	{
		if ( desc.pfnCallback == pfnCallback )
		{
			desc.iDrawOrder = iDrawOrder;
			if ( pszName )
				desc.pszName = pszName;

			std::stable_sort( storage.begin(), storage.end(),
			    []( const TransparentTriangleDescriptor &a, const TransparentTriangleDescriptor &b ) {
				    return a.iDrawOrder < b.iDrawOrder;
			    } );
			return;
		}
	}

	TransparentTriangleDescriptor desc;
	desc.pfnCallback = pfnCallback;
	desc.pHook       = nullptr;
	desc.iDrawOrder  = iDrawOrder;
	desc.pszName     = pszName;
	storage.push_back( desc );

	std::stable_sort( storage.begin(), storage.end(),
	    []( const TransparentTriangleDescriptor &a, const TransparentTriangleDescriptor &b ) {
		    return a.iDrawOrder < b.iDrawOrder;
	    } );

	s_hasTransparentHooks = true;
}

bool TriangleRenderRegistry::UnregisterTransparentCallback( TransparentTriangleCallback pfnCallback )
{
	if ( !pfnCallback )
		return false;

	auto &storage = GetTransparentStorage();
	for ( auto it = storage.begin(); it != storage.end(); ++it )
	{
		if ( it->pfnCallback == pfnCallback )
		{
			storage.erase( it );
			s_hasTransparentHooks = !storage.empty();
			return true;
		}
	}
	return false;
}

void TriangleRenderRegistry::RegisterTransparentHook( ITransparentTriangleHook *pHook, int iDrawOrder, const char *pszName )
{
	if ( !pHook )
		return;

	auto &storage = GetTransparentStorage();
	for ( auto &desc : storage )
	{
		if ( desc.pHook == pHook )
		{
			desc.iDrawOrder = iDrawOrder;
			if ( pszName )
				desc.pszName = pszName;

			std::stable_sort( storage.begin(), storage.end(),
			    []( const TransparentTriangleDescriptor &a, const TransparentTriangleDescriptor &b ) {
				    return a.iDrawOrder < b.iDrawOrder;
			    } );
			return;
		}
	}

	TransparentTriangleDescriptor desc;
	desc.pfnCallback = nullptr;
	desc.pHook       = pHook;
	desc.iDrawOrder  = iDrawOrder;
	desc.pszName     = pszName;
	storage.push_back( desc );

	std::stable_sort( storage.begin(), storage.end(),
	    []( const TransparentTriangleDescriptor &a, const TransparentTriangleDescriptor &b ) {
		    return a.iDrawOrder < b.iDrawOrder;
	    } );

	s_hasTransparentHooks = true;
}

bool TriangleRenderRegistry::UnregisterTransparentHook( ITransparentTriangleHook *pHook )
{
	if ( !pHook )
		return false;

	auto &storage = GetTransparentStorage();
	for ( auto it = storage.begin(); it != storage.end(); ++it )
	{
		if ( it->pHook == pHook )
		{
			storage.erase( it );
			s_hasTransparentHooks = !storage.empty();
			return true;
		}
	}
	return false;
}

void TriangleRenderRegistry::DispatchTransparentTriangles( void )
{
	if ( !s_hasTransparentHooks )
		return;

	const auto &storage = GetTransparentStorage();
	for ( size_t i = 0; i < storage.size(); ++i )
	{
		const auto &desc = storage[i];
		if ( desc.pHook )
		{
			desc.pHook->RenderTransparentTriangles();
		}
		else if ( desc.pfnCallback )
		{
			desc.pfnCallback();
		}
	}
}

const std::vector<TransparentTriangleDescriptor> &TriangleRenderRegistry::GetTransparentRenderers( void )
{
	return GetTransparentStorage();
}

const TransparentTriangleDescriptor *TriangleRenderRegistry::FindTransparentRenderer( const char *pszName )
{
	if ( !pszName )
		return nullptr;

	const auto &storage = GetTransparentStorage();
	for ( const auto &desc : storage )
	{
		if ( desc.pszName && strcmp( desc.pszName, pszName ) == 0 )
		{
			return &desc;
		}
	}
	return nullptr;
}

void TriangleRenderRegistry::ClearTransparent( void )
{
	GetTransparentStorage().clear();
	s_hasTransparentHooks = false;
}

// ---------------------------------------------------------------------------
// Normal Triangles
// ---------------------------------------------------------------------------

void TriangleRenderRegistry::RegisterNormalCallback( NormalTriangleCallback pfnCallback, int iDrawOrder, const char *pszName )
{
	if ( !pfnCallback )
		return;

	auto &storage = GetNormalStorage();
	for ( auto &desc : storage )
	{
		if ( desc.pfnCallback == pfnCallback )
		{
			desc.iDrawOrder = iDrawOrder;
			if ( pszName )
				desc.pszName = pszName;

			std::stable_sort( storage.begin(), storage.end(),
			    []( const NormalTriangleDescriptor &a, const NormalTriangleDescriptor &b ) {
				    return a.iDrawOrder < b.iDrawOrder;
			    } );
			return;
		}
	}

	NormalTriangleDescriptor desc;
	desc.pfnCallback = pfnCallback;
	desc.pHook       = nullptr;
	desc.iDrawOrder  = iDrawOrder;
	desc.pszName     = pszName;
	storage.push_back( desc );

	std::stable_sort( storage.begin(), storage.end(),
	    []( const NormalTriangleDescriptor &a, const NormalTriangleDescriptor &b ) {
		    return a.iDrawOrder < b.iDrawOrder;
	    } );

	s_hasNormalHooks = true;
}

bool TriangleRenderRegistry::UnregisterNormalCallback( NormalTriangleCallback pfnCallback )
{
	if ( !pfnCallback )
		return false;

	auto &storage = GetNormalStorage();
	for ( auto it = storage.begin(); it != storage.end(); ++it )
	{
		if ( it->pfnCallback == pfnCallback )
		{
			storage.erase( it );
			s_hasNormalHooks = !storage.empty();
			return true;
		}
	}
	return false;
}

void TriangleRenderRegistry::RegisterNormalHook( INormalTriangleHook *pHook, int iDrawOrder, const char *pszName )
{
	if ( !pHook )
		return;

	auto &storage = GetNormalStorage();
	for ( auto &desc : storage )
	{
		if ( desc.pHook == pHook )
		{
			desc.iDrawOrder = iDrawOrder;
			if ( pszName )
				desc.pszName = pszName;

			std::stable_sort( storage.begin(), storage.end(),
			    []( const NormalTriangleDescriptor &a, const NormalTriangleDescriptor &b ) {
				    return a.iDrawOrder < b.iDrawOrder;
			    } );
			return;
		}
	}

	NormalTriangleDescriptor desc;
	desc.pfnCallback = nullptr;
	desc.pHook       = pHook;
	desc.iDrawOrder  = iDrawOrder;
	desc.pszName     = pszName;
	storage.push_back( desc );

	std::stable_sort( storage.begin(), storage.end(),
	    []( const NormalTriangleDescriptor &a, const NormalTriangleDescriptor &b ) {
		    return a.iDrawOrder < b.iDrawOrder;
	    } );

	s_hasNormalHooks = true;
}

bool TriangleRenderRegistry::UnregisterNormalHook( INormalTriangleHook *pHook )
{
	if ( !pHook )
		return false;

	auto &storage = GetNormalStorage();
	for ( auto it = storage.begin(); it != storage.end(); ++it )
	{
		if ( it->pHook == pHook )
		{
			storage.erase( it );
			s_hasNormalHooks = !storage.empty();
			return true;
		}
	}
	return false;
}

void TriangleRenderRegistry::DispatchNormalTriangles( void )
{
	if ( !s_hasNormalHooks )
		return;

	const auto &storage = GetNormalStorage();
	for ( size_t i = 0; i < storage.size(); ++i )
	{
		const auto &desc = storage[i];
		if ( desc.pHook )
		{
			desc.pHook->RenderNormalTriangles();
		}
		else if ( desc.pfnCallback )
		{
			desc.pfnCallback();
		}
	}
}

const std::vector<NormalTriangleDescriptor> &TriangleRenderRegistry::GetNormalRenderers( void )
{
	return GetNormalStorage();
}

const NormalTriangleDescriptor *TriangleRenderRegistry::FindNormalRenderer( const char *pszName )
{
	if ( !pszName )
		return nullptr;

	const auto &storage = GetNormalStorage();
	for ( const auto &desc : storage )
	{
		if ( desc.pszName && strcmp( desc.pszName, pszName ) == 0 )
		{
			return &desc;
		}
	}
	return nullptr;
}

void TriangleRenderRegistry::ClearNormal( void )
{
	GetNormalStorage().clear();
	s_hasNormalHooks = false;
}

// ---------------------------------------------------------------------------
// Clear All
// ---------------------------------------------------------------------------

void TriangleRenderRegistry::Clear( void )
{
	ClearTransparent();
	ClearNormal();
}
