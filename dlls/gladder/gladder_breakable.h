/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	gladder_breakable - Wave-Resettable Destructible Entity (SPEC §2, §7.2, §11.2, §11.3, Issue #142)
 *
 ****/

#pragma once

#ifndef GLADDER_BREAKABLE_H
#define GLADDER_BREAKABLE_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "systems/func_break.h"
#include "gladder_palette.h"

//
// gladder_breakable
// Wave-resettable destructible entity. Inherits from CBreakable.
// When destroyed, executes full break side-effects (shards, gibs, acoustic cues,
// explosions, and target firing), and dynamically spawns any contained supply item
// at its world origin, but enters a hidden EF_NODRAW/SOLID_NOT state in memory instead
// of being removed via SUB_Remove.
// Regenerated back to its fresh original state upon each wave reset.
//
class CGladderBreakable : public CBreakable
{
  public:
	CGladderBreakable();

	void Spawn( void ) override;
	void Die( void ) override;
	void Killed( entvars_t *pevAttacker, int iGib ) override;
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) override;
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType ) override;
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) override;

	void Reset( void );

	// Procedural supply housing & category glow shell (SPEC §2, §7.2, §11.2, Issue #142)
	void SetContainedSupply( const char *szItemClassname, const GladderColorRGB &glowColor );
	void ClearContainedSupply( void );
	const char *GetContainedSupply( void ) const;
	bool HasContainedSupply( void ) const { return m_iszSupplyClassname != 0; }

	bool IsBroken( void ) const { return m_bBroken; }
	float GetOriginalHealth( void ) const { return m_flOriginalHealth; }
	int GetOriginalSolid( void ) const { return m_iOriginalSolid; }
	const Vector &GetOriginalMins( void ) const { return m_vecOriginalMins; }
	const Vector &GetOriginalMaxs( void ) const { return m_vecOriginalMaxs; }
	const Vector &GetOriginalOrigin( void ) const { return m_vecOriginalOrigin; }
	const Vector &GetCrateCenter( void ) const { return m_vecCrateCenter; }
	const char *GetOriginalModel( void ) const { return STRING( m_iszOriginalModel ); }

  private:
	float m_flOriginalHealth;
	string_t m_iszOriginalModel;
	string_t m_iszOriginalTargetname;
	Vector m_vecOriginalMins;
	Vector m_vecOriginalMaxs;
	Vector m_vecOriginalOrigin;
	Vector m_vecOriginalAngles;
	Vector m_vecCrateCenter;
	int m_iOriginalSolid;
	bool m_bBroken;
	string_t m_iszSupplyClassname;
};

#endif // GLADDER_BREAKABLE_H
