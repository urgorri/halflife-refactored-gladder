/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Canonical Glow & Dynamic Light Palette
 *	(SPEC §7.2, GitHub Issue #107)
 *
 ****/

#pragma once

#ifndef GLADDER_PALETTE_H
#define GLADDER_PALETTE_H

struct GladderColorRGB
{
	unsigned char r;
	unsigned char g;
	unsigned char b;

	GladderColorRGB() : r( 255 ), g( 255 ), b( 255 ) {}
	GladderColorRGB( unsigned char red, unsigned char green, unsigned char blue )
	    : r( red ), g( green ), b( blue )
	{
	}
};

namespace GladderPalette
{
	// Canonical glow & dynamic-light palette (SPEC §7.2, GitHub Issue #107)
	// Health: Medkits, wall func_healthcharger
	static const GladderColorRGB Health( 0, 255, 50 );

	// Shield: Batteries, wall func_recharge (HEV charger)
	static const GladderColorRGB Shield( 0, 220, 255 );

	// Ammunition: Ammo boxes, clips, cells
	static const GladderColorRGB Ammunition( 240, 255, 20 );

	// Fixed Weapons: Shotgun, MP5, Crossbow, Gauss, etc.
	static const GladderColorRGB FixedWeapons( 170, 30, 255 );

	// Consumables: Grenades, Satchels, Snarks, Tripmines
	static const GladderColorRGB Consumables( 255, 20, 160 );

	// Collectible: Rotating Lambda logo
	static const GladderColorRGB Collectible( 255, 120, 0 );

	// Threat: Elite Champion monsters
	static const GladderColorRGB Threat( 255, 20, 20 );

	// Level Design Triggers: Start & Finish boundaries
	static const GladderColorRGB Triggers( 255, 32, 32 );
}

#endif // GLADDER_PALETTE_H
