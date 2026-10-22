//=========== (C) Copyright 1996-2002 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: TFC Class Menu
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#include "VGUI_Font.h"
#include <VGUI_TextImage.h>

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

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_TeamMenuPanel.h"
#include "vgui_ClassMenuPanel.h"
#include "vgui_CommandMenuPanel.h"
#include "vgui_ServerBrowser.h"

// Class Menu Dimensions
#define CLASSMENU_TITLE_X XRES( 40 )
#define CLASSMENU_TITLE_Y YRES( 32 )
#define CLASSMENU_TOPLEFT_BUTTON_X XRES( 40 )
#define CLASSMENU_TOPLEFT_BUTTON_Y YRES( 80 )
#define CLASSMENU_BUTTON_SIZE_X XRES( 124 )
#define CLASSMENU_BUTTON_SIZE_Y YRES( 24 )
#define CLASSMENU_BUTTON_SPACER_Y YRES( 8 )
#define CLASSMENU_WINDOW_X XRES( 176 )
#define CLASSMENU_WINDOW_Y YRES( 80 )
#define CLASSMENU_WINDOW_SIZE_X XRES( 424 )
#define CLASSMENU_WINDOW_SIZE_Y YRES( 312 )
#define CLASSMENU_WINDOW_TEXT_X XRES( 150 )
#define CLASSMENU_WINDOW_TEXT_Y YRES( 80 )
#define CLASSMENU_WINDOW_NAME_X XRES( 150 )
#define CLASSMENU_WINDOW_NAME_Y YRES( 8 )
#define CLASSMENU_WINDOW_PLAYERS_Y YRES( 42 )

// Creation
CClassMenuPanel::CClassMenuPanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall )
    : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	// Get the scheme used for the Titles
	CSchemeManager *pSchemes    = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	int r, g, b, a;

	// Create the title
	Label *pLabel = new Label( "", CLASSMENU_TITLE_X, CLASSMENU_TITLE_Y );
	pLabel->setParent( this );
	pLabel->setFont( pSchemes->getFont( hTitleScheme ) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pLabel->setBgColor( r, g, b, a );
	pLabel->setContentAlignment( vgui::Label::a_west );
	pLabel->setText( gHUD.m_TextMessage.BufferedLocaliseTextString( "#Title_SelectYourClass" ) );

	// Create the Scroll panel
	m_pScrollPanel = new CTFScrollPanel( CLASSMENU_WINDOW_X, CLASSMENU_WINDOW_Y, CLASSMENU_WINDOW_SIZE_X, CLASSMENU_WINDOW_SIZE_Y );
	m_pScrollPanel->setParent( this );
	m_pScrollPanel->setScrollBarAutoVisible( false, false );
	m_pScrollPanel->setScrollBarVisible( true, true );
	m_pScrollPanel->setBorder( new LineBorder( Color( 255 * 0.7, 170 * 0.7, 0, 0 ) ) );
	m_pScrollPanel->validate();

	int clientWide = m_pScrollPanel->getClient()->getWide();

	m_pScrollPanel->setScrollBarAutoVisible( false, true );
	m_pScrollPanel->setScrollBarVisible( false, false );
	m_pScrollPanel->validate();

	m_pSelectionPanel = new CClassMenuSelectionPanel( 0, 0, wide, tall );
	m_pSelectionPanel->setParent( this );
	m_pSelectionPanel->InitializeButtons( this );

#ifdef _TFC
	for ( int i = 0; i <= PC_RANDOM; i++ )
	{
		m_pClassInfoPanel[i] = new CClassMenuDescriptionPanel( i, 0, 0, clientWide, CLASSMENU_WINDOW_SIZE_Y, clientWide );
		m_pClassInfoPanel[i]->setParent( m_pScrollPanel->getClient() );

		gHUD.m_TextMessage.LocaliseTextString( "#Title_CurrentlyOnYourTeam", m_sPlayersOnTeamString, STRLENMAX_PLAYERSONTEAM );
	}
#endif

	m_iCurrentInfo = 0;
}

