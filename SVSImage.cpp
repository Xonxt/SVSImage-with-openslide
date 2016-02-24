/*********************************************************************************************************************/
/* Datei: SVSImage.cpp                                                                                               */
/*********************************************************************************************************************/
/* Autor:   Daniel Heim		                                                                                           */
/* Datum:   28.06.2013                                                                                               */
/* Projekt: svsimage.dll                                                                                             */
/* Version: 1.4                                                                                                      */
/* Firma:   VMscope GmbH                                                                                             */
/*********************************************************************************************************************/
/* Modification:  Nikita Kovalenko                                                                                   */
/* Date:          24.02.2016                                                                                         */
/* Description:   Replaced the libTIFF with libOpenSlide to open and read slides provided by Camelyon16 challenge    */
/*********************************************************************************************************************/

#include <windows.h>
#include <sstream>
#include <string>

#include "Session.h"

/*********************************************************************************************************************/
/********************************************** Konstanten-Deklarationen *********************************************/
/*********************************************************************************************************************/

#define LABEL_IMAGE	1
#define MACRO_IMAGE	2


/*********************************************************************************************************************/
/********************************************** Funktions-Deklarationen **********************************************/
/*********************************************************************************************************************/

void Convert2Y1CbCrTileToArgb(unsigned char* src, unsigned char* dst, int width, int height);
void Convert24BgrTo32Argb(unsigned char* src, unsigned char* dst, int width, int height);
void Convert16BitGreyToArgb(unsigned char* src, unsigned char* dst, int width, int height);
bool GetValue(char* imageDescription, std::string key, std::string* value);
BOOL GetOpenSlideTile(INT64 handle, INT32 level, INT32 x, INT32 y);
bool DescriptionContains(char* image, std::string searchString);
int LevelToTiffDirectory(Session* session, int level);
INT32 GetDpi(char* imageDescription);
float StringToFloat(std::string s);


/*********************************************************************************************************************/
/************************************************ Funktion: OpenImage ************************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) INT64 OpenImage(wchar_t* filename)
{
	//*** Variablen-Deklaration ***************************************************************************************
	int additionalImageCount = 0;
	char* imageDescription;
	int layerImageCount = 1;
	Session* session;
	int maxWidth;	
	openslide_t* slide;
	int maxDir;
	int width;
	int dir;
	int64_t imgWidth, imgHeight;

	//*** Den Dateinamen prüfen ***************************************************************************************
	if (filename == NULL) return 0;

	//*** Den Schnitt öffnen ******************************************************************************************	
	if ((slide = openslide_open((const char*)filename)) == NULL)
	{	
		return 0;
	}	

	//*** Die Werte initialisieren ************************************************************************************
	maxWidth = 0;
	maxDir = 0;
	dir = 0;

	//*** Die Session-Struktur anlegen ********************************************************************************
	session = new Session(slide);

	//*** Den Dateinamen übernehmen ***********************************************************************************
	session->filename = filename;
	session->slide = slide;

	//*** Die Kompression der Kacheln bestimmen ***********************************************************************
	/*
	* ImageFormat.Uncompressed	== 1
	* ImageFormat.LZW						== 5
	* ImageFormat.Jpeg2000YCbCr == 33003
	* ImageFormat.Jpeg2000Rgb		== 33005
	* ImageFormat.Jpeg					== 7
	*/
	session->compressionSheme = 1;

	//*** Prüfen in welchem Farbformat die Daten vorliegen ************************************************************
	session->subX = 2;
	session->subY = 2;
	session->photoMetric = 2; // 1 == Gray, 2 == RGB, 6 == YCbCr

	//*** Die Kachelgröße bestimmen ***********************************************************************************
	session->tileHeight = atoi(openslide_get_property_value(slide, "openslide.level[0].tile-height"));
	session->tileWidth = atoi(openslide_get_property_value(slide, "openslide.level[0].tile-width"));
	
	//*** Die Bildgröße bestimmen *************************************************************************************
	openslide_get_level0_dimensions(slide, &imgWidth, &imgHeight);
	session->imageWidth = (uint32)imgWidth;
	session->imageHeight = (uint32)imgHeight;

	//*** Die Dpi bestimmen *******************************************************************************************
	if ((imageDescription = (char*)openslide_get_property_value(slide, "philips.DICOM_DERIVATION_DESCRIPTION")) == NULL) imageDescription = NULL;
	session->dpi = GetDpi(imageDescription);
	
	//*** Das BaseLayer festlegen *************************************************************************************
	session->baseLayerOffset = maxDir;

	//*** Die Anzahl der Zwischenebenen bestimmen *********************************************************************
	session->macroImageDir = 0;
	session->labelImageDir = 0;
	session->levels = openslide_get_level_count(slide);

	//*** Die erforderliche Größe für den Lese-Puffer bestimmen *******************************************************
	session->bufferSize = 4 * session->tileWidth * session->tileHeight * sizeof(BYTE);

	//*** Den Lese-Puffer anlegen *************************************************************************************
	if ((session->buffer = (BYTE*) malloc(session->bufferSize)) == NULL)
	{		
		//*** Die Session-Struktur wieder freigeben *******************************************************************
		delete session;

		//*** Ende ****************************************************************************************************
		return 0;
	}
	
	//*** Ende ********************************************************************************************************
	return (INT64)session;
}


