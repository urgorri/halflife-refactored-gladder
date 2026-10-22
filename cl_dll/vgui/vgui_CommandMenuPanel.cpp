#include "../hud/hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_CommandMenuPanel.h"
#include <VGUI_Button.h>

using namespace vgui;

class CException;

void CCommandMenu::AddButton( CommandButton *pButton )
{
	if ( m_iButtons >= MAX_BUTTONS )
		return;

	m_aButtons[m_iButtons] = pButton;
	m_iButtons++;
	pButton->setParent( this );
	pButton->setFont( Scheme::sf_primary3 );

	// give the button a default key binding
	if ( m_iButtons < 10 )
	{
		pButton->setBoundKey( m_iButtons + '0' );
	}
	else if ( m_iButtons == 10 )
	{
		pButton->setBoundKey( '0' );
	}
}

void CCommandMenu::RemoveAllButtons( void )
{
	/*
	for(int i=0;i<m_iButtons;i++)
	{
	    CommandButton *pTemp = m_aButtons[i];
	    m_aButtons[i] = NULL;

	    pTemp
	    if(pTemp)
	    {
	        delete(pTemp);
	    }

	}
	*/
	removeAllChildren();
	m_iButtons = 0;
}

bool CCommandMenu::KeyInput( int keyNum )
{
	// loop through all our buttons looking for one bound to keyNum
	for ( int i = 0; i < m_iButtons; i++ )
	{
		if ( !m_aButtons[i]->IsNotValid() )
		{
			if ( m_aButtons[i]->getBoundKey() == keyNum )
			{
				// hit the button
				if ( m_aButtons[i]->GetSubMenu() )
				{
					// open the sub menu
					gViewPort->SetCurrentCommandMenu( m_aButtons[i]->GetSubMenu() );
					return false;
				}
				else
				{
					// run the bound command
					m_aButtons[i]->fireActionSignal();
					return true;
				}
			}
		}
	}

	return false;
}

void CCommandMenu::ClearButtonsOfArmedState( void )
{
	for ( int i = 0; i < GetNumButtons(); i++ )
	{
		m_aButtons[i]->setArmed( false );

		if ( m_aButtons[i]->GetSubMenu() )
		{
			m_aButtons[i]->GetSubMenu()->ClearButtonsOfArmedState();
		}
	}
}

CommandButton *CCommandMenu::FindButtonWithSubmenu( CCommandMenu *pSubMenu )
{
	for ( int i = 0; i < GetNumButtons(); i++ )
	{
		if ( m_aButtons[i]->GetSubMenu() == pSubMenu )
			return m_aButtons[i];
	}

	return NULL;
}

bool CCommandMenu::RecalculateVisibles( int iYOffset, bool bHideAll )
{
	int i, iCurrentY = 0;
	int iVisibleButtons = 0;

	// Cycle through all the buttons in this menu, and see which will be visible
	for ( i = 0; i < m_iButtons; i++ )
	{
		int iClass = m_aButtons[i]->GetPlayerClass();

		if ( ( iClass && iClass != g_iPlayerClass ) || ( m_aButtons[i]->IsNotValid() ) || bHideAll )
		{
			m_aButtons[i]->setVisible( false );
			if ( m_aButtons[i]->GetSubMenu() != NULL )
			{
				( m_aButtons[i]->GetSubMenu() )->RecalculateVisibles( 0, true );
			}
		}
		else
		{
			// If it's got a submenu, force it to check visibilities
			if ( m_aButtons[i]->GetSubMenu() != NULL )
			{
				if ( !( m_aButtons[i]->GetSubMenu() )->RecalculateVisibles( 0, false ) )
				{
					// The submenu had no visible buttons, so don't display this button
					m_aButtons[i]->setVisible( false );
					continue;
				}
			}

			m_aButtons[i]->setVisible( true );
			iVisibleButtons++;
		}
	}

	// Set Size
	setSize( _size[0], ( iVisibleButtons * ( m_flButtonSizeY - 1 ) ) + 1 );

	if ( iYOffset )
	{
		m_iYOffset = iYOffset;
	}

	for ( i = 0; i < m_iButtons; i++ )
	{
		if ( m_aButtons[i]->isVisible() )
		{
			if ( m_aButtons[i]->GetSubMenu() != NULL )
				( m_aButtons[i]->GetSubMenu() )->RecalculateVisibles( iCurrentY + m_iYOffset, false );

			// Make sure it's at the right Y position
			// m_aButtons[i]->getPos( iXPos, iYPos );

			if ( m_iDirection )
			{
				m_aButtons[i]->setPos( 0, ( iVisibleButtons - 1 ) * ( m_flButtonSizeY - 1 ) - iCurrentY );
			}
			else
			{
				m_aButtons[i]->setPos( 0, iCurrentY );
			}

			iCurrentY += ( m_flButtonSizeY - 1 );
		}
	}

	return iVisibleButtons ? true : false;
}

