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

#include <cstring>
#include "core/user_message_registry.h"
#include "core/extdll.h"
#include "core/util.h"

std::vector<UserMessageDescriptor> UserMessageRegistry::s_descriptors;

void UserMessageRegistry::Register( const char *pszName, int iSize, int *pVariable )
{
	if ( !pszName )
	{
		return;
	}

	UserMessageDescriptor desc;
	desc.pszName    = pszName;
	desc.iSize      = iSize;
	desc.pVariable  = pVariable;
	desc.iMessageId = 0;

	s_descriptors.push_back( desc );
}

void UserMessageRegistry::LinkAll( void )
{
	for ( auto &desc : s_descriptors )
	{
		int msgId = REG_USER_MSG( (char *)desc.pszName, desc.iSize );
		desc.iMessageId = msgId;
		if ( desc.pVariable )
		{
			*desc.pVariable = msgId;
		}
	}
}

int UserMessageRegistry::GetMessageId( const char *pszName )
{
	if ( !pszName )
	{
		return 0;
	}

	for ( const auto &desc : s_descriptors )
	{
		if ( std::strcmp( desc.pszName, pszName ) == 0 )
		{
			return desc.iMessageId;
		}
	}

	return 0;
}

const std::vector<UserMessageDescriptor> &UserMessageRegistry::GetDescriptors( void )
{
	return s_descriptors;
}

void UserMessageRegistry::Clear( void )
{
	s_descriptors.clear();
}
