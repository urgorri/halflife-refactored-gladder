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

#include "entities/entity_visual_registry.h"

bool EntityVisualRegistry::s_hasModifiers = false;
std::vector<EntityVisualModifierFunc> EntityVisualRegistry::s_modifiers;

void EntityVisualRegistry::RegisterModifier( EntityVisualModifierFunc pfnModifier )
{
	if ( !pfnModifier )
	{
		return;
	}

	s_modifiers.push_back( pfnModifier );
	s_hasModifiers = true;
}

void EntityVisualRegistry::ApplyModifiers( int type, struct cl_entity_s *ent, const char *modelname )
{
	for ( auto pfn : s_modifiers )
	{
		pfn( type, ent, modelname );
	}
}

const std::vector<EntityVisualModifierFunc> &EntityVisualRegistry::GetModifiers( void )
{
	return s_modifiers;
}

void EntityVisualRegistry::Clear( void )
{
	s_modifiers.clear();
	s_hasModifiers = false;
}
