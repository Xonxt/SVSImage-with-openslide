/*********************************************************************************************************************/
/* Datei: Session.h                                                                                                  */
/*********************************************************************************************************************/
/* Autor:   Daniel Heim		                                                                                         */
/* Datum:   28.06.2013                                                                                               */
/* Projekt: svsimage.dll                                                                                             */
/* Version: 1.4                                                                                                      */
/* Firma:   VMscope GmbH                                                                                             */
/*********************************************************************************************************************/

#include <stdint.h>

// add reference to OpenSlide libraries
#include "openslide.h"
#include "openslide-features.h"

typedef UINT16 uint16;
typedef UINT32 uint32;
typedef	INT32 tsize_t;


/*********************************************************************************************************************/
/************************************************* Struktur: Session *************************************************/
/*********************************************************************************************************************/

struct Session
{
	//*** Variablen-Deklarationen *************************************************************************************
	uint16 compressionSheme;
	uint16 labelImageDir;
	uint16 macroImageDir;
	int baseLayerOffset;
	uint32 imageHeight;
	openslide_t* slide;
	wchar_t* filename;
	uint32 tileHeight;
	uint32 imageWidth;
	short photoMetric;	
	INT32 bufferSize;
	uint32 tileWidth;
	BYTE* buffer;
	INT32 levels;
	short subX;
	short subY;
	INT32 dpi;

	
	/*****************************************************************************************************************/
	/************************************************** Konstruktor **************************************************/
	/*****************************************************************************************************************/
	
	//Session(TIFF* img)
	Session(openslide_t* img)
	{
		//*** Die Variablen initialisieren ****************************************************************************
		filename=L"";
		bufferSize=0;
		buffer=NULL;
		compressionSheme=0;
		labelImageDir=0;
		macroImageDir=0;
		baseLayerOffset=0;

		//*** Referenz auf das Tiffbild übernehmen ********************************************************************
		slide=img;
	}

};

/**********************************************************#**********************************************************/