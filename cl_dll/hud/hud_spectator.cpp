//========= Copyright ? 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Spectator HUD subsystem (lifecycle, director, overview, and UI controls).
//
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "vgui/vgui_TeamFortressViewport.h"
#include "vgui/vgui_SpectatorPanel.h"
#include "hltv.h"

#include "pm_shared.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "parsemsg.h"
#include "entity_types.h"

#include "com_model.h"
#include "demo_api.h"
#include "event_api.h"
#include "studio/studio_util.h"
#include "screenfade.h"
#include <math.h>

#pragma warning( disable : 4244 )

extern "C" int iJumpSpectator;
extern "C" float vJumpOrigin[3];
extern "C" float vJumpAngles[3];

extern extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1];

extern void V_GetInEyePos( int entity, float *origin, float *angles );
extern void V_ResetChaseCam();
extern void V_GetChasePos( int target, float *cl_angles, float *origin, float *angles );
extern float *GetClientColor( int clientIndex );

extern vec3_t v_origin;
extern vec3_t v_angles;
extern vec3_t v_cl_angles;
extern vec3_t v_sim_org;

void SpectatorMode( void )
{
	if ( gEngfuncs.Cmd_Argc() <= 1 )
	{
		gEngfuncs.Con_Printf( "usage:  spec_mode <Main Mode> [<Inset Mode>]\n" );
		return;
	}

	if ( gEngfuncs.Cmd_Argc() == 2 )
		gHUD.m_Spectator.SetModes( atoi( gEngfuncs.Cmd_Argv( 1 ) ), -1 );
	else if ( gEngfuncs.Cmd_Argc() == 3 )
		gHUD.m_Spectator.SetModes( atoi( gEngfuncs.Cmd_Argv( 1 ) ), atoi( gEngfuncs.Cmd_Argv( 2 ) ) );
}

