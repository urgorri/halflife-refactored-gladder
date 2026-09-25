/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Network User Messages Definition
 *
 ****/

#pragma once

#ifndef GLADDER_USERMSG_H
#define GLADDER_USERMSG_H

#ifdef VALVE_DLL
#include "core/user_message_registry.h"

extern int gmsgGladderWave;
extern int gmsgGladderTelemetry;
#endif

// Message names
#define GLADDER_MSG_WAVE_NAME "GladderWave"
#define GLADDER_MSG_TELEMETRY_NAME "GladderTelemetry"

// Fixed sizes for network messages (-1 means variable, or exact byte count)
// GladderWave:
// byte iState, short iWaveNumber, float flSessionTimeRemaining, float flLapTime
#define GLADDER_MSG_WAVE_SIZE 9

// GladderTelemetry:
// float flFastestLap, float flSlowestLap, float flAverageLap, short iCompletedLaps, short iFrags
#define GLADDER_MSG_TELEMETRY_SIZE 16

#endif // GLADDER_USERMSG_H