void CCommandMenu::RecalculatePositions( int iYOffset )
{
	int iTop;
	int iAdjust = 0;

	m_iYOffset += iYOffset;

	if ( m_iDirection )
		iTop = ScreenHeight - ( m_iYOffset + _size[1] );
	else
		iTop = m_iYOffset;

	if ( iTop < 0 )
		iTop = 0;

	// Calculate if this is going to fit onscreen, and shuffle it up if it won't
	int iBottom = iTop + _size[1];

	if ( iBottom > ScreenHeight )
	{
		// Move in increments of button sizes
		while ( iAdjust < ( iBottom - ScreenHeight ) )
		{
			iAdjust += m_flButtonSizeY - 1;
		}

		iTop -= iAdjust;

		// Make sure it doesn't move off the top of the screen (the menu's too big to fit it all)
		if ( iTop < 0 )
		{
			iAdjust -= ( 0 - iTop );
			iTop = 0;
		}
	}

	setPos( _pos[0], iTop );

	// We need to force all menus below this one to update their positions now, because they
	// might have submenus riding off buttons in this menu that have just shifted.
	for ( int i = 0; i < m_iButtons; i++ )
		m_aButtons[i]->UpdateSubMenus( iAdjust );
}

void CCommandMenu::MakeVisible( CCommandMenu *pChildMenu )
{
	/*
	    // Push down the button leading to the child menu
	    for (int i = 0; i < m_iButtons; i++)
	    {
	        if ( (pChildMenu != NULL) && (m_aButtons[i]->GetSubMenu() == pChildMenu) )
	        {
	            m_aButtons[i]->setArmed( true );
	        }
	        else
	        {
	            m_aButtons[i]->setArmed( false );
	        }
	    }
	*/

	setVisible( true );
	if ( m_pParentMenu )
		m_pParentMenu->MakeVisible( this );
}

void CCommandMenu::paintBackground()
{
	// Transparent black background

	if ( m_iSpectCmdMenu )
		drawSetColor( 0, 0, 0, 64 );
	else
		drawSetColor( Scheme::sc_primary3 );

	drawFilledRect( 0, 0, _size[0], _size[1] );
}

void CMenuHandler_StringCommandClassSelect::actionPerformed( Panel *panel )
{
	CMenuHandler_StringCommand::actionPerformed( panel );

	// THIS IS NOW BEING DONE ON THE TFC SERVER TO AVOID KILLING SOMEONE THEN
	// HAVE THE SERVER SAY "SORRY...YOU CAN'T BE THAT CLASS".

#if !defined _TFC
	bool bAutoKill = CVAR_GET_FLOAT( "hud_classautokill" ) != 0;
	if ( bAutoKill && g_iPlayerClass != 0 )
		gEngfuncs.pfnClientCmd( "kill" );
#endif
}