void SpectatorSpray( void )
{
	vec3_t forward;
	char string[128];

	if ( !gEngfuncs.IsSpectateOnly() )
		return;

	AngleVectors( v_angles, forward, NULL, NULL );
	VectorScale( forward, 128, forward );
	VectorAdd( forward, v_origin, forward );
	pmtrace_t *trace = gEngfuncs.PM_TraceLine( v_origin, forward, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
	if ( trace->fraction != 1.0 )
	{
		snprintf( string, sizeof( string ), "drc_spray %.2f %.2f %.2f %i", trace->endpos[0], trace->endpos[1], trace->endpos[2], trace->ent );
		gEngfuncs.pfnServerCmd( string );
	}
}

void SpectatorHelp( void )
{
	if ( gViewPort )
	{
		gViewPort->ShowVGUIMenu( MENU_SPECHELP );
	}
	else
	{
		char *text = CHudTextMessage::BufferedLocaliseTextString( "#Spec_Help_Text" );

		if ( text )
		{
			while ( *text )
			{
				if ( *text != 13 )
					gEngfuncs.Con_Printf( "%c", *text );
				text++;
			}
		}
	}
}

void SpectatorMenu( void )
{
	if ( gEngfuncs.Cmd_Argc() <= 1 )
	{
		gEngfuncs.Con_Printf( "usage:  spec_menu <0|1>\n" );
		return;
	}

	if ( gViewPort && gViewPort->m_pSpectatorPanel )
		gViewPort->m_pSpectatorPanel->ShowMenu( atoi( gEngfuncs.Cmd_Argv( 1 ) ) != 0 );
}

void ToggleScores( void )
{
	if ( gViewPort )
	{
		if ( gViewPort->IsScoreBoardVisible() )
		{
			gViewPort->HideScoreBoard();
		}
		else
		{
			gViewPort->ShowScoreBoard();
		}
	}
}

int CHudSpectator::Init()
{
	gHUD.AddHudElem( this );

	m_iFlags |= HUD_ACTIVE;
	m_flNextObserverInput = 0.0f;
	m_zoomDelta           = 0.0f;
	m_moveDelta           = 0.0f;
	m_chatEnabled         = ( gHUD.m_SayText.m_HUD_saytext->value != 0 );
	m_iObserverFlags      = 0;
	m_iSpectatorNumber    = 0;
	m_FOV                 = 90.0f;
	m_IsInterpolating     = false;
	m_ChaseEntity         = 0;

	memset( &m_OverviewData, 0, sizeof( m_OverviewData ) );
	memset( &m_OverviewEntities, 0, sizeof( m_OverviewEntities ) );
	memset( &m_HUDMessages, 0, sizeof( m_HUDMessages ) );
	m_lastHudMessage = 0;

	m_autoDirector = gEngfuncs.pfnRegisterVariable( "spec_autodirector", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	m_pip          = gEngfuncs.pfnRegisterVariable( "spec_pip", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	m_drawnames    = gEngfuncs.pfnRegisterVariable( "spec_drawnames", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	m_drawcone     = gEngfuncs.pfnRegisterVariable( "spec_drawcone", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	m_drawstatus   = gEngfuncs.pfnRegisterVariable( "spec_drawstatus", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

	if ( !m_autoDirector || !m_pip || !m_drawnames || !m_drawcone || !m_drawstatus )
	{
		gEngfuncs.Con_Printf( "ERROR! Couldn't register spectator cvars!\n" );
		return 0;
	}

	gEngfuncs.pfnAddCommand( "spec_mode", SpectatorMode );
	gEngfuncs.pfnAddCommand( "spec_decal", SpectatorSpray );
	gEngfuncs.pfnAddCommand( "spec_help", SpectatorHelp );
	gEngfuncs.pfnAddCommand( "spec_menu", SpectatorMenu );
	gEngfuncs.pfnAddCommand( "togglescores", ToggleScores );

	m_hsprPlayerBlue = 0;
	m_hsprPlayerRed  = 0;
	m_hsprPlayer     = 0;
	m_hsprCamera     = 0;
	m_hsprPlayerDead = 0;
	m_hsprViewcone   = 0;
	m_hsprUnkownMap  = 0;
	m_hsprBeam       = 0;
	m_hCrosshair     = 0;

	return 1;
}

int CHudSpectator::VidInit()
{
	m_hsprPlayerBlue = gHUD.GetSpriteIndex( "spec_player_blue" );
	m_hsprPlayerRed  = gHUD.GetSpriteIndex( "spec_player_red" );
	m_hsprPlayer     = gHUD.GetSpriteIndex( "spec_player" );
	m_hsprCamera     = gHUD.GetSpriteIndex( "spec_camera" );
	m_hsprPlayerDead = gHUD.GetSpriteIndex( "spec_player_dead" );
	m_hsprViewcone   = gHUD.GetSpriteIndex( "spec_viewcone" );
	m_hsprUnkownMap  = gHUD.GetSpriteIndex( "spec_unknown_map" );
	m_hsprBeam       = gHUD.GetSpriteIndex( "spec_beam" );
	m_hCrosshair     = gHUD.GetSpriteIndex( "spec_crosshair" );

	return 1;
}

int CHudSpectator::Draw( float flTime )
{
	int lx;
	char string[256];
	float *color;

	if ( !g_iUser1 )
		return 0;

	if ( ( m_zoomDelta != 0.0f ) && ( g_iUser1 == OBS_MAP_FREE ) )
	{
		m_mapZoom += m_zoomDelta;

		if ( m_mapZoom > 3.0f )
			m_mapZoom = 3.0f;

		if ( m_mapZoom < 0.5f )
			m_mapZoom = 0.5f;
	}

	if ( ( m_moveDelta != 0.0f ) && ( g_iUser1 != OBS_ROAMING ) )
	{
		vec3_t right;
		AngleVectors( v_angles, NULL, right, NULL );
		VectorNormalize( right );
		VectorScale( right, m_moveDelta, right );

		VectorAdd( m_mapOrigin, right, m_mapOrigin );
	}

	if ( g_iUser1 < OBS_MAP_FREE )
		return 1;

	if ( !m_drawnames->value )
		return 1;

	if ( gViewPort )
	{
		gViewPort->GetAllPlayersInfo();
	}

	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		if ( m_vPlayerPos[i][2] < 0 )
			continue;

		if ( m_pip->value != INSET_OFF )
		{
			if ( m_vPlayerPos[i][0] > XRES_HD( m_OverviewData.insetWindowX ) &&
			     m_vPlayerPos[i][1] > YRES_HD( m_OverviewData.insetWindowY ) &&
			     m_vPlayerPos[i][0] < XRES_HD( m_OverviewData.insetWindowX + m_OverviewData.insetWindowWidth ) &&
			     m_vPlayerPos[i][1] < YRES_HD( m_OverviewData.insetWindowY + m_OverviewData.insetWindowHeight ) )
				continue;
		}

		color = GetClientColor( i + 1 );

		snprintf( string, sizeof( string ), "%s", g_PlayerInfoList[i + 1].name );

		lx = strlen( string ) * 3;

		gEngfuncs.pfnDrawSetTextColor( color[0], color[1], color[2] );
		DrawConsoleString( m_vPlayerPos[i][0] - lx, m_vPlayerPos[i][1], string );
	}

	return 1;
}

void CHudSpectator::Reset()
{
	const char *levelName = gEngfuncs.pfnGetLevelName();
	if ( levelName && *levelName && strcmp( m_OverviewData.map, levelName ) )
	{
		ParseOverviewFile();
		LoadMapSprites();
	}

	memset( &m_OverviewEntities, 0, sizeof( m_OverviewEntities ) );
	memset( &m_HUDMessages, 0, sizeof( m_HUDMessages ) );
	m_lastHudMessage = 0;

	m_FOV             = 90.0f;
	m_IsInterpolating = false;
	m_ChaseEntity     = 0;
	m_WayPoint        = 0;
	m_NumWayPoints    = 0;

	SetSpectatorStartPosition();
}

void CHudSpectator::InitHUDData( void )
{
	m_lastPrimaryObject   = 0;
	m_lastSecondaryObject = 0;
	m_iObserverFlags      = 0;
	m_iSpectatorNumber    = 0;
	m_lastHudMessage      = 0;

	memset( &m_OverviewEntities, 0, sizeof( m_OverviewEntities ) );
	memset( &m_HUDMessages, 0, sizeof( m_HUDMessages ) );

	if ( ParseOverviewFile() )
	{
		LoadMapSprites();
	}

	CheckSettings();
}

static int UTIL_FindEntityInMap( const char *name, float *origin, float *angle )
{
	int n, found = 0;
	char keyname[256];
	char token[1024];

	cl_entity_t *pEnt = gEngfuncs.GetEntityByIndex( 0 );

	if ( !pEnt )
		return 0;

	if ( !pEnt->model )
		return 0;

	char *data = pEnt->model->entities;

	while ( data )
	{
		data = gEngfuncs.COM_ParseFile( data, token );

		if ( ( token[0] == '}' ) || ( token[0] == 0 ) )
			break;

		if ( !data )
		{
			gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: EOF without closing brace\n" );
			return 0;
		}

		if ( token[0] != '{' )
		{
			gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: expected {\n" );
			return 0;
		}

		while ( 1 )
		{
			data = gEngfuncs.COM_ParseFile( data, token );
			if ( token[0] == '}' )
				break;

			if ( !data )
			{
				gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: EOF without closing brace\n" );
				return 0;
			}

			strncpy( keyname, token, sizeof( keyname ) - 1 );
			keyname[sizeof( keyname ) - 1] = '\0';

			n = strlen( keyname );
			while ( n && keyname[n - 1] == ' ' )
			{
				keyname[n - 1] = 0;
				n--;
			}

			data = gEngfuncs.COM_ParseFile( data, token );
			if ( !data )
			{
				gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: EOF without closing brace\n" );
				return 0;
			}

			if ( token[0] == '}' )
			{
				gEngfuncs.Con_DPrintf( "UTIL_FindEntityInMap: closing brace without data" );
				return 0;
			}

			if ( !strcmp( keyname, "classname" ) )
			{
				if ( !strcmp( token, name ) )
				{
					found = 1;
				}
			}

			if ( !strcmp( keyname, "angle" ) )
			{
				float y = atof( token );

				if ( y >= 0 )
				{
					angle[0] = 0.0f;
					angle[1] = y;
					angle[2] = 0.0f;
				}
				else if ( (int)y == -1 )
				{
					angle[0] = -90.0f;
					angle[1] = 0.0f;
					angle[2] = 0.0f;
				}
				else
				{
					angle[0] = 90.0f;
					angle[1] = 0.0f;
					angle[2] = 0.0f;
				}
			}

			if ( !strcmp( keyname, "angles" ) )
			{
				sscanf( token, "%f %f %f", &angle[0], &angle[1], &angle[2] );
			}

			if ( !strcmp( keyname, "origin" ) )
			{
				sscanf( token, "%f %f %f", &origin[0], &origin[1], &origin[2] );
			}
		}

		if ( found )
			return 1;
	}

	return 0;
}

void CHudSpectator::SetSpectatorStartPosition()
{
	// search for info_player start
	if ( UTIL_FindEntityInMap( "trigger_camera", m_cameraOrigin, m_cameraAngles ) )
		iJumpSpectator = 1;

	else if ( UTIL_FindEntityInMap( "info_player_start", m_cameraOrigin, m_cameraAngles ) )
		iJumpSpectator = 1;

	else if ( UTIL_FindEntityInMap( "info_player_deathmatch", m_cameraOrigin, m_cameraAngles ) )
		iJumpSpectator = 1;

	else if ( UTIL_FindEntityInMap( "info_player_coop", m_cameraOrigin, m_cameraAngles ) )
		iJumpSpectator = 1;
	else
	{
		// jump to 0,0,0 if no better position was found
		VectorCopy( vec3_origin, m_cameraOrigin );
		VectorCopy( vec3_origin, m_cameraAngles );
	}

	VectorCopy( m_cameraOrigin, vJumpOrigin );
	VectorCopy( m_cameraAngles, vJumpAngles );

	iJumpSpectator = 1; // jump anyway
}

void CHudSpectator::SetCameraView( vec3_t pos, vec3_t angle, float fov )
{
	m_FOV = fov;
	VectorCopy( pos, vJumpOrigin );
	VectorCopy( angle, vJumpAngles );
	gEngfuncs.SetViewAngles( vJumpAngles );
	iJumpSpectator = 1; // jump anyway
}

void CHudSpectator::AddWaypoint( float time, vec3_t pos, vec3_t angle, float fov, int flags )
{
	if ( !flags == 0 && time == 0.0f )
	{
		// switch instantly to this camera view
		SetCameraView( pos, angle, fov );
		return;
	}

	if ( m_NumWayPoints >= MAX_CAM_WAYPOINTS )
	{
		gEngfuncs.Con_Printf( "Too many camera waypoints!\n" );
		return;
	}

	VectorCopy( angle, m_CamPath[m_NumWayPoints].angle );
	VectorCopy( pos, m_CamPath[m_NumWayPoints].position );
	m_CamPath[m_NumWayPoints].flags = flags;
	m_CamPath[m_NumWayPoints].fov   = fov;
	m_CamPath[m_NumWayPoints].time  = time;

	gEngfuncs.Con_DPrintf( "Added waypoint %i\n", m_NumWayPoints );

	m_NumWayPoints++;
}

void CHudSpectator::SetWayInterpolation( cameraWayPoint_t *prev, cameraWayPoint_t *start, cameraWayPoint_t *end, cameraWayPoint_t *next )
{
	m_WayInterpolation.SetViewAngles( start->angle, end->angle );

	m_WayInterpolation.SetFOVs( start->fov, end->fov );

	m_WayInterpolation.SetSmoothing( ( start->flags & DRC_FLAG_SLOWSTART ) != 0,
	                                 ( start->flags & DRC_FLAG_SLOWEND ) != 0 );

	if ( prev && next )
	{
		m_WayInterpolation.SetWaypoints( &prev->position, start->position, end->position, &next->position );
	}
	else if ( prev )
	{
		m_WayInterpolation.SetWaypoints( &prev->position, start->position, end->position, NULL );
	}
	else if ( next )
	{
		m_WayInterpolation.SetWaypoints( NULL, start->position, end->position, &next->position );
	}
	else
	{
		m_WayInterpolation.SetWaypoints( NULL, start->position, end->position, NULL );
	}
}

bool CHudSpectator::GetDirectorCamera( vec3_t &position, vec3_t &angle )
{
	float now = gHUD.m_flTime;
	float fov = 90.0f;

	if ( m_ChaseEntity )
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( m_ChaseEntity );

		if ( ent )
		{
			vec3_t vt = ent->curstate.origin;

			if ( m_ChaseEntity <= gEngfuncs.GetMaxClients() )
			{
				if ( ent->curstate.solid == SOLID_NOT )
				{
					vt[2] += -8; // PM_DEAD_VIEWHEIGHT
				}
				else if ( ent->curstate.usehull == 1 )
				{
					vt[2] += 12; // VEC_DUCK_VIEW;
				}
				else
				{
					vt[2] += 28; // DEFAULT_VIEWHEIGHT
				}
			}

			vt = vt - position;
			VectorAngles( vt, angle );
			angle[0] = -angle[0];
			return true;
		}
		else
		{
			return false;
		}
	}

	if ( !m_IsInterpolating )
		return false;

	if ( m_WayPoint < 0 || m_WayPoint >= ( m_NumWayPoints - 1 ) )
		return false;

	cameraWayPoint_t *wp1 = &m_CamPath[m_WayPoint];
	cameraWayPoint_t *wp2 = &m_CamPath[m_WayPoint + 1];

	if ( now < wp1->time )
		return false;

	while ( now > wp2->time )
	{
		// go to next waypoint, if possible
		m_WayPoint++;

		if ( m_WayPoint >= ( m_NumWayPoints - 1 ) )
		{
			m_IsInterpolating = false;
			return false; // there is no following waypoint
		}

		wp1 = wp2;
		wp2 = &m_CamPath[m_WayPoint + 1];

		if ( m_WayPoint > 0 )
		{
			// we have a predecessor

			if ( m_WayPoint < ( m_NumWayPoints - 1 ) )
			{
				// we have also a successor
				SetWayInterpolation( &m_CamPath[m_WayPoint - 1], wp1, wp2, &m_CamPath[m_WayPoint + 2] );
			}
			else
			{
				SetWayInterpolation( &m_CamPath[m_WayPoint - 1], wp1, wp2, NULL );
			}
		}
		else if ( m_WayPoint < ( m_NumWayPoints - 1 ) )
		{
			// we only have a successor
			SetWayInterpolation( NULL, wp1, wp2, &m_CamPath[m_WayPoint + 2] );
		}
		else
		{
			// we have only two waypoints
			SetWayInterpolation( NULL, wp1, wp2, NULL );
		}
	}

	if ( wp2->time <= wp1->time )
		return false;

	float fraction = ( now - wp1->time ) / ( wp2->time - wp1->time );

	if ( fraction < 0.0f )
		fraction = 0.0f;
	else if ( fraction > 1.0f )
		fraction = 1.0f;

	m_WayInterpolation.Interpolate( fraction, position, angle, &fov );

	SetCameraView( position, angle, fov );

	return true;
}

float CHudSpectator::GetFOV()
{
	return m_FOV;
}

void CHudSpectator::DirectorMessage( int iSize, void *pbuf )
{
	float f1, f2;
	char *string;
	vec3_t v1, v2;
	int i1, i2, i3;

	BEGIN_READ( pbuf, iSize );

	int cmd = READ_BYTE();

	switch ( cmd ) // director command byte
	{
	case DRC_CMD_START:
		// now we have to do some things clientside, since the proxy doesn't know our mod
		g_iPlayerClass = 0;
		g_iTeamNumber  = 0;

		// fake a InitHUD & ResetHUD message
		gHUD.MsgFunc_InitHUD( NULL, 0, NULL );
		gHUD.MsgFunc_ResetHUD( NULL, 0, NULL );

		break;

	case DRC_CMD_EVENT: // old director style message
		m_lastPrimaryObject   = READ_WORD();
		m_lastSecondaryObject = READ_WORD();
		m_iObserverFlags      = READ_LONG();

		if ( m_autoDirector->value )
		{
			if ( ( g_iUser2 != m_lastPrimaryObject ) || ( g_iUser3 != m_lastSecondaryObject ) )
				V_ResetChaseCam();

			g_iUser2          = m_lastPrimaryObject;
			g_iUser3          = m_lastSecondaryObject;
			m_IsInterpolating = false;
			m_ChaseEntity     = 0;
		}
		break;
	case DRC_CMD_MODE:
		if ( m_autoDirector->value )
		{
			SetModes( READ_BYTE(), -1 );
		}
		break;

	case DRC_CMD_CAMERA:
		v1[0] = READ_COORD(); // position
		v1[1] = READ_COORD();
		v1[2] = READ_COORD(); // vJumpOrigin

		v2[0] = READ_COORD(); // view angle
		v2[1] = READ_COORD(); // vJumpAngles
		v2[2] = READ_COORD();
		f1    = READ_BYTE(); // fov
		i1    = READ_WORD(); // target

		if ( m_autoDirector->value )
		{
			SetModes( OBS_ROAMING, -1 );
			SetCameraView( v1, v2, f1 );
			m_ChaseEntity = i1;
		}
		break;

	case DRC_CMD_MESSAGE:
	{
		client_textmessage_t *msg = &m_HUDMessages[m_lastHudMessage];

		msg->effect = READ_BYTE(); // effect

		UnpackRGB( (int &)msg->r1, (int &)msg->g1, (int &)msg->b1, READ_LONG() ); // color
		msg->r2 = msg->r1;
		msg->g2 = msg->g1;
		msg->b2 = msg->b1;
		msg->a2 = msg->a1 = 0xFF; // not transparent

		msg->x = READ_FLOAT(); // x pos
		msg->y = READ_FLOAT(); // y pos

		msg->fadein   = READ_FLOAT(); // fadein
		msg->fadeout  = READ_FLOAT(); // fadeout
		msg->holdtime = READ_FLOAT(); // holdtime
		msg->fxtime   = READ_FLOAT(); // fxtime;

		strncpy( m_HUDMessageText[m_lastHudMessage], READ_STRING(), 128 );
		m_HUDMessageText[m_lastHudMessage][127] = 0; // text

		msg->pMessage = m_HUDMessageText[m_lastHudMessage];
		msg->pName    = "HUD_MESSAGE";

		gHUD.m_Message.MessageAdd( msg );

		m_lastHudMessage++;
		m_lastHudMessage %= MAX_SPEC_HUD_MESSAGES;
	}

	break;

	case DRC_CMD_SOUND:
		string = READ_STRING();
		f1     = READ_FLOAT();

		gEngfuncs.pEventAPI->EV_PlaySound( 0, v_origin, CHAN_BODY, string, f1, ATTN_NORM, 0, PITCH_NORM );

		break;

	case DRC_CMD_TIMESCALE:
		f1 = READ_FLOAT(); // ignore this command (maybe show slowmo sign)
		break;

	case DRC_CMD_STATUS:
		READ_LONG();                      // total number of spectator slots
		m_iSpectatorNumber = READ_LONG(); // total number of spectator
		READ_WORD();                      // total number of relay proxies

		if ( gViewPort )
			gViewPort->UpdateSpectatorPanel();
		break;

	case DRC_CMD_BANNER:
	{
		const char *banner = READ_STRING();
		if ( gViewPort && gViewPort->m_pSpectatorPanel && gViewPort->m_pSpectatorPanel->m_TopBanner )
		{
			gViewPort->m_pSpectatorPanel->m_TopBanner->LoadImage( banner );
			gViewPort->UpdateSpectatorPanel();
		}
		break;
	}

	case DRC_CMD_STUFFTEXT:
		EngineFilteredClientCmd( READ_STRING() );
		break;

	case DRC_CMD_CAMPATH:
		v1[0] = READ_COORD(); // position
		v1[1] = READ_COORD();
		v1[2] = READ_COORD(); // vJumpOrigin

		v2[0] = READ_COORD(); // view angle
		v2[1] = READ_COORD(); // vJumpAngles
		v2[2] = READ_COORD();
		f1    = READ_BYTE(); // FOV
		i1    = READ_BYTE(); // flags

		if ( m_autoDirector->value )
		{
			SetModes( OBS_ROAMING, -1 );
			SetCameraView( v1, v2, f1 );
		}
		break;

	case DRC_CMD_WAYPOINTS:
		i1             = READ_BYTE();
		m_NumWayPoints = 0;
		m_WayPoint     = 0;
		for ( i2 = 0; i2 < i1; i2++ )
		{
			f1 = gHUD.m_flTime + (float)( READ_SHORT() ) / 100.0f;

			v1[0] = READ_COORD(); // position
			v1[1] = READ_COORD();
			v1[2] = READ_COORD(); // vJumpOrigin

			v2[0] = READ_COORD(); // view angle
			v2[1] = READ_COORD(); // vJumpAngles
			v2[2] = READ_COORD();
			f2    = READ_BYTE(); // fov
			i3    = READ_BYTE(); // flags

			AddWaypoint( f1, v1, v2, f2, i3 );
		}

		if ( !m_autoDirector->value )
		{
			// ignore waypoints
			m_NumWayPoints = 0;
			break;
		}

		SetModes( OBS_ROAMING, -1 );

		m_IsInterpolating = true;

		if ( m_NumWayPoints > 2 )
		{
			SetWayInterpolation( NULL, &m_CamPath[0], &m_CamPath[1], &m_CamPath[2] );
		}
		else
		{
			SetWayInterpolation( NULL, &m_CamPath[0], &m_CamPath[1], NULL );
		}
		break;

	default:
		gEngfuncs.Con_DPrintf( "CHudSpectator::DirectorMessage: unknown command %i.\n", cmd );
	}
}

bool CHudSpectator::ParseOverviewFile()
{
	char filename[255];
	char levelname[255];
	char token[1024];
	float height;

	char *pfile = NULL;

	memset( &m_OverviewData, 0, sizeof( m_OverviewData ) );

	// fill in standrd values
	m_OverviewData.insetWindowX      = 4; // upper left corner
	m_OverviewData.insetWindowY      = 4;
	m_OverviewData.insetWindowHeight = 180;
	m_OverviewData.insetWindowWidth  = 240;
	m_OverviewData.origin[0]         = 0.0f;
	m_OverviewData.origin[1]         = 0.0f;
	m_OverviewData.origin[2]         = 0.0f;
	m_OverviewData.zoom              = 1.0f;
	m_OverviewData.layers            = 0;
	m_OverviewData.layersHeights[0]  = 0.0f;
	strncpy( m_OverviewData.map, gEngfuncs.pfnGetLevelName(), sizeof( m_OverviewData.map ) - 1 );
	m_OverviewData.map[sizeof( m_OverviewData.map ) - 1] = '\0';

	if ( strlen( m_OverviewData.map ) == 0 )
		return false; // not active yet

	strncpy( levelname, m_OverviewData.map + 5, sizeof( levelname ) - 1 );
	levelname[sizeof( levelname ) - 1] = '\0';
	levelname[strlen( levelname ) - 4] = 0;

	snprintf( filename, sizeof( filename ), "overviews/%s.txt", levelname );

	pfile = (char *)gEngfuncs.COM_LoadFile( filename, 5, NULL );

	if ( !pfile )
	{
		gEngfuncs.Con_DPrintf( "Couldn't open file %s. Using default values for overiew mode.\n", filename );
		return false;
	}

	while ( true )
	{
		pfile = gEngfuncs.COM_ParseFile( pfile, token );

		if ( !pfile )
			break;

		if ( !stricmp( token, "global" ) )
		{
			// parse the global data
			pfile = gEngfuncs.COM_ParseFile( pfile, token );
			if ( stricmp( token, "{" ) )
			{
				gEngfuncs.Con_Printf( "Error parsing overview file %s. (expected { )\n", filename );
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile( pfile, token );

			while ( stricmp( token, "}" ) )
			{
				if ( !stricmp( token, "zoom" ) )
				{
					pfile               = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.zoom = atof( token );
				}
				else if ( !stricmp( token, "origin" ) )
				{
					pfile                    = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.origin[0] = atof( token );
					pfile                    = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.origin[1] = atof( token );
					pfile                    = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.origin[2] = atof( token );
				}
				else if ( !stricmp( token, "rotated" ) )
				{
					pfile                  = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.rotated = atoi( token );
				}
				else if ( !stricmp( token, "inset" ) )
				{
					pfile                            = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.insetWindowX      = atof( token );
					pfile                            = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.insetWindowY      = atof( token );
					pfile                            = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.insetWindowWidth  = atof( token );
					pfile                            = gEngfuncs.COM_ParseFile( pfile, token );
					m_OverviewData.insetWindowHeight = atof( token );
				}
				else
				{
					gEngfuncs.Con_Printf( "Error parsing overview file %s. (%s unkown)\n", filename, token );
					return false;
				}

				pfile = gEngfuncs.COM_ParseFile( pfile, token ); // parse next token
			}
		}
		else if ( !stricmp( token, "layer" ) )
		{
			// parse a layer data

			if ( m_OverviewData.layers == OVERVIEW_MAX_LAYERS )
			{
				gEngfuncs.Con_Printf( "Error parsing overview file %s. ( too many layers )\n", filename );
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile( pfile, token );

			if ( stricmp( token, "{" ) )
			{
				gEngfuncs.Con_Printf( "Error parsing overview file %s. (expected { )\n", filename );
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile( pfile, token );

			while ( stricmp( token, "}" ) )
			{
				if ( !stricmp( token, "image" ) )
				{
					pfile = gEngfuncs.COM_ParseFile( pfile, token );
					strncpy( m_OverviewData.layersImages[m_OverviewData.layers], token, sizeof( m_OverviewData.layersImages[m_OverviewData.layers] ) - 1 );
					m_OverviewData.layersImages[m_OverviewData.layers][sizeof( m_OverviewData.layersImages[m_OverviewData.layers] ) - 1] = '\0';
				}
				else if ( !stricmp( token, "height" ) )
				{
					pfile                                               = gEngfuncs.COM_ParseFile( pfile, token );
					height                                              = atof( token );
					m_OverviewData.layersHeights[m_OverviewData.layers] = height;
				}
				else
				{
					gEngfuncs.Con_Printf( "Error parsing overview file %s. (%s unkown)\n", filename, token );
					return false;
				}

				pfile = gEngfuncs.COM_ParseFile( pfile, token ); // parse next token
			}

			m_OverviewData.layers++;
		}
	}

	gEngfuncs.COM_FreeFile( pfile );

	m_mapZoom   = m_OverviewData.zoom;
	m_mapOrigin = m_OverviewData.origin;

	return true;
}

void CHudSpectator::LoadMapSprites()
{
	if ( m_OverviewData.layers > 0 )
	{
		m_MapSprite = gEngfuncs.LoadMapSprite( m_OverviewData.layersImages[0] );
	}
	else
		m_MapSprite = NULL;
}

void CHudSpectator::DrawOverviewLayer()
{
	float screenaspect, xs, ys, xStep, yStep, x, y, z;
	int ix, iy, i, xTiles, yTiles, frame;

	qboolean maybeCanDoTriAPI = true;

	if ( !m_MapSprite )
		return;

	z = m_OverviewData.layersHeights[0];

	i = m_MapSprite->numframes / ( 4 * 3 );

	i = sqrt( (double)i );

	xTiles = i * 4;
	yTiles = i * 3;

	screenaspect = 4.0f / 3.0f;

	xs = m_OverviewData.origin[0];
	ys = m_OverviewData.origin[1];

	xStep = ( 4096.0f / m_OverviewData.zoom ) / xTiles;
	yStep = -( ( 4096.0f / m_OverviewData.zoom ) / screenaspect ) / yTiles;

	xs -= ( ( 4096.0f / m_OverviewData.zoom ) / 2 );
	ys += ( ( ( 4096.0f / m_OverviewData.zoom ) / screenaspect ) / 2 );

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, 1.0 );

	if ( m_OverviewData.rotated )
	{
		for ( ix = 0; ix < yTiles; ix++ )
		{
			x = xs + ( ix * ( 4096.0f / m_OverviewData.zoom ) / (float)yTiles );

			for ( iy = 0; iy < xTiles; iy++ )
			{
				y = ys + ( ( iy + 1 ) * ( -( 4096.0f / m_OverviewData.zoom ) / screenaspect ) / (float)xTiles );

				frame = iy * yTiles + ( ( yTiles - 1 ) - ix );

				gEngfuncs.pTriAPI->SpriteTexture( m_MapSprite, frame );

				gEngfuncs.pTriAPI->Begin( TRI_QUADS );
				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x, y, z );

				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x, y - yStep, z );

				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y - yStep, z );

				gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y, z );
				gEngfuncs.pTriAPI->End();
			}
		}
	}
	else
	{
		for ( iy = 0; iy < yTiles; iy++ )
		{
			y = ys + ( iy * yStep );

			for ( ix = 0; ix < xTiles; ix++ )
			{
				x = xs + ( ix * xStep );

				frame = iy * xTiles + ix;

				gEngfuncs.pTriAPI->SpriteTexture( m_MapSprite, frame );

				gEngfuncs.pTriAPI->Begin( TRI_QUADS );
				gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x, y, z );

				gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x, y + yStep, z );

				gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y + yStep, z );

				gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
				gEngfuncs.pTriAPI->Vertex3f( x + xStep, y, z );
				gEngfuncs.pTriAPI->End();
			}
		}
	}
}

