#include "tests/mock_client_engine.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined( _WIN32 )
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

// Global definitions
TeamFortressViewport *gViewPort = nullptr;

cl_enginefunc_t g_mockClientEngineFuncs;
engine_studio_api_t g_mockStudioApi;

std::vector<std::string> g_mockServerCmds;
std::vector<std::string> g_mockClientCmds;
std::vector<std::string> g_mockHookedEvents;
std::unordered_map<std::string, cvar_t *> g_mockCvars;

std::string g_mockMapName = "mock_test_map";
int g_mockPlayerCount     = 1;
float g_mockClientTime    = 1.0f;
SCREENINFO g_mockScreenInfo;

pfnInitialize_t g_pfnInitialize                                   = nullptr;
pfnHUD_Init_t g_pfnHUD_Init                                       = nullptr;
pfnHUD_VidInit_t g_pfnHUD_VidInit                                 = nullptr;
pfnHUD_Reset_t g_pfnHUD_Reset                                     = nullptr;
pfnHUD_GetStudioModelInterface_t g_pfnHUD_GetStudioModelInterface = nullptr;

#if defined( _WIN32 )
static HMODULE s_hClientLib = NULL;
#else
static void *s_hClientLib = nullptr;
#endif

// Dummy entity and player state storage
static cl_entity_t s_mockEntities[MAX_CLIENTS + 1];
static player_info_t s_mockPlayerInfo[MAX_CLIENTS + 1];
static struct demo_api_s s_mockDemoApi;

// -----------------------------------------------------------------------------
// cl_enginefunc_t Stubs
// -----------------------------------------------------------------------------
static struct cvar_s *stub_RegisterVariable( char *szName, char *szValue, int flags )
{
	if ( !szName )
		return nullptr;

	auto it = g_mockCvars.find( szName );
	if ( it != g_mockCvars.end() )
	{
		cvar_t *c = it->second;
		if ( szValue )
		{
			free( c->string );
			c->string = (char *)malloc( strlen( szValue ) + 1 );
			strcpy( c->string, szValue );
			c->value = (float)atof( szValue );
		}
		return c;
	}

	cvar_t *c = (cvar_t *)malloc( sizeof( cvar_t ) );
	c->name   = (char *)malloc( strlen( szName ) + 1 );
	strcpy( c->name, szName );

	if ( szValue )
	{
		c->string = (char *)malloc( strlen( szValue ) + 1 );
		strcpy( c->string, szValue );
		c->value = (float)atof( szValue );
	}
	else
	{
		c->string = (char *)malloc( 1 );
		c->string[0] = '\0';
		c->value = 0.0f;
	}

	c->flags = flags;
	c->next  = nullptr;

	g_mockCvars[szName] = c;
	return c;
}

static struct cvar_s *stub_GetCvarPointer( const char *szName )
{
	if ( !szName )
		return nullptr;

	auto it = g_mockCvars.find( szName );
	if ( it != g_mockCvars.end() )
		return it->second;

	return nullptr;
}

static float stub_GetCvarFloat( char *szName )
{
	cvar_t *c = stub_GetCvarPointer( szName );
	return c ? c->value : 0.0f;
}

static char *stub_GetCvarString( char *szName )
{
	static char s_empty[1] = { 0 };
	cvar_t *c = stub_GetCvarPointer( szName );
	return c ? c->string : s_empty;
}

static void stub_Cvar_SetValue( char *szName, float value )
{
	cvar_t *c = stub_GetCvarPointer( szName );
	if ( c )
	{
		c->value = value;
		char buf[64];
		snprintf( buf, sizeof( buf ), "%f", value );
		free( c->string );
		c->string = (char *)malloc( strlen( buf ) + 1 );
		strcpy( c->string, buf );
	}
}

static int stub_AddCommand( char *cmd_name, void ( *function )( void ) )
{
	(void)cmd_name;
	(void)function;
	return 1;
}

static int stub_HookUserMsg( char *szMsgName, pfnUserMsgHook pfn )
{
	(void)szMsgName;
	(void)pfn;
	return 1;
}

static void stub_HookEvent( char *name, void ( *pfnEvent )( struct event_args_s *args ) )
{
	(void)pfnEvent;
	if ( name )
		g_mockHookedEvents.push_back( name );
}