int TeamFortressViewport::CreateCommandMenu( char *menuFile, int direction, int yOffset, bool flatDesign, float flButtonSizeX, float flButtonSizeY, int xOffset )
{
	// COMMAND MENU
	// Create the root of this new Command Menu

	int newIndex = m_iNumMenus;

	m_pCommandMenus[newIndex] = new CCommandMenu( NULL, direction, xOffset, yOffset, flButtonSizeX, 300 ); // This will be resized once we know how many items are in it
	m_pCommandMenus[newIndex]->setParent( this );
	m_pCommandMenus[newIndex]->setVisible( false );
	m_pCommandMenus[newIndex]->m_flButtonSizeY = flButtonSizeY;
	m_pCommandMenus[newIndex]->m_iSpectCmdMenu = direction;

	m_iNumMenus++;

	// Read Command Menu from the txt file
	char token[1024];
	char *pfile = (char *)gEngfuncs.COM_LoadFile( menuFile, 5, NULL );
	if ( !pfile )
	{
		gEngfuncs.Con_DPrintf( "Unable to open %s\n", menuFile );
		SetCurrentCommandMenu( NULL );
		return newIndex;
	}

#ifdef _WIN32
	try
	{
#endif
		// First, read in the localisation strings

		// Detpack strings
		gHUD.m_TextMessage.LocaliseTextString( "#DetpackSet_For5Seconds", m_sDetpackStrings[0], MAX_BUTTON_SIZE );
		gHUD.m_TextMessage.LocaliseTextString( "#DetpackSet_For20Seconds", m_sDetpackStrings[1], MAX_BUTTON_SIZE );
		gHUD.m_TextMessage.LocaliseTextString( "#DetpackSet_For50Seconds", m_sDetpackStrings[2], MAX_BUTTON_SIZE );

		// Now start parsing the menu structure
		m_pCurrentCommandMenu     = m_pCommandMenus[newIndex];
		char szLastButtonText[32] = "file start";
		pfile                     = gEngfuncs.COM_ParseFile( pfile, token );
		while ( ( strlen( token ) > 0 ) && ( m_iNumMenus < MAX_MENUS ) )
		{
			// Keep looping until we hit the end of this menu
			while ( token[0] != '}' && ( strlen( token ) > 0 ) )
			{
				char cText[32]                  = "";
				char cBoundKey[32]              = "";
				char cCustom[32]                = "";
				static const int cCommandLength = 128;
				char cCommand[cCommandLength]   = "";
				char szMap[MAX_MAPNAME]         = "";
				int iPlayerClass                = 0;
				int iCustom                     = false;
				int iTeamOnly                   = -1;
				int iToggle                     = 0;
				int iButtonY;
				bool bGetExtraToken    = true;
				CommandButton *pButton = NULL;

				// We should never be here without a Command Menu
				if ( !m_pCurrentCommandMenu )
				{
					gEngfuncs.Con_Printf( "Error in %s file after '%s'.\n", menuFile, szLastButtonText );
					m_iInitialized = false;
					return newIndex;
				}

				// token should already be the bound key, or the custom name
				strncpy( cCustom, token, 32 );
				cCustom[31] = '\0';

				// See if it's a custom button
				if ( !strcmp( cCustom, "CUSTOM" ) )
				{
					iCustom = true;

					// Get the next token
					pfile = gEngfuncs.COM_ParseFile( pfile, token );
				}
				// See if it's a map
				else if ( !strcmp( cCustom, "MAP" ) )
				{
					// Get the mapname
					pfile = gEngfuncs.COM_ParseFile( pfile, token );
					strncpy( szMap, token, MAX_MAPNAME );
					szMap[MAX_MAPNAME - 1] = '\0';

					// Get the next token
					pfile = gEngfuncs.COM_ParseFile( pfile, token );
				}
				else if ( !strncmp( cCustom, "TEAM", 4 ) ) // TEAM1, TEAM2, TEAM3, TEAM4
				{
					// make it a team only button
					iTeamOnly = atoi( cCustom + 4 );

					// Get the next token
					pfile = gEngfuncs.COM_ParseFile( pfile, token );
				}
				else if ( !strncmp( cCustom, "TOGGLE", 6 ) )
				{
					iToggle = true;
					// Get the next token
					pfile = gEngfuncs.COM_ParseFile( pfile, token );
				}
				else
				{
					// See if it's a Class
#ifdef _TFC
					for ( int i = 1; i <= PC_ENGINEER; i++ )
					{
						if ( !strcmp( token, sTFClasses[i] ) )
						{
							// Save it off
							iPlayerClass = i;

							// Get the button text
							pfile = gEngfuncs.COM_ParseFile( pfile, token );
							break;
						}
					}
#endif
				}

				// Get the button bound key
				strncpy( cBoundKey, token, 32 );
				cText[31] = '\0';

				// Get the button text
				pfile = gEngfuncs.COM_ParseFile( pfile, token );
				CHudTextMessage::LocaliseTextString( token, cText, sizeof( cText ) );

				// save off the last button text we've come across (for error reporting)
				strncpy( szLastButtonText, cText, sizeof( szLastButtonText ) - 1 );
				szLastButtonText[sizeof( szLastButtonText ) - 1] = '\0';

				// Get the button command
				pfile = gEngfuncs.COM_ParseFile( pfile, token );
				strncpy( cCommand, token, cCommandLength );
				cCommand[cCommandLength - 1] = '\0';

				iButtonY = ( BUTTON_SIZE_Y - 1 ) * m_pCurrentCommandMenu->GetNumButtons();

				// Custom button handling
				if ( iCustom )
				{
					pButton = CreateCustomButton( cText, cCommand, iButtonY );

					// Get the next token to see if we're a menu
					pfile = gEngfuncs.COM_ParseFile( pfile, token );

					if ( token[0] == '{' )
					{
						strncpy( cCommand, token, sizeof( cCommand ) - 1 );
						cCommand[sizeof( cCommand ) - 1] = '\0';
					}
					else
					{
						bGetExtraToken = false;
					}
				}
				else if ( szMap[0] != '\0' )
				{
					// create a map button
					pButton = new MapButton( szMap, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY );
				}
				else if ( iTeamOnly != -1 )
				{
					// button that only shows up if the player is on team iTeamOnly
					pButton = new TeamOnlyCommandButton( iTeamOnly, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign );
				}
				else if ( iToggle && direction == 0 )
				{
					pButton = new ToggleCommandButton( cCommand, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign );
				}
				else if ( direction == 1 )
				{
					if ( iToggle )
						pButton = new SpectToggleButton( cCommand, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign );
					else
						pButton = new SpectButton( iPlayerClass, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY );
				}
				else
				{
					// normal button
					pButton = new CommandButton( iPlayerClass, cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY, flatDesign );
				}

				// add the button into the command menu
				if ( pButton )
				{
					m_pCurrentCommandMenu->AddButton( pButton );
					pButton->setBoundKey( cBoundKey[0] );
					pButton->setParentMenu( m_pCurrentCommandMenu );

					// Override font in CommandMenu
					pButton->setFont( Scheme::sf_primary3 );
				}

				// Find out if it's a submenu or a button we're dealing with
				if ( cCommand[0] == '{' )
				{
					if ( m_iNumMenus >= MAX_MENUS )
					{
						gEngfuncs.Con_Printf( "Too many menus in %s past '%s'\n", menuFile, szLastButtonText );
					}
					else
					{
						// Create the menu
						m_pCommandMenus[m_iNumMenus] = CreateSubMenu( pButton, m_pCurrentCommandMenu, iButtonY );
						m_pCurrentCommandMenu        = m_pCommandMenus[m_iNumMenus];
						m_iNumMenus++;
					}
				}
				else if ( !iCustom )
				{
					// Create the button and attach it to the current menu
					if ( iToggle )
						pButton->addActionSignal( new CMenuHandler_ToggleCvar( cCommand ) );
					else
						pButton->addActionSignal( new CMenuHandler_StringCommand( cCommand ) );
					// Create an input signal that'll popup the current menu
					pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
				}

				// Get the next token
				if ( bGetExtraToken )
				{
					pfile = gEngfuncs.COM_ParseFile( pfile, token );
				}
			}

			// Move back up a menu
			m_pCurrentCommandMenu = m_pCurrentCommandMenu->GetParentMenu();

			pfile = gEngfuncs.COM_ParseFile( pfile, token );
		}
#ifdef _WIN32
	}
	catch ( CException *e )
	{
		e;
		// e->Delete();
		e              = NULL;
		m_iInitialized = false;
		return newIndex;
	}
#endif

	SetCurrentMenu( NULL );
	SetCurrentCommandMenu( NULL );
	gEngfuncs.COM_FreeFile( pfile );

	m_iInitialized = true;
	return newIndex;
}