void CHudSpectator::DrawOverviewEntities()
{
	int i, ir, ig, ib;
	struct model_s *hSpriteModel;
	vec3_t origin, angles, point, forward, right, up, left, offset, screen;
	float x, y, z, r, g, b, sizeScale = 1.0;
	cl_entity_t *ent;
	float rmatrix[3][4]; // rotation matrix

	float zScale = ( 90.0f - v_angles[0] ) / 90.0f;

	z = m_OverviewData.layersHeights[0] * zScale;
	UnpackRGB( ir, ig, ib, RGB_YELLOWISH );
	r = (float)ir / 255.0f;
	g = (float)ig / 255.0f;
	b = (float)ib / 255.0f;

	gEngfuncs.pTriAPI->CullFace( TRI_NONE );

	for ( i = 0; i < MAX_PLAYERS; i++ )
		m_vPlayerPos[i][2] = -1;

	for ( i = 0; i < MAX_OVERVIEW_ENTITIES; i++ )
	{
		if ( !m_OverviewEntities[i].hSprite )
			continue;

		hSpriteModel = (struct model_s *)gEngfuncs.GetSpritePointer( m_OverviewEntities[i].hSprite );
		ent          = m_OverviewEntities[i].entity;

		gEngfuncs.pTriAPI->SpriteTexture( hSpriteModel, 0 );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );

		AngleVectors( ent->angles, right, up, NULL );

		VectorCopy( ent->origin, origin );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );

		gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, 1.0 );

		gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
		VectorMA( origin, 16.0f * sizeScale, up, point );
		VectorMA( point, 16.0f * sizeScale, right, point );
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );

		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );

		VectorMA( origin, 16.0f * sizeScale, up, point );
		VectorMA( point, -16.0f * sizeScale, right, point );
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );

		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		VectorMA( origin, -16.0f * sizeScale, up, point );
		VectorMA( point, -16.0f * sizeScale, right, point );
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );

		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		VectorMA( origin, -16.0f * sizeScale, up, point );
		VectorMA( point, 16.0f * sizeScale, right, point );
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv( point );

		gEngfuncs.pTriAPI->End();

		if ( !ent->player )
			continue;

		origin[2] *= zScale;

		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );

		hSpriteModel = (struct model_s *)gEngfuncs.GetSpritePointer( m_hsprBeam );
		gEngfuncs.pTriAPI->SpriteTexture( hSpriteModel, 0 );

		gEngfuncs.pTriAPI->Color4f( r, g, b, 0.3 );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] + 4, origin[1] + 4, origin[2] - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] - 4, origin[1] - 4, origin[2] - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] - 4, origin[1] - 4, z );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] + 4, origin[1] + 4, z );
		gEngfuncs.pTriAPI->End();

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] - 4, origin[1] + 4, origin[2] - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] + 4, origin[1] - 4, origin[2] - zScale );
		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] + 4, origin[1] - 4, z );
		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		gEngfuncs.pTriAPI->Vertex3f( origin[0] - 4, origin[1] + 4, z );
		gEngfuncs.pTriAPI->End();

		if ( gEngfuncs.pTriAPI->WorldToScreen( origin, screen ) )
			continue;

		screen[0] = XPROJECT( screen[0] );
		screen[1] = YPROJECT( screen[1] );
		screen[2] = 0.0f;

		origin[0] += 32.0f;
		origin[1] += 32.0f;

		gEngfuncs.pTriAPI->WorldToScreen( origin, offset );

		offset[0] = XPROJECT( offset[0] );
		offset[1] = YPROJECT( offset[1] );
		offset[2] = 0.0f;

		VectorSubtract( offset, screen, offset );

		int playerNum = ent->index - 1;

		m_vPlayerPos[playerNum][0] = screen[0];
		m_vPlayerPos[playerNum][1] = screen[1] + Length( offset );
		m_vPlayerPos[playerNum][2] = 1;
	}

	if ( !m_pip->value || !m_drawcone->value )
		return;

	if ( m_pip->value == INSET_IN_EYE || g_iUser1 == OBS_IN_EYE )
	{
		V_GetInEyePos( g_iUser2, origin, angles );
	}
	else if ( m_pip->value == INSET_CHASE_FREE || g_iUser1 == OBS_CHASE_FREE )
	{
		V_GetChasePos( g_iUser2, v_cl_angles, origin, angles );
	}
	else if ( g_iUser1 == OBS_ROAMING )
	{
		VectorCopy( v_sim_org, origin );
		VectorCopy( v_cl_angles, angles );
	}
	else
		V_GetChasePos( g_iUser2, NULL, origin, angles );

	x = origin[0];
	y = origin[1];
	z = origin[2];

	angles[0] = 0;

	hSpriteModel = (struct model_s *)gEngfuncs.GetSpritePointer( m_hsprCamera );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->SpriteTexture( hSpriteModel, 0 );

	gEngfuncs.pTriAPI->Color4f( r, g, b, 1.0 );

	AngleVectors( angles, forward, NULL, NULL );
	VectorScale( forward, 512.0f, forward );

	offset[0] = 0.0f;
	offset[1] = 45.0f;
	offset[2] = 0.0f;

	AngleMatrix( offset, rmatrix );
	VectorTransform( forward, rmatrix, right );

	offset[1] = -45.0f;
	AngleMatrix( offset, rmatrix );
	VectorTransform( forward, rmatrix, left );

	gEngfuncs.pTriAPI->Begin( TRI_TRIANGLES );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3f( x + right[0], y + right[1], ( z + right[2] ) * zScale );

	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3f( x, y, z * zScale );

	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3f( x + left[0], y + left[1], ( z + left[2] ) * zScale );
	gEngfuncs.pTriAPI->End();
}

