//==============================================================================
//
// grph.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-05 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "tools.h"
#include "clnt.h"
#include "grph.h"
#include "colormap.h"
#include "font.h"
#include "window.h"
#include "event.h"
#include "gcontext.h"
#include "x_gem.h"
#include "gemx.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/cookie.h>

#include <X11/Xprotostr.h>


#define _CONST_DPI 72


#define DFLT_IMSK    (NoEventMask)

short GRPH_Handle = -1;
short GRPH_Vdi    = 0;
short GRPH_ScreenW, GRPH_ScreenH, GRPH_Depth;
short GRPH_muWidth, GRPH_muHeight;
int   GRPH_DepthNum = 1;
short GRPH_Format   = SCRN_INTERLEAVED;
short GRPH_Fonts    = 0x8000;

struct {
	xDepth      dpth;
	xVisualType visl;
}
__depth_msb_1 = {
	{ 1, 00, 1 }, { DFLT_VISUAL +0, StaticGray,  1,   2, 0,0,0 } },
__depth_msb_n = {
	{ 8, 00, 1 }, { DFLT_VISUAL +1, StaticColor, 8, 256, 0,0,0 } },
__depth_lsb_1, __depth_lsb_n,
* GRPH_DepthMSB[2] = { &__depth_msb_1, &__depth_msb_n },
* GRPH_DepthLSB[2] = { &__depth_lsb_1, &__depth_lsb_n };


//==============================================================================
BOOL
GrphInit(void)
{
	if (GRPH_Vdi <= 0) {
		int work_in[16] = { 1, SOLID,BLACK, MRKR_DOT,BLACK, 1,BLACK,
		                    FIS_SOLID,0,0, 2, 0, 0,0, 0,0 };
		int w_out[57], dummy;
		
		int hdl = GRPH_Handle = graf_handle (&dummy, &dummy, &dummy, &dummy);
		v_opnvwk (work_in, &hdl, w_out);
		if (hdl <= 0) {
			printf("\33pFATAL\33q can't initialize VDI! (%i)\n", hdl);
			return xFalse;
		}
		GRPH_Vdi      = hdl;
		GRPH_ScreenW  = w_out[0];
		GRPH_ScreenH  = w_out[1];
		GRPH_muWidth  = w_out[3];
		GRPH_muHeight = w_out[4];
		GRPH_Fonts   |= w_out[10];
		vq_extnd (hdl, 1, w_out);
		GRPH_Depth    = w_out[4];
		
		vst_alignment (hdl, TA_LEFT, TA_BASE, w_out, w_out);
		vsf_perimeter (hdl, PERIMETER_OFF);
	}
	return xTrue;
}

//==============================================================================
void
GrphExit(void)
{
	if (GRPH_Vdi > 0) {
		if (GRPH_Fonts > 0) {
			vst_unload_fonts (GRPH_Vdi, 0);
		}
		v_clsvwk (GRPH_Vdi);
	}
}