static int stub_ServerCmd( char *szCmdString )
{
	if ( szCmdString )
		g_mockServerCmds.push_back( szCmdString );
	return 1;
}

static int stub_ClientCmd( char *szCmdString )
{
	if ( szCmdString )
		g_mockClientCmds.push_back( szCmdString );
	return 1;
}

static const char *stub_GetLevelName( void )
{
	return g_mockMapName.c_str();
}

static const char *stub_GetGameDirectory( void )
{
	return "valve";
}

static int stub_GetScreenInfo( SCREENINFO *pscrinfo )
{
	if ( pscrinfo )
	{
		pscrinfo->iSize   = sizeof( SCREENINFO );
		pscrinfo->iWidth  = g_mockScreenInfo.iWidth;
		pscrinfo->iHeight = g_mockScreenInfo.iHeight;
		pscrinfo->iFlags  = g_mockScreenInfo.iFlags;
		pscrinfo->iCharHeight = 13;
		return 1;
	}
	return 0;
}

static struct cl_entity_s *stub_GetEntityByIndex( int index )
{
	if ( index >= 0 && index <= MAX_CLIENTS )
		return &s_mockEntities[index];
	return nullptr;
}

static struct cl_entity_s *stub_GetLocalPlayer( void )
{
	return &s_mockEntities[1];
}

static float stub_GetClientTime( void )
{
	return g_mockClientTime;
}

static void stub_Con_Printf( char *fmt, ... )
{
	(void)fmt;
}

static void stub_Con_DPrintf( char *fmt, ... )
{
	(void)fmt;
}

static void stub_Con_NPrintf( int pos, char *fmt, ... )
{
	(void)pos;
	(void)fmt;
}

static void stub_Con_NXPrintf( struct con_nprint_s *info, char *fmt, ... )
{
	(void)info;
	(void)fmt;
}

static int stub_COM_ExpandFilename( const char *fileName, char *nameOutBuffer, int nameOutBufferSize )
{
	(void)fileName;
	(void)nameOutBuffer;
	(void)nameOutBufferSize;
	// Return 0 to prevent loading particleman dynamically in unit tests
	return 0;
}

static char *stub_COM_ParseFile( char *data, char *token )
{
	if ( !data || !token )
		return nullptr;

	token[0] = 0;
	while ( *data && ( *data <= ' ' || *data == '\t' || *data == '\r' || *data == '\n' ) )
		data++;

	if ( !*data )
		return nullptr;

	char *t = token;
	while ( *data && *data > ' ' && *data != '\t' && *data != '\r' && *data != '\n' )
	{
		*t++ = *data++;
	}
	*t = 0;
	return data;
}

static unsigned char *stub_COM_LoadFile( char *path, int usehunk, int *pLength )
{
	(void)path;
	(void)usehunk;
	if ( pLength )
		*pLength = 0;
	return nullptr;
}

static void stub_COM_FreeFile( void *buffer )
{
	free( buffer );
}

static int stub_IsSpectateOnly( void )
{
	return 0;
}

static void *stub_VGui_GetPanel( void )
{
	return nullptr;
}

static client_sprite_t s_mockSprites[] = {
	{ "number_0", "640hud7", 1, 640, { 0, 0, 16, 16 } },
	{ "number_1", "640hud7", 2, 640, { 0, 0, 16, 16 } },
	{ "number_2", "640hud7", 3, 640, { 0, 0, 16, 16 } },
	{ "number_3", "640hud7", 4, 640, { 0, 0, 16, 16 } },
	{ "number_4", "640hud7", 5, 640, { 0, 0, 16, 16 } },
	{ "number_5", "640hud7", 6, 640, { 0, 0, 16, 16 } },
	{ "number_6", "640hud7", 7, 640, { 0, 0, 16, 16 } },
	{ "number_7", "640hud7", 8, 640, { 0, 0, 16, 16 } },
	{ "number_8", "640hud7", 9, 640, { 0, 0, 16, 16 } },
	{ "number_9", "640hud7", 10, 640, { 0, 0, 16, 16 } },
	{ "bucket1", "640hud1", 11, 640, { 0, 0, 16, 16 } },
	{ "bucket2", "640hud1", 12, 640, { 0, 0, 16, 16 } },
	{ "bucket3", "640hud1", 13, 640, { 0, 0, 16, 16 } },
	{ "bucket4", "640hud1", 14, 640, { 0, 0, 16, 16 } },
	{ "bucket5", "640hud1", 15, 640, { 0, 0, 16, 16 } },
	{ "selection", "640hud1", 16, 640, { 0, 0, 16, 16 } },
	{ "spec_player_blue", "640hud1", 17, 640, { 0, 0, 16, 16 } },
	{ "spec_player_red", "640hud1", 18, 640, { 0, 0, 16, 16 } },
	{ "spec_player", "640hud1", 19, 640, { 0, 0, 16, 16 } },
	{ "spec_camera", "640hud1", 20, 640, { 0, 0, 16, 16 } },
	{ "spec_player_dead", "640hud1", 21, 640, { 0, 0, 16, 16 } },
	{ "spec_viewcone", "640hud1", 22, 640, { 0, 0, 16, 16 } },
	{ "spec_unknown_map", "640hud1", 23, 640, { 0, 0, 16, 16 } },
	{ "spec_beam", "640hud1", 24, 640, { 0, 0, 16, 16 } },
	{ "spec_crosshair", "640hud1", 25, 640, { 0, 0, 16, 16 } },
};