void CHudSpectator::DrawOverview()
{
	if ( !g_iUser1 )
		return;

	if ( m_iDrawCycle == 0 && ( ( g_iUser1 != OBS_MAP_FREE ) && ( g_iUser1 != OBS_MAP_CHASE ) ) )
		return;

	if ( m_iDrawCycle == 1 && m_pip->value < INSET_MAP_FREE )
		return;

	DrawOverviewLayer();
	DrawOverviewEntities();
	CheckOverviewEntities();
}

void CHudSpectator::CheckOverviewEntities()
{
	double time = gEngfuncs.GetClientTime();

	for ( int i = 0; i < MAX_OVERVIEW_ENTITIES; i++ )
	{
		if ( m_OverviewEntities[i].killTime < time )
		{
			memset( &m_OverviewEntities[i], 0, sizeof( overviewEntity_t ) );
		}
	}
}

bool CHudSpectator::AddOverviewEntity( int type, struct cl_entity_s *ent, const char *modelname )
{
	HSPRITE hSprite = 0;
	double duration = -1.0f;

	if ( !ent )
		return false;

	if ( type == ET_PLAYER )
	{
		if ( ent->curstate.solid != SOLID_NOT )
		{
			switch ( g_PlayerExtraInfo[ent->index].teamnumber )
			{
			case 1:
				hSprite = m_hsprPlayerBlue;
				break;
			case 2:
				hSprite = m_hsprPlayerRed;
				break;
			default:
				hSprite = m_hsprPlayer;
				break;
			}
		}
		else
			return false;
	}
	else if ( type == ET_NORMAL )
	{
		return false;
	}
	else
		return false;

	return AddOverviewEntityToList( hSprite, ent, gEngfuncs.GetClientTime() + duration );
}