/*********************************************************************************************************************/
/************************************************ Funktion: CloseImage ***********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void CloseImage(INT64 handle)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Das TiffBild schließen **************************************************************************************
	openslide_close(session->slide);

	//*** Den Lese-Puffer wieder freigeben ****************************************************************************
	free(session->buffer);
	
	//*** Die Session-Struktur freigeben ******************************************************************************
	delete session;
}


/*********************************************************************************************************************/
/*********************************************** Funktion: GetImageSize **********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void GetImageSize(INT64 handle, INT32* x, INT32* y)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Die Höhe und Breite des Bildes zuweisen *********************************************************************
	*y = session->imageHeight;
	*x = session->imageWidth;
}


/*********************************************************************************************************************/
/*********************************************** Funktion: GetImageDpi ***********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void GetImageDpi(INT64 handle, INT32* dpi)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Die Auflösung des Bildes zuweisen ***************************************************************************
	*dpi = session->dpi;
}


/*********************************************************************************************************************/
/*********************************************** Funktion: GetTileSize ***********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void GetTileSize(INT64 handle, INT32* x, INT32* y)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Die Höhe und Breite der Bildkacheln zuweisen ****************************************************************
	*x = session->tileWidth;
	*y = session->tileHeight;
}


/*********************************************************************************************************************/
/********************************************** Funktion: GetTileFormat **********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void GetTileFormat(INT64 handle, INT32* format)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** TODO Finde eine Lösung dafür ... ****************************************************************************
	if (session->compressionSheme == 33005) *format = 33003;

	//*** Das Bildformat der Kacheln zuweisen *************************************************************************
	else *format = session->compressionSheme;
}


/*********************************************************************************************************************/
/********************************************** Funktion: GetPhotometric *********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void GetPhotometric(INT64 handle, INT32* photometric)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Die Auflösung des Bildes zuweisen ***************************************************************************
	*photometric = (int)session->photoMetric;
}


/*********************************************************************************************************************/
/******************************************* Funktion: GetYCbCrSubsampling *******************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void GetYCbCrSubsampling(INT64 handle, INT32* subX, INT32* subY)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Die Unterabtastund des Bildes in X-Richtung zuweisen ********************************************************
	*subX = (int)session->subX;

	//*** Die Unterabtastund des Bildes in X-Richtung zuweisen ********************************************************
	*subY = (int)session->subY;
}


/*********************************************************************************************************************/
/************************************************ Funktion: GetLevels ************************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) void GetLevels(INT64 handle, INT32* levels)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Die Anzahl der Levels zuweisen ******************************************************************************
	*levels = session->levels;
}


/*********************************************************************************************************************/
/********************************************** Funktion: GetLevelSize ***********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) BOOL GetLevelSize(INT64 handle, INT32 level, INT32* x, INT32* y)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return false;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Größe der Pyramidenstufe bestimmen **************************************************************************
	int64_t w, h;

	openslide_get_level_dimensions(session->slide, level, &w, &h);

	*x = (INT32)w;
	*y = (INT32)h;

	//*** Ende ********************************************************************************************************
	return true;
}


// This function should replace the two following functions for extracting tiles
// OpenSlide lib only reads the tile as a UINT32 buffer, so there's no point
// the following functions will just be "wrappers" around this one
BOOL GetOpenSlideTile(INT64 handle, INT32 level, INT32 x, INT32 y) 
{
	Session* session;
	tsize_t tileSize;
	int64_t tileWidth, tileHeight;
	uint32_t *tileBuffer = NULL;	
	int64_t l_width, l_height;
	int64_t step_x, step_y;

	if (handle == 0) 
		return false;

	session = (Session*)handle;

	tileSize = session->bufferSize;
	tileWidth = session->tileWidth;
	tileHeight = session->tileHeight;

	// prepare an unsigned-int-32 bit buffer of size [tileWidth x tileHeight] for the tile
	tileBuffer = (uint32_t*)malloc(tileWidth * tileHeight * sizeof(uint32_t));

	// get the size of the slide at current detalization level
	openslide_get_level_dimensions(session->slide, level, &l_width, &l_height);

	// recalculate the step for the tiles
	step_y = (int64_t)floor((double)session->imageHeight/ ((double)l_height / (double)session->tileHeight));
	step_x = (int64_t)floor((double)session->imageWidth / ((double)l_width/ (double)session->tileWidth));
	
	// read the tile of size [tileWidth x tileHeight] from the slide t current level
	// note: the coordinates (x; y) are relative to the lowest level, hence the step_x and xtep_y
	openslide_read_region(session->slide, tileBuffer, x * step_x, y * step_y, level, tileWidth, tileHeight);
	
	// the tile buffer will be set to NULL if there was an error 
	if (tileBuffer == NULL) {
		// it is possible to get the error description, if there was one
		// char* errorText = openslide_get_error(session->slide);
		return false;
	}
	
	// copy the uint32 buffer into a byte buffer of the same size 
	// thus every uint32 value is divided into four separate RGBA bytes
	std::memcpy(session->buffer, tileBuffer, session->bufferSize);

	// free the buffer
	free(tileBuffer);

	return true;
}

/*********************************************************************************************************************/
/*********************************************** Funktion: GetTileJP2C ***********************************************/
/*********************************************************************************************************************/



