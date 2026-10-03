#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

typedef unsigned char byte;
typedef unsigned short word;
typedef float vec_t;
typedef float vec3_t[3];
typedef int ( *pfnUserMsgHook )( const char *pszName, int iSize, void *pbuf );

#if !defined( WRECTH )
#define WRECTH
typedef struct rect_s
{
	int left, right, top, bottom;
} wrect_t;
#endif

#include "common/const.h"
#include "common/cvardef.h"
#include "common/com_model.h"
#include "common/demo_api.h"
#include "common/r_studioint.h"
#ifdef _WIN32
#include "common/winsani_in.h"
#include <windows.h>
#include "common/winsani_out.h"
#endif
#include "engine/cdll_int.h"

// Forward declaration of TeamFortressViewport for pointer verification
class TeamFortressViewport;
extern TeamFortressViewport *gViewPort;

// Mock engine function tables
extern cl_enginefunc_t g_mockClientEngineFuncs;
extern engine_studio_api_t g_mockStudioApi;

// Command and message recording buffers
extern std::vector<std::string> g_mockServerCmds;
extern std::vector<std::string> g_mockClientCmds;
extern std::vector<std::string> g_mockHookedEvents;
extern std::unordered_map<std::string, cvar_t *> g_mockCvars;

// Simulated engine environment state
extern std::string g_mockMapName;
extern int g_mockPlayerCount;
extern float g_mockClientTime;
extern SCREENINFO g_mockScreenInfo;

// Initialization and teardown
void InitMockClientEngine( void );
void ResetMockClientEngine( void );

// State mutators for test scenarios
void SetMockMapName( const char *pszMapName );
void SetMockPlayerCount( int count );
void SetMockClientTime( float flTime );
void SetMockScreenInfo( int width, int height );
void SetMockSpriteList( struct client_sprite_s *pSprites, int count );
void ResetMockSpriteList( void );

// Dynamic library loading and client export resolution
bool LoadClientLibrary( const char *customPath = nullptr );
void UnloadClientLibrary( void );

// Function pointer signatures for client entry points
typedef int ( *pfnInitialize_t )( cl_enginefunc_t *pEnginefuncs, int iVersion );
typedef void ( *pfnHUD_Init_t )( void );
typedef int ( *pfnHUD_VidInit_t )( void );
typedef void ( *pfnHUD_Reset_t )( void );
typedef int ( *pfnHUD_GetStudioModelInterface_t )( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio );

extern pfnInitialize_t g_pfnInitialize;
extern pfnHUD_Init_t g_pfnHUD_Init;
extern pfnHUD_VidInit_t g_pfnHUD_VidInit;
extern pfnHUD_Reset_t g_pfnHUD_Reset;
extern pfnHUD_GetStudioModelInterface_t g_pfnHUD_GetStudioModelInterface;

// Client entry point wrappers
int ClientInitialize( cl_enginefunc_t *pEnginefuncs, int iVersion );
void ClientHUD_Init( void );
int ClientHUD_VidInit( void );
void ClientHUD_Reset( void );
int ClientHUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio );
