//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// Purpose: Client DLL VGUI Viewport Modular Subsystems
//
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

using namespace vgui;

extern int g_iVisibleMouse;
extern int g_iPlayerClass;
extern int g_iTeamNumber;
extern int g_iUser1;
extern int g_iUser2;
extern int g_iUser3;

void IN_ResetMouse( void );
void IN_ResetRelativeMouseState( void );
extern CMenuPanel *CMessageWindowPanel_Create( const char *szMOTD, const char *szTitle, int iShadeFullscreen, int iRemoveMe, int x, int y, int wide, int tall );
extern float *GetClientColor( int clientIndex );
void TeamFortressViewport::ToggleServerBrowser()
{
	if ( !m_iInitialized )
		return;

	if ( !m_pServerBrowser )
		return;

	if ( m_pServerBrowser->isVisible() )
	{
		m_pServerBrowser->setVisible( false );
	}
	else
	{
		m_pServerBrowser->setVisible( true );
	}

	UpdateCursorState();
}

//=======================================================================
void TeamFortressViewport::ShowCommandMenu( int menuIndex )
{
	if ( !m_iInitialized )
		return;

	// Already have a menu open.
	if ( m_pCurrentMenu )
		return;

	// is the command menu open?
	if ( m_pCurrentCommandMenu == m_pCommandMenus[menuIndex] )
	{
		HideCommandMenu();
		return;
	}

	// Not visible while in intermission
	if ( gHUD.m_iIntermission )
		return;

	// Recalculate visible menus
	UpdateCommandMenu( menuIndex );
	HideVGUIMenu();

	SetCurrentCommandMenu( m_pCommandMenus[menuIndex] );
	m_flMenuOpenTime = gHUD.m_flTime;
	UpdateCursorState();

	// get command menu parameters
	for ( int i = 2; i < gEngfuncs.Cmd_Argc(); i++ )
	{
		const char *param = gEngfuncs.Cmd_Argv( i - 1 );
		if ( param )
		{
			if ( m_pCurrentCommandMenu->KeyInput( param[0] ) )
			{
				// kill the menu open time, since the key input is final
				HideCommandMenu();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the key input of "-commandmenu"
// Input  :
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputSignalHideCommandMenu()
{
	if ( !m_iInitialized )
		return;

	// if they've just tapped the command menu key, leave it open
	if ( ( m_flMenuOpenTime + 0.3 ) > gHUD.m_flTime )
		return;

	HideCommandMenu();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the command menu
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideCommandMenu()
{
	if ( !m_iInitialized )
		return;

	if ( m_pCommandMenus[m_StandardMenu] )
	{
		m_pCommandMenus[m_StandardMenu]->ClearButtonsOfArmedState();
	}

	if ( m_pCommandMenus[m_SpectatorOptionsMenu] )
	{
		m_pCommandMenus[m_SpectatorOptionsMenu]->ClearButtonsOfArmedState();
	}

	if ( m_pCommandMenus[m_SpectatorCameraMenu] )
	{
		m_pCommandMenus[m_SpectatorCameraMenu]->ClearButtonsOfArmedState();
	}

	if ( m_pCommandMenus[m_PlayerMenu] )
	{
		m_pCommandMenus[m_PlayerMenu]->ClearButtonsOfArmedState();
	}

	m_flMenuOpenTime = 0.0f;
	SetCurrentCommandMenu( NULL );
	UpdateCursorState();
}

//-----------------------------------------------------------------------------
// Purpose: Bring up the scoreboard
//-----------------------------------------------------------------------------

void TeamFortressViewport::SetCurrentCommandMenu( CCommandMenu *pNewMenu )
{
	for ( int i = 0; i < m_iNumMenus; i++ )
		m_pCommandMenus[i]->setVisible( false );

	m_pCurrentCommandMenu = pNewMenu;

	if ( m_pCurrentCommandMenu )
		m_pCurrentCommandMenu->MakeVisible( NULL );
}

void TeamFortressViewport::UpdateCommandMenu( int menuIndex )
{
	// if its the player menu update the player list
	if ( menuIndex == m_PlayerMenu )
	{
		m_pCommandMenus[m_PlayerMenu]->RemoveAllButtons();
		UpdatePlayerMenu( m_PlayerMenu );
	}

	m_pCommandMenus[menuIndex]->RecalculateVisibles( 0, false );
	m_pCommandMenus[menuIndex]->RecalculatePositions( 0 );
}

void TeamFortressViewport::UpdatePlayerMenu( int menuIndex )
{

	cl_entity_t *pEnt = NULL;
	float flLabelSize = ( ( ScreenWidth - ( XRES( CAMOPTIONS_BUTTON_X ) + 15 ) ) - XRES( 24 + 15 ) ) - XRES( ( 15 + OPTIONS_BUTTON_X + 15 ) + 38 );
	gViewPort->GetAllPlayersInfo();

	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		// if ( g_PlayerInfoList[i].name == NULL )
		//	continue; // empty player slot, skip

		pEnt = gEngfuncs.GetEntityByIndex( i );

		if ( !gHUD.m_Spectator.IsActivePlayer( pEnt ) )
			continue;

		// if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
		//	continue; // skip over players who are not in a team

		SpectButton *pButton = new SpectButton( 1, g_PlayerInfoList[pEnt->index].name, XRES( ( 15 + OPTIONS_BUTTON_X + 15 ) + 31 ), PANEL_HEIGHT + ( i - 1 ) * CMENU_SIZE_X, flLabelSize, BUTTON_SIZE_Y / 2 );

		pButton->setBoundKey( (char)255 );
		pButton->setContentAlignment( vgui::Label::a_center );
		m_pCommandMenus[menuIndex]->AddButton( pButton );
		pButton->setParentMenu( m_pCommandMenus[menuIndex] );

		// Override font in CommandMenu
		pButton->setFont( Scheme::sf_primary3 );

		pButton->addActionSignal( new CMenuHandler_SpectateFollow( g_PlayerInfoList[pEnt->index].name ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCommandMenus[menuIndex] ) );
	}
}

void COM_FileBase( const char *in, char *out );

void TeamFortressViewport::CreateServerBrowser( void )
{
	m_pServerBrowser = new ServerBrowser( 0, 0, ScreenWidth, ScreenHeight );
	m_pServerBrowser->setParent( this );
	m_pServerBrowser->setVisible( false );
}

//======================================================================
// Set the VGUI Menu
void TeamFortressViewport::SetCurrentMenu( CMenuPanel *pMenu )
{
	m_pCurrentMenu = pMenu;
	if ( m_pCurrentMenu )
	{
		// Don't open menus in demo playback
		if ( gEngfuncs.pDemoAPI->IsPlayingback() )
			return;

		m_pCurrentMenu->Open();
	}
	else
	{
		gEngfuncs.pfnClientCmd( "closemenus;" );
	}
}

//================================================================
// Text Window
CMenuPanel *TeamFortressViewport::CreateTextWindow( int iTextToShow )
{
	char sz[256];
	char *cText;
	char *pfile                       = NULL;
	static const int MAX_TITLE_LENGTH = 64;
	char cTitle[MAX_TITLE_LENGTH];

	if ( iTextToShow == SHOW_MOTD )
	{
		if ( !m_szServerName || !m_szServerName[0] )
		{
			strncpy( cTitle, "Half-Life", sizeof( cTitle ) - 1 );
			cTitle[sizeof( cTitle ) - 1] = '\0';
		}
		else
			strncpy( cTitle, m_szServerName, sizeof( cTitle ) );
		cTitle[sizeof( cTitle ) - 1] = 0;
		cText                        = m_szMOTD;
	}
	else if ( iTextToShow == SHOW_MAPBRIEFING )
	{
		// Get the current mapname, and open it's map briefing text
		if ( m_sMapName && m_sMapName[0] )
		{
			snprintf( sz, sizeof( sz ), "maps/%s.txt", m_sMapName );
		}
		else
		{
			const char *level = gEngfuncs.pfnGetLevelName();
			if ( !level )
				return NULL;

			strncpy( sz, level, sizeof( sz ) - 1 );
			sz[sizeof( sz ) - 1] = '\0';
			char *ch = strchr( sz, '.' );
			*ch      = '\0';
			strncat( sz, ".txt", sizeof( sz ) - strlen( sz ) - 1 );

			// pull out the map name
			strncpy( m_sMapName, level, sizeof( m_sMapName ) - 1 );
			m_sMapName[sizeof( m_sMapName ) - 1] = '\0';
			ch = strchr( m_sMapName, '.' );
			if ( ch )
			{
				*ch = 0;
			}

			ch = strchr( m_sMapName, '/' );
			if ( ch )
			{
				// move the string back over the '/'
				memmove( m_sMapName, ch + 1, strlen( ch ) + 1 );
			}
		}

		pfile = (char *)gEngfuncs.COM_LoadFile( sz, 5, NULL );

		if ( !pfile )
			return NULL;

		cText = pfile;

		strncpy( cTitle, m_sMapName, MAX_TITLE_LENGTH );
		cTitle[MAX_TITLE_LENGTH - 1] = 0;
	}
#ifdef _TFC
	else if ( iTextToShow == SHOW_CLASSDESC )
	{
		switch ( g_iPlayerClass )
		{
		case PC_SCOUT:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_scout" );
			CHudTextMessage::LocaliseTextString( "#Title_scout", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_SNIPER:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_sniper" );
			CHudTextMessage::LocaliseTextString( "#Title_sniper", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_SOLDIER:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_soldier" );
			CHudTextMessage::LocaliseTextString( "#Title_soldier", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_DEMOMAN:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_demoman" );
			CHudTextMessage::LocaliseTextString( "#Title_demoman", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_MEDIC:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_medic" );
			CHudTextMessage::LocaliseTextString( "#Title_medic", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_HVYWEAP:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_hwguy" );
			CHudTextMessage::LocaliseTextString( "#Title_hwguy", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_PYRO:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_pyro" );
			CHudTextMessage::LocaliseTextString( "#Title_pyro", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_SPY:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_spy" );
			CHudTextMessage::LocaliseTextString( "#Title_spy", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_ENGINEER:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_engineer" );
			CHudTextMessage::LocaliseTextString( "#Title_engineer", cTitle, MAX_TITLE_LENGTH );
			break;
		case PC_CIVILIAN:
			cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_civilian" );
			CHudTextMessage::LocaliseTextString( "#Title_civilian", cTitle, MAX_TITLE_LENGTH );
			break;
		default:
			return NULL;
		}

		if ( g_iPlayerClass == PC_CIVILIAN )
		{
			snprintf( sz, sizeof( sz ), "classes/long_civilian.txt" );
		}
		else
		{
			snprintf( sz, sizeof( sz ), "classes/long_%s.txt", sTFClassSelection[g_iPlayerClass] );
		}
		char *pfile = (char *)gEngfuncs.COM_LoadFile( sz, 5, NULL );
		if ( pfile )
		{
			cText = pfile;
		}
	}
#endif
	else if ( iTextToShow == SHOW_SPECHELP )
	{
		CHudTextMessage::LocaliseTextString( "#Spec_Help_Title", cTitle, MAX_TITLE_LENGTH );
		cTitle[MAX_TITLE_LENGTH - 1] = 0;

		char *pfile = CHudTextMessage::BufferedLocaliseTextString( "#Spec_Help_Text" );
		if ( pfile )
		{
			cText = pfile;
		}
	}

	// if we're in the game (ie. have selected a class), flag the menu to be only grayed in the dialog box, instead of full screen
	CMenuPanel *pMOTDPanel = CMessageWindowPanel_Create( cText, cTitle, g_iPlayerClass == PC_UNDEFINED, false, 0, 0, ScreenWidth, ScreenHeight );
	pMOTDPanel->setParent( this );

	if ( pfile )
		gEngfuncs.COM_FreeFile( pfile );

	return pMOTDPanel;
}

//================================================================
// VGUI Menus
void TeamFortressViewport::ShowVGUIMenu( int iMenu )
{
	CMenuPanel *pNewMenu = NULL;

	// Don't open menus in demo playback
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return;

	// Don't open any menus except the MOTD during intermission
	// MOTD needs to be accepted because it's sent down to the client
	// after map change, before intermission's turned off
	if ( gHUD.m_iIntermission && iMenu != MENU_INTRO )
		return;

	// Don't create one if it's already in the list
	if ( m_pCurrentMenu )
	{
		CMenuPanel *pMenu = m_pCurrentMenu;
		while ( pMenu != NULL )
		{
			if ( pMenu->GetMenuID() == iMenu )
				return;
			pMenu = pMenu->GetNextMenu();
		}
	}

	switch ( iMenu )
	{
	case MENU_TEAM:
		pNewMenu = ShowTeamMenu();
		break;

	// Map Briefing removed now that it appears in the team menu
	case MENU_MAPBRIEFING:
		pNewMenu = CreateTextWindow( SHOW_MAPBRIEFING );
		break;

	case MENU_INTRO:
		pNewMenu = CreateTextWindow( SHOW_MOTD );
		break;

	case MENU_CLASSHELP:
		pNewMenu = CreateTextWindow( SHOW_CLASSDESC );
		break;

	case MENU_SPECHELP:
		pNewMenu = CreateTextWindow( SHOW_SPECHELP );
		break;
	case MENU_CLASS:
		pNewMenu = ShowClassMenu();
		break;

	default:
		break;
	}

	if ( !pNewMenu )
		return;

	// Close the Command Menu if it's open
	HideCommandMenu();

	pNewMenu->SetMenuID( iMenu );
	pNewMenu->SetActive( true );
	pNewMenu->setParent( this );

	// See if another menu is visible, and if so, cache this one for display once the other one's finished
	if ( m_pCurrentMenu )
	{
		if ( m_pCurrentMenu->GetMenuID() == MENU_CLASS && iMenu == MENU_TEAM )
		{
			CMenuPanel *pPrevMenu = m_pCurrentMenu;
			m_pCurrentMenu->Close();
			m_pCurrentMenu = pNewMenu;
			m_pCurrentMenu->SetNextMenu( pPrevMenu );
			m_pCurrentMenu->Open();
			UpdateCursorState();
		}
		else
		{
			m_pCurrentMenu->SetNextMenu( pNewMenu );
		}
	}
	else
	{
		m_pCurrentMenu = pNewMenu;
		m_pCurrentMenu->Open();
		UpdateCursorState();
	}
}

// Removes all VGUI Menu's onscreen
void TeamFortressViewport::HideVGUIMenu()
{
	while ( m_pCurrentMenu )
	{
		HideTopMenu();
	}
}

// Remove the top VGUI menu, and bring up the next one
void TeamFortressViewport::HideTopMenu()
{
	if ( m_pCurrentMenu )
	{
		// Close the top one
		m_pCurrentMenu->Close();

		// Bring up the next one
		gViewPort->SetCurrentMenu( m_pCurrentMenu->GetNextMenu() );
	}

	UpdateCursorState();
}

// Return TRUE if the HUD's allowed to print text messages
bool TeamFortressViewport::AllowedToPrintText( void )
{
	// Prevent text messages when fullscreen menus are up
	if ( m_pCurrentMenu && g_iPlayerClass == 0 )
	{
		int iId = m_pCurrentMenu->GetMenuID();
		if ( iId == MENU_TEAM || iId == MENU_CLASS || iId == MENU_INTRO || iId == MENU_CLASSHELP )
			return FALSE;
	}

	return TRUE;
}

//======================================================================================
// TEAM MENU
//======================================================================================
// Bring up the Team selection Menu
CMenuPanel *TeamFortressViewport::ShowTeamMenu()
{
	// Don't open menus in demo playback
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return NULL;

	m_pTeamMenu->Reset();
	return m_pTeamMenu;
}

void TeamFortressViewport::CreateTeamMenu()
{
	// Create the panel
	m_pTeamMenu = new CTeamMenuPanel( 100, false, 0, 0, ScreenWidth, ScreenHeight );
	m_pTeamMenu->setParent( this );
	m_pTeamMenu->setVisible( false );
}

//======================================================================================
// CLASS MENU
//======================================================================================
// Bring up the Class selection Menu
CMenuPanel *TeamFortressViewport::ShowClassMenu()
{
	// Don't open menus in demo playback
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return NULL;

	m_pClassMenu->Reset();
	return m_pClassMenu;
}

void TeamFortressViewport::CreateClassMenu()
{
	// Create the panel
	m_pClassMenu = new CClassMenuPanel( 100, false, 0, 0, ScreenWidth, ScreenHeight );
	m_pClassMenu->setParent( this );
	m_pClassMenu->setVisible( false );
}

//======================================================================================
//======================================================================================
// SPECTATOR MENU
//======================================================================================
// Spectator "Menu" explaining the Spectator buttons
void TeamFortressViewport::CreateSpectatorMenu()
{
	// Create the Panel
	m_pSpectatorPanel = new SpectatorPanel( 0, 0, ScreenWidth, ScreenHeight );
	m_pSpectatorPanel->setParent( this );
	m_pSpectatorPanel->setVisible( false );
	m_pSpectatorPanel->Initialize();
}

//======================================================================================
// UPDATE HUD SECTIONS
//======================================================================================
// We've got an update on player info
// Recalculate any menus that use it.