// Update
void CClassMenuPanel::Update()
{
	// Don't allow the player to join a team if they're not in a team
	if ( !g_iTeamNumber )
		return;

	int iYPos = CLASSMENU_TOPLEFT_BUTTON_Y;

	m_pSelectionPanel->UpdateButtons( iYPos, m_iCurrentInfo );

#ifdef _TFC
	for ( int i = 0; i <= PC_RANDOM; i++ )
	{
		bool bCivilian = ( gViewPort->GetValidClasses( g_iTeamNumber ) == -1 );

		// Now count the number of teammembers of this class
		int iTotal = 0;
		for ( int j = 1; j < MAX_PLAYERS; j++ )
		{
			if ( g_PlayerInfoList[j].name == NULL )
				continue; // empty player slot, skip
			if ( g_PlayerExtraInfo[j].teamname[0] == 0 )
				continue; // skip over players who are not in a team
			if ( g_PlayerInfoList[j].thisplayer )
				continue; // skip this player
			if ( g_PlayerExtraInfo[j].teamnumber != g_iTeamNumber )
				continue; // skip over players in other teams

			// If this team is forced to be civilians, just count the number of teammates
			if ( g_PlayerExtraInfo[j].playerclass != i && !bCivilian )
				continue;

			iTotal++;
		}

		m_pClassInfoPanel[i]->UpdatePlayerCount( iTotal, m_sPlayersOnTeamString, iTeamColors[g_iTeamNumber % iNumberOfTeamColors][0], iTeamColors[g_iTeamNumber % iNumberOfTeamColors][1], iTeamColors[g_iTeamNumber % iNumberOfTeamColors][2] );

		m_pClassInfoPanel[i]->SetActiveTeamGraphic( g_iTeamNumber - 1 );
	}
#endif
}

//======================================
// Key inputs for the Class Menu
bool CClassMenuPanel::SlotInput( int iSlot )
{
	return m_pSelectionPanel->SlotInput( iSlot );
}

//======================================
// Update the Class menu before opening it
void CClassMenuPanel::Open( void )
{
	Update();
	CMenuPanel::Open();
}

//-----------------------------------------------------------------------------
// Purpose: Called each time a new level is started.
//-----------------------------------------------------------------------------
void CClassMenuPanel::Initialize( void )
{
	setVisible( false );
	m_pScrollPanel->setScrollValue( 0, 0 );
}