void CHudSpectator::DeathMessage( int victim )
{
	cl_entity_t *pl = gEngfuncs.GetEntityByIndex( victim );

	if ( pl && pl->player )
		AddOverviewEntityToList( m_hsprPlayerDead, pl, gEngfuncs.GetClientTime() + 2.0f );
}

bool CHudSpectator::AddOverviewEntityToList( HSPRITE sprite, cl_entity_t *ent, double killTime )
{
	for ( int i = 0; i < MAX_OVERVIEW_ENTITIES; i++ )
	{
		if ( m_OverviewEntities[i].entity == NULL )
		{
			m_OverviewEntities[i].entity   = ent;
			m_OverviewEntities[i].hSprite  = sprite;
			m_OverviewEntities[i].killTime = killTime;
			return true;
		}
	}

	return false;
}

void CHudSpectator::FindNextPlayer( bool bReverse )
{
	// if we are NOT in HLTV mode, spectator targets are set on server
	if ( !gEngfuncs.IsSpectateOnly() )
	{
		char cmdstring[32];
		// forward command to server
		snprintf( cmdstring, sizeof( cmdstring ), "follownext %i", bReverse ? 1 : 0 );
		gEngfuncs.pfnServerCmd( cmdstring );
		return;
	}

	int iStart = g_iUser2;

	if ( iStart < 1 || iStart > MAX_PLAYERS )
		iStart = 1;

	int iCurrent = iStart;

	int iDir = bReverse ? -1 : 1;

	cl_entity_t *pEnt = NULL;

	// make sure we have player info
	if ( gViewPort )
		gViewPort->GetAllPlayersInfo();

	do
	{
		iCurrent += iDir;

		// Loop through the clients
		if ( iCurrent > MAX_PLAYERS )
			iCurrent = 1;
		if ( iCurrent < 1 )
			iCurrent = MAX_PLAYERS;

		pEnt = gEngfuncs.GetEntityByIndex( iCurrent );

		if ( !IsActivePlayer( pEnt ) )
			continue;

		g_iUser2 = iCurrent;
		break;

	} while ( iCurrent != iStart );

	// Did we find a target?
	if ( !g_iUser2 )
	{
		gEngfuncs.Con_DPrintf( "No observer targets.\n" );
		// take save camera position
		VectorCopy( m_cameraOrigin, vJumpOrigin );
		VectorCopy( m_cameraAngles, vJumpAngles );
	}
	else
	{
		// use new entity position for roaming
		VectorCopy( pEnt->origin, vJumpOrigin );
		VectorCopy( pEnt->angles, vJumpAngles );
	}

	iJumpSpectator = 1;
	if ( gViewPort )
		gViewPort->MsgFunc_ResetFade( NULL, 0, NULL );
}

