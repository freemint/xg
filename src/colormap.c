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


typedef struct {
	const char * name;
	RGB          rgb;
} CMAP_NAME;

static CARD8 _CMAP_TransGrey[32] = {
	G_BLACK,244,243,255,245,236, 58,246, 242,254,247,249,101,241,235,240,
	    239,248,238,144,234,237,253,252, 233,187,232,231,251,250,230,G_WHITE
};


//==============================================================================
void
CmapPalette (void)
{
	short mbuf[8] = { COLORS_CHANGED, ApplId(0), 0,0,0,0,0,0 };
	short rgb[3];
	int   i, c = 16;
	for (rgb[0] = 0; rgb[0] <= 1000; rgb[0] += 200) {
		for (rgb[1] = 0; rgb[1] <= 1000; rgb[1] += 200) {
			rgb[2] = (rgb[0] | rgb[1] ? 0 : 200);
			for ( ; rgb[2] <= 1000; rgb[2] += 200) {
				vs_color (GRPH_Vdi, c++, rgb);
			}
		}
	}
	for (i = 1; i < 31; i++) {
		rgb[0] = rgb[1] = rgb[2] = (1001 * i) /31;
		vs_color (GRPH_Vdi, _CMAP_TransGrey[i], rgb);
	}
	shel_write (SWM_BROADCAST, 0, 0, (void*)mbuf, NULL);
}


//==============================================================================
void
CmapInit(void)
{
	int i;
	
	if (GRPH_Depth == 1) {
		for (i =  1; i < 16; _CMAP_TransGrey[i++] = G_BLACK);
		for (i = 16; i < 31; _CMAP_TransGrey[i++] = G_WHITE);
	
	} else if (GRPH_Depth <= 4) {
		for (i =  1; i <  8; _CMAP_TransGrey[i++] = G_BLACK);
		for (i =  8; i < 16; _CMAP_TransGrey[i++] = G_LBLACK);
		for (i = 16; i < 24; _CMAP_TransGrey[i++] = G_LWHITE);
		for (i = 24; i < 31; _CMAP_TransGrey[i++] = G_WHITE);
		
	} else if (GRPH_Depth <= 8) {
		CmapPalette();
		
	} else { // TrueColor
		
	}
}


//------------------------------------------------------------------------------
static const RGB *
_Cmap_LookupName (const char * name, size_t len)
{
	static CMAP_NAME Cmap_NameDB[] = {
		#define ENTRY(name, r,g,b)\
			{name, {(r<<8)|r, (g<<8)|g, (b<<8)|b}}
		#include "color_db.c"
			{NULL, {0, 0, 0}} };
	CMAP_NAME * p = Cmap_NameDB;
	char buf[32];
	int  i;
	
	if (len >= sizeof(buf)) len = sizeof(buf) -1;
	for (i = 0; i < len; ++i) buf[i] = tolower(name[i]);
	buf[len] = '\0';
	
	while (p->name) {
		if (!strcmp (p->name, buf)) {
			return &p->rgb;
		}
		p++;
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
	
	} else if (max_p - min_p < 4) {
		int c = (src->r >> 8) & 0xF8;
		pixel = _CMAP_TransGrey[c/8];
		c    |= c >> 5;
		dst->r = dst->g = dst->b = PIXEL(c);
		
	} else if (GRPH_Depth == 8) {
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
	
	DEBUG (AllocColor," M:%lX rgb=%u,%u,%u", q->cmap, q->red, q->green, q->blue);
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
	xrgb * rgb = (xrgb*)(r +1);
	int i;
	
	DEBUG (QueryColors,"- M:%lX (%lu)", q->cmap, len);
	
	r->nColors = (clnt->DoSwap ? Swap16(len) : len);
	for (i = 0; i < len; ++i, ++pix) {
		CARD32 color = (clnt->DoSwap ? Swap32(*pix) : *pix);
		short  vrgb[3];
		vq_color (GRPH_Vdi, color, 1, vrgb);
		vrgb[0] = ((long)vrgb[0] * 256) /1001; rgb->red   = PIXEL(vrgb[0]);
		vrgb[1] = ((long)vrgb[1] * 256) /1001; rgb->green = PIXEL(vrgb[1]);
		vrgb[2] = ((long)vrgb[2] * 256) /1001; rgb->blue  = PIXEL(vrgb[2]);
		DEBUG (,"+- %lu", color);
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
