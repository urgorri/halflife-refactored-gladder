//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL VGUI Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include <VGUI_Cursor.h>
#include <VGUI_Frame.h>
#include <VGUI_Label.h>
#include <VGUI_Surface.h>
#include <VGUI_BorderLayout.h>
#include <VGUI_Panel.h>
#include <VGUI_ImagePanel.h>
#include <VGUI_Button.h>
#include <VGUI_ActionSignal.h>
#include <VGUI_InputSignal.h>
#include <VGUI_MenuSeparator.h>
#include <VGUI_TextPanel.h>
#include <VGUI_LoweredBorder.h>
#include <VGUI_LineBorder.h>
#include <VGUI_Scheme.h>
#include <VGUI_Font.h>
#include <VGUI_App.h>
#include <VGUI_BuildGroup.h>

#include "../hud/hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"
#include "pm_shared.h"
#include "keydefs.h"
#include "demo.h"
#include "demo_api.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_TeamMenuPanel.h"
#include "vgui_ClassMenuPanel.h"
#include "vgui_CommandMenuPanel.h"
#include "vgui_ServerBrowser.h"
#include "vgui_ScorePanel.h"
#include "vgui_SpectatorPanel.h"
#include "vgui_TeamMenuPanel.h"

#include "shake.h"
#include "screenfade.h"

extern int g_iVisibleMouse;
class CCommandMenu;
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

// Scoreboard positions
#define SBOARD_INDENT_X XRES( 104 )
#define SBOARD_INDENT_Y YRES( 40 )

// low-res scoreboard indents
#define SBOARD_INDENT_X_512 30
#define SBOARD_INDENT_Y_512 30

#define SBOARD_INDENT_X_400 0
#define SBOARD_INDENT_Y_400 20

void IN_ResetMouse( void );
void IN_ResetRelativeMouseState( void );
void COM_FileBase( const char *in, char *out );
extern CMenuPanel *CMessageWindowPanel_Create( const char *szMOTD, const char *szTitle, int iShadeFullscreen, int iRemoveMe, int x, int y, int wide, int tall );
extern float *GetClientColor( int clientIndex );

using namespace vgui;

// Team Colors
int iNumberOfTeamColors = 5;
int iTeamColors[5][3] =
    {
        { 255, 170, 0 },   // HL orange (default)
        { 125, 165, 210 }, // Blue
        { 200, 90, 70 },   // Red
        { 225, 205, 45 },  // Yellow
        { 145, 215, 140 }, // Green
};

// Used for Class specific buttons
char *sTFClasses[] =
    {
        "",
        "SCOUT",
        "SNIPER",
        "SOLDIER",
        "DEMOMAN",
        "MEDIC",
        "HWGUY",
        "PYRO",
        "SPY",
        "ENGINEER",
        "CIVILIAN",
};

char *sLocalisedClasses[] =
    {
        "#Civilian",
        "#Scout",
        "#Sniper",
        "#Soldier",
        "#Demoman",
        "#Medic",
        "#HWGuy",
        "#Pyro",
        "#Spy",
        "#Engineer",
        "#Random",
        "#Civilian",
};

char *sTFClassSelection[] =
    {
        "civilian",
        "scout",
        "sniper",
        "soldier",
        "demoman",
        "medic",
        "hwguy",
        "pyro",
        "spy",
        "engineer",
        "randompc",
        "civilian",
};

#ifdef _TFC
int iBuildingCosts[] =
    {
        BUILD_COST_DISPENSER,
        BUILD_COST_SENTRYGUN,
        BUILD_COST_TELEPORTER };

// This maps class numbers to the Invalid Class bit.
// This is needed for backwards compatability in maps that were finished before
// all the classes were in TF. Hence the wacky sequence.
int sTFValidClassInts[] =
    {
        0,
        TF_ILL_SCOUT,
        TF_ILL_SNIPER,
        TF_ILL_SOLDIER,
        TF_ILL_DEMOMAN,
        TF_ILL_MEDIC,
        TF_ILL_HVYWEP,
        TF_ILL_PYRO,
        TF_ILL_SPY,
        TF_ILL_ENGINEER,
        TF_ILL_RANDOMPC,
};
#endif

