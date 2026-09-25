/***
 *
 *	Half-Life: Gladder - Gauntlet Runner Mod
 *	Network User Messages Registration
 *
 ****/

#include "gladder_usermsg.h"

#ifdef VALVE_DLL
int gmsgGladderWave = 0;
int gmsgGladderTelemetry = 0;

REGISTER_USER_MSG( GladderWave, GLADDER_MSG_WAVE_SIZE, &gmsgGladderWave );
REGISTER_USER_MSG( GladderTelemetry, GLADDER_MSG_TELEMETRY_SIZE, &gmsgGladderTelemetry );
#endif