extern "C" __declspec(dllexport) BOOL GetTileJP2C(INT64 handle, INT32 level, INT32 x, INT32 y, BYTE** data, INT32* length)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** False, wenn kein gültiges Handle angegeben ist **************************************************************
	if (handle == 0) return false;

	// Use the OpenSlide code:
	if (!GetOpenSlideTile(handle, level, x, y)) {
		return false;
	}

	session = (Session*)handle;

	level = session->bufferSize;
	
	//*** Den Zeiger auf die Bilddaten ünernehmen *********************************************************************
	std::memcpy(data, session->buffer, session->bufferSize);

	//*** Default: true ***********************************************************************************************
	return true;
}


/*********************************************************************************************************************/
/************************************************* Funktion: GetTile *************************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) BOOL GetTileDecoded(INT64 handle, INT32 level, INT32 x, INT32 y, BYTE* data)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;

	//*** False, wenn kein gültiges Handle angegeben ist **************************************************************
	if (handle == 0) return false;

	// Use the OpenSlide code:
	if (!GetOpenSlideTile(handle, level, x, y)) {
		return false;
	}

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;
	
	// copy data from the buffer into the output argument
	std::memcpy(data, session->buffer, session->bufferSize);

	// no need to convert the data from BGR to RGBA, as openslide already provides output as 32-bit RGBA buffer
	
	//*** Default: true ***********************************************************************************************
	return true;
}


/*********************************************************************************************************************/
/******************************************** Funktion: GetSingleImageSize *******************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) BOOL GetSingleImageSize(INT64 handle, INT32 type, INT32* x, INT32* y)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;
	uint16 dir;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return false;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Das entsprechende ImageDirectory bestimmen ******************************************************************
	if (type == LABEL_IMAGE)
	{
		dir = session->labelImageDir;
	}
	else if (type == MACRO_IMAGE)
	{
		dir = session->macroImageDir;
	}
	else return false;

	//*** False zurückgeben, wenn das entsprechende Bild nicht vorhanden ist ******************************************
	if (dir == 0)
	{
		*x = 0;
		*y = 0;
		return false;
	}

	int64_t w, h;

	openslide_get_level0_dimensions(session->slide, &w, &h);

	*x = (INT32)w;
	*y = (INT32)h;

	//*** Ende ********************************************************************************************************
	return true;
}


/*********************************************************************************************************************/
/********************************************** Funktion: GetSingleImage *********************************************/
/*********************************************************************************************************************/