// Get the name of TGA file, based on GameDir
char *GetVGUITGAName( const char *pszName )
{
	int i;
	char sz[256];
	static char gd[256];
	const char *gamedir;

	if ( ScreenWidth < 640 )
		i = 320;
	else
		i = 640;
	snprintf( sz, sizeof( sz ), pszName, i );

	gamedir = gEngfuncs.pfnGetGameDirectory();
	snprintf( gd, sizeof( gd ), "%s/gfx/vgui/%s.tga", gamedir, sz );

	return gd;
}

//================================================================
// COMMAND MENU
//================================================================

//-----------------------------------------------------------------------------
// Purpose: Tries to find a button that has a key bound to the input, and
//			presses the button if found
// Input  : keyNum - the character number of the input key
// Output : Returns true if the command menu should close, false otherwise
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: clears the current menus buttons of any armed (highlighted)
//			state, and all their sub buttons
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pSubMenu -
// Output : CommandButton
//-----------------------------------------------------------------------------

// Recalculate the visible buttons

// Make sure all submenus can fit on the screen

// Make this menu and all menus above it in the chain visible

//================================================================
// CreateSubMenu

//-----------------------------------------------------------------------------
// Purpose: Makes sure the memory allocated for TeamFortressViewport is nulled out
// Input  : stAllocateBlock -
// Output : void *
//-----------------------------------------------------------------------------
void *TeamFortressViewport::operator new( size_t stAllocateBlock )
{
	//	void *mem = Panel::operator new( stAllocateBlock );
	void *mem = ::operator new( stAllocateBlock );
	memset( mem, 0, stAllocateBlock );
	return mem;
}

//-----------------------------------------------------------------------------
// Purpose: InputSignal handler for the main viewport
//-----------------------------------------------------------------------------
class CViewPortInputHandler : public InputSignal
{
  public:
	bool bPressed;

	CViewPortInputHandler()
	{
	}

	virtual void cursorMoved( int x, int y, Panel *panel ) {}
	virtual void cursorEntered( Panel *panel ) {}
	virtual void cursorExited( Panel *panel ) {}
	virtual void mousePressed( MouseCode code, Panel *panel )
	{
		if ( code != MOUSE_LEFT )
		{
			// send a message to close the command menu
			// this needs to be a message, since a direct call screws the timing
			gEngfuncs.pfnClientCmd( "ForceCloseCommandMenu\n" );
		}
	}
	virtual void mouseReleased( MouseCode code, Panel *panel )
	{
	}

	virtual void mouseDoublePressed( MouseCode code, Panel *panel ) {}
	virtual void mouseWheeled( int delta, Panel *panel ) {}
	virtual void keyPressed( KeyCode code, Panel *panel ) {}
	virtual void keyTyped( KeyCode code, Panel *panel ) {}
	virtual void keyReleased( KeyCode code, Panel *panel ) {}
	virtual void keyFocusTicked( Panel *panel ) {}
};

