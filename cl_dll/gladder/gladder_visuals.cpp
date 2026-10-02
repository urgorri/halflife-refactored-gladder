/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Client-Side Arcade Visuals: Pickups Floating, Bobbing & Dynamic Lights
 *	(SPEC §7.2, GitHub Issue #12)
 *
 ****/

#include "gladder_visuals.h"
#include <cstring>
#include <cmath>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_types.h"
#include "r_efx.h"
#include "entities/entity_visual_registry.h"
#endif

EGladderPickupCategory ClassifyPickupModel( const char *pszModelName )
{
	if ( !pszModelName || !*pszModelName )
		return GLADDER_PICKUP_NONE;

	// Lambda / Collectible item
	if ( strstr( pszModelName, "item_collectible" ) ||
	     strstr( pszModelName, "gladder/lambda" ) ||
	     strstr( pszModelName, "lambda.mdl" ) )
	{
		return GLADDER_PICKUP_LAMBDA;
	}

	// Health & Medical
	if ( strstr( pszModelName, "w_medkit" ) ||
	     strstr( pszModelName, "w_healthkit" ) )
	{
		return GLADDER_PICKUP_HEALTH;
	}

	// Armor / HEV Batteries
	if ( strstr( pszModelName, "w_battery" ) )
	{
		return GLADDER_PICKUP_HEV;
	}

	// Ammunition & magazines
	if ( strstr( pszModelName, "w_9mmclip" ) ||
	     strstr( pszModelName, "w_357ammo" ) ||
	     strstr( pszModelName, "w_shotbox" ) ||
	     strstr( pszModelName, "w_crossbow_clip" ) ||
	     strstr( pszModelName, "w_rpgammo" ) ||
	     strstr( pszModelName, "w_gaussammo" ) ||
	     strstr( pszModelName, "w_saw_clip" ) ||
	     strstr( pszModelName, "w_m40a1clip" ) ||
	     strstr( pszModelName, "ammo_" ) )
	{
		return GLADDER_PICKUP_AMMO;
	}

	// Weapons — general catch for weapon models
	if ( strstr( pszModelName, "models/w_" ) ||
	     strstr( pszModelName, "/w_" ) ||
	     strstr( pszModelName, "w_crowbar" ) ||
	     strstr( pszModelName, "w_pipe_wrench" ) ||
	     strstr( pszModelName, "w_knife" ) ||
	     strstr( pszModelName, "w_desert_eagle" ) ||
	     strstr( pszModelName, "w_m40a1" ) ||
	     strstr( pszModelName, "w_saw" ) )
	{
		return GLADDER_PICKUP_WEAPON;
	}

	return GLADDER_PICKUP_NONE;
}

#ifdef CLIENT_DLL
void GladderPickupVisualModifier( int type, struct cl_entity_s *ent, const char *modelname )
{
	if ( !ent || !modelname )
		return;

	EGladderPickupCategory cat = ClassifyPickupModel( modelname );
	if ( cat == GLADDER_PICKUP_NONE )
		return;

	float clientTime = gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0f;

	// 1. Continuous vertical yaw rotation (90 deg/sec)
	ent->angles[1] = fmodf( clientTime * 90.0f, 360.0f );

	// 2. Smooth vertical sine bobbing (~4 units amplitude)
	float flSineOffset = sinf( clientTime * 3.0f ) * 4.0f;
	ent->origin[2] += flSineOffset;

	// 3. Render glow shell color-coded by pickup category (SPEC §7.2)
	unsigned char r = 255, g = 255, b = 255;
	switch ( cat )
	{
	case GLADDER_PICKUP_HEALTH:
		r = 32; g = 255; b = 32;   // Vibrant Green
		break;
	case GLADDER_PICKUP_HEV:
		r = 32; g = 192; b = 255;  // Bright Cyan / Blue
		break;
	case GLADDER_PICKUP_AMMO:
		r = 255; g = 180; b = 20;  // Golden Amber
		break;
	case GLADDER_PICKUP_LAMBDA:
		r = 255; g = 140; b = 20;  // Vibrant Orange
		break;
	case GLADDER_PICKUP_WEAPON:
		r = 220; g = 40; b = 220;  // Arcade Magenta / Violet
		break;
	default:
		break;
	}

	ent->curstate.renderfx = kRenderFxGlowShell;
	ent->curstate.rendercolor.r = r;
	ent->curstate.rendercolor.g = g;
	ent->curstate.rendercolor.b = b;
	ent->curstate.renderamt = 16;

	// 4. Localized dynamic colored point light projection
	if ( gEngfuncs.pEfxAPI && gEngfuncs.pEfxAPI->CL_AllocDlight )
	{
		dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( ent->index );
		if ( dl )
		{
			dl->origin = ent->origin;
			dl->radius = ( cat == GLADDER_PICKUP_LAMBDA ) ? 140.0f : 100.0f;
			dl->color.r = r;
			dl->color.g = g;
			dl->color.b = b;
			dl->die = clientTime + 0.05f;
			dl->decay = 300.0f;
		}
	}
}

REGISTER_ENTITY_VISUAL_MODIFIER( GladderPickupVisualModifier );
#endif
