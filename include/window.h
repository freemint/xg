//==============================================================================
//
// window.h
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-05 - Initial Version.
//==============================================================================
//
#ifndef ___WINDOW_H__
# define ___WINDOW_H__

#include "drawable.h"


#define ROOT_DEPTH   1


#ifndef __PXY
# define __PXY
typedef struct s_PXY {
	short x;
	short y;
} PXY;
#endif

#ifndef __GRECT
# define __GRECT
typedef struct s_GRECT {
	short x, y;
	short w, h;
} GRECT;
#endif


typedef struct {
	CARD32   Mask;
	p_CLIENT Client;
} WINDEVNT;

typedef struct s_WINDOW {
	XRSC(DRAWABLE, isWind);
	
	GRECT  Rect;           // W,H matches fd_w,fd_h
	CARD16 BorderWidth:16;
	INT16  Handle     :16;
	CARD16 Depth;          // matches fd_nplanes
	
	BOOL  ClassInOut  : 1;
	BOOL  Override    : 1;
	CARD8 BackingStore: 2;
	CARD8 WinGravity  : 4;
	CARD8 BitGravity  : 4;
	BOOL  SaveUnder   : 1;
	
	BOOL isMapped     : 1;
	BOOL hasBorder    : 1;
	BOOL hasBackGnd   : 1;
	BOOL hasBackPix   : 1;
	
	BOOL GwmParent    : 1;
	BOOL GwmDecor     : 1;
	BOOL GwmIcon      : 1;
	
	CARD16 nSelections:12;
	
	CARD32     BorderPixel;
	union {
		CARD32   Pixel;
		p_PIXMAP Pixmap;
	}          Back;
	
	CARD32     PropagateMask; // reverted do-not-propagate-mask
	union {
		WINDEVNT  Event; // Mask >= 0: use this
		struct {
			long     AllMasks;   // in both cases, holds all ORed event masks
			struct {
				        CARD16   Length;
				        WINDEVNT Event[1];
			} *      p;
		}         List; // AllMasks must be < 0
	}          u;
	p_WINDOW   Parent;
	p_WINDOW   PrevSibl, NextSibl;
	p_WINDOW   StackBot, StackTop;
	p_CURSOR   Cursor;
	p_PROPERTY Properties;
} WINDOW;

extern WINDOW WIND_Root;


void WindInit (BOOL initNreset);

void   WindDelete   (p_WINDOW , p_CLIENT);
CARD16 WindClipLock (p_WINDOW , CARD16 border, const GRECT * clip,
                                short n_clip, p_PXY orig, GRECT ** pBuf);
void   WindClipOff  (void);
void   WindPutMono     (p_WINDOW , p_GC , p_GRECT , p_MFDB src);
void   WindPutColor    (p_WINDOW , p_GC , p_GRECT , p_MFDB src);
void   WindDrawSection (p_WINDOW , const GRECT * sect);
BOOL   WindCirculate   (p_WINDOW , CARD8 place);
void   WindResize      (p_WINDOW , p_GRECT diff);
BOOL   WindMap         (p_WINDOW , BOOL visible);
void   WindUnmap       (p_WINDOW , BOOL by_conf);

// utility functions
BOOL   WindVisible    (p_WINDOW );
short  WindOrigin     (p_WINDOW , p_PXY   dst);
short  WindGeometry   (p_WINDOW , p_GRECT dst, CARD16 border);
PXY    WindPointerPos (p_WINDOW );

static inline p_WINDOW WindFind (CARD32 id) {
	p_WINDOW wind = DrawFind(id).Window;
	if ((DBG_XRSC_TypeError = (wind && !wind->isWind))) wind = NULL;
	return wind;
}


#endif ___WINDOW_H__