//==============================================================================
int
GrphSetup (void * format_arr)
{
	int             n_form = 1;
	xPixmapFormat * pfrm   = format_arr;
	CARD8           r_dpth = GRPH_DepthMSB[0]->dpth.depth;
	VisualID        r_visl = GRPH_DepthMSB[0]->visl.visualID;
	xWindowRoot   * root;
	int             i;
	
	CmapInit();
	
	pfrm->depth = pfrm->bitsPerPixel = 1;
	pfrm->scanLinePad                = PADD_BITS;
	if (GRPH_Depth > 1) {
		n_form++;
		pfrm++;
		pfrm->depth        = GRPH_Depth;
		pfrm->bitsPerPixel = (GRPH_Depth +1) & ~1;
		pfrm->scanLinePad  = PADD_BITS;
		
		GRPH_DepthNum++;
		GRPH_DepthMSB[1]->dpth.depth = r_dpth  = GRPH_Depth;
		GRPH_DepthMSB[1]->visl.colormapEntries = 1 << GRPH_Depth;
		if (GRPH_Depth >= 15) {
			GRPH_DepthMSB[1]->visl.bitsPerRGB   = 5;
			GRPH_DepthMSB[1]->visl.redMask      = 0x0000F800uL;
			GRPH_DepthMSB[1]->visl.greenMask    = 0x000003E0uL;
			GRPH_DepthMSB[1]->visl.blueMask     = 0x0000001FuL;
			GRPH_DepthMSB[1]->visl.class        = DirectColor;
		} else {
			GRPH_DepthMSB[1]->visl.bitsPerRGB   = pfrm->depth;
		}
		r_visl = GRPH_DepthMSB[1]->visl.visualID;
	}
	root = (xWindowRoot*)(pfrm +1);
	root->windowId         = ROOT_WINDOW;
	root->defaultColormap  = DFLT_COLORMAP;
	root->whitePixel       = WHITE;
	root->blackPixel       = BLACK;
	root->currentInputMask = DFLT_IMSK;
	root->pixWidth         = WIND_Root.Rect.w;
	root->pixHeight        = WIND_Root.Rect.h;
#ifdef _CONST_DPI
	root->mmWidth
	    = (((long)WIND_Root.Rect.w * 6502u + (_CONST_DPI /2)) / _CONST_DPI) >> 8;
	root->mmHeight
	    = (((long)WIND_Root.Rect.h * 6502u + (_CONST_DPI /2)) / _CONST_DPI) >> 8;
#else
	root->mmWidth          = ((long)WIND_Root.Rect.w * GRPH_muWidth +500) /1000;
	root->mmHeight         = ((long)WIND_Root.Rect.h * GRPH_muHeight +500) /1000;
#endif
	root->minInstalledMaps = 1;
	root->maxInstalledMaps = 1;
	root->backingStore     = NotUseful;
	root->saveUnders       = xFalse;
	root->rootVisualID     =                   r_visl;
	root->rootDepth        = WIND_Root.Depth = r_dpth;
	root->nDepths          = GRPH_DepthNum;
	
	for (i = 0; i < GRPH_DepthNum; ++i) {
		GRPH_DepthLSB[i]->dpth.depth    = GRPH_DepthMSB[i]->dpth.depth;
		GRPH_DepthLSB[i]->dpth.nVisuals = Swap16(GRPH_DepthMSB[i]->dpth.nVisuals);
		memcpy (&GRPH_DepthLSB[i]->visl, &GRPH_DepthMSB[i]->visl, sz_xVisualType);
		ClntSwap (&GRPH_DepthLSB[i]->visl, "v2:lll");
	}
	
	printf ("  AES id #%i at VDI #%i:%i [%i/%i] (%i*%i mm) %i plane%s\n",
	        ApplId(0), GRPH_Handle, GRPH_Vdi, WIND_Root.Rect.w, WIND_Root.Rect.h,
	        root->mmWidth,root->mmHeight,
	        GRPH_Depth, (GRPH_Depth == 1 ? "" : "s"));
	
	{	// search for EdDI ----------------------------------
		short (*func)(short) = NULL;
		if (!Getcookie (C_EdDI, (long*)&func) && func) {
			int w_out[273];
			register short ver = 0;
			__asm__ volatile ("
				clr.l		d0;
				jsr		(%1);
				move.w	d0, %0;
				"
				: "=d"(ver)                 // output
				: "a"(func)                 // input
				: "d0","d1","d2", "a0","a1" // clobbered
			);
			
			vq_scrninfo (GRPH_Vdi, w_out);
			GRPH_Format = w_out[0];
			
			printf ("  screen format (EdDI %X.%02X) is %s \n",
			        ver >>8, ver & 0xFF,
			        (GRPH_Format == SCRN_INTERLEAVED ? "interleaved planes" :
			         GRPH_Format == SCRN_STANDARD    ? "standard format" :
			         GRPH_Format == SCRN_PACKEDPIXEL ? "packed pixels" :
			         "<undefined>"));
		}
	}
	
	GRPH_Fonts &= 0x7FFF;
	GRPH_Fonts += vst_load_fonts (GRPH_Vdi, 0);
	FontInit (GRPH_Fonts);
	
	return n_form;
}