//================================================================
TeamFortressViewport::TeamFortressViewport( int x, int y, int wide, int tall )
    : Panel( x, y, wide, tall ), m_SchemeManager( wide, tall )
{
	gViewPort             = this;
	m_iInitialized        = false;
	m_pTeamMenu           = NULL;
	m_pClassMenu          = NULL;
	m_pScoreBoard         = NULL;
	m_pSpectatorPanel     = NULL;
	m_pCurrentMenu        = NULL;
	m_pCurrentCommandMenu = NULL;

	Initialize();
	addInputSignal( new CViewPortInputHandler );

	int r, g, b, a;

	Scheme *pScheme = App::getInstance()->getScheme();

	// primary text color
	// Get the colors
	//!! two different types of scheme here, need to integrate
	SchemeHandle_t hPrimaryScheme = m_SchemeManager.getSchemeHandle( "Primary Button Text" );
	{
		// font
		pScheme->setFont( Scheme::sf_primary1, m_SchemeManager.getFont( hPrimaryScheme ) );

		// text color
		m_SchemeManager.getFgColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor( Scheme::sc_primary1, r, g, b, a ); // sc_primary1 is non-transparent orange

		// background color (transparent black)
		m_SchemeManager.getBgColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor( Scheme::sc_primary3, r, g, b, a );

		// armed foreground color
		m_SchemeManager.getFgArmedColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor( Scheme::sc_secondary2, r, g, b, a );

		// armed background color
		m_SchemeManager.getBgArmedColor( hPrimaryScheme, r, g, b, a );
		pScheme->setColor( Scheme::sc_primary2, r, g, b, a );

		//!! need to get this color from scheme file
		// used for orange borders around buttons
		m_SchemeManager.getBorderColor( hPrimaryScheme, r, g, b, a );
		// pScheme->setColor(Scheme::sc_secondary1, r, g, b, a );
		pScheme->setColor( Scheme::sc_secondary1, 255 * 0.7, 170 * 0.7, 0, 0 );
	}

	// Change the second primary font (used in the scoreboard)
	SchemeHandle_t hScoreboardScheme = m_SchemeManager.getSchemeHandle( "Scoreboard Text" );
	{
		pScheme->setFont( Scheme::sf_primary2, m_SchemeManager.getFont( hScoreboardScheme ) );
	}

	// Change the third primary font (used in command menu)
	SchemeHandle_t hCommandMenuScheme = m_SchemeManager.getSchemeHandle( "CommandMenu Text" );
	{
		pScheme->setFont( Scheme::sf_primary3, m_SchemeManager.getFont( hCommandMenuScheme ) );
	}

	App::getInstance()->setScheme( pScheme );

	// VGUI MENUS
	CreateTeamMenu();
	CreateClassMenu();
	CreateSpectatorMenu();
	CreateScoreBoard();
	// Init command menus
	m_iNumMenus          = 0;
	m_iCurrentTeamNumber = m_iUser1 = m_iUser2 = m_iUser3 = 0;

	m_StandardMenu         = CreateCommandMenu( "commandmenu.txt", 0, CMENU_TOP, false, CMENU_SIZE_X, BUTTON_SIZE_Y, 0 );
	m_SpectatorOptionsMenu = CreateCommandMenu( "spectatormenu.txt", 1, PANEL_HEIGHT, true, CMENU_SIZE_X, BUTTON_SIZE_Y / 2, 0 );                                // above bottom bar, flat design
	m_SpectatorCameraMenu  = CreateCommandMenu( "spectcammenu.txt", 1, PANEL_HEIGHT, true, XRES( 200 ), BUTTON_SIZE_Y / 2, ScreenWidth - ( XRES( 200 ) + 15 ) ); // above bottom bar, flat design

	m_PlayerMenu = m_iNumMenus;
	m_iNumMenus++;

	float flLabelSize = ( ( ScreenWidth - ( XRES( CAMOPTIONS_BUTTON_X ) + 15 ) ) - XRES( 24 + 15 ) ) - XRES( ( 15 + OPTIONS_BUTTON_X + 15 ) + 38 );

	m_pCommandMenus[m_PlayerMenu] = new CCommandMenu( NULL, 1, XRES( ( 15 + OPTIONS_BUTTON_X + 15 ) + 31 ), PANEL_HEIGHT, flLabelSize, 300 );
	m_pCommandMenus[m_PlayerMenu]->setParent( this );
	m_pCommandMenus[m_PlayerMenu]->setVisible( false );
	m_pCommandMenus[m_PlayerMenu]->m_flButtonSizeY = BUTTON_SIZE_Y / 2;
	m_pCommandMenus[m_PlayerMenu]->m_iSpectCmdMenu = 1;

	UpdatePlayerMenu( m_PlayerMenu );

	CreateServerBrowser();
}

