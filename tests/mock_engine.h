#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "extdll.h"

extern enginefuncs_t g_engfuncs;
extern globalvars_t *gpGlobals;

// Message recording buffers for testing engine network messages
extern std::vector<uint8_t> g_mockMessageBuffer;
extern std::vector<std::string> g_mockServerCommands;
extern int g_mockMessageDest;
extern int g_mockMessageType;
extern float g_mockMessageOrigin[3];
extern edict_t *g_mockMessageEdict;
extern TraceResult g_mockTraceResult;

void SetMockTraceLineResult( const TraceResult &tr );
void ResetMockEngine();
void InitMockEngine();
void UTIL_PrecacheOtherWeapon( const char *szClassname );
void UTIL_PrecacheOther( const char *szClassname );

void SetMockCvar( const char *szVarName, float flValue );
void SetMockCvar( const char *szVarName, const char *szValue );
void ClearMockCvars();

edict_t *GetMockClientEntity( int clientIndex );

typedef void ( *ENTITYFACTORY )( entvars_t *pev );
void RegisterMockEntityFactory( const char *pszClassname, ENTITYFACTORY pfnFactory );
void ClearMockEntityFactories();

extern int g_teamplay;
extern cvar_t teamplay;
extern cvar_t sv_busters;

extern std::string g_mockLastSaveChunk;
extern int g_mockLastSaveFieldCount;
extern TYPEDESCRIPTION *g_mockLastSaveFields;
extern std::string g_mockLastRestoreChunk;
extern std::string g_mockRestoreAvailableChunk;

extern std::vector<std::string> g_mockAlertMessages;
extern std::vector<std::string> g_mockClientCommands;