//==============================================================================
BOOL
GrphIntersect (p_GRECT dst, const struct s_GRECT * src)
{
#if 1
	register BOOL res = 1;
	__asm__ volatile ("
		movem.l	(%1), d0/d1; | a.x:a.y/a.w:a.h
		movem.l	(%2), d2/d3; | b.x:b.y/b.w:b.h
		add.w		d0, d1; | a.h += a.y
		add.w		d2, d3; | b.h += b.y
			cmp.w		d0, d2;
			ble.b		1f;   | ? b.y <= a.y
			move.w	d2, d0; | a.y = b.y
		1:	cmp.w		d1, d3;
			bge.b		2f;   | ? b.h >= a.h
			move.w	d3, d1; | a.h = b.h
		2:	sub.w		d0, d1; | a.h -= a.y
			bhi.b		3f;   | ? a.h > a.y
			clr.b		%0;
		3:
		swap		d0; | a.y:a.x
		swap		d1; | a.h:a.w
		add.w		d0, d1; | a.w += a.x
		swap		d2; | b.y:b.x
		swap		d3; | b.h:b.w
		add.w		d2, d3; | b.w += b.x
			cmp.w		d0, d2;
			ble.b		5f;   | ? b.x <= a.x
			move.w	d2, d0; | a.x = b.x
		5:	cmp.w		d1, d3;
			bge.b		6f;   | ? b.w >= a.w
			move.w	d3, d1; | a.w = b.w
		6:	sub.w		d0, d1; | a.w -= a.x
			bhi.b		7f;   | ? a.w > a.x
			clr.b		%0;
		7:
		swap		d0;
		swap		d1;
		movem.l	d0/d1, (%1); | a.x:a.y/a.w:a.h
		"
		: "=d"(res)           // output
		: "a"(dst),"a"(src)   // input
		: "d0","d1","d2","d3" // clobbered
	);
	return res;
# else
	short p = dst->x + dst->w -1;
	short q = src->x + src->w -1;
	dst->x = (dst->x >= src->x ? dst->x : src->x);
	dst->w = (p <= q ? p : q) - dst->x +1;
	p = dst->y + dst->h -1;
	q = src->y + src->h -1;
	dst->y = (dst->y >= src->y ? dst->y : src->y);
	dst->h = (p <= q ? p : q) - dst->y  +1;
	
	return (dst->w > 0  &&  dst->h > 0);
# endif
}

//==============================================================================
#ifndef INL_ISCT
BOOL
GrphIntersectP (p_PXY dst, const struct s_PXY * src)
{
# if 1
	register BOOL res = 1;
	__asm__ volatile ("
		movem.l	(%1), d0/d1; | a.x0:a.y0/a.x1:a.y1
		movem.l	(%2), d2/d3; | b.x0:b.y0/b.x1:b.y1
			cmp.w		d0, d2;
			ble.b		1f;   | ? b.y0 <= a.y0
			move.w	d2, d0; | a.y0 = b.y0
		1:	cmp.w		d1, d3;
			bge.b		2f;   | ? b.y1 >= a.y1
			move.w	d3, d1; | a.y1 = b.y1
		2:	cmp.w		d0, d1;
			bge.b		3f;   | ? a.y1 >= a.y0
			clr.b		%0;
		3:
		swap		d0; | a.y0:a.x0
		swap		d1; | a.y1:a.x1
		swap		d2; | b.y0:b.x0
		swap		d3; | b.y1:b.x1
			cmp.w		d0, d2;
			ble.b		5f;   | ? b.x0 <= a.x0
			move.w	d2, d0; | a.x0 = b.x0
		5:	cmp.w		d1, d3;
			bge.b		6f;   | ? b.x1 >= a.x1
			move.w	d3, d1; | a.x1 = b.x1
		6:	cmp.w		d0, d1;
			bge.b		7f;   | ? a.x1 >= a.x0
			clr.b		%0;
		7:
		swap		d0;
		swap		d1;
		movem.l	d0/d1, (%1); | a.x:a.y/a.w:a.h
		"
		: "=d"(res)           // output
		: "a"(dst),"a"(src)   // input
		: "d0","d1","d2","d3" // clobbered
	);
	return res;
# else
	if (dst[0].x < src[0].x) dst[0].x = src[0].x;
	if (dst[0].y < src[0].y) dst[0].y = src[0].y;
	if (dst[1].x > src[1].x) dst[1].x = src[1].x;
	if (dst[1].y > src[1].y) dst[1].y = src[1].y;
	
	return (dst[0].x <= dst[1].x  &&  dst[0].y <= dst[1].y);
# endif
}
#endif INL_ISCT