static HSPRITE stub_SPR_Load( const char *szPicName )
{
	(void)szPicName;
	return (HSPRITE)1;
}

static int stub_SPR_Frames( HSPRITE hPic )
{
	(void)hPic;
	return 1;
}

static int stub_SPR_Height( HSPRITE hPic, int frame )
{
	(void)hPic;
	(void)frame;
	return 16;
}

static int stub_SPR_Width( HSPRITE hPic, int frame )
{
	(void)hPic;
	(void)frame;
	return 16;
}

static void stub_SPR_Set( HSPRITE hPic, int r, int g, int b )
{
	(void)hPic;
	(void)r;
	(void)g;
	(void)b;
}

static void stub_SPR_Draw( int frame, int x, int y, const struct rect_s *prc )
{
	(void)frame;
	(void)x;
	(void)y;
	(void)prc;
}

static void stub_SPR_DrawHoles( int frame, int x, int y, const struct rect_s *prc )
{
	(void)frame;
	(void)x;
	(void)y;
	(void)prc;
}

static void stub_SPR_DrawAdditive( int frame, int x, int y, const struct rect_s *prc )
{
	(void)frame;
	(void)x;
	(void)y;
	(void)prc;
}

static void stub_SPR_EnableScissor( int x, int y, int width, int height )
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
}

static void stub_SPR_DisableScissor( void )
{
}

static struct client_sprite_s *stub_SPR_GetList( char *psz, int *piCount )
{
	(void)psz;
	if ( piCount )
		*piCount = sizeof( s_mockSprites ) / sizeof( s_mockSprites[0] );
	return s_mockSprites;
}

static void stub_SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b )
{
	(void)hspr;
	(void)rc;
	(void)r;
	(void)g;
	(void)b;
}

static void stub_FillRGBA( int x, int y, int width, int height, int r, int g, int b, int a )
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	(void)r;
	(void)g;
	(void)b;
	(void)a;
}

static void stub_DrawSetTextColor( float r, float g, float b )
{
	(void)r;
	(void)g;
	(void)b;
}

static int stub_DrawConsoleString( int x, int y, char *string )
{
	(void)x;
	(void)y;
	return string ? (int)strlen( string ) * 8 : 0;
}

static void stub_DrawConsoleStringLen( const char *string, int *length, int *height )
{
	if ( length )
		*length = string ? (int)strlen( string ) * 8 : 0;
	if ( height )
		*height = 13;
}

static int stub_IsDemoPlaying( void )
{
	return 0;
}

static int stub_IsDemoRecording( void )
{
	return 0;
}

static int stub_CheckParm( char *parm, char **ppnext )
{
	(void)parm;
	if ( ppnext )
		*ppnext = nullptr;
	return 0;
}

static double stub_GetAbsoluteTime( void )
{
	return (double)g_mockClientTime;
}

static int stub_GetMaxClients( void )
{
	return MAX_CLIENTS;
}

static int stub_GetWindowCenterX( void )
{
	return 320;
}

static int stub_GetWindowCenterY( void )
{
	return 240;
}

