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

// Message names (must be <= 11 characters in GoldSrc engine REG_USER_MSG)
#define GLADDER_MSG_WAVE_NAME "GladWave"
#define GLADDER_MSG_TELEMETRY_NAME "GladTelem"

// Fixed sizes for network messages (-1 means variable, or exact byte count)
// GladderWave:
// byte iState, short iWaveNumber, float flSessionTimeRemaining, float flLapTime
#define GLADDER_MSG_WAVE_SIZE 9

// GladderTelemetry:
// float flFastestLap, float flSlowestLap, float flAverageLap, short iCompletedLaps, short iFrags
#define GLADDER_MSG_TELEMETRY_SIZE 16

#endif // GLADDER_USERMSG_H