//-----------------------------------------------------------------------------
// Purpose: Called everytime a new level is started. Viewport clears out it's data.
//-----------------------------------------------------------------------------
void TeamFortressViewport::Initialize( void )
{
	// Force each menu to Initialize
	if ( m_pTeamMenu )
	{
		m_pTeamMenu->Initialize();
	}
	if ( m_pClassMenu )
	{
		m_pClassMenu->Initialize();
	}
	if ( m_pScoreBoard )
	{
		m_pScoreBoard->Initialize();
		HideScoreBoard();
	}
	if ( m_pSpectatorPanel )
	{
		// Spectator menu doesn't need initializing
		m_pSpectatorPanel->setVisible( false );
	}

	// Make sure all menus are hidden
	HideVGUIMenu();
	HideCommandMenu();

	// Clear out some data
	m_iGotAllMOTD                 = true;
	m_iRandomPC                   = false;
	m_flScoreBoardLastUpdated     = 0;
	m_flSpectatorPanelLastUpdated = 0;

	// reset player info
	g_iPlayerClass = 0;
	g_iTeamNumber  = 0;

	m_sMapName[0]    = '\0';
	m_szServerName[0] = '\0';
	for ( int i = 0; i < 5; i++ )
	{
		m_iValidClasses[i] = 0;
		m_sTeamNames[i][0] = '\0';
	}

	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_none ) );
}

class CException;
//-----------------------------------------------------------------------------
// Purpose: Read the Command Menu structure from the txt file and create the menu.
//			Returns Index of menu in m_pCommandMenus
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Creates all the class choices under a spy's disguise menus, and
//			maps a command to them
// Output : CCommandMenu
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pButtonText -
//			*pButtonName -
// Output : CommandButton
//-----------------------------------------------------------------------------