//==============================================================================
void
GrphRasterI4 (short * dst, char * src, CARD16 width, CARD16 height)
{
	CARD8 val[16] = { 0,15,1,2,4,6,3,5, 7,8,9,10,12,14,11,13 };
	
	while (height--) {
		int c = -1, w = (width +1) /2;
		
		while (w) {
			dst[0] = dst[1] = dst[2] = dst[3] = 0;
			c      = 15;
			do {
				register unsigned char  q = *(src++);
				register unsigned short p;
				p = val[q >> 4];
				dst[0] |=  (p      & 1) <<c;
				dst[1] |= ((p >>1) & 1) <<c;
				dst[2] |= ((p >>2) & 1) <<c;
				dst[3] |= ((p >>3) & 1) <<c;
				c--;
				p = val[q & 15];
				dst[0] |=  (p      & 1) <<c;
				dst[1] |= ((p >>1) & 1) <<c;
				dst[2] |= ((p >>2) & 1) <<c;
				dst[3] |= ((p >>3) & 1) <<c;
			} while (--w && c--);
			dst += 4;
		}
		src += (int)src & 1;
	}
}

//==============================================================================
void
GrphRasterI8 (short * dst, char * src, CARD16 width, CARD16 height)
{
	__asm__ volatile ("
		movea.l	%0, a0;
		movea.l	%1, a1;
		lea		%3, a2;
		
		1:
		move.w	%2, d7; | width counter
		
		4:|line
		moveq.l	#0, d0;
		moveq.l	#0, d1;
		moveq.l	#0, d2;
		moveq.l	#0, d3;
		moveq.l	#15, d6; | cluster counter
		
		5:|cluster
		moveq.l	#0, d4;
		move.b	(a0)+, d4;
		beq		7f;
		cmpi.b	#1, d4;
		bne.s		6f;
		move.l	#0x00010001, d5; | move.b	#255, d4;
		lsl.l		d6, d5;
		or.l		d5, d0;
		or.l		d5, d1;
		or.l		d5, d2;
		or.l		d5, d3;
		bra		7f;
		6:
		lsl.l  #8, d4;
		move.l d4, d5;  andi.w #0x0300, d5;  lsl.l #7, d5;  rol.w #1, d5;
		swap   d5;      lsl.l  d6, d5;       or.l  d5, d0;
		move.l d4, d5;  andi.w #0x0C00, d5;  lsl.l #5, d5;  rol.w #1, d5;
		swap   d5;      lsl.l  d6, d5;       or.l  d5, d1;
		move.l d4, d5;  andi.w #0x3000, d5;  lsl.l #3, d5;  rol.w #1, d5;
		swap   d5;      lsl.l  d6, d5;       or.l  d5, d2;
		move.l d4, d5;  andi.w #0xC000, d5;  lsl.l #1, d5;  rol.w #1, d5;
		swap   d5;      lsl.l  d6, d5;       or.l  d5, d3;
		7:
		subq.w	#1, d7;
		dbeq		d6, 5b|cluster
		
		movem.l	d0-d3, (a1);
		adda.w	#16, a1;
		tst.w		d7;
		bgt		4b|line
		
		sub.w		#1, (a2);
		ble		9f|end
		
		move		a0, d4;
		btst		#0, d4;
		beq		1b;
		addq.w	#1, a0;
		bra		1b;
		9:
		"
		:                                          // output
		: "g"(src),"g"(dst),"g"(width),"g"(height) // input
		: "d0","d1","d2","d3","d4","d5","d6","d7", "a0","a1","a2" // clobbered
	);
}

//==============================================================================
void
GrphRasterP8 (char * dst, char * src, CARD16 width, CARD16 height)
{
	int i;
	while (height--) {
		for (i = 0; i < width; i++) {
			dst[i] = (src[i] == 1 ? 255 : src[i]);
		}
		src += (width +1) & ~1;
		dst += (width +15) & ~15;
	}
}


//==============================================================================
//
// Byte order depend Callback Functions

