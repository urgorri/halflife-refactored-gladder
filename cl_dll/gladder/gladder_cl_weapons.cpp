/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Opposing Force Arsenal Client Prediction Registration
 *	(SPEC §7.1, GitHub Issue #13)
 *
 ****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#include "weapons/gladder_weapons.h"
#include "weapons/weapon_knife.h"
#include "weapons/weapon_pipewrench.h"
#include "weapons/weapon_eagle.h"
#include "weapons/weapon_sniper.h"
#include "weapons/weapon_m249.h"

#ifdef CLIENT_DLL
#include "hl/client_weapon_manager.h"

REGISTER_CLIENT_WEAPON( CKnife );
REGISTER_CLIENT_WEAPON( CPipewrench );
REGISTER_CLIENT_WEAPON( CEagle );
REGISTER_CLIENT_WEAPON( CSniperRifle );
REGISTER_CLIENT_WEAPON( CM249 );
#endif
