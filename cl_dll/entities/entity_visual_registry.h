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

#ifndef ENTITY_VISUAL_REGISTRY_H
#define ENTITY_VISUAL_REGISTRY_H

#include <vector>

struct cl_entity_s;

typedef void ( *EntityVisualModifierFunc )( int type, struct cl_entity_s *ent, const char *modelname );

class EntityVisualRegistry
{
  public:
	static void RegisterModifier( EntityVisualModifierFunc pfnModifier );
	static inline bool HasModifiers( void ) { return s_hasModifiers; }
	static void ApplyModifiers( int type, struct cl_entity_s *ent, const char *modelname );
	static const std::vector<EntityVisualModifierFunc> &GetModifiers( void );
	static void Clear( void );

  private:
	static bool s_hasModifiers;
	static std::vector<EntityVisualModifierFunc> s_modifiers;
};

#define REGISTER_ENTITY_VISUAL_MODIFIER( modifierFunc )                       \
	static struct EntityVisualAutoReg_##modifierFunc                          \
	{                                                                         \
		EntityVisualAutoReg_##modifierFunc()                                  \
		{                                                                     \
			EntityVisualRegistry::RegisterModifier( modifierFunc );           \
		}                                                                     \
	} s_autoRegVisual_##modifierFunc;

#endif // ENTITY_VISUAL_REGISTRY_H