//======================================
// Mouse is over a class button, bring up the class info
void CClassMenuPanel::SetActiveInfo( int iInput )
{
	// Remove all the Info panels and bring up the specified one
#ifdef _TFC
	for ( int i = 0; i <= PC_RANDOM; i++ )
	{
		m_pClassInfoPanel[i]->setVisible( false );
	}

	if ( iInput > PC_RANDOM || iInput < 0 )
#endif
		iInput = 0;

	m_pSelectionPanel->SetActiveButton( iInput );
	m_pClassInfoPanel[iInput]->setVisible( true );
	m_iCurrentInfo = iInput;

	m_pScrollPanel->setScrollValue( 0, 0 );
	m_pScrollPanel->validate();
}
CClassMenuDescriptionPanel::CClassMenuDescriptionPanel( int iClass, int x, int y, int wide, int tall, int clientWide )
    : CTransparentPanel( 255, x, y, clientWide, tall )
{
	m_iClass = iClass;

	bool bShowClassGraphic = true;
	if ( ScreenWidth < 640 )
	{
		bShowClassGraphic = false;
	}

	memset( m_pClassImages, 0, sizeof( m_pClassImages ) );

	CSchemeManager *pSchemes        = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme     = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );
	int r, g, b, a;

	int textOffs = XRES( 8 );
	if ( bShowClassGraphic )
	{
		textOffs = XRES( 150 ); // CLASSMENU_WINDOW_NAME_X
	}

	char sz[256];
	snprintf( sz, sizeof( sz ), "#Title_%s", sTFClassSelection[iClass] );
	char *localName   = CHudTextMessage::BufferedLocaliseTextString( sz );
	Label *pNameLabel = new Label( "", textOffs, YRES( 8 ) ); // CLASSMENU_WINDOW_NAME_Y
	pNameLabel->setFont( pSchemes->getFont( hTitleScheme ) );
	pNameLabel->setParent( this );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pNameLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pNameLabel->setBgColor( r, g, b, a );
	pNameLabel->setContentAlignment( vgui::Label::a_west );
	pNameLabel->setText( "%s", localName );

	if ( bShowClassGraphic )
	{
		for ( int team = 0; team < 2; team++ )
		{
			if ( team == 1 )
			{
				snprintf( sz, sizeof( sz ), "%sred", sTFClassSelection[iClass] );
			}
			else
			{
				snprintf( sz, sizeof( sz ), "%sblue", sTFClassSelection[iClass] );
			}

			m_pClassImages[team] = new CImageLabel( sz, 0, 0, XRES( 150 ), YRES( 80 ) ); // CLASSMENU_WINDOW_TEXT_X, CLASSMENU_WINDOW_TEXT_Y

			CImageLabel *pLabel = m_pClassImages[team];
			pLabel->setParent( this );

			if ( team != 1 )
			{
				pLabel->setVisible( false );
			}

			int xOut, yOut;
			pNameLabel->getTextSize( xOut, yOut );
			pLabel->setPos( ( XRES( 150 ) - pLabel->getWide() ) / 2, yOut / 2 );
		}
	}

	m_pPlayers = new Label( "", textOffs, YRES( 42 ) ); // CLASSMENU_WINDOW_PLAYERS_Y
	m_pPlayers->setParent( this );
	m_pPlayers->setBgColor( 0, 0, 0, 255 );
	m_pPlayers->setContentAlignment( vgui::Label::a_west );
	m_pPlayers->setFont( pSchemes->getFont( hClassWindowText ) );

	snprintf( sz, sizeof( sz ), "classes/short_%s.txt", sTFClassSelection[iClass] );
	char *cText = "Class Description not available.";
	char *pfile = (char *)gEngfuncs.COM_LoadFile( sz, 5, NULL );
	if ( pfile )
	{
		cText = pfile;
	}

	TextPanel *pTextWindow = new TextPanel( cText, textOffs, YRES( 80 ), ( XRES( 424 ) - textOffs ) - 5, tall - YRES( 80 ) ); // CLASSMENU_WINDOW_TEXT_Y, CLASSMENU_WINDOW_SIZE_X
	pTextWindow->setParent( this );
	pTextWindow->setFont( pSchemes->getFont( hClassWindowText ) );
	pSchemes->getFgColor( hClassWindowText, r, g, b, a );
	pTextWindow->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hClassWindowText, r, g, b, a );
	pTextWindow->setBgColor( r, g, b, a );

	int wideText, tallText;
	pTextWindow->getTextImage()->getTextSizeWrapped( wideText, tallText );
	pTextWindow->setSize( wideText, tallText );

	int xx, yy;
	pTextWindow->getPos( xx, yy );
	int maxX = xx + wideText;
	int maxY = yy + tallText;

	if ( m_pClassImages[0] != null )
	{
		m_pClassImages[0]->getPos( xx, yy );
		if ( ( yy + m_pClassImages[0]->getTall() ) > maxY )
		{
			maxY = yy + m_pClassImages[0]->getTall();
		}
	}

	setSize( maxX, maxY );
	if ( pfile )
		gEngfuncs.COM_FreeFile( pfile );
}

void CClassMenuDescriptionPanel::UpdatePlayerCount( int iTotal, const char *szPlayersOnTeamString, int r, int g, int b )
{
	char sz[256];
	snprintf( sz, sizeof( sz ), szPlayersOnTeamString, iTotal );
	m_pPlayers->setText( "%s", sz );
	m_pPlayers->setFgColor( r, g, b, 0 );
}

void CClassMenuDescriptionPanel::SetActiveTeamGraphic( int team )
{
	for ( int i = 0; i < MAX_TEAMS; i++ )
	{
		if ( m_pClassImages[i] )
		{
			m_pClassImages[i]->setVisible( false );
		}
	}

	if ( m_pClassImages[team] != NULL )
	{
		m_pClassImages[team]->setVisible( true );
	}
	else if ( m_pClassImages[0] )
	{
		m_pClassImages[0]->setVisible( true );
	}
}

CClassMenuSelectionPanel::CClassMenuSelectionPanel( int x, int y, int wide, int tall )
    : CTransparentPanel( 0, x, y, wide, tall )
{
	memset( m_pButtons, 0, sizeof( m_pButtons ) );
}