void TeamFortressViewport::ShowScoreBoard( void )
{
	if ( m_pScoreBoard )
	{
		// No Scoreboard in single-player
		if ( gEngfuncs.GetMaxClients() > 1 )
		{
			m_pScoreBoard->Open();
			UpdateCursorState();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the scoreboard is up
//-----------------------------------------------------------------------------
bool TeamFortressViewport::IsScoreBoardVisible( void )
{
	if ( m_pScoreBoard )
		return m_pScoreBoard->isVisible();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Hide the scoreboard
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideScoreBoard( void )
{
	// Prevent removal of scoreboard during intermission
	if ( gHUD.m_iIntermission )
		return;

	if ( m_pScoreBoard )
	{
		m_pScoreBoard->setVisible( false );

		GetClientVoiceMgr()->StopSquelchMode();

		UpdateCursorState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activate's the player special ability
//			called when the player hits their "special" key
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputPlayerSpecial( void )
{
	if ( !m_iInitialized )
		return;

#ifdef _TFC
	if ( g_iPlayerClass == PC_ENGINEER || g_iPlayerClass == PC_SPY )
	{
		ShowCommandMenu( gViewPort->m_StandardMenu );

		if ( m_pCurrentCommandMenu )
		{
			m_pCurrentCommandMenu->KeyInput( '7' );
		}
	}
	else
#endif
	{
		// if it's any other class, just send the command down to the server
		EngineClientCmd( "_special" );
	}
}

// Set the submenu of the Command Menu

void TeamFortressViewport::UpdateSpectatorPanel()
{
	m_iUser1 = g_iUser1;
	m_iUser2 = g_iUser2;
	m_iUser3 = g_iUser3;

	if ( !m_pSpectatorPanel )
		return;

	if ( g_iUser1 && gHUD.m_pCvarDraw->value && !gHUD.m_iIntermission ) // don't draw in dev_overview mode
	{
		char bottomText[128];
		char helpString2[128];
		char tempString[128];
		char *name;
		char *pBottomText = NULL;
		int player        = 0;

		// check if spectator combinations are still valid
		gHUD.m_Spectator.CheckSettings();

		if ( !m_pSpectatorPanel->isVisible() )
		{
			m_pSpectatorPanel->setVisible( true ); // show spectator panel, but
			m_pSpectatorPanel->ShowMenu( false );  // dsiable all menus/buttons

			_snprintf( tempString, sizeof( tempString ) - 1, "%c%s", HUD_PRINTCENTER, CHudTextMessage::BufferedLocaliseTextString( "#Spec_Duck" ) );
			tempString[sizeof( tempString ) - 1] = '\0';

			gHUD.m_TextMessage.MsgFunc_TextMsg( NULL, strlen( tempString ) + 1, tempString );
		}

		snprintf( bottomText, sizeof( bottomText ), "#Spec_Mode%d", g_iUser1 );
		snprintf( helpString2, sizeof( helpString2 ), "#Spec_Mode%d", g_iUser1 );

		if ( gEngfuncs.IsSpectateOnly() )
			strncat( helpString2, " - HLTV", sizeof( helpString2 ) - strlen( helpString2 ) - 1 );

		// check if we're locked onto a target, show the player's name
		if ( ( g_iUser2 > 0 ) && ( g_iUser2 <= gEngfuncs.GetMaxClients() ) && ( g_iUser1 != OBS_ROAMING ) )
		{
			player = g_iUser2;
		}

		// special case in free map and inset off, don't show names
		if ( ( g_iUser1 == OBS_MAP_FREE ) && !gHUD.m_Spectator.m_pip->value )
			name = NULL;
		else
			name = g_PlayerInfoList[player].name;

		// create player & health string
		if ( player && name )
		{
			strncpy( bottomText, name, sizeof( bottomText ) );
			bottomText[sizeof( bottomText ) - 1] = 0;
			pBottomText                          = bottomText;
		}
		else
		{
			pBottomText = CHudTextMessage::BufferedLocaliseTextString( bottomText );
		}

		// in first person mode colorize player names
		if ( ( g_iUser1 == OBS_IN_EYE ) && player )
		{
			float *color = GetClientColor( player );
			int r        = color[0] * 255;
			int g        = color[1] * 255;
			int b        = color[2] * 255;

			// set team color, a bit transparent
			m_pSpectatorPanel->m_BottomMainLabel->setFgColor( r, g, b, 0 );
			m_pSpectatorPanel->m_BottomMainButton->setFgColor( r, g, b, 0 );
		}
		else
		{ // restore GUI color
			m_pSpectatorPanel->m_BottomMainLabel->setFgColor( 143, 143, 54, 0 );
			m_pSpectatorPanel->m_BottomMainButton->setFgColor( 143, 143, 54, 0 );
		}

		// add sting auto if we are in auto directed mode
		if ( gHUD.m_Spectator.m_autoDirector->value )
		{
			char tempString[128];
			snprintf( tempString, sizeof( tempString ), "#Spec_Auto %s", helpString2 );
			strncpy( helpString2, tempString, sizeof( helpString2 ) - 1 );
			helpString2[sizeof( helpString2 ) - 1] = '\0';
		}

		m_pSpectatorPanel->m_BottomMainLabel->setText( "%s", pBottomText );
		m_pSpectatorPanel->m_BottomMainButton->setText( pBottomText );

		// update extra info field
		char szText[64];

		if ( gEngfuncs.IsSpectateOnly() )
		{
			// in HLTV mode show number of spectators
			_snprintf( szText, 63, "%s: %d", CHudTextMessage::BufferedLocaliseTextString( "#Spectators" ), gHUD.m_Spectator.m_iSpectatorNumber );
		}
		else
		{
			// otherwise show map name
			char szMapName[64];
			COM_FileBase( gEngfuncs.pfnGetLevelName(), szMapName );

			_snprintf( szText, 63, "%s: %s", CHudTextMessage::BufferedLocaliseTextString( "#Spec_Map" ), szMapName );
		}

		szText[63] = 0;

		m_pSpectatorPanel->m_ExtraInfo->setText( szText );

		/*
		int timer = (int)( gHUD.m_roundTimer.m_flTimeEnd - gHUD.m_flTime );

		if ( timer < 0 )
		     timer	= 0;

		_snprintf ( szText, 63, "%d:%02d\n", (timer / 60), (timer % 60) );

		szText[63] = 0;

		m_pSpectatorPanel->m_CurrentTime->setText( szText ); */

		// update spectator panel
		gViewPort->m_pSpectatorPanel->Update();
	}
	else
	{
		if ( m_pSpectatorPanel->isVisible() )
		{
			m_pSpectatorPanel->setVisible( false );
			m_pSpectatorPanel->ShowMenu( false ); // dsiable all menus/buttons
		}
	}

	m_flSpectatorPanelLastUpdated = gHUD.m_flTime + 1.0; // update every second
}

//======================================================================
void TeamFortressViewport::CreateScoreBoard( void )
{
	int xdent = SBOARD_INDENT_X, ydent = SBOARD_INDENT_Y;
	if ( ScreenWidth == 512 )
	{
		xdent = SBOARD_INDENT_X_512;
		ydent = SBOARD_INDENT_Y_512;
	}
	else if ( ScreenWidth == 400 )
	{
		xdent = SBOARD_INDENT_X_400;
		ydent = SBOARD_INDENT_Y_400;
	}

	m_pScoreBoard = new ScorePanel( xdent, ydent, ScreenWidth - ( xdent * 2 ), ScreenHeight - ( ydent * 2 ) );
	m_pScoreBoard->setParent( this );
	m_pScoreBoard->setVisible( false );
}

void TeamFortressViewport::UpdateOnPlayerInfo()
{
	if ( m_pTeamMenu )
		m_pTeamMenu->Update();
	if ( m_pClassMenu )
		m_pClassMenu->Update();
	if ( m_pScoreBoard )
		m_pScoreBoard->Update();
}

void TeamFortressViewport::UpdateCursorState()
{
	// Need cursor if any VGUI window is up
	if ( m_pSpectatorPanel->m_menuVisible || m_pCurrentMenu || m_pTeamMenu->isVisible() || m_pServerBrowser->isVisible() || GetClientVoiceMgr()->IsInSquelchMode() )
	{
		g_iVisibleMouse = true;
		App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_arrow ) );
		return;
	}
	else if ( m_pCurrentCommandMenu )
	{
		// commandmenu doesn't have cursor if hud_capturemouse is turned off
		if ( gHUD.m_pCvarStealMouse->value != 0.0f )
		{
			g_iVisibleMouse = true;
			App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_arrow ) );
			return;
		}
	}

	// Don't reset mouse in demo playback
	if ( !gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		IN_ResetMouse();
	}

	if ( g_iVisibleMouse )
	{
		// Clear any residual input so our camera doesn't jerk when dismissing the UI
		IN_ResetRelativeMouseState();
	}

	g_iVisibleMouse = false;
	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_none ) );
}