extern "C" __declspec(dllexport) BOOL GetSingleImage(INT64 handle, INT32 type, BYTE* buffer)
{
	//*** Variablen-Deklarationen *************************************************************************************
	Session* session;
	uint32 height;
	uint32 width;
	uint16 dir;

	//*** Verlassen, wenn kein gültiges Handle angegeben ist **********************************************************
	if (handle == 0) return false;

	//*** Die entsprechende Session-Struktur bestimmen ****************************************************************
	session = (Session*)handle;

	//*** Das entsprechende ImageDirectory bestimmen ******************************************************************
	if (type == LABEL_IMAGE)
	{
		dir = session->labelImageDir;
	}
	else if (type == MACRO_IMAGE)
	{
		dir = session->macroImageDir;
	}
	else return false;

	//*** False, wenn das entsprechende Bild nicht vorhanden ist ******************************************************
	if(dir==0) return false;

	if (handle == 0)
		return false;

	session = (Session*)handle;

	width = session->imageWidth;
	height = session->imageHeight;

	uint32_t* imageBuffer = (uint32_t*)malloc(sizeof(uint32_t) * width * height);

	openslide_read_region(session->slide, imageBuffer, 0, 0, dir, width, height);

	std::memcpy(buffer, imageBuffer, 4 * width * height * sizeof(BYTE));

	//*** Ende ********************************************************************************************************
	return true;
}


/*********************************************************************************************************************/
/******************************************* Funktion: Convert24BgrTo32Argb ******************************************/
/*********************************************************************************************************************/

void Convert24BgrTo32Argb(unsigned char* src, unsigned char* dst, int width, int height)
{
	//*** Variablen-Deklaration ***************************************************************************************
	int posOld;
	int posNew;
	int strideo = width * 3;
	int striden = width * 4;

	//*** Für alle Pixel **********************************************************************************************
	for (int dy = 0; dy < height; dy++)
	{
		for (int dx = 0; dx < width; dx++)
		{
			//*** Position im jeweiligen Array bestimmen **************************************************************
			posOld = dy*strideo + dx * 3;
			posNew = dy*striden + dx * 4;

			//*** Farbkanäle vertauscht übernehmen und AlphaKanal hinzufügen ******************************************
			dst[posNew] = src[posOld + 2];
			dst[posNew + 1] = src[posOld + 1];
			dst[posNew + 2] = src[posOld];
			dst[posNew + 3] = 0xFF;
		}
	}
}


/*********************************************************************************************************************/
/********************************************** Funktion: YCbYrToARGB ************************************************/
/*********************************************************************************************************************/

void YCbYrToArgb(BYTE Y, BYTE Cb, BYTE Cr, BYTE* dest)
{
	//*** Variablen-Deklaration ***************************************************************************************
	double R, G, B;

	//*** RGB-Komponenten bestimmen ***********************************************************************************
	R = Y + 1.402*(Cr - 128);
	G = Y - 0.34414*(Cb - 128) - 0.71414*(Cr - 128);
	B = Y + 1.772*(Cb - 128);

	//*** Werte auf 0-255 beschneiden *********************************************************************************
	if (R > 255) R = 255;
	if (R<0) R = 0;
	if (G>255) G = 255;
	if (G<0) G = 0;
	if (B>255) B = 255;
	if (B < 0) B = 0;

	//*** Werte in das Zielarray schreiben ****************************************************************************
	dest[3] = 255;
	dest[2] = (BYTE)R;
	dest[1] = (BYTE)G;
	dest[0] = (BYTE)B;
}


/*********************************************************************************************************************/
/******************************************** Funktion: HuronTileToARGB **********************************************/
/*********************************************************************************************************************/

/* Konvertiert Kacheln mit Subsampling X=2 Y=1 (=> Y1Y2CbCrY3...000...) nach ARGB */
void Convert2Y1CbCrTileToArgb(unsigned char* source, unsigned char* destination, int width, int height)
{
	//*** Für alle Zeilen *********************************************************************************************
	for (int dy = 0; dy < height; dy++)
	{
		//*** Für jede Zweite Spalte **********************************************************************************
		for (int dx = 0; dx < width / 2; dx++)
		{
			//*** Die Komponeten zu 2 Pixeln einer Zeile einlesen und den Zeiger inkrementieren ***********************
			byte y1 = *source++;
			byte y2 = *source++;
			byte cb = *source++;
			byte cr = *source++;

			//*** Den ersten ARGB Pixel erzeugen **********************************************************************
			YCbYrToArgb(y1, cb, cr, destination);

			//*** Den Zeiger auf das Ziel-Array erhöhen ***************************************************************
			destination += 4;

			//*** Den zweiten ARGB Pixel erzeugen *********************************************************************
			YCbYrToArgb(y2, cb, cr, destination);

			//*** Den Zeiger auf das Ziel-Array erhöhen ***************************************************************
			destination += 4;
		}

		//*** Die Null-Bytes am Ende der Zeile überspringen ***********************************************************
		source += width;
	}
}