void CClassMenuSelectionPanel::InitializeButtons( Panel *pParent )
{
#ifdef _TFC
	for ( int i = 0; i <= PC_RANDOM; i++ )
	{
		char sz[256];
		int iYPos = CLASSMENU_TOPLEFT_BUTTON_Y + ( ( CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y ) * i );

		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect( sTFClassSelection[i], true );

		snprintf( sz, sizeof( sz ), "%s", CHudTextMessage::BufferedLocaliseTextString( sLocalisedClasses[i] ) );
		m_pButtons[i] = new ClassButton( i, sz, CLASSMENU_TOPLEFT_BUTTON_X, iYPos, CLASSMENU_BUTTON_SIZE_X, CLASSMENU_BUTTON_SIZE_Y, true );
		if ( i >= 1 && i <= 9 )
		{
			snprintf( sz, sizeof( sz ), "%d", i );
		}
		else
		{
			snprintf( sz, sizeof( sz ), "0" );
		}
		m_pButtons[i]->setBoundKey( sz[0] );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver( (CMenuPanel *)pParent, i ) );
		m_pButtons[i]->setParent( this );
	}
#endif

	m_pCancelButton = new CommandButton( gHUD.m_TextMessage.BufferedLocaliseTextString( "#Menu_Cancel" ), CLASSMENU_TOPLEFT_BUTTON_X, 0, CLASSMENU_BUTTON_SIZE_X, CLASSMENU_BUTTON_SIZE_Y );
	m_pCancelButton->setParent( this );
	m_pCancelButton->addActionSignal( new CMenuHandler_TextWindow( HIDE_TEXTWINDOW ) );
}

void CClassMenuSelectionPanel::UpdateButtons( int &iYPos, int &iCurrentInfo )
{
#ifdef _TFC
	for ( int i = 0; i <= PC_RANDOM; i++ )
	{
		bool bCivilian = ( gViewPort->GetValidClasses( g_iTeamNumber ) == -1 );

		if ( bCivilian )
		{
			if ( i == 0 )
			{
				m_pButtons[0]->setVisible( true );
				if ( getParent() )
				{
					( (CClassMenuPanel *)getParent() )->SetActiveInfo( 0 );
				}
				iYPos += CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y;
			}
			else
			{
				m_pButtons[i]->setVisible( false );
			}
		}
		else
		{
			if ( m_pButtons[i]->IsNotValid() || i == 0 )
			{
				m_pButtons[i]->setVisible( false );
			}
			else
			{
				m_pButtons[i]->setVisible( true );
				m_pButtons[i]->setPos( CLASSMENU_TOPLEFT_BUTTON_X, iYPos );
				iYPos += CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y;

				if ( !iCurrentInfo && getParent() )
				{
					( (CClassMenuPanel *)getParent() )->SetActiveInfo( i );
				}
			}
		}
	}
#endif

	if ( g_iPlayerClass )
	{
		m_pCancelButton->setPos( CLASSMENU_TOPLEFT_BUTTON_X, iYPos );
		m_pCancelButton->setVisible( true );
	}
	else
	{
		m_pCancelButton->setVisible( false );
	}
}

bool CClassMenuSelectionPanel::SlotInput( int iSlot )
{
	if ( ( iSlot < 0 ) || ( iSlot > 9 ) )
		return false;
	if ( !m_pButtons[iSlot] )
		return false;

	if ( iSlot == 0 )
	{
		if ( gViewPort->GetValidClasses( g_iTeamNumber ) == -1 )
		{
			m_pButtons[0]->fireActionSignal();
			return true;
		}
		iSlot = 10;
	}

	if ( !( m_pButtons[iSlot]->IsNotValid() ) )
	{
		m_pButtons[iSlot]->fireActionSignal();
		return true;
	}

	return false;
}

void CClassMenuSelectionPanel::SetActiveButton( int iInput )
{
#ifdef _TFC
	for ( int i = 0; i <= PC_RANDOM; i++ )
	{
		if ( m_pButtons[i] )
			m_pButtons[i]->setArmed( false );
	}
	if ( iInput > PC_RANDOM || iInput < 0 )
		iInput = 0;
	if ( m_pButtons[iInput] )
		m_pButtons[iInput]->setArmed( true );
#endif
}