void TeamFortressViewport::UpdateHighlights()
{
	if ( m_pCurrentCommandMenu )
		m_pCurrentCommandMenu->MakeVisible( NULL );
}

void TeamFortressViewport::GetAllPlayersInfo( void )
{
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		gEngfuncs.pfnGetPlayerInfo( i, &g_PlayerInfoList[i] );

		if ( g_PlayerInfoList[i].thisplayer )
			m_pScoreBoard->m_iPlayerNum = i; // Initialize active local player slot index
	}
}

void TeamFortressViewport::paintBackground()
{
	int wide, tall;
	getParent()->getSize( wide, tall );
	setSize( wide, tall );
	if ( m_pScoreBoard )
	{
		int x, y;
		getApp()->getCursorPos( x, y );
		m_pScoreBoard->cursorMoved( x, y, m_pScoreBoard );
	}

	// See if the command menu is visible and needs recalculating due to some external change
	if ( g_iTeamNumber != m_iCurrentTeamNumber )
	{
		UpdateCommandMenu( m_StandardMenu );

		if ( m_pClassMenu )
		{
			m_pClassMenu->Update();
		}

		m_iCurrentTeamNumber = g_iTeamNumber;
	}

	if ( g_iPlayerClass != m_iCurrentPlayerClass )
	{
		UpdateCommandMenu( m_StandardMenu );

		m_iCurrentPlayerClass = g_iPlayerClass;
	}

	// See if the Spectator Menu needs to be update
	if ( ( g_iUser1 != m_iUser1 || g_iUser2 != m_iUser2 ) ||
	     ( m_flSpectatorPanelLastUpdated < gHUD.m_flTime ) )
	{
		UpdateSpectatorPanel();
	}

	// Update the Scoreboard, if it's visible
	if ( m_pScoreBoard->isVisible() && ( m_flScoreBoardLastUpdated < gHUD.m_flTime ) )
	{
		m_pScoreBoard->Update();
		m_flScoreBoardLastUpdated = gHUD.m_flTime + 0.5;
	}

	int extents[4];
	getAbsExtents( extents[0], extents[1], extents[2], extents[3] );
	VGui_ViewportPaintBackground( extents );
}

