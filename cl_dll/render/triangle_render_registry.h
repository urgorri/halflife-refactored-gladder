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

#pragma once

#ifndef TRIANGLE_RENDER_REGISTRY_H
#define TRIANGLE_RENDER_REGISTRY_H

#include <vector>

// Callback function type for transparent triangle rendering
typedef void ( *TransparentTriangleCallback )( void );

// Callback function type for normal triangle rendering
typedef void ( *NormalTriangleCallback )( void );

// Interface for object-oriented transparent triangle rendering
class ITransparentTriangleHook
{
  public:
	virtual ~ITransparentTriangleHook() = default;
	virtual void RenderTransparentTriangles( void ) = 0;
};

// Interface for object-oriented normal triangle rendering
class INormalTriangleHook
{
  public:
	virtual ~INormalTriangleHook() = default;
	virtual void RenderNormalTriangles( void ) = 0;
};

// Registration descriptor for transparent triangle renderers
struct TransparentTriangleDescriptor
{
	TransparentTriangleCallback pfnCallback;
	ITransparentTriangleHook *pHook;
	int iDrawOrder;
	const char *pszName;
};

// Registration descriptor for normal triangle renderers
struct NormalTriangleDescriptor
{
	NormalTriangleCallback pfnCallback;
	INormalTriangleHook *pHook;
	int iDrawOrder;
	const char *pszName;
};

class TriangleRenderRegistry
{
  public:
	// --- Transparent Triangles ---
	static void RegisterTransparentCallback( TransparentTriangleCallback pfnCallback, int iDrawOrder = 0, const char *pszName = nullptr );
	static bool UnregisterTransparentCallback( TransparentTriangleCallback pfnCallback );

	static void RegisterTransparentHook( ITransparentTriangleHook *pHook, int iDrawOrder = 0, const char *pszName = nullptr );
	static bool UnregisterTransparentHook( ITransparentTriangleHook *pHook );

	static inline void RegisterTransparentRenderer( TransparentTriangleCallback pfnCallback, int iDrawOrder = 0, const char *pszName = nullptr )
	{
		RegisterTransparentCallback( pfnCallback, iDrawOrder, pszName );
	}

	static inline void RegisterTransparentRenderer( ITransparentTriangleHook *pHook, int iDrawOrder = 0, const char *pszName = nullptr )
	{
		RegisterTransparentHook( pHook, iDrawOrder, pszName );
	}

	static inline bool UnregisterTransparentRenderer( TransparentTriangleCallback pfnCallback )
	{
		return UnregisterTransparentCallback( pfnCallback );
	}

	static inline bool UnregisterTransparentRenderer( ITransparentTriangleHook *pHook )
	{
		return UnregisterTransparentHook( pHook );
	}

	static inline bool HasTransparentHooks( void ) { return s_hasTransparentHooks; }
	static void DispatchTransparentTriangles( void );
	static const std::vector<TransparentTriangleDescriptor> &GetTransparentRenderers( void );
	static const TransparentTriangleDescriptor *FindTransparentRenderer( const char *pszName );
	static void ClearTransparent( void );

	// --- Normal Triangles ---
	static void RegisterNormalCallback( NormalTriangleCallback pfnCallback, int iDrawOrder = 0, const char *pszName = nullptr );
	static bool UnregisterNormalCallback( NormalTriangleCallback pfnCallback );

	static void RegisterNormalHook( INormalTriangleHook *pHook, int iDrawOrder = 0, const char *pszName = nullptr );
	static bool UnregisterNormalHook( INormalTriangleHook *pHook );

	static inline void RegisterNormalRenderer( NormalTriangleCallback pfnCallback, int iDrawOrder = 0, const char *pszName = nullptr )
	{
		RegisterNormalCallback( pfnCallback, iDrawOrder, pszName );
	}

	static inline void RegisterNormalRenderer( INormalTriangleHook *pHook, int iDrawOrder = 0, const char *pszName = nullptr )
	{
		RegisterNormalHook( pHook, iDrawOrder, pszName );
	}