static void stub_GetViewAngles( float *va )
{
	if ( va )
		va[0] = va[1] = va[2] = 0.0f;
}

static void stub_SetViewAngles( float *va )
{
	(void)va;
}

static void stub_GetMousePosition( int *mx, int *my )
{
	if ( mx )
		*mx = 320;
	if ( my )
		*my = 240;
}

// -----------------------------------------------------------------------------
// engine_studio_api_t Stubs
// -----------------------------------------------------------------------------
static void *stub_Mem_Calloc( int number, size_t size )
{
	return calloc( number, size );
}

static void *stub_Cache_Check( struct cache_user_s *c )
{
	(void)c;
	return nullptr;
}

static void stub_LoadCacheFile( char *path, struct cache_user_s *cu )
{
	(void)path;
	(void)cu;
}

static struct model_s *stub_Mod_ForName( const char *name, int crash_if_missing )
{
	(void)name;
	(void)crash_if_missing;
	return nullptr;
}

static struct cl_entity_s *stub_StudioGetCurrentEntity( void )
{
	return &s_mockEntities[1];
}

static struct player_info_s *stub_PlayerInfo( int index )
{
	if ( index >= 0 && index <= MAX_CLIENTS )
		return &s_mockPlayerInfo[index];
	return nullptr;
}

static struct cl_entity_s *stub_GetViewEntity( void )
{
	return &s_mockEntities[1];
}

static void stub_GetTimes( int *framecount, double *current, double *old )
{
	if ( framecount )
		*framecount = 1;
	if ( current )
		*current = (double)g_mockClientTime;
	if ( old )
		*old = 0.0;
}

static struct cvar_s *stub_StudioGetCvar( const char *name )
{
	return stub_GetCvarPointer( name );
}

#ifndef MAXSTUDIOBONES
#define MAXSTUDIOBONES 128
#endif

static int s_mockStudioModelCount = 0;
static int s_mockModelsDrawn      = 0;
static float s_mockBoneTransform[MAXSTUDIOBONES][3][4];
static float s_mockLightTransform[MAXSTUDIOBONES][3][4];
static float s_mockAliasTransform[3][4];
static float s_mockRotationMatrix[3][4];

static struct model_s *stub_GetChromeSprite( void )
{
	return nullptr;
}

static void stub_GetModelCounters( int **s, int **a )
{
	if ( s )
		*s = &s_mockStudioModelCount;
	if ( a )
		*a = &s_mockModelsDrawn;
}

static float ****stub_StudioGetBoneTransform( void )
{
	return (float ****)&s_mockBoneTransform;
}

static float ****stub_StudioGetLightTransform( void )
{
	return (float ****)&s_mockLightTransform;
}

static float ***stub_StudioGetAliasTransform( void )
{
	return (float ***)&s_mockAliasTransform;
}

static float ***stub_StudioGetRotationMatrix( void )
{
	return (float ***)&s_mockRotationMatrix;
}

