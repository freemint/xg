#ifndef __GCONTEXT_H__
#	define __GCONTEXT_H__

#include "fontable.h"

#include <X11/X.h>
#include <X11/Xproto.h>


#ifndef __PXY
# define __PXY
typedef struct s_PXY {
	short x;
	short y;
} PXY;
#endif

typedef struct s_GC {
	XRSC(FONTABLE, isFont);
	
	struct s_FONTFACE * FontFace;
	short               FontIndex   :16;
	unsigned            FontEffects : 3;
	unsigned            FontPoints  :13;
	unsigned            FontWidth;
	
	CARD16   Depth;
	CARD8    Function;
	BOOL     GraphExpos;
	CARD8    SubwindMode;
	CARD8    DashList;
	CARD16   DashOffset;
	CARD16   LineWidth;
	CARD8    LineStyle, CapStyle, JoinStyle;
	CARD8    FillStyle, FillRule;
	CARD8    ArcMode;
	CARD32   PlaneMask, Foreground, Background;
	p_PIXMAP Tile, Stipple, ClipMask;
	PXY      TileStip,      Clip;
	short    Vdi;
} GC;


void GcntDelete (p_GC , p_CLIENT);

void GcntActivate (GC * gc);

static inline p_GC GcntFind (CARD32 id) {
	p_GC gc = FablFind(id).Gc;
	if ((DBG_XRSC_TypeError = (gc && gc->isFont))) gc = NULL;
	return gc;
}


#endif __GCONTEXT_H__
