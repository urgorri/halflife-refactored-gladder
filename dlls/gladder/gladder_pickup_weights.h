/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Procedural Pickup Weight Calculation & Loadout-Aware Ammo Balancing
 *	(SPEC §3, GitHub Issue #93)
 *
 ****/

#pragma once

#ifndef GLADDER_PICKUP_WEIGHTS_H
#define GLADDER_PICKUP_WEIGHTS_H

#include <vector>
#include <string>
#include <unordered_map>
#include <cstddef>
#include <algorithm>

enum GladderPickupCategory
{
	GLADDER_CAT_AMMO = 0,
	GLADDER_CAT_HEALTH_ARMOR,
	GLADDER_CAT_ORDNANCE,
	GLADDER_CAT_WEAPON,
	GLADDER_CAT_COUNT
};

struct GladderWeightedPickup
{
	const char *szClassname;
	float flWeight;
	int iCategory;

	GladderWeightedPickup() : szClassname( "" ), flWeight( 0.0f ), iCategory( 0 ) {}
	GladderWeightedPickup( const char *pszName, float flW, int iCat )
	    : szClassname( pszName ), flWeight( flW ), iCategory( iCat ) {}
};

struct GladderPickupWeightInput
{
	int iWaveNumber;
	bool allowFixedWeapons;
	std::vector<std::string> ownedWeapons;
	// Ammo reserve fraction [0.0, 1.0] keyed by ammo type ("uranium", "9mm") or entity ("ammo_gaussclip")
	std::unordered_map<std::string, float> ammoReserveFractions;
	// Consecutive waves elapsed since ammo entity was last spawned
	std::unordered_map<std::string, int> wavesSinceLastAmmoSpawn;

	GladderPickupWeightInput() : iWaveNumber( 1 ), allowFixedWeapons( true ) {}

	bool HasWeapon( const char *szClassname ) const;
	float GetAmmoReserveFraction( const char *szClassname, const char *szAmmoType ) const;
	int GetWavesSinceLastAmmoSpawn( const char *szClassname ) const;
};

// Data definition for ammo to weapon compatibility
struct GladderAmmoMapping
{
	const char *szClassname;
	const char *szAmmoType;
	int iTier;
	float flBaseWeight;
	const char *compatibleWeapons[4]; // null-terminated weapon classnames
	int iMaxCarry;

	GladderAmmoMapping()
	    : szClassname( "" ), szAmmoType( "" ), iTier( 1 ), flBaseWeight( 0.0f ), iMaxCarry( 100 )
	{
		compatibleWeapons[0] = nullptr;
		compatibleWeapons[1] = nullptr;
		compatibleWeapons[2] = nullptr;
		compatibleWeapons[3] = nullptr;
	}

	GladderAmmoMapping( const char *pszClass, const char *pszType, int tier, float baseW,
	                    const char *w0, const char *w1, const char *w2, int maxCarry )
	    : szClassname( pszClass ), szAmmoType( pszType ), iTier( tier ), flBaseWeight( baseW ), iMaxCarry( maxCarry )
	{
		compatibleWeapons[0] = w0;
		compatibleWeapons[1] = w1;
		compatibleWeapons[2] = w2;
		compatibleWeapons[3] = nullptr;
	}
};

// Pure math & logic: Computes weighted pickup list given wave and player loadout state
std::vector<GladderWeightedPickup> ComputePickupWeights( const GladderPickupWeightInput &input );

// Pure tier curve helper functions
float GetWeaponTierWeight( int tier, int iWaveNumber );
float GetOrdnanceTierWeight( int tier, int iWaveNumber );

// Query ammo mapping table
const std::vector<GladderAmmoMapping> &GetGladderAmmoMappings( void );

#endif // GLADDER_PICKUP_WEIGHTS_H
