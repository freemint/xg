//==============================================================================
//
// colormap.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-07-27 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "Request.h"
#include "tools.h"
#include "clnt.h"
#include "grph.h"
#include "colormap.h"
#include "x_gem.h"

#include <stdio.h> // printf
#include <string.h>
#include <ctype.h>

#include <X11/X.h>


static CARD8 _CMAP_TransGrey[32] = {
	G_BLACK,244,243,255,245,236, 58,246, 242,254,247,249,101,241,235,240,
	    239,248,238,144,234,237,253,252, 233,187,232,231,251,250,230,G_WHITE
};
static RGB   _CMAP_VdiRGB[256];


//==============================================================================
void
CmapPalette (CARD16 handle)
{
	short i, hdl;
	RGB * rgb;
	
	if (handle) {
		i   = 0;
		hdl = handle;
	} else {
		i   = 16;
		hdl = GRPH_Vdi;
	}
	rgb = _CMAP_VdiRGB + i;
	for ( ; i < 256; vs_color (GRPH_Vdi, i++, (short*)(rgb++)));
	
	if (!handle) {
		short mbuf[8] = { COLORS_CHANGED, ApplId(0), 0,0,0,0,0,0 };
		shel_write (SWM_BROADCAST, 0, 0, (void*)mbuf, NULL);
	}
}


//==============================================================================
void
CmapInit(void)
{
	int i;
	
	vq_color (GRPH_Vdi, G_WHITE, 1, (short*)&_CMAP_VdiRGB[G_WHITE]);
	vq_color (GRPH_Vdi, G_BLACK, 1, (short*)&_CMAP_VdiRGB[G_BLACK]);
	
	if (GRPH_Depth > 1) {
		RGB * rgb = _CMAP_VdiRGB +2;
		for (i = 2; i < 16; vq_color (GRPH_Vdi, i++, 1, (short*)(rgb++)));
	
		if (GRPH_Depth <= 4) {
			for (i =  1; i <  8; _CMAP_TransGrey[i++] = G_BLACK);
			for (i =  8; i < 16; _CMAP_TransGrey[i++] = G_LBLACK);
			for (i = 16; i < 24; _CMAP_TransGrey[i++] = G_LWHITE);
			for (i = 24; i < 31; _CMAP_TransGrey[i++] = G_WHITE);
		
		} else {
			int r, g, b = 200;
			for (r = 0; r <= 1000; r += 200) {
				for (g = 0; g <= 1000; g += 200) {
					for ( ; b <= 1000; b += 200) {
						rgb->r = r;
						rgb->g = g;
						rgb->b = b;
						rgb++;
					}
					b = 0;
				}
			}
			for (i = 1; i < 31; i++) {
				rgb = _CMAP_VdiRGB + _CMAP_TransGrey[i];
				rgb->r = rgb->g = rgb->b = (1001 * i) /31;
			}
			CmapPalette (0);
		}
	}
}


//------------------------------------------------------------------------------
static const RGB *
_Cmap_LookupName (const char * name, size_t len)
{
	typedef struct {
		const char * name;
		RGB          rgb;
	} CMAP_NAME;
	
	static CMAP_NAME Cmap_NameDB[] = {
		#define ENTRY(name, r,g,b)\
			{name, {(r<<8)|r, (g<<8)|g, (b<<8)|b}}
		#include "color_db.c"
	};
	short beg = 0;
	short end = (sizeof(Cmap_NameDB) / sizeof(*Cmap_NameDB)) -1;
	char  buf[32];
	int   i;
	
	if (len >= sizeof(buf)) len = sizeof(buf) -1;
	for (i = 0; i < len; ++i) buf[i] = tolower(name[i]);
	buf[len] = '\0';
	
	while (end >= beg) {
		short num = (end + beg) /2;
		short dir = strcmp (buf, Cmap_NameDB[num].name);
		if      (dir > 0) beg  = num +1;
		else if (dir < 0) end  = num -1;
		else              return &Cmap_NameDB[num].rgb;
	}
	return NULL;
}

