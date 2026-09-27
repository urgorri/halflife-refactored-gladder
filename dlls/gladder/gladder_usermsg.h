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
// GladderWave wire format (7 bytes total):
//   WRITE_BYTE  iState                  (1 byte)
//   WRITE_SHORT iWaveNumber             (2 bytes)
//   WRITE_COORD flSessionTimeRemaining  (2 bytes — GoldSrc WRITE_COORD is a scaled short)
//   WRITE_COORD flLapTime               (2 bytes)
#define GLADDER_MSG_WAVE_SIZE 7

// GladderTelemetry wire format (10 bytes total):
//   WRITE_COORD flFastestLap    (2 bytes — GoldSrc WRITE_COORD is a scaled short)
//   WRITE_COORD flSlowestLap    (2 bytes)
//   WRITE_COORD flAverageLap    (2 bytes)
//   WRITE_SHORT iCompletedLaps  (2 bytes)
//   WRITE_SHORT iFrags          (2 bytes)
#define GLADDER_MSG_TELEMETRY_SIZE 10

#endif // GLADDER_USERMSG_H
