#pragma once
#ifndef TURRETS_H
#define TURRETS_H

#include "core/extdll.h"
#include "core/util.h"
#include "core/cbase.h"
#include "ai/monsters.h"
#include "systems/effects.h"
#include "weapons/weapon_base.h"

#define TURRET_SHOTS 2
#define TURRET_RANGE ( 100 * 12 )
#define TURRET_SPREAD Vector( 0, 0, 0 )
#define TURRET_TURNRATE 30 // angles per 0.1 second
#define TURRET_MAXWAIT 15  // seconds turret will stay active w/o a target
#define TURRET_MAXSPIN 5   // seconds turret barrel will spin w/o a target
#define TURRET_MACHINE_VOLUME 0.5

typedef enum
{
	TURRET_ANIM_NONE = 0,
	TURRET_ANIM_FIRE,
	TURRET_ANIM_SPIN,
	TURRET_ANIM_DEPLOY,
	TURRET_ANIM_RETIRE,
	TURRET_ANIM_DIE,
} TURRET_ANIM;

class CBaseTurret : public CBaseMonster
{
  public:
	void Spawn( void );
	virtual void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int Classify( void );

	int BloodColor( void ) { return DONT_BLEED; }
	void GibMonster( void ) {}

	void EXPORT ActiveThink( void );
	void EXPORT SearchThink( void );
	void EXPORT AutoSearchThink( void );
	void EXPORT TurretDeath( void );

	virtual void EXPORT SpinDownCall( void ) { m_iSpin = 0; }
	virtual void EXPORT SpinUpCall( void ) { m_iSpin = 1; }

	void EXPORT Deploy( void );
	void EXPORT Retire( void );

	void EXPORT Initialize( void );

	virtual void Ping( void );
	virtual void EyeOn( void );
	virtual void EyeOff( void );
	void UpdateOnRemove( void );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	void SetTurretAnim( TURRET_ANIM anim );
	int MoveTurret( void );
	virtual void Shoot( Vector &vecSrc, Vector &vecDirToEnemy ) {};

	float m_flMaxSpin;
	int m_iSpin;

	CSprite *m_pEyeGlow;
	int m_eyeBrightness;

	int m_iDeployHeight;
	int m_iRetractHeight;
	int m_iMinPitch;

	int m_iBaseTurnRate;
	float m_fTurnRate;
	int m_iOrientation;
	int m_iOn;
	int m_fBeserk;
	int m_iAutoStart;

	Vector m_vecLastSight;
	float m_flLastSight;
	float m_flMaxWait;
	int m_iSearchSpeed;

	float m_flStartYaw;
	Vector m_vecCurAngles;
	Vector m_vecGoalAngles;

	float m_flPingTime;
	float m_flSpinUpTime;
};

class CTurret : public CBaseTurret
{
  public:
	void Spawn( void );
	void Precache( void );
	void SpinUpCall( void );
	void SpinDownCall( void );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	void Shoot( Vector &vecSrc, Vector &vecDirToEnemy );

  private:
	int m_iStartSpin;
};

class CMiniTurret : public CBaseTurret
{
  public:
	void Spawn();
	void Precache( void );
	void Shoot( Vector &vecSrc, Vector &vecDirToEnemy );
};

class CSentry : public CBaseTurret
{
  public:
	void Spawn();
	void Precache( void );
	void Shoot( Vector &vecSrc, Vector &vecDirToEnemy );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void EXPORT SentryTouch( CBaseEntity *pOther );
	void EXPORT SentryDeath( void );
};

#endif // TURRETS_H