//================================================================
// Input Handler for Drag N Drop panels
void CDragNDropHandler::cursorMoved( int x, int y, Panel *panel )
{
	if ( m_bDragging )
	{
		App::getInstance()->getCursorPos( x, y );
		m_pPanel->setPos( m_iaDragOrgPos[0] + ( x - m_iaDragStart[0] ), m_iaDragOrgPos[1] + ( y - m_iaDragStart[1] ) );

		if ( m_pPanel->getParent() != null )
		{
			m_pPanel->getParent()->repaint();
		}
	}
}

void CDragNDropHandler::mousePressed( MouseCode code, Panel *panel )
{
	int x, y;
	App::getInstance()->getCursorPos( x, y );
	m_bDragging      = true;
	m_iaDragStart[0] = x;
	m_iaDragStart[1] = y;
	m_pPanel->getPos( m_iaDragOrgPos[0], m_iaDragOrgPos[1] );
	App::getInstance()->setMouseCapture( panel );

	m_pPanel->setDragged( m_bDragging );
	m_pPanel->requestFocus();
}

void CDragNDropHandler::mouseReleased( MouseCode code, Panel *panel )
{
	m_bDragging = false;
	m_pPanel->setDragged( m_bDragging );
	App::getInstance()->setMouseCapture( null );
}

//================================================================
// Number Key Input
bool TeamFortressViewport::SlotInput( int iSlot )
{
	// If there's a menu up, give it the input
	if ( m_pCurrentMenu )
		return m_pCurrentMenu->SlotInput( iSlot );

	return FALSE;
}

// Direct Key Input
int TeamFortressViewport::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	// Enter gets out of Spectator Mode by bringing up the Team Menu
	if ( m_iUser1 && gEngfuncs.Con_IsVisible() == false )
	{
		if ( down && ( keynum == K_ENTER || keynum == K_KP_ENTER ) )
			ShowVGUIMenu( MENU_TEAM );
	}

	// Open Text Window?
	if ( m_pCurrentMenu && gEngfuncs.Con_IsVisible() == false )
	{
		int iMenuID = m_pCurrentMenu->GetMenuID();

		// Get number keys as Input for Team/Class menus
		if ( iMenuID == MENU_TEAM || iMenuID == MENU_CLASS )
		{
			// Escape gets you out of Team/Class menus if the Cancel button is visible
			if ( keynum == K_ESCAPE )
			{
				if ( ( iMenuID == MENU_TEAM && g_iTeamNumber ) || ( iMenuID == MENU_CLASS && g_iPlayerClass ) )
				{
					HideTopMenu();
					return 0;
				}
			}

			for ( int i = '0'; i <= '9'; i++ )
			{
				if ( down && ( keynum == i ) )
				{
					SlotInput( i - '0' );
					return 0;
				}
			}
		}

		// Grab enter keys to close TextWindows
		if ( down && ( keynum == K_ENTER || keynum == K_KP_ENTER || keynum == K_SPACE || keynum == K_ESCAPE ) )
		{
			if ( iMenuID == MENU_MAPBRIEFING || iMenuID == MENU_INTRO || iMenuID == MENU_CLASSHELP )
			{
				HideTopMenu();
				return 0;
			}
		}

		// Grab jump key on Team Menu as autoassign
		if ( pszCurrentBinding && down && !strcmp( pszCurrentBinding, "+jump" ) )
		{
			if ( iMenuID == MENU_TEAM )
			{
				m_pTeamMenu->SlotInput( 5 );
				return 0;
			}
		}
	}

	// if we're in a command menu, try hit one of it's buttons
	if ( down && m_pCurrentCommandMenu )
	{
		// Escape hides the command menu
		if ( keynum == K_ESCAPE )
		{
			HideCommandMenu();
			return 0;
		}

		// only trap the number keys
		if ( keynum >= '0' && keynum <= '9' )
		{
			if ( m_pCurrentCommandMenu->KeyInput( keynum ) )
			{
				// a final command has been issued, so close the command menu
				HideCommandMenu();
			}

			return 0;
		}
	}

	return 1;
}

//================================================================
// Message Handlers