// -----------------------------------------------------------------------------
// Init and Reset
// -----------------------------------------------------------------------------
void InitMockClientEngine( void )
{
	ResetMockClientEngine();

	memset( &g_mockClientEngineFuncs, 0, sizeof( g_mockClientEngineFuncs ) );
	memset( &g_mockStudioApi, 0, sizeof( g_mockStudioApi ) );
	memset( s_mockEntities, 0, sizeof( s_mockEntities ) );
	memset( s_mockPlayerInfo, 0, sizeof( s_mockPlayerInfo ) );
	memset( &s_mockDemoApi, 0, sizeof( s_mockDemoApi ) );

	// Setup Demo API
	s_mockDemoApi.IsPlayingback = stub_IsDemoPlaying;
	s_mockDemoApi.IsRecording  = stub_IsDemoRecording;

	// Setup mock entities and player info
	for ( int i = 1; i <= MAX_CLIENTS; i++ )
	{
		s_mockEntities[i].index              = i;
		s_mockEntities[i].player             = ( i <= g_mockPlayerCount ) ? 1 : 0;
		s_mockEntities[i].curstate.solid     = SOLID_SLIDEBOX;
		snprintf( s_mockPlayerInfo[i].name, sizeof( s_mockPlayerInfo[i].name ), "Player_%d", i );
	}

	// Screen info defaults
	g_mockScreenInfo.iSize   = sizeof( SCREENINFO );
	g_mockScreenInfo.iWidth  = 640;
	g_mockScreenInfo.iHeight = 480;
	g_mockScreenInfo.iFlags  = 0;

	// Populate cl_enginefunc_t table
	g_mockClientEngineFuncs.pfnSPR_Load             = stub_SPR_Load;
	g_mockClientEngineFuncs.pfnSPR_Frames           = stub_SPR_Frames;
	g_mockClientEngineFuncs.pfnSPR_Height           = stub_SPR_Height;
	g_mockClientEngineFuncs.pfnSPR_Width            = stub_SPR_Width;
	g_mockClientEngineFuncs.pfnSPR_Set              = stub_SPR_Set;
	g_mockClientEngineFuncs.pfnSPR_Draw             = stub_SPR_Draw;
	g_mockClientEngineFuncs.pfnSPR_DrawHoles        = stub_SPR_DrawHoles;
	g_mockClientEngineFuncs.pfnSPR_DrawAdditive     = stub_SPR_DrawAdditive;
	g_mockClientEngineFuncs.pfnSPR_EnableScissor    = stub_SPR_EnableScissor;
	g_mockClientEngineFuncs.pfnSPR_DisableScissor   = stub_SPR_DisableScissor;
	g_mockClientEngineFuncs.pfnSPR_GetList          = stub_SPR_GetList;
	g_mockClientEngineFuncs.pfnRegisterVariable     = stub_RegisterVariable;
	g_mockClientEngineFuncs.pfnGetCvarPointer       = stub_GetCvarPointer;
	g_mockClientEngineFuncs.pfnGetCvarFloat         = stub_GetCvarFloat;
	g_mockClientEngineFuncs.pfnGetCvarString        = stub_GetCvarString;
	g_mockClientEngineFuncs.Cvar_SetValue           = stub_Cvar_SetValue;
	g_mockClientEngineFuncs.pfnAddCommand           = stub_AddCommand;
	g_mockClientEngineFuncs.pfnHookUserMsg          = stub_HookUserMsg;
	g_mockClientEngineFuncs.pfnHookEvent            = stub_HookEvent;
	g_mockClientEngineFuncs.pfnServerCmd            = stub_ServerCmd;
	g_mockClientEngineFuncs.pfnClientCmd            = stub_ClientCmd;
	g_mockClientEngineFuncs.pfnGetLevelName         = stub_GetLevelName;
	g_mockClientEngineFuncs.pfnGetGameDirectory     = stub_GetGameDirectory;
	g_mockClientEngineFuncs.pfnGetScreenInfo        = stub_GetScreenInfo;
	g_mockClientEngineFuncs.GetEntityByIndex        = stub_GetEntityByIndex;
	g_mockClientEngineFuncs.GetLocalPlayer          = stub_GetLocalPlayer;
	g_mockClientEngineFuncs.GetClientTime           = stub_GetClientTime;
	g_mockClientEngineFuncs.CheckParm               = stub_CheckParm;
	g_mockClientEngineFuncs.GetAbsoluteTime         = stub_GetAbsoluteTime;
	g_mockClientEngineFuncs.GetMaxClients           = stub_GetMaxClients;
	g_mockClientEngineFuncs.GetWindowCenterX        = stub_GetWindowCenterX;
	g_mockClientEngineFuncs.GetWindowCenterY        = stub_GetWindowCenterY;
	g_mockClientEngineFuncs.GetViewAngles           = stub_GetViewAngles;
	g_mockClientEngineFuncs.SetViewAngles           = stub_SetViewAngles;
	g_mockClientEngineFuncs.GetMousePosition        = stub_GetMousePosition;
	g_mockClientEngineFuncs.Con_Printf              = stub_Con_Printf;
	g_mockClientEngineFuncs.Con_DPrintf             = stub_Con_DPrintf;
	g_mockClientEngineFuncs.Con_NPrintf             = stub_Con_NPrintf;
	g_mockClientEngineFuncs.Con_NXPrintf            = stub_Con_NXPrintf;
	g_mockClientEngineFuncs.COM_ExpandFilename      = stub_COM_ExpandFilename;
	g_mockClientEngineFuncs.COM_ParseFile           = stub_COM_ParseFile;
	g_mockClientEngineFuncs.COM_LoadFile            = stub_COM_LoadFile;
	g_mockClientEngineFuncs.COM_FreeFile            = stub_COM_FreeFile;
	g_mockClientEngineFuncs.IsSpectateOnly          = stub_IsSpectateOnly;
	g_mockClientEngineFuncs.VGui_GetPanel           = stub_VGui_GetPanel;
	g_mockClientEngineFuncs.pfnSetCrosshair         = stub_SetCrosshair;
	g_mockClientEngineFuncs.pfnFillRGBA             = stub_FillRGBA;
	g_mockClientEngineFuncs.pfnDrawSetTextColor     = stub_DrawSetTextColor;
	g_mockClientEngineFuncs.pfnDrawConsoleString    = stub_DrawConsoleString;
	g_mockClientEngineFuncs.pfnDrawConsoleStringLen = stub_DrawConsoleStringLen;
	g_mockClientEngineFuncs.pDemoAPI                = &s_mockDemoApi;

	// Populate engine_studio_api_t table
	g_mockStudioApi.Mem_Calloc              = stub_Mem_Calloc;
	g_mockStudioApi.Cache_Check             = stub_Cache_Check;
	g_mockStudioApi.LoadCacheFile           = stub_LoadCacheFile;
	g_mockStudioApi.Mod_ForName             = stub_Mod_ForName;
	g_mockStudioApi.GetCurrentEntity        = stub_StudioGetCurrentEntity;
	g_mockStudioApi.PlayerInfo              = stub_PlayerInfo;
	g_mockStudioApi.GetViewEntity           = stub_GetViewEntity;
	g_mockStudioApi.GetTimes                = stub_GetTimes;
	g_mockStudioApi.GetCvar                 = stub_StudioGetCvar;
	g_mockStudioApi.GetChromeSprite         = stub_GetChromeSprite;
	g_mockStudioApi.GetModelCounters        = stub_GetModelCounters;
	g_mockStudioApi.StudioGetBoneTransform  = stub_StudioGetBoneTransform;
	g_mockStudioApi.StudioGetLightTransform = stub_StudioGetLightTransform;
	g_mockStudioApi.StudioGetAliasTransform = stub_StudioGetAliasTransform;
	g_mockStudioApi.StudioGetRotationMatrix = stub_StudioGetRotationMatrix;

	// Always ensure gViewPort is explicitly NULL at engine startup (pre-video init)
	gViewPort = nullptr;
}

