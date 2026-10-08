"GameMenu"
{
	"1"
	{
		"label" "#GameUI_GameMenu_ResumeGame"
		"command" "ResumeGame"
		"OnlyInGame" "1"
	}
	"2"
	{
		"label" "Restart Game"
		"command" "engine restart"
		"OnlyInGame" "1"
	}
	"7"
	{
		"label" "Elegir Mapa"
		"command" "OpenCreateMultiplayerGameDialog"
		"HelpText" "Seleccionar mapa e iniciar partida"
	}
	"18"
	{
		"label" "#GameUI_GameMenu_Options"
		"command" "OpenOptionsDialog"
		"HelpText" "#GameUI_MainMenu_Hint_Configuration"
	}
	"20"
	{
		"label" "#GameUI_GameMenu_LeaveGame"
		"command" "Disconnect"
		"OnlyInGame" "1"
		"notmulti" "1"
	}
	"21"
	{
		"label" "#GameUI_GameMenu_Quit"
		"command" "Quit"
		"HelpText" "#GameUI_MainMenu_Hint_QuitGame"
	}
}
