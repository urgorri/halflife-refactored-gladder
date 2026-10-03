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
#include <cctype>

#ifdef CLIENT_DLL
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_types.h"
#include "r_efx.h"
#include "entities/entity_visual_registry.h"
#else
#include "common/mathlib.h"
#include "common/const.h"
#include "common/entity_state.h"
#include "common/cl_entity.h"
#endif

EGladderPickupCategory ClassifyPickupModel( const char *pszModelName )
{
	if ( !pszModelName || !*pszModelName )
		return GLADDER_PICKUP_NONE;

	// Case-insensitive model name normalization
	char szLower[256];
	size_t len = strlen( pszModelName );
	if ( len >= sizeof( szLower ) )
		len = sizeof( szLower ) - 1;
	for ( size_t i = 0; i < len; ++i )
	{
		szLower[i] = (char)tolower( (unsigned char)pszModelName[i] );
	}
	szLower[len] = '\0';

	// Lambda / Collectible item
	if ( strstr( szLower, "item_collectible" ) ||
	     strstr( szLower, "gladder/lambda" ) ||
	     strstr( szLower, "lambda.mdl" ) )
	{
		return GLADDER_PICKUP_LAMBDA;
	}

	// Health & Medical
	if ( strstr( szLower, "w_medkit" ) ||
	     strstr( szLower, "w_healthkit" ) )
	{
		return GLADDER_PICKUP_HEALTH;
	}

	// Armor / HEV Batteries
	if ( strstr( szLower, "w_battery" ) )
	{
		return GLADDER_PICKUP_HEV;
	}

	// Ammunition & magazines (must be checked BEFORE weapons since models/w_9mmARclip starts with w_)
	if ( strstr( szLower, "9mmarclip" ) ||
	     strstr( szLower, "9mmclip" ) ||
	     strstr( szLower, "9mmbox" ) ||
	     strstr( szLower, "argrenade" ) ||
	     strstr( szLower, "357ammo" ) ||
	     strstr( szLower, "shotbox" ) ||
	     strstr( szLower, "crossbow_clip" ) ||
	     strstr( szLower, "rpgammo" ) ||
	     strstr( szLower, "gaussammo" ) ||
	     strstr( szLower, "saw_clip" ) ||
	     strstr( szLower, "m40a1clip" ) ||
	     strstr( szLower, "chainammo" ) ||
	     strstr( szLower, "spore_ammo" ) ||
	     strstr( szLower, "ammo_" ) )
	{
		return GLADDER_PICKUP_AMMO;
	}

	// Live snarks running or jumping in combat: strictly excluded from pickup visuals (SPEC §7.2, Issue #40)
	if ( strstr( szLower, "w_squeak" ) )
	{
		return GLADDER_PICKUP_NONE;
	}

	// Weapons — general catch for weapon models, including dropped ordnance items
	if ( strstr( szLower, "models/w_" ) ||
	     strstr( szLower, "/w_" ) ||
	     strstr( szLower, "w_crowbar" ) ||
	     strstr( szLower, "w_pipe_wrench" ) ||
	     strstr( szLower, "w_knife" ) ||
	     strstr( szLower, "w_desert_eagle" ) ||
	     strstr( szLower, "w_m40a1" ) ||
	     strstr( szLower, "w_saw" ) ||
	     strstr( szLower, "w_9mmar" ) ||
	     strstr( szLower, "w_grenade" ) ||
	     strstr( szLower, "w_satchel" ) ||
	     strstr( szLower, "w_sqknest" ) ||
	     strstr( szLower, "tripmine" ) )
	{
		return GLADDER_PICKUP_WEAPON;
	}

	return GLADDER_PICKUP_NONE;
}

bool ShouldApplyPickupVisuals( const struct cl_entity_s *ent, const char *pszModelName )
{
	if ( !pszModelName || !*pszModelName )
		return false;

	EGladderPickupCategory cat = ClassifyPickupModel( pszModelName );
	if ( cat == GLADDER_PICKUP_NONE )
		return false;

	if ( !ent )
		return true;

	// In-flight or active combat ordnance guard (SPEC §7.2, Issue #40):
	// Thrown grenades, satchels, or jumping/running snarks have MOVETYPE_BOUNCE or MOVETYPE_STEP,
	// planted tripmines or rockets have MOVETYPE_FLY, and active projectiles have owner > 0.
	if ( ent->curstate.movetype == MOVETYPE_BOUNCE ||
	     ent->curstate.movetype == MOVETYPE_FLY ||
	     ent->curstate.movetype == MOVETYPE_STEP ||
	     ent->curstate.owner > 0 )
	{
		return false;
	}

	// Dual-use models (grenades, satchels, tripmines) share assets between dropped collectible
	// weapons and active/planted ordnance. In GoldSrc, dropped collectible items have
	// solid == SOLID_TRIGGER, whereas deployed/thrown ordnance have SOLID_BBOX or SOLID_NOT.
	char szLower[256];
	size_t len = strlen( pszModelName );
	if ( len >= sizeof( szLower ) )
		len = sizeof( szLower ) - 1;
	for ( size_t i = 0; i < len; ++i )
	{
		szLower[i] = (char)tolower( (unsigned char)pszModelName[i] );
	}
	szLower[len] = '\0';

	if ( strstr( szLower, "w_grenade" ) ||
	     strstr( szLower, "w_satchel" ) ||
	     strstr( szLower, "tripmine" ) )
	{
		if ( ent->curstate.solid != SOLID_TRIGGER )
		{
			return false;
		}
	}

	return true;
}

#ifdef CLIENT_DLL
void GladderPickupVisualModifier( int type, struct cl_entity_s *ent, const char *modelname )
{
	if ( !ent || !modelname )
		return;

	if ( !ShouldApplyPickupVisuals( ent, modelname ) )
		return;

	EGladderPickupCategory cat = ClassifyPickupModel( modelname );
	if ( cat == GLADDER_PICKUP_NONE )
		return;

	float clientTime = gEngfuncs.GetClientTime ? gEngfuncs.GetClientTime() : 0.0f;

	// 1. Continuous vertical yaw rotation (90 deg/sec) across all renderer representation fields
	float flYaw = fmodf( clientTime * 90.0f, 360.0f );
	ent->angles[1] = flYaw;
	ent->curstate.angles[1] = flYaw;
	ent->latched.prevangles[1] = flYaw;

	// 2. Smooth vertical sine bobbing elevated by +16 units (SPEC §7.2, Issue #12)
	// Base elevation of +16 units ensures items and weapons never clip or sink into the floor
	float flSineOffset = 16.0f + sinf( clientTime * 3.0f ) * 4.0f;
	ent->origin[2] += flSineOffset;
	ent->curstate.origin[2] += flSineOffset;

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