CCommandMenu *TeamFortressViewport::CreateDisguiseSubmenu( CommandButton *pButton, CCommandMenu *pParentMenu, const char *commandText, int iYOffset, int iXOffset )
{
	// create the submenu, under which the class choices will be listed
	CCommandMenu *pMenu          = CreateSubMenu( pButton, pParentMenu, iYOffset, iXOffset );
	m_pCommandMenus[m_iNumMenus] = pMenu;
	m_iNumMenus++;

	// create the class choice buttons
#ifdef _TFC
	for ( int i = PC_SCOUT; i <= PC_ENGINEER; i++ )
	{
		CommandButton *pDisguiseButton = new CommandButton( CHudTextMessage::BufferedLocaliseTextString( sLocalisedClasses[i] ), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );

		char sz[256];
		snprintf( sz, sizeof( sz ), "%s %d", commandText, i );
		pDisguiseButton->addActionSignal( new CMenuHandler_StringCommand( sz ) );

		pMenu->AddButton( pDisguiseButton );
	}
#endif

	return pMenu;
}

CommandButton *TeamFortressViewport::CreateCustomButton( char *pButtonText, char *pButtonName, int iYOffset )
{
	CommandButton *pButton = NULL;
	CCommandMenu *pMenu    = NULL;

	// ChangeTeam
	if ( !strcmp( pButtonName, "!CHANGETEAM" ) )
	{
		// ChangeTeam Submenu
		pButton = new CommandButton( pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );

		// Create the submenu
		pMenu                        = CreateSubMenu( pButton, m_pCurrentCommandMenu, iYOffset );
		m_pCommandMenus[m_iNumMenus] = pMenu;
		m_iNumMenus++;

		// ChangeTeam buttons
		for ( int i = 0; i < 4; i++ )
		{
			char sz[256];
			snprintf( sz, sizeof( sz ), "jointeam %d", i + 1 );
			m_pTeamButtons[i] = new TeamButton( i + 1, "teamname", 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
			m_pTeamButtons[i]->addActionSignal( new CMenuHandler_StringCommandWatch( sz ) );
			pMenu->AddButton( m_pTeamButtons[i] );
		}

		// Auto Assign button
		m_pTeamButtons[4] = new TeamButton( 5, gHUD.m_TextMessage.BufferedLocaliseTextString( "#Team_AutoAssign" ), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		m_pTeamButtons[4]->addActionSignal( new CMenuHandler_StringCommand( "jointeam 5" ) );
		pMenu->AddButton( m_pTeamButtons[4] );

		// Spectate button
		m_pTeamButtons[5] = new SpectateButton( CHudTextMessage::BufferedLocaliseTextString( "#Menu_Spectate" ), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false );
		m_pTeamButtons[5]->addActionSignal( new CMenuHandler_StringCommand( "spectate" ) );
		pMenu->AddButton( m_pTeamButtons[5] );
	}
	// ChangeClass
	else if ( !strcmp( pButtonName, "!CHANGECLASS" ) )
	{
		// Create the Change class menu
		pButton = new ClassButton( -1, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false );

		// ChangeClass Submenu
		pMenu                        = CreateSubMenu( pButton, m_pCurrentCommandMenu, iYOffset );
		m_pCommandMenus[m_iNumMenus] = pMenu;
		m_iNumMenus++;

#ifdef _TFC
		for ( int i = PC_SCOUT; i <= PC_RANDOM; i++ )
		{
			char sz[256];

			// ChangeClass buttons
			CHudTextMessage::LocaliseTextString( sLocalisedClasses[i], sz, 256 );
			ClassButton *pClassButton = new ClassButton( i, sz, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false );

			snprintf( sz, sizeof( sz ), "%s", sTFClassSelection[i] );
			pClassButton->addActionSignal( new CMenuHandler_StringCommandClassSelect( sz ) );
			pMenu->AddButton( pClassButton );
		}
#endif
	}
#ifdef _TFC
	// Map Briefing
	else if ( !strcmp( pButtonName, "!MAPBRIEFING" ) )
	{
		pButton = new CommandButton( pButtonText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_TextWindow( MENU_MAPBRIEFING ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	// Class Descriptions
	else if ( !strcmp( pButtonName, "!CLASSDESC" ) )
	{
		pButton = new ClassButton( 0, pButtonText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y, false );
		pButton->addActionSignal( new CMenuHandler_TextWindow( MENU_CLASSHELP ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!SERVERINFO" ) )
	{
		pButton = new ClassButton( 0, pButtonText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y, false );
		pButton->addActionSignal( new CMenuHandler_TextWindow( MENU_INTRO ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	// Spy abilities
	else if ( !strcmp( pButtonName, "!SPY" ) )
	{
		pButton = new DisguiseButton( 0, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
	}
	// Feign
	else if ( !strcmp( pButtonName, "!FEIGN" ) )
	{
		pButton = new FeignButton( FALSE, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "feign" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	// Feign Silently
	else if ( !strcmp( pButtonName, "!FEIGNSILENT" ) )
	{
		pButton = new FeignButton( FALSE, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "sfeign" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	// Stop Feigning
	else if ( !strcmp( pButtonName, "!FEIGNSTOP" ) )
	{
		pButton = new FeignButton( TRUE, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "feign" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	// Disguise
	else if ( !strcmp( pButtonName, "!DISGUISEENEMY" ) )
	{
		// Create the disguise enemy button, which active only if there are 2 teams
		pButton = new DisguiseButton( DISGUISE_TEAM2, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		CreateDisguiseSubmenu( pButton, m_pCurrentCommandMenu, "disguise_enemy", iYOffset );
	}
	else if ( !strcmp( pButtonName, "!DISGUISEFRIENDLY" ) )
	{
		// Create the disguise friendly button, which active only if there are 1 or 2 teams
		pButton = new DisguiseButton( DISGUISE_TEAM1 | DISGUISE_TEAM2, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		CreateDisguiseSubmenu( pButton, m_pCurrentCommandMenu, "disguise_friendly", iYOffset );
	}
	else if ( !strcmp( pButtonName, "!DISGUISE" ) )
	{
		// Create the Disguise button
		pButton                      = new DisguiseButton( DISGUISE_TEAM3 | DISGUISE_TEAM4, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		CCommandMenu *pDisguiseMenu  = CreateSubMenu( pButton, m_pCurrentCommandMenu, iYOffset );
		m_pCommandMenus[m_iNumMenus] = pDisguiseMenu;
		m_iNumMenus++;

		// Disguise Enemy submenu buttons
		for ( int i = 1; i <= 4; i++ )
		{
			// only show the 4th disguise button if we have 4 teams
			m_pDisguiseButtons[i] = new DisguiseButton( ( ( i < 4 ) ? DISGUISE_TEAM3 : 0 ) | DISGUISE_TEAM4, "Disguise", 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );

			pDisguiseMenu->AddButton( m_pDisguiseButtons[i] );
			m_pDisguiseButtons[i]->setParentMenu( pDisguiseMenu );

			char sz[256];
			snprintf( sz, sizeof( sz ), "disguise %d", i );
			CreateDisguiseSubmenu( m_pDisguiseButtons[i], pDisguiseMenu, sz, iYOffset, CMENU_SIZE_X - 1 );
		}
	}
	// Start setting a Detpack
	else if ( !strcmp( pButtonName, "!DETPACKSTART" ) )
	{
		// Detpack Submenu
		pButton = new DetpackButton( 2, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );

		// Create the submenu
		pMenu                        = CreateSubMenu( pButton, m_pCurrentCommandMenu, iYOffset );
		m_pCommandMenus[m_iNumMenus] = pMenu;
		m_iNumMenus++;

		// Set detpack buttons
		CommandButton *pDetButton;
		pDetButton = new CommandButton( m_sDetpackStrings[0], 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pDetButton->addActionSignal( new CMenuHandler_StringCommand( "detstart 5" ) );
		pMenu->AddButton( pDetButton );
		pDetButton = new CommandButton( m_sDetpackStrings[1], 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pDetButton->addActionSignal( new CMenuHandler_StringCommand( "detstart 20" ) );
		pMenu->AddButton( pDetButton );
		pDetButton = new CommandButton( m_sDetpackStrings[2], 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pDetButton->addActionSignal( new CMenuHandler_StringCommand( "detstart 50" ) );
		pMenu->AddButton( pDetButton );
	}
	// Stop setting a Detpack
	else if ( !strcmp( pButtonName, "!DETPACKSTOP" ) )
	{
		pButton = new DetpackButton( 1, pButtonText, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "detstop" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	// Engineer building
	else if ( !strcmp( pButtonName, "!BUILD" ) )
	{
		// only appears if the player is an engineer, and either they have built something or have enough metal to build
		pButton = new BuildButton( BUILDSTATE_BASE, 0, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
	}
	else if ( !strcmp( pButtonName, "!BUILDSENTRY" ) )
	{
		pButton = new BuildButton( BUILDSTATE_CANBUILD, BuildButton::SENTRYGUN, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "build 2" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!BUILDDISPENSER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_CANBUILD, BuildButton::DISPENSER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "build 1" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!ROTATESENTRY180" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "rotatesentry180" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!ROTATESENTRY" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "rotatesentry" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DISMANTLEDISPENSER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::DISPENSER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "dismantle 1" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DISMANTLESENTRY" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "dismantle 2" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DETONATEDISPENSER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::DISPENSER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "detdispenser" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DETONATESENTRY" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "detsentry" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!BUILDENTRYTELEPORTER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_CANBUILD, BuildButton::ENTRY_TELEPORTER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "build 4" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DISMANTLEENTRYTELEPORTER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::ENTRY_TELEPORTER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "dismantle 4" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DETONATEENTRYTELEPORTER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::ENTRY_TELEPORTER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "detentryteleporter" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!BUILDEXITTELEPORTER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_CANBUILD, BuildButton::EXIT_TELEPORTER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "build 5" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DISMANTLEEXITTELEPORTER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::EXIT_TELEPORTER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "dismantle 5" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	else if ( !strcmp( pButtonName, "!DETONATEEXITTELEPORTER" ) )
	{
		pButton = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::EXIT_TELEPORTER, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "detexitteleporter" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
	// Stop building
	else if ( !strcmp( pButtonName, "!BUILDSTOP" ) )
	{
		pButton = new BuildButton( BUILDSTATE_BUILDING, 0, pButtonText, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		pButton->addActionSignal( new CMenuHandler_StringCommand( "build" ) );
		// Create an input signal that'll popup the current menu
		pButton->addInputSignal( new CMenuHandler_PopupSubMenuInput( pButton, m_pCurrentCommandMenu ) );
	}
#endif

	return pButton;
}

CCommandMenu *TeamFortressViewport::CreateSubMenu( CommandButton *pButton, CCommandMenu *pParentMenu, int iYOffset, int iXOffset )
{
	int iXPos      = 0;
	int iYPos      = 0;
	int iWide      = CMENU_SIZE_X;
	int iTall      = 0;
	int iDirection = 0;

	if ( pParentMenu )
	{
		iXPos      = m_pCurrentCommandMenu->GetXOffset() + ( CMENU_SIZE_X - 1 ) + iXOffset;
		iYPos      = m_pCurrentCommandMenu->GetYOffset() + iYOffset;
		iDirection = pParentMenu->GetDirection();
	}

	CCommandMenu *pMenu = new CCommandMenu( pParentMenu, iDirection, iXPos, iYPos, iWide, iTall );
	pMenu->setParent( this );
	pButton->AddSubMenu( pMenu );
	pButton->setFont( Scheme::sf_primary3 );
	pMenu->m_flButtonSizeY = m_pCurrentCommandMenu->m_flButtonSizeY;

	// Create the Submenu-open signal
	InputSignal *pISignal = new CMenuHandler_PopupSubMenuInput( pButton, pMenu );
	pButton->addInputSignal( pISignal );

	// Put a > to show it's a submenu
	CImageLabel *pLabel = new CImageLabel( "arrowright", XRES( CMENU_SIZE_X - SUBMENU_SIZE_X ), YRES( SUBMENU_SIZE_Y ) );
	pLabel->setParent( pButton );
	pLabel->addInputSignal( pISignal );

	// Reposition
	pLabel->getPos( iXPos, iYPos );
	pLabel->setPos( pButton->getWide() - pLabel->getImageWide() - 4, -4 );

	// Create the mouse off signal for the Label too
	if ( !pButton->m_bNoHighlight )
		pLabel->addInputSignal( new CHandler_CommandButtonHighlight( pButton ) );

	return pMenu;
}