/*********************************************************************************************************************/
/****************************************** Funktion: Tile16BitGrayToArgb ********************************************/
/*********************************************************************************************************************/

void Convert16BitGreyToArgb(unsigned char* src, unsigned char* dst, int width, int height)
{
	//*** Variablen-Deklaration ***************************************************************************************
	int posOld = 0;
	int posNew = 0;
	int val = 0;

	//*** Für alle Pixel **********************************************************************************************
	for (int dy = 0; dy < height; dy++)
	{
		for (int dx = 0; dx < width; dx++)
		{
			val = src[posOld + 1];
			val = (val << 8) | src[posOld];
			val = val / 256;
			dst[posNew + 0] = val;
			dst[posNew + 1] = val;
			dst[posNew + 2] = val;
			dst[posNew + 3] = 255;
			posNew += 4;
			posOld += 2;
		}
	}
}


/*********************************************************************************************************************/
/*********************************************** Funktion: IsMacroImage **********************************************/
/*********************************************************************************************************************/

bool DescriptionContains(char* imageDescription, std::string searchString)
{
	//*** Variablen-Deklaration ***************************************************************************************
	std::string description("");
	size_t pos;
	size_t pos2 = std::string::npos;

	//*** Wenn keine Beschreibung vorhanden ist false zurückgeben ****************************************************
	if (imageDescription == NULL) return false;

	description.append(imageDescription);

	//*** Nach der Zeichkette macro iin der Beschreibung suchen *******************************************************
	pos = description.find(searchString);

	//*** Testen ob "macro" gefunden wurde ****************************************************************************
	if (pos == std::string::npos) return false;
	else return true;
}


/*********************************************************************************************************************/
/******************************************* Funktion: LevelToTiffDirectory ******************************************/
/*********************************************************************************************************************/

int LevelToTiffDirectory(Session* session, int level)
{
	if (session->baseLayerOffset == 0)
	{
		if (level == 0) return 0;
		else return level + 1;
	//	return level;
	}

	return session->baseLayerOffset + level;
}


/*********************************************************************************************************************/
/********************************************** Funktion: StringToFloat **********************************************/
/*********************************************************************************************************************/

float StringToFloat(std::string s)
{
	//*** Variablen-Deklaration ***************************************************************************************
	std::stringstream stream;
	float f;

	//*** Über den stringstream nach float kopieren *******************************************************************
	stream << s;
	stream >> f;

	return f;
}


/*********************************************************************************************************************/
/************************************************ Funktion: GetValue *************************************************/
/*********************************************************************************************************************/

bool GetValue(char* imageDescription, std::string key, std::string* value)
{
	//*** Variablen-Deklaration ***************************************************************************************
	std::string desc(imageDescription);
	size_t begin;
	size_t end;

	//*** Wenn keine ImageDesc angegeben 0 zurück *********************************************************************
	if (imageDescription == NULL) return false;

	//*** Nach dem key in der Beschreibung suchen *********************************************************************
	begin = desc.find(key);

	//*** Wenn nicht gefunden 0 zurück ********************************************************************************
	if (begin == std::string::npos) return false;

	//*** Das Ende der Key Value Beschreibung suchen ******************************************************************
	end = desc.find("|", begin);

	//*** Den Key überspringen ****************************************************************************************
	begin = begin + key.length();

	//*** Die Zeichkette mit dem Wert ausschneiden ********************************************************************
	*value = desc.substr(begin, end - begin);

	return true;
}


/*********************************************************************************************************************/
/************************************************* Funktion: GetDpi **************************************************/
/*********************************************************************************************************************/

INT32 GetDpi(char* imageDescription)
{
	//*** Variablen-Deklaration ***************************************************************************************
	std::string µpp_string = "";
	float µpp;
	int dpi;

	// this is a hard-coded value for the Camelyon16 slides
	µpp = 0.2430939999999998;

	//*** Von µpp nach dpi umrechnen **********************************************************************************
	dpi = (int)((1 / µpp) * 25400);

	return dpi;
}

extern "C" __declspec(dllexport) int Get() { return 43; }
/**********************************************************#**********************************************************/