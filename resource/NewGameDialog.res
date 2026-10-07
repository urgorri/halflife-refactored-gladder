"Resource\NewGameDialog.res"
{
	"NewGameDialog"
	{
		"ControlName"		"Frame"
		"fieldName"		"NewGameDialog"
		"xpos"			"390"
		"ypos"			"254"
		"wide"			"280"
		"tall"			"302"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
	}

	"Label4"
	{
		"ControlName"		"Label"
		"fieldName"		"Label4"
		"xpos"			"20"
		"ypos"			"28"
		"wide"			"240"
		"tall"			"22"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"labelText"		"Seleccionar Mapa"
		"textAlignment"		"west"
		"dulltext"		"0"
		"brighttext"		"0"
	}

	// ==========================================
	// Fila 1: gl01 (Desierto)
	// ==========================================

	"MapPic1"
	{
		"ControlName"		"ImagePanel"
		"fieldName"		"MapPic1"
		"xpos"			"20"
		"ypos"			"54"
		"wide"			"48"
		"tall"			"48"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"image"			"resource/gl01"
		"scaleImage"		"1"
	}

	"MapTitle1"
	{
		"ControlName"		"Label"
		"fieldName"		"MapTitle1"
		"xpos"			"76"
		"ypos"			"58"
		"wide"			"142"
		"tall"			"18"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"labelText"		"gl01"
		"textAlignment"		"west"
		"dulltext"		"0"
		"brighttext"		"1"
	}

	"MapSubtitle1"
	{
		"ControlName"		"Label"
		"fieldName"		"MapSubtitle1"
		"xpos"			"76"
		"ypos"			"78"
		"wide"			"142"
		"tall"			"18"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"labelText"		"Desierto - 16 jug."
		"textAlignment"		"west"
		"dulltext"		"1"
		"brighttext"		"0"
	}

	// PlayGame es la flecha de lanzamiento directo para gl01
	"PlayGame"
	{
		"ControlName"		"Button"
		"fieldName"		"PlayGame"
		"xpos"			"224"
		"ypos"			"60"
		"wide"			"36"
		"tall"			"36"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"1"
		"labelText"		"➔"
		"textAlignment"		"center"
		"dulltext"		"0"
		"command"		"Play"
		"default"		"1"
	}

	// ==========================================
	// Fila 2: gl02 (Nieve)
	// ==========================================

	"MapPic2"
	{
		"ControlName"		"ImagePanel"
		"fieldName"		"MapPic2"
		"xpos"			"20"
		"ypos"			"110"
		"wide"			"48"
		"tall"			"48"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"image"			"resource/gl02"
		"scaleImage"		"1"
	}

	"MapTitle2"
	{
		"ControlName"		"Label"
		"fieldName"		"MapTitle2"
		"xpos"			"76"
		"ypos"			"114"
		"wide"			"142"
		"tall"			"18"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"labelText"		"gl02"
		"textAlignment"		"west"
		"dulltext"		"0"
		"brighttext"		"1"
	}

	"MapSubtitle2"
	{
		"ControlName"		"Label"
		"fieldName"		"MapSubtitle2"
		"xpos"			"76"
		"ypos"			"134"
		"wide"			"142"
		"tall"			"18"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"labelText"		"Nieve - 32 jug."
		"dulltext"		"1"
		"brighttext"		"0"
	}

	"LaunchButton2"
	{
		"ControlName"		"Button"
		"fieldName"		"LaunchButton2"
		"xpos"			"224"
		"ypos"			"116"
		"wide"			"36"
		"tall"			"36"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"0"
		"tabPosition"		"2"
		"labelText"		"➔"
		"textAlignment"		"center"
		"dulltext"		"0"
		"default"		"0"
	}

	// ==========================================
	// Fila 3: gl03 (Almacén)
	// ==========================================

	"MapPic3"
	{
		"ControlName"		"ImagePanel"
		"fieldName"		"MapPic3"
		"xpos"			"20"
		"ypos"			"166"
		"wide"			"48"
		"tall"			"48"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"image"			"resource/gl03"
		"scaleImage"		"1"
	}

	"MapTitle3"
	{
		"ControlName"		"Label"
		"fieldName"		"MapTitle3"
		"xpos"			"76"
		"ypos"			"170"
		"wide"			"142"
		"tall"			"18"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"labelText"		"gl03"
		"textAlignment"		"west"
		"dulltext"		"0"
		"brighttext"		"1"
	}

	"MapSubtitle3"
	{
		"ControlName"		"Label"
		"fieldName"		"MapSubtitle3"
		"xpos"			"76"
		"ypos"			"190"
		"wide"			"142"
		"tall"			"18"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"labelText"		"Almacén - 12 jug."
		"dulltext"		"1"
		"brighttext"		"0"
	}

	"LaunchButton3"
	{
		"ControlName"		"Button"
		"fieldName"		"LaunchButton3"
		"xpos"			"224"
		"ypos"			"172"
		"wide"			"36"
		"tall"			"36"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"0"
		"tabPosition"		"3"
		"labelText"		"➔"
		"textAlignment"		"center"
		"dulltext"		"0"
		"default"		"0"
	}

	// ==========================================
	// Botón Cancelar (Abajo a la derecha)
	// ==========================================

	"Cancel"
	{
		"ControlName"		"Button"
		"fieldName"		"Cancel"
		"xpos"			"180"
		"ypos"			"256"
		"wide"			"80"
		"tall"			"24"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"4"
		"labelText"		"#GameUI_Cancel"
		"textAlignment"		"center"
		"dulltext"		"0"
		"command"		"Close"
		"default"		"0"
	}

	// ==========================================
	// Controles legacy del motor (ocultos fuera de pantalla)
	// ==========================================

	"Difficulty"
	{
		"ControlName"		"ComboBox"
		"fieldName"		"Difficulty"
		"xpos"			"-500"
		"ypos"			"-500"
		"wide"			"10"
		"tall"			"10"
		"visible"		"0"
		"enabled"		"0"
	}

	"Training"
	{
		"ControlName"		"Button"
		"fieldName"		"Training"
		"xpos"			"-500"
		"ypos"			"-500"
		"wide"			"10"
		"tall"			"10"
		"visible"		"0"
		"enabled"		"0"
	}

	"TrainingButton"
	{
		"ControlName"		"Button"
		"fieldName"		"Training"
		"xpos"			"-500"
		"ypos"			"-500"
		"wide"			"10"
		"tall"			"10"
		"visible"		"0"
		"enabled"		"0"
	}

	"PlayGame_Alt"
	{
		"ControlName"		"Button"
		"fieldName"		"PlayGame_Alt"
		"xpos"			"-500"
		"ypos"			"-500"
		"wide"			"10"
		"tall"			"10"
		"visible"		"0"
		"enabled"		"0"
	}

	"BuildDialog"
	{
		"ControlName"		"BuildModeDialog"
		"fieldName"		"BuildDialog"
		"xpos"			"87"
		"ypos"			"250"
		"wide"			"285"
		"tall"			"665"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
	}
}