void ResetMockClientEngine( void )
{
	g_mockServerCmds.clear();
	g_mockClientCmds.clear();
	g_mockHookedEvents.clear();

	for ( auto &pair : g_mockCvars )
	{
		free( pair.second->name );
		free( pair.second->string );
		free( pair.second );
	}
	g_mockCvars.clear();

	for ( int i = 1; i <= MAX_CLIENTS; i++ )
	{
		memset( s_mockPlayerInfo[i].name, 0, sizeof( s_mockPlayerInfo[i].name ) );
	}

	g_mockMapName     = "mock_test_map";
	g_mockPlayerCount = 1;
	g_mockClientTime  = 1.0f;
	gViewPort         = nullptr;
}

void SetMockMapName( const char *pszMapName )
{
	if ( pszMapName )
		g_mockMapName = pszMapName;
	else
		g_mockMapName = "";
}

void SetMockPlayerCount( int count )
{
	g_mockPlayerCount = ( count > MAX_CLIENTS ) ? MAX_CLIENTS : ( count < 0 ? 0 : count );
	for ( int i = 1; i <= MAX_CLIENTS; i++ )
	{
		s_mockEntities[i].player = ( i <= g_mockPlayerCount ) ? 1 : 0;
	}
}

void SetMockClientTime( float flTime )
{
	g_mockClientTime = flTime;
}

void SetMockScreenInfo( int width, int height )
{
	g_mockScreenInfo.iWidth  = width;
	g_mockScreenInfo.iHeight = height;
}

