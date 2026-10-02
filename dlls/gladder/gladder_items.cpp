/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Iconic Collectible Item (item_gladder_lambda) (SPEC §7.3, GitHub Issue #11)
 *
 ****/

#include "gladder_items.h"
#include "gladder_audio.h"
#include "gladder_rules.h"
#include "gladder_scoring.h"
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

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;

	// Vibrant orange glow shell presentation (SPEC §7.3)
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector( 255, 140, 20 );
	pev->renderamt = 16;

	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 32 ) );
	SetTouch( &CGladderItemLambda::ItemTouch );
}

void CGladderItemLambda::ItemTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

	// Award score bonus & telemetry progression
	if ( g_pGameRules )
	{
		CGladderRules *pRules = (CGladderRules *)g_pGameRules;
		if ( pRules )
		{
			pRules->IncrementCollectibles();
		}
	}

	GladderAudio::PlayCue( GLADDER_CUE_COLLECTIBLE, pOther );
	ALERT( at_console, "[Gladder] Collectible picked up by %s (+%d pts)\n",
	       STRING( pOther->pev->netname ), GladderScoring::LAMBDA_COLLECTIBLE_POINTS );

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

static struct GladderItemAutoReg
{
	GladderItemAutoReg()
	{
		CGladderItemLambda::RegisterItems();
	}
} s_autoRegGladderItems;