//------------------------------------------------------------------------------
CARD32
CmapLookup (RGB * dst, const RGB * src)
{
#	define PIXEL(c) ((c << 8) | c)
	CARD8  max_p = (src->r >= src->b ? src->r >= src->g ? src->r : src->g
	                                 : src->b >= src->g ? src->b : src->g) >> 8;
	CARD8  min_p = (src->r <= src->b ? src->r <= src->g ? src->r : src->g
	                                 : src->b <= src->g ? src->b : src->g) >> 8;
	CARD32 pixel;
	
	if (GRPH_Depth == 1) {
		if ((long)src->r + src->g + src->b > 98302uL) {
			pixel                    = G_WHITE;
			dst->r = dst->g = dst->b = 0x0000;
		} else {
			pixel                    = G_BLACK;
			dst->r = dst->g = dst->b = 0xFFFF;
		}
	
	} else if (max_p - min_p < 12) {
		int c = (((CARD32)(src->g << 1) + src->r + src->b) >> 10) & 0xF8;
		pixel = _CMAP_TransGrey[c>>3];
		c    |= c >> 5;
		dst->r = dst->g = dst->b = PIXEL(c);
		
	} else if (GRPH_Depth >= 8) {
		CARD16 val[] = { 0,PIXEL(49),PIXEL(99),PIXEL(156),PIXEL(206),PIXEL(255) };
		BYTE r = ((src->r >> 7) *3) >> 8,
		     g = ((src->g >> 7) *3) >> 8,
		     b = ((src->b >> 7) *3) >> 8;
		pixel  = 15 + ((r *6 +g) *6 +b);
		dst->r = val[r];
		dst->g = val[g];
		dst->b = val[b];
		
	} else {
		pixel = ((src->r & 0xC000)>>4) | ((src->g & 0xC000)>>8)
		      | ((src->b & 0xC000)>>12);
		#define X 0xFFFF
		#define H 0x8000
		#define SET(R,G,B, C) dst->r = R; dst->g = G; dst->b = B; pixel = C; break
		
		switch (pixel) {
			
			case 0xC00: case 0xC04: case 0xC40: case 0xC44: SET (X,0,0, G_RED);
			case 0x800: case 0x400:                         SET (H,0,0, G_LRED);
			case 0x0C0: case 0x0C4: case 0x4C0: case 0x4C4: SET (0,X,0, G_GREEN);
			case 0x080: case 0x040:                         SET (0,H,0, G_LGREEN);
			case 0x00C: case 0x04C: case 0x40C: case 0x44C: SET (0,0,X, G_BLUE);
			case 0x008: case 0x004:                         SET (0,0,H, G_LBLUE);
			
			case 0xCC0: case 0xC80: case 0x8C0: case 0xCC4: SET (X,X,0, G_YELLOW);
			case 0x880: case 0x840: case 0x480: case 0x440: SET (H,H,0, G_LYELLOW);
			case 0xC0C: case 0xC08: case 0x80C: case 0xC4C: SET (X,0,X, G_MAGENTA);
			case 0x808: case 0x804: case 0x408: case 0x404: SET (H,0,H, G_LMAGENTA);
			case 0x0CC: case 0x0C8: case 0x08C: case 0x4CC: SET (0,X,X, G_CYAN);
			case 0x088: case 0x084: case 0x048: case 0x044: SET (0,H,H, G_LCYAN);
			
			default: {
				pixel = (long)src->r + src->g + src->b;
				if (pixel <= 0x17FFE) {
					if (pixel <= 0x0BFFF) {
						dst->r = dst->g = dst->b = 0x3FFF; pixel = G_BLACK;
					} else {
						dst->r = dst->g = dst->b = 0x7FFF; pixel = G_LBLACK;
					}
				} else {
					if (pixel <= 0x23FFE) {
						dst->r = dst->g = dst->b = 0xBFFF; pixel = G_LWHITE;
					} else {
						dst->r = dst->g = dst->b = 0xFFFF; pixel = G_WHITE;
					}
				}
			}
		}
	}
	return pixel;
}


//==============================================================================
//
// Callback Functions


//------------------------------------------------------------------------------
void
RQ_CreateColormap (CLIENT * clnt, xCreateColormapReq * q)
{
	PRINT (- X_CreateColormap," M:%lX W:%lX V:%lX alloc=%s",
	       q->mid, q->window, q->visual, (q->alloc ? "All" : "None"));
}


//------------------------------------------------------------------------------
void
RQ_FreeColormap (CLIENT * clnt, xFreeColormapReq * q)
{
	PRINT (- X_FreeColormap," M:%lX", q->id);
}


//------------------------------------------------------------------------------
void
RQ_CopyColormapAndFree (CLIENT * clnt, xCopyColormapAndFreeReq * q)
{
	PRINT (- X_CopyColormapAndFree," M:%lX from M:%lX", q->mid, q->srcCmap);
}


//------------------------------------------------------------------------------
void
RQ_InstallColormap (CLIENT * clnt, xInstallColormapReq * q)
{
	PRINT (- X_InstallColormap," M:%lX", q->id);
}


//------------------------------------------------------------------------------
void
RQ_UninstallColormap (CLIENT * clnt, xUninstallColormapReq * q)
{
	PRINT (- X_UninstallColormap," M:%lX", q->id);
}


//------------------------------------------------------------------------------
void
RQ_ListInstalledColormaps (CLIENT * clnt, xListInstalledColormapsReq * q)
{
	PRINT (- X_ListInstalledColormaps," W:%lX", q->id);
}