// -----------------------------------------------------------------------------
// Dynamic Library Loading & Resolution
// -----------------------------------------------------------------------------
bool LoadClientLibrary( const char *customPath )
{
	if ( s_hClientLib != NULL )
		return true;

	std::vector<std::string> candidatePaths;

	if ( customPath && customPath[0] )
		candidatePaths.push_back( customPath );

	const char *envLib = getenv( "HL_CLIENT_LIB" );
	if ( envLib && envLib[0] )
		candidatePaths.push_back( envLib );

#if defined( _WIN32 )
	candidatePaths.push_back( "build-cmake/bin/Release/client.dll" );
	candidatePaths.push_back( "build-cmake/bin/client.dll" );
	candidatePaths.push_back( "build/bin/Release/client.dll" );
	candidatePaths.push_back( "build/bin/client.dll" );
	candidatePaths.push_back( "bin/Release/client.dll" );
	candidatePaths.push_back( "bin/client.dll" );
	candidatePaths.push_back( "projects/vs2019/Release/hl_cdll/client.dll" );
	candidatePaths.push_back( "Release/hl_cdll/client.dll" );
	candidatePaths.push_back( "../hl_cdll/client.dll" );
	candidatePaths.push_back( "client.dll" );
#else
	candidatePaths.push_back( "build-cmake/bin/client.so" );
	candidatePaths.push_back( "build/bin/client.so" );
	candidatePaths.push_back( "bin/client.so" );
	candidatePaths.push_back( "linux/release/cl_dlls/client.so" );
	candidatePaths.push_back( "release/cl_dlls/client.so" );
	candidatePaths.push_back( "cl_dlls/client.so" );
	candidatePaths.push_back( "../cl_dlls/client.so" );
	candidatePaths.push_back( "./client.so" );
#endif

#if defined( _WIN32 )
	const char *vguiPaths[] = { "lib/public/vgui.dll", "../lib/public/vgui.dll", "../../lib/public/vgui.dll", "vgui.dll" };
	for ( const char *vp : vguiPaths )
	{
		char fullVgui[MAX_PATH];
		if ( GetFullPathNameA( vp, MAX_PATH, fullVgui, NULL ) > 0 )
		{
			HMODULE hVgui = LoadLibraryA( fullVgui );
			if ( hVgui )
				break;
		}
	}
	const char *sdlPaths[] = { "lib/public/SDL2.dll", "../lib/public/SDL2.dll", "../../lib/public/SDL2.dll", "SDL2.dll" };
	for ( const char *sp : sdlPaths )
	{
		char fullSdl[MAX_PATH];
		if ( GetFullPathNameA( sp, MAX_PATH, fullSdl, NULL ) > 0 )
		{
			HMODULE hSdl = LoadLibraryA( fullSdl );
			if ( hSdl )
				break;
		}
	}
#else
	const char *vguiLinuxPaths[] = { "linux/vgui.so", "../linux/vgui.so", "../../linux/vgui.so", "vgui.so" };
	for ( const char *vp : vguiLinuxPaths )
	{
		void *hVgui = dlopen( vp, RTLD_NOW | RTLD_GLOBAL );
		if ( hVgui )
			break;
	}
	const char *sdlLinuxPaths[] = { "linux/libSDL2.so", "../linux/libSDL2.so", "../../linux/libSDL2.so", "libSDL2.so", "libSDL2-2.0.so.0" };
	for ( const char *sp : sdlLinuxPaths )
	{
		void *hSdl = dlopen( sp, RTLD_NOW | RTLD_GLOBAL );
		if ( hSdl )
			break;
	}
#endif

	for ( const auto &path : candidatePaths )
	{
#if defined( _WIN32 )
		char fullPath[MAX_PATH];
		const char *loadPath = path.c_str();
		if ( GetFullPathNameA( path.c_str(), MAX_PATH, fullPath, NULL ) > 0 )
		{
			loadPath = fullPath;
			char fullDir[MAX_PATH];
			char drive[_MAX_DRIVE], dir[_MAX_DIR];
			_splitpath( fullPath, drive, dir, NULL, NULL );
			snprintf( fullDir, sizeof( fullDir ), "%s%s", drive, dir );
			if ( fullDir[0] )
			{
				SetDllDirectoryA( fullDir );
			}
		}
		s_hClientLib = LoadLibraryExA( loadPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
#else
		s_hClientLib = dlopen( path.c_str(), RTLD_NOW | RTLD_GLOBAL );
#endif
		if ( s_hClientLib != NULL )
			break;
	}

	if ( !s_hClientLib )
		return false;

#if defined( _WIN32 )
	g_pfnInitialize                  = (pfnInitialize_t)GetProcAddress( s_hClientLib, "Initialize" );
	g_pfnHUD_Init                    = (pfnHUD_Init_t)GetProcAddress( s_hClientLib, "HUD_Init" );
	g_pfnHUD_VidInit                 = (pfnHUD_VidInit_t)GetProcAddress( s_hClientLib, "HUD_VidInit" );
	g_pfnHUD_Reset                   = (pfnHUD_Reset_t)GetProcAddress( s_hClientLib, "HUD_Reset" );
	g_pfnHUD_GetStudioModelInterface = (pfnHUD_GetStudioModelInterface_t)GetProcAddress( s_hClientLib, "HUD_GetStudioModelInterface" );
#else
	g_pfnInitialize                  = (pfnInitialize_t)dlsym( s_hClientLib, "Initialize" );
	g_pfnHUD_Init                    = (pfnHUD_Init_t)dlsym( s_hClientLib, "HUD_Init" );
	g_pfnHUD_VidInit                 = (pfnHUD_VidInit_t)dlsym( s_hClientLib, "HUD_VidInit" );
	g_pfnHUD_Reset                   = (pfnHUD_Reset_t)dlsym( s_hClientLib, "HUD_Reset" );
	g_pfnHUD_GetStudioModelInterface = (pfnHUD_GetStudioModelInterface_t)dlsym( s_hClientLib, "HUD_GetStudioModelInterface" );
#endif

	// Fallback to F() export table if direct exports are absent
	if ( !g_pfnInitialize || !g_pfnHUD_Init )
	{
#if defined( _WIN32 )
		typedef void ( *pfnF_t )( void *pv );
		pfnF_t pfnF = (pfnF_t)GetProcAddress( s_hClientLib, "F" );
#else
		typedef void ( *pfnF_t )( void *pv );
		pfnF_t pfnF = (pfnF_t)dlsym( s_hClientLib, "F" );
#endif
		if ( pfnF )
		{
			cldll_func_t funcs;
			memset( &funcs, 0, sizeof( funcs ) );
			pfnF( &funcs );

			if ( !g_pfnInitialize )
				g_pfnInitialize = funcs.pInitFunc;
			if ( !g_pfnHUD_Init )
				g_pfnHUD_Init = funcs.pHudInitFunc;
			if ( !g_pfnHUD_VidInit )
				g_pfnHUD_VidInit = funcs.pHudVidInitFunc;
			if ( !g_pfnHUD_Reset )
				g_pfnHUD_Reset = funcs.pHudResetFunc;
			if ( !g_pfnHUD_GetStudioModelInterface )
				g_pfnHUD_GetStudioModelInterface = funcs.pStudioInterface;
		}
	}

	return ( g_pfnInitialize != nullptr && g_pfnHUD_Init != nullptr );
}

void UnloadClientLibrary( void )
{
	if ( s_hClientLib != NULL )
	{
#if defined( _WIN32 )
		FreeLibrary( s_hClientLib );
#else
		dlclose( s_hClientLib );
#endif
		s_hClientLib = NULL;
	}

	g_pfnInitialize                  = nullptr;
	g_pfnHUD_Init                    = nullptr;
	g_pfnHUD_VidInit                 = nullptr;
	g_pfnHUD_Reset                   = nullptr;
	g_pfnHUD_GetStudioModelInterface = nullptr;
}

int ClientInitialize( cl_enginefunc_t *pEnginefuncs, int iVersion )
{
	if ( !LoadClientLibrary() || !g_pfnInitialize )
		return 0;
	return g_pfnInitialize( pEnginefuncs, iVersion );
}

void ClientHUD_Init( void )
{
	if ( LoadClientLibrary() && g_pfnHUD_Init )
		g_pfnHUD_Init();
}

int ClientHUD_VidInit( void )
{
	if ( LoadClientLibrary() && g_pfnHUD_VidInit )
		return g_pfnHUD_VidInit();
	return 0;
}

void ClientHUD_Reset( void )
{
	if ( LoadClientLibrary() && g_pfnHUD_Reset )
		g_pfnHUD_Reset();
}

int ClientHUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio )
{
	if ( !LoadClientLibrary() || !g_pfnHUD_GetStudioModelInterface )
		return 0;
	return g_pfnHUD_GetStudioModelInterface( version, ppinterface, pstudio );
}