	static inline bool UnregisterNormalRenderer( NormalTriangleCallback pfnCallback )
	{
		return UnregisterNormalCallback( pfnCallback );
	}

	static inline bool UnregisterNormalRenderer( INormalTriangleHook *pHook )
	{
		return UnregisterNormalHook( pHook );
	}

	static inline bool HasNormalHooks( void ) { return s_hasNormalHooks; }
	static void DispatchNormalTriangles( void );
	static const std::vector<NormalTriangleDescriptor> &GetNormalRenderers( void );
	static const NormalTriangleDescriptor *FindNormalRenderer( const char *pszName );
	static void ClearNormal( void );

	// --- All ---
	static void Clear( void );

  private:
	static bool s_hasTransparentHooks;
	static bool s_hasNormalHooks;
};

// Convenience macros for static registration
#define REGISTER_TRANSPARENT_TRIANGLE_RENDERER( renderFunc )                  \
	static struct AutoRegTransparentTriangle_##renderFunc                     \
	{                                                                         \
		AutoRegTransparentTriangle_##renderFunc()                             \
		{                                                                     \
			TriangleRenderRegistry::RegisterTransparentCallback( renderFunc, 0, #renderFunc ); \
		}                                                                     \
	} s_autoRegTransparentTriangle_##renderFunc;

#define REGISTER_TRANSPARENT_TRIANGLE_RENDERER_ORDERED( renderFunc, drawOrder ) \
	static struct AutoRegTransparentTriangle_##renderFunc                     \
	{                                                                         \
		AutoRegTransparentTriangle_##renderFunc()                             \
		{                                                                     \
			TriangleRenderRegistry::RegisterTransparentCallback( renderFunc, drawOrder, #renderFunc ); \
		}                                                                     \
	} s_autoRegTransparentTriangle_##renderFunc;

#define REGISTER_TRANSPARENT_TRIANGLE_HOOK( hookInstance, drawOrder )         \
	static struct AutoRegTransparentHook_##hookInstance                       \
	{                                                                         \
		AutoRegTransparentHook_##hookInstance()                               \
		{                                                                     \
			TriangleRenderRegistry::RegisterTransparentHook( &hookInstance, drawOrder, #hookInstance ); \
		}                                                                     \
	} s_autoRegTransparentHook_##hookInstance;

#define REGISTER_NORMAL_TRIANGLE_RENDERER( renderFunc )                       \
	static struct AutoRegNormalTriangle_##renderFunc                          \
	{                                                                         \
		AutoRegNormalTriangle_##renderFunc()                                  \
		{                                                                     \
			TriangleRenderRegistry::RegisterNormalCallback( renderFunc, 0, #renderFunc ); \
		}                                                                     \
	} s_autoRegNormalTriangle_##renderFunc;

#define REGISTER_NORMAL_TRIANGLE_RENDERER_ORDERED( renderFunc, drawOrder )    \
	static struct AutoRegNormalTriangle_##renderFunc                          \
	{                                                                         \
		AutoRegNormalTriangle_##renderFunc()                                  \
		{                                                                     \
			TriangleRenderRegistry::RegisterNormalCallback( renderFunc, drawOrder, #renderFunc ); \
		}                                                                     \
	} s_autoRegNormalTriangle_##renderFunc;

#define REGISTER_NORMAL_TRIANGLE_HOOK( hookInstance, drawOrder )              \
	static struct AutoRegNormalHook_##hookInstance                            \
	{                                                                         \
		AutoRegNormalHook_##hookInstance()                                    \
		{                                                                     \
			TriangleRenderRegistry::RegisterNormalHook( &hookInstance, drawOrder, #hookInstance ); \
		}                                                                     \
	} s_autoRegNormalHook_##hookInstance;

#endif // TRIANGLE_RENDER_REGISTRY_H