//------------------------------------------------------------------------------
void
RQ_AllocColor (CLIENT * clnt, xAllocColorReq * q)
{
	ClntReplyPtr (AllocColor, r);
	
	r->pixel = CmapLookup ((RGB*)&r->red, (RGB*)&q->red);
	
	ClntReply (AllocColor,, ":.2l");
	
	DEBUG (AllocColor," M:%lX rgb=%04X,%04X,%04X",
	       q->cmap, q->red, q->green, q->blue);
}


//------------------------------------------------------------------------------
void
RQ_AllocNamedColor (CLIENT * clnt, xAllocNamedColorReq * q)
{
	ClntReplyPtr (AllocNamedColor, r);
	const RGB * rgb = _Cmap_LookupName ((char*)(q +1), q->nbytes);
	
	if (!rgb) {
		Bad(Name,, AllocNamedColor,);
	
	} else {
		r->exactRed     = rgb->r;
		r->exactGreen   = rgb->g;
		r->exactBlue    = rgb->b;
		r->pixel        = CmapLookup ((RGB*)&r->screenRed, rgb);
		
		ClntReply (AllocNamedColor,, "l:::");
	}
	PRINT (AllocNamedColor," M:%lX '%.*s'", q->cmap, q->nbytes, (char*)(q +1));
}


//------------------------------------------------------------------------------
void
RQ_AllocColorCells (CLIENT * clnt, xAllocColorCellsReq * q)
{
//	ClntReplyPtr (AllocColorCells, r);
	
	PRINT (- X_AllocColorCells," M:%lX %i/%i", q->cmap, q->colors, q->planes);
	
	Bad(Alloc,, AllocColorCells,);
	/*
	r->nPixels = q->colors;
	r->nMasks  = q->planes;
	
	ClntReply (AllocColorCells, (r->nPixels + r->nPixels) *4, ":");
	*/
}


//------------------------------------------------------------------------------
void
RQ_AllocColorPlanes (CLIENT * clnt, xAllocColorPlanesReq * q)
{
	PRINT (- X_AllocColorPlanes," M:%lX %i rgb=%u,%u,%u",
	       q->cmap, q->colors, q->red, q->green, q->blue);
}


//------------------------------------------------------------------------------
void
RQ_FreeColors (CLIENT * clnt, xFreeColorsReq * q)
{
//	PRINT (- X_FreeColors," M:%lX 0x%lX", q->cmap, q->planeMask);
}


//------------------------------------------------------------------------------
void
RQ_StoreColors (CLIENT * clnt, xStoreColorsReq * q)
{
	size_t len  = ((q->length *4) - sizeof (xStoreColorsReq))
	              / sizeof(xColorItem);
	
	PRINT (- X_StoreColors," M:%lX (%lu)", q->cmap, len);
}


//------------------------------------------------------------------------------
void
RQ_StoreNamedColor (CLIENT * clnt, xStoreNamedColorReq * q)
{
	PRINT (- X_StoreNamedColor," M:%lX 0x%lX='%.*s'",
	       q->cmap, q->pixel, q->nbytes, (char*)(q +1));
}


//------------------------------------------------------------------------------
void
RQ_QueryColors (CLIENT * clnt, xQueryColorsReq * q)
{
	size_t   len = ((q->length *4) - sizeof (xQueryColorsReq)) / sizeof(CARD32);
	CARD32 * pix = (CARD32*)(q +1);
	ClntReplyPtr (QueryColors, r);
	xrgb * dst = (xrgb*)(r +1);
	int i;
	
	DEBUG (QueryColors,"- M:%lX (%lu)", q->cmap, len);
	
	r->nColors = (clnt->DoSwap ? Swap16(len) : len);
	for (i = 0; i < len; i++, pix++) {
		CARD32 pixel = (clnt->DoSwap ? Swap32(*pix) : *pix);
		RGB  * rgb   = _CMAP_VdiRGB +pixel;
		short  red   = ((long)rgb->r * 256) /1001,
		       green = ((long)rgb->g * 256) /1001,
		       blue  = ((long)rgb->b * 256) /1001;
		dst->red   = PIXEL(red);
		dst->green = PIXEL(green);
		dst->blue  = PIXEL(blue);
		rgb++;
		DEBUG (,"+- %lu", pixel);
	}
	DEBUG (,"+");
	
	ClntReply (QueryColors, (sizeof(xrgb) * len), ".");
}


//------------------------------------------------------------------------------
void
RQ_LookupColor (CLIENT * clnt, xLookupColorReq * q)
{
	ClntReplyPtr (LookupColor, r);
	const RGB * rgb = _Cmap_LookupName ((char*)(q +1), q->nbytes);
	
	if (!rgb) {
		Bad(Name,, LookupColor,);
	
	} else {
		r->exactRed     = rgb->r;
		r->exactGreen   = rgb->g;
		r->exactBlue    = rgb->b;
		CmapLookup ((RGB*)&r->screenRed, rgb);
		
		ClntReply (LookupColor,, ":::");
	}

	DEBUG (LookupColor," M:%lX '%.*s'", q->cmap, q->nbytes, (char*)(q +1));
}