void CHudSpectator::FindPlayer( const char *name )
{
	// if we are NOT in HLTV mode, spectator targets are set on server
	if ( !gEngfuncs.IsSpectateOnly() )
	{
		char cmdstring[32];
		// forward command to server
		snprintf( cmdstring, sizeof( cmdstring ), "follow %s", name );
		gEngfuncs.pfnServerCmd( cmdstring );
		return;
	}

	g_iUser2 = 0;

	// make sure we have player info
	if ( gViewPort )
		gViewPort->GetAllPlayersInfo();

	cl_entity_t *pEnt = NULL;

	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		pEnt = gEngfuncs.GetEntityByIndex( i );

		if ( !IsActivePlayer( pEnt ) )
			continue;

		if ( !stricmp( g_PlayerInfoList[pEnt->index].name, name ) )
		{
			g_iUser2 = i;
			break;
		}
	}

	// Did we find a target?
	if ( !g_iUser2 )
	{
		gEngfuncs.Con_DPrintf( "No observer targets.\n" );
		// take save camera position
		VectorCopy( m_cameraOrigin, vJumpOrigin );
		VectorCopy( m_cameraAngles, vJumpAngles );
	}
	else
	{
		// use new entity position for roaming
		VectorCopy( pEnt->origin, vJumpOrigin );
		VectorCopy( pEnt->angles, vJumpAngles );
	}

	iJumpSpectator = 1;
	if ( gViewPort )
		gViewPort->MsgFunc_ResetFade( NULL, 0, NULL );
}

