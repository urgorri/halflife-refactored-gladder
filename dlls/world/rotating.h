/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
#ifndef ROTATING_H
#define ROTATING_H

#include "core/cbase.h"

#define noiseMovement noise
#define noiseStopMoving noise1
#define noiseRunning noise3

#define FANPITCHMIN 30
#define FANPITCHMAX 100

// Spawn flags for func_rotating
#define SF_BRUSH_ROTATE_START_ON 1
#define SF_BRUSH_ROTATE_BACKWARDS 2
#define SF_BRUSH_ROTATE_Z_AXIS 4
#define SF_BRUSH_ROTATE_X_AXIS 8
#define SF_BRUSH_ACCELLERATION 16
#define SF_BRUSH_ACCDCC 16
#define SF_BRUSH_ROTATE_TOUCH 32
#define SF_BRUSH_HURT 32
#define SF_BRUSH_ROTATE_INSTANT 1
#define SF_BRUSH_ROTATE_NOT_SOLID 64
#define SF_ROTATING_NOT_SOLID 64
#define SF_BRUSH_ROTATE_SMALLRADIUS 128
#define SF_BRUSH_ROTATE_MEDIUMRADIUS 256
#define SF_BRUSH_ROTATE_LARGERADIUS 512

class CFuncRotating : public CBaseEntity
{
  public:
	void Spawn( void );
	void Precache( void );
	void EXPORT SpinUp( void );
	void EXPORT SpinDown( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT HurtTouch( CBaseEntity *pOther );
	void EXPORT RotatingUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT Rotate( void );
	void RampPitchVol( int fUp );
	void Blocked( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	float m_flFanFriction;
	float m_flAttenuation;
	float m_flVolume;
	float m_pitch;
	int m_sounds;
};

// Spawn flags for func_pendulum
#define SF_PENDULUM_START_ON 1
#define SF_PENDULUM_SWING 2
#define SF_PENDULUM_PASSABLE 32
#define SF_DOOR_PASSABLE 8
#define SF_PENDULUM_AUTO_RETURN 16

class CPendulum : public CBaseEntity
{
  public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT Swing( void );
	void EXPORT PendulumUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT Stop( void );
	void Touch( CBaseEntity *pOther );
	void EXPORT RopeTouch( CBaseEntity *pOther ); // this touch func makes the pendulum a rope
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	void Blocked( CBaseEntity *pOther );

	static TYPEDESCRIPTION m_SaveData[];

	float m_accel;    // Acceleration
	float m_distance; //
	float m_time;
	float m_damp;
	float m_maxSpeed;
	float m_dampSpeed;
	vec3_t m_center;
	vec3_t m_start;
};

#endif // ROTATING_H