//------------------------------------------------------------------------------
void
FT_Grph_ShiftArc_MSB (const p_PXY origin, xArc * arc, size_t num, short mode)
{
	while (num--) {
		if (mode == ArcChord) {
			arc->angle1 = 0;
			arc->angle2 = 3600;
		} else {
			INT16 beg = (INT16)(((long)arc->angle2 *5) >> 5);
			INT16 end = (INT16)(((long)arc->angle1 *5) >> 5);
			arc->angle1 = (beg > 0 ? (beg >=  3600 ? 0    : 3600 -beg)
			                       : (beg <= -3600 ? 3600 :      -beg));
			arc->angle2 = (end > 0 ? (end >=  3600 ? 0    : 3600 -end)
			                       : (end <= -3600 ? 3600 :      -end));
		}
		arc->x += origin->x + (arc->width  /= 2);
		arc->y += origin->y + (arc->height /= 2);
		arc++;
	}
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void
FT_Grph_ShiftArc_LSB (const p_PXY origin, xArc * arc, size_t num, short mode)
{
	while (num--) {
		if (mode == ArcChord) {
			arc->angle1 = 0;
			arc->angle2 = 3600;
		} else {
			INT16 beg = (INT16)(((long)Swap16(arc->angle2) *5) >> 5);
			INT16 end = (INT16)(((long)Swap16(arc->angle1) *5) >> 5);
			arc->angle1 = (beg > 0 ? (beg >=  3600 ? 0    : 3600 -beg)
			                       : (beg <= -3600 ? 3600 :      -beg));
			arc->angle2 = (end > 0 ? (end >=  3600 ? 0    : 3600 -end)
			                       : (end <= -3600 ? 3600 :      -end));
		}
		arc->width  = Swap16(arc->width)  /2;
		arc->height = Swap16(arc->height) /2;
		arc->x = Swap16(arc->x) + origin->x + arc->width;
		arc->y = Swap16(arc->y) + origin->y + arc->height;
		arc++;
	}
}

//------------------------------------------------------------------------------
void
FT_Grph_ShiftPnt_MSB (const p_PXY origin, p_PXY pxy, size_t num, short mode)
{
	if (!num) return;
	
	if (origin) {
		pxy->x += origin->x;
		pxy->y += origin->y;
	}
	if (mode == CoordModePrevious) {
		while (--num) {
			PXY * prev = pxy++;
			pxy->x += prev->x;
			pxy->y += prev->y;
		}
	
	} else if (origin) { // && CoordModeOrigin
		while (--num) {
			pxy++;
			pxy->x += origin->x;
			pxy->y += origin->y;
		}
	}
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void
FT_Grph_ShiftPnt_LSB (const p_PXY origin, p_PXY pxy, size_t num, short mode)
{
	if (!num) return;
	
	if (origin) {
		pxy->x = Swap16(pxy->x) + origin->x;
		pxy->y = Swap16(pxy->y) + origin->y;
	} else {
		SwapPXY (pxy, pxy);
	}
	if (mode == CoordModePrevious) {
		while (--num) {
			PXY * prev = pxy++;
			pxy->x = Swap16(pxy->x) + prev->x;
			pxy->y = Swap16(pxy->y) + prev->y;
		}
	
	} else if (origin) { // && CoordModeOrigin
		while (--num) {
			pxy++;
			pxy->x = Swap16(pxy->x) + origin->x;
			pxy->y = Swap16(pxy->y) + origin->y;
		}
	
	} else { // CoordModeOrigin && !origin
		while (--num) {
			pxy++;
			SwapPXY (pxy, pxy);
		}
	}
}

//------------------------------------------------------------------------------
void
FT_Grph_ShiftR2P_MSB (const p_PXY origin, p_GRECT rct, size_t num)
{
	if (origin) {
		while (num--) {
			rct->w += (rct->x += origin->x) -1;
			rct->h += (rct->y += origin->y) -1;
			rct++;
		}
	} else {
		while (num--) {
			rct->w += rct->x -1;
			rct->h += rct->y -1;
			rct++;
		}
	}
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void
FT_Grph_ShiftR2P_LSB (const p_PXY origin, p_GRECT rct, size_t num)
{
	if (origin) {
		while (num--) {
			rct->w = Swap16(rct->w) + (rct->x = Swap16(rct->x) + origin->x) -1;
			rct->h = Swap16(rct->h) + (rct->y = Swap16(rct->y) + origin->y) -1;
			rct++;
		}
	} else {
		while (num--) {
			rct->w = Swap16(rct->w) + (rct->x = Swap16(rct->x)) -1;
			rct->h = Swap16(rct->h) + (rct->y = Swap16(rct->y)) -1;
			rct++;
		}
	}
}



//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_QueryBestSize (CLIENT * clnt, xQueryBestSizeReq * q)
{
	ClntReplyPtr (QueryBestSize, r);
	
	PRINT (- X_QueryBestSize," #%i of D:%lX (%u/%u)",
	       q->class, q->drawable, q->width, q->height);
	
	r->width  = 16;
	r->height = 16;
	
	ClntReply (QueryBestSize,, "P");
}