void CHudSpectator::HandleButtonsDown( int ButtonPressed )
{
	double time = gEngfuncs.GetClientTime();

	int newMainMode  = g_iUser1;
	int newInsetMode = m_pip->value;

	if ( !gViewPort )
		return;

	// Not in intermission.
	if ( gHUD.m_iIntermission )
		return;

	if ( !g_iUser1 )
		return; // dont do anything if not in spectator mode

	// don't handle buttons during normal demo playback
	if ( gEngfuncs.pDemoAPI->IsPlayingback() && !gEngfuncs.IsSpectateOnly() )
		return;
	// Slow down mouse clicks.
	if ( m_flNextObserverInput > time )
		return;

	// enable spectator screen
	if ( ButtonPressed & IN_DUCK )
	{
		if ( gViewPort && gViewPort->m_pSpectatorPanel )
			gViewPort->m_pSpectatorPanel->ShowMenu( !gViewPort->m_pSpectatorPanel->m_menuVisible );
	}

	//  'Use' changes inset window mode
	if ( ButtonPressed & IN_USE )
	{
		newInsetMode = ToggleInset( true );
	}

	// if not in HLTV mode, buttons are handled server side
	if ( gEngfuncs.IsSpectateOnly() )
	{
		// changing target or chase mode not in overviewmode without inset window

		// Jump changes main window modes
		if ( ButtonPressed & IN_JUMP )
		{
			if ( g_iUser1 == OBS_CHASE_LOCKED )
				newMainMode = OBS_CHASE_FREE;

			else if ( g_iUser1 == OBS_CHASE_FREE )
				newMainMode = OBS_IN_EYE;

			else if ( g_iUser1 == OBS_IN_EYE )
				newMainMode = OBS_ROAMING;

			else if ( g_iUser1 == OBS_ROAMING )
				newMainMode = OBS_MAP_FREE;

			else if ( g_iUser1 == OBS_MAP_FREE )
				newMainMode = OBS_MAP_CHASE;

			else
				newMainMode = OBS_CHASE_FREE;
		}

		// Attack moves to the next player
		if ( ButtonPressed & ( IN_ATTACK | IN_ATTACK2 ) )
		{
			FindNextPlayer( ( ButtonPressed & IN_ATTACK2 ) ? true : false );

			if ( g_iUser1 == OBS_ROAMING )
			{
				gEngfuncs.SetViewAngles( vJumpAngles );
				iJumpSpectator = 1;
			}
			// release directed mode if player wants to see another player
			m_autoDirector->value = 0.0f;
		}
	}

	SetModes( newMainMode, newInsetMode );

	if ( g_iUser1 == OBS_MAP_FREE )
	{
		if ( ButtonPressed & IN_FORWARD )
			m_zoomDelta = 0.01f;

		if ( ButtonPressed & IN_BACK )
			m_zoomDelta = -0.01f;

		if ( ButtonPressed & IN_MOVELEFT )
			m_moveDelta = -12.0f;

		if ( ButtonPressed & IN_MOVERIGHT )
			m_moveDelta = 12.0f;
	}

	m_flNextObserverInput = time + 0.2;
}

void CHudSpectator::HandleButtonsUp( int ButtonPressed )
{
	if ( !gViewPort || !gViewPort->m_pSpectatorPanel )
		return;

	if ( !gViewPort->m_pSpectatorPanel->isVisible() )
		return;

	if ( ButtonPressed & ( IN_FORWARD | IN_BACK ) )
		m_zoomDelta = 0.0f;

	if ( ButtonPressed & ( IN_MOVELEFT | IN_MOVERIGHT ) )
		m_moveDelta = 0.0f;
}

