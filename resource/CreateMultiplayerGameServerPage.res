"Resource/CreateMultiplayerGameServerPage.res"
{
	"Label1"
	{
		"ControlName"		"Label"
		"fieldName"		"Label1"
		"xpos"			"30"
		"ypos"			"36"
		"wide"			"120"
		"tall"			"24"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"labelText"		"Mapa a jugar:"
		"textAlignment"		"west"
		"associate"		"MapList"
		"dulltext"		"0"
		"brighttext"		"1"
	}

	"MapList"
	{
		"ControlName"		"ComboBox"
		"fieldName"		"MapList"
		"xpos"			"150"
		"ypos"			"36"
		"wide"			"200"
		"tall"			"24"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"1"
		"textHidden"		"0"
		"editable"		"0"
		"maxchars"		"-1"
		"NumericInputOnly"	"0"
	}

	"HelpLabel"
	{
		"ControlName"		"Label"
		"fieldName"		"HelpLabel"
		"xpos"			"30"
		"ypos"			"80"
		"wide"			"340"
		"tall"			"40"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"labelText"		"Selecciona el mapa que deseas jugar y presiona Iniciar."
		"textAlignment"		"north-west"
		"dulltext"		"1"
		"brighttext"		"0"
		"wrap"			"1"
	}

	// ==========================================
	// Opciones multijugador no deseadas (ocultas)
	// ==========================================

	"EnableSteamNetworkingCheck"
	{
		"ControlName"		"CheckButton"
		"fieldName"		"EnableSteamNetworkingCheck"
		"xpos"			"28"
		"ypos"			"83"
		"wide"			"320"
		"tall"			"24"
		"visible"		"0"
		"enabled"		"0"
	}

	"EnableBotsCheck"
	{
		"ControlName"		"CheckButton"
		"fieldName"		"EnableBotsCheck"
		"visible"		"0"
		"enabled"		"0"
	}

	"BotQuotaCombo"
	{
		"ControlName"		"TextEntry"
		"fieldName"		"BotQuotaCombo"
		"visible"		"0"
		"enabled"		"0"
	}

	"SkillLevelExpert"
	{
		"ControlName"		"RadioButton"
		"fieldName"		"SkillLevelExpert"
		"visible"		"0"
		"enabled"		"0"
	}

	"SkillLevelHard"
	{
		"ControlName"		"RadioButton"
		"fieldName"		"SkillLevelHard"
		"visible"		"0"
		"enabled"		"0"
	}

	"SkillLevelNormal"
	{
		"ControlName"		"RadioButton"
		"fieldName"		"SkillLevelNormal"
		"visible"		"0"
		"enabled"		"0"
	}

	"SkillLevelEasy"
	{
		"ControlName"		"RadioButton"
		"fieldName"		"SkillLevelEasy"
		"visible"		"0"
		"enabled"		"0"
	}

	"BotDifficultyLabel"
	{
		"ControlName"		"Label"
		"fieldName"		"BotDifficultyLabel"
		"visible"		"0"
		"enabled"		"0"
	}

	"Divider1"
	{
		"ControlName"		"Divider"
		"fieldName"		"Divider1"
		"visible"		"0"
		"enabled"		"0"
	}

	"BotLabel2"
	{
		"ControlName"		"Label"
		"fieldName"		"BotLabel2"
		"visible"		"0"
		"enabled"		"0"
	}
}
