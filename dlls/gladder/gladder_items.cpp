/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Iconic Collectible Item (item_gladder_lambda) (SPEC §7.3, GitHub Issue #11)
 *
 ****/

#include "gladder_items.h"
#include "gladder_palette.h"
#include "gladder_audio.h"
#include "gladder_rules.h"
#include "gladder_scoring.h"
#include "gladder_usermsg.h"
#include "items/item_registry.h"


LINK_ENTITY_TO_CLASS( item_gladder_lambda, CGladderItemLambda );
LINK_ENTITY_TO_CLASS( item_gladder_collectible, CGladderItemLambda );

void CGladderItemLambda::PrecacheItem( void )
{
	PRECACHE_MODEL( "models/item_collectible.mdl" );
	GladderAudio::Precache();
}

void CGladderItemLambda::Precache( void )
{
	PrecacheItem();
}

void CGladderItemLambda::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/item_collectible.mdl" );

	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_TRIGGER;
	pev->avelocity = Vector( 0, 90.0f, 0 );

	// Vibrant orange glow shell presentation (SPEC §7.2, §7.3, Issue #107)
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector( GladderPalette::Collectible.r, GladderPalette::Collectible.g, GladderPalette::Collectible.b );
	pev->renderamt = 16;

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 32 ) );
	SetTouch( &CGladderItemLambda::ItemTouch );
}

void CGladderItemLambda::ItemTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

	// Award score bonus & telemetry progression
	int iCollectibleCount = 1;
	if ( g_pGameRules )
	{
		CGladderRules *pRules = (CGladderRules *)g_pGameRules;
		if ( pRules )
		{
			pRules->IncrementCollectibles();
			iCollectibleCount = pRules->GetCollectiblesCount();
		}
	}

	GladderAudio::PlayCue( GLADDER_CUE_COLLECTIBLE, pOther );
	ALERT( at_console, "[Gladder] Collectible picked up by %s (+%d pts)\n",
	       STRING( pOther->pev->netname ), GladderScoring::LAMBDA_COLLECTIBLE_POINTS );

	// Center-Screen Lambda Pickup Notification (SPEC §7.3, Issue #92)
	if ( gmsgGladderLambda && pOther->pev )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgGladderLambda, NULL, pOther->pev );
		WRITE_LONG( GladderScoring::LAMBDA_COLLECTIBLE_POINTS );
		WRITE_SHORT( iCollectibleCount );
		MESSAGE_END();
	}

	SetTouch( nullptr );
	UTIL_Remove( this );
}

void CGladderItemLambda::RegisterItems( void )
{
	ItemDescriptor descLambda;
	descLambda.pszClassname   = "item_gladder_lambda";
	descLambda.iPriorityOrder = 100;
	descLambda.pfnPrecache    = CGladderItemLambda::PrecacheItem;
	ItemRegistry::Register( descLambda );

	ItemDescriptor descCollectible;
	descCollectible.pszClassname   = "item_gladder_collectible";
	descCollectible.iPriorityOrder = 100;
	descCollectible.pfnPrecache    = CGladderItemLambda::PrecacheItem;
	ItemRegistry::Register( descCollectible );
}

void GladderRegisterImpulse101Items( void )
{
	// Opposing Force Extended Arsenal and custom ammunition (SPEC §7.1, §7.12, Issue #94)
	ItemRegistry::RegisterImpulse101Item( "weapon_pipewrench" );
	ItemRegistry::RegisterImpulse101Item( "weapon_knife" );
	ItemRegistry::RegisterImpulse101Item( "weapon_eagle" );
	ItemRegistry::RegisterImpulse101Item( "weapon_sniper" );
	ItemRegistry::RegisterImpulse101Item( "ammo_762" );
	ItemRegistry::RegisterImpulse101Item( "weapon_m249" );
	ItemRegistry::RegisterImpulse101Item( "ammo_556" );
	ItemRegistry::RegisterImpulse101Item( "ammo_egonclip" );
}

static struct GladderItemAutoReg
{
	GladderItemAutoReg()
	{
		CGladderItemLambda::RegisterItems();
		GladderRegisterImpulse101Items();
	}
} s_autoRegGladderItems;