void CHudSpectator::SetModes( int iNewMainMode, int iNewInsetMode )
{
	// if value == -1 keep old value
	if ( iNewMainMode == -1 )
		iNewMainMode = g_iUser1;

	if ( iNewInsetMode == -1 )
		iNewInsetMode = m_pip->value;

	// inset mode is handled only clients side
	m_pip->value = iNewInsetMode;

	if ( iNewMainMode < OBS_CHASE_LOCKED || iNewMainMode > OBS_MAP_CHASE )
	{
		gEngfuncs.Con_Printf( "Invalid spectator mode.\n" );
		return;
	}

	m_IsInterpolating = false;
	m_ChaseEntity     = 0;

	// main mode settings will override inset window settings
	if ( iNewMainMode != g_iUser1 )
	{
		// if we are NOT in HLTV mode, main spectator mode is set on server
		if ( !gEngfuncs.IsSpectateOnly() )
		{
			char cmdstring[32];
			// forward command to server
			snprintf( cmdstring, sizeof( cmdstring ), "specmode %i", iNewMainMode );
			gEngfuncs.pfnServerCmd( cmdstring );
			return;
		}

		if ( !g_iUser2 && ( iNewMainMode != OBS_ROAMING ) )
		{
			// choose last Director object if still available
			if ( IsActivePlayer( gEngfuncs.GetEntityByIndex( m_lastPrimaryObject ) ) )
			{
				g_iUser2 = m_lastPrimaryObject;
				g_iUser3 = m_lastSecondaryObject;
			}
			else
				FindNextPlayer( false );
		}

		switch ( iNewMainMode )
		{
		case OBS_CHASE_LOCKED:
			g_iUser1 = OBS_CHASE_LOCKED;
			break;

		case OBS_CHASE_FREE:
			g_iUser1 = OBS_CHASE_FREE;
			break;

		case OBS_ROAMING:
			g_iUser1 = OBS_ROAMING;
			if ( g_iUser2 )
			{
				V_GetChasePos( g_iUser2, v_cl_angles, vJumpOrigin, vJumpAngles );
				gEngfuncs.SetViewAngles( vJumpAngles );
				iJumpSpectator = 1;
			}
			break;

		case OBS_IN_EYE:
			g_iUser1 = OBS_IN_EYE;
			break;

		case OBS_MAP_FREE:
			g_iUser1    = OBS_MAP_FREE;
			m_mapZoom   = m_OverviewData.zoom;
			m_mapOrigin = m_OverviewData.origin;
			break;

		case OBS_MAP_CHASE:
			g_iUser1    = OBS_MAP_CHASE;
			m_mapZoom   = m_OverviewData.zoom;
			m_mapOrigin = m_OverviewData.origin;
			break;
		}

		if ( ( g_iUser1 == OBS_IN_EYE ) || ( g_iUser1 == OBS_ROAMING ) )
		{
			m_crosshairRect.left   = 24;
			m_crosshairRect.top    = 0;
			m_crosshairRect.right  = 48;
			m_crosshairRect.bottom = 24;

			SetCrosshair( m_hCrosshair, m_crosshairRect, 255, 255, 255 );
		}
		else
		{
			memset( &m_crosshairRect, 0, sizeof( m_crosshairRect ) );
			SetCrosshair( 0, m_crosshairRect, 0, 0, 0 );
		}

		if ( gViewPort )
			gViewPort->MsgFunc_ResetFade( NULL, 0, NULL );

		char string[128];
		snprintf( string, sizeof( string ), "#Spec_Mode%d", g_iUser1 );
		snprintf( string, sizeof( string ), "%c%s", HUD_PRINTCENTER, CHudTextMessage::BufferedLocaliseTextString( string ) );
		gHUD.m_TextMessage.MsgFunc_TextMsg( NULL, strlen( string ) + 1, string );
	}

	if ( gViewPort )
		gViewPort->UpdateSpectatorPanel();
}

bool CHudSpectator::IsActivePlayer( cl_entity_t *ent )
{
	return ( ent &&
	         ent->player &&
	         ent->curstate.solid != SOLID_NOT &&
	         ent != gEngfuncs.GetLocalPlayer() &&
	         g_PlayerInfoList[ent->index].name != NULL );
}

void CHudSpectator::CheckSettings()
{
	m_pip->value = (int)m_pip->value;

	if ( ( g_iUser1 < OBS_MAP_FREE ) && ( m_pip->value == INSET_CHASE_FREE || m_pip->value == INSET_IN_EYE ) )
	{
		m_pip->value = INSET_MAP_FREE;
	}

	if ( ( g_iUser1 >= OBS_MAP_FREE ) && ( m_pip->value >= INSET_MAP_FREE ) )
	{
		m_pip->value = INSET_CHASE_FREE;
	}

	if ( gHUD.m_iIntermission )
		m_pip->value = INSET_OFF;

	if ( m_chatEnabled != ( gHUD.m_SayText.m_HUD_saytext->value != 0 ) )
	{
		m_chatEnabled = ( gHUD.m_SayText.m_HUD_saytext->value != 0 );

		if ( gEngfuncs.IsSpectateOnly() )
		{
			char chatcmd[32];
			snprintf( chatcmd, sizeof( chatcmd ), "ignoremsg %i", m_chatEnabled ? 0 : 1 );
			gEngfuncs.pfnServerCmd( chatcmd );
		}
	}

	if ( ( g_iUser1 == OBS_IN_EYE ) || ( g_iUser1 == OBS_ROAMING ) )
	{
		m_crosshairRect.left   = 24;
		m_crosshairRect.top    = 0;
		m_crosshairRect.right  = 48;
		m_crosshairRect.bottom = 24;

		SetCrosshair( m_hCrosshair, m_crosshairRect, 255, 255, 255 );
	}
	else
	{
		memset( &m_crosshairRect, 0, sizeof( m_crosshairRect ) );
		SetCrosshair( 0, m_crosshairRect, 0, 0, 0 );
	}

	if ( ( ( g_iTeamNumber == 1 ) || ( g_iTeamNumber == 2 ) ) && ( g_iUser1 == OBS_IN_EYE ) )
		m_pip->value = INSET_OFF;

	if ( gViewPort && gViewPort->m_pSpectatorPanel )
		gViewPort->m_pSpectatorPanel->EnableInsetView( m_pip->value != INSET_OFF );
}

int CHudSpectator::ToggleInset( bool allowOff )
{
	int newInsetMode = (int)m_pip->value + 1;

	if ( g_iUser1 < OBS_MAP_FREE )
	{
		if ( newInsetMode > INSET_MAP_CHASE )
		{
			if ( allowOff )
				newInsetMode = INSET_OFF;
			else
				newInsetMode = INSET_MAP_FREE;
		}

		if ( newInsetMode == INSET_CHASE_FREE )
			newInsetMode = INSET_MAP_FREE;
	}
	else
	{
		if ( newInsetMode > INSET_IN_EYE )
		{
			if ( allowOff )
				newInsetMode = INSET_OFF;
			else
				newInsetMode = INSET_CHASE_FREE;
		}
	}

	return newInsetMode;
}
