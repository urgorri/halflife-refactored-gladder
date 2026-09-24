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

#ifndef USER_MESSAGE_REGISTRY_H
#define USER_MESSAGE_REGISTRY_H

#include <vector>

struct UserMessageDescriptor
{
	const char *pszName;
	int iSize;
	int *pVariable;
	int iMessageId;
};

class UserMessageRegistry
{
  public:
	static void Register( const char *pszName, int iSize, int *pVariable = nullptr );
	static void LinkAll( void );
	static int GetMessageId( const char *pszName );
	static const std::vector<UserMessageDescriptor> &GetDescriptors( void );
	static void Clear( void );

  private:
	static std::vector<UserMessageDescriptor> s_descriptors;
};

#define REGISTER_USER_MSG( name, size, pVar )                                 \
	static struct UserMsgAutoReg_##name                                       \
	{                                                                         \
		UserMsgAutoReg_##name()                                               \
		{                                                                     \
			UserMessageRegistry::Register( #name, size, pVar );               \
		}                                                                     \
	} s_autoRegMsg_##name;

#endif // USER_MESSAGE_REGISTRY_H
