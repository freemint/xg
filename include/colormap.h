//==============================================================================
//
// colormap.h
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-07-27 - Initial Version.
//==============================================================================
//
#ifndef __COLORMAP_H__
# define __COLORMAP_H__


typedef struct {
	CARD16 r, g, b;
} RGB;


void CmapInit(void);

void   CmapPalette (CARD16 handle);
CARD32 CmapLookup  (RGB * dst, const RGB * src);

static inline CARD16 CmapPixelIdx (CARD32 pixel, CARD16 depth)
{
	if (depth > 8) {
		extern CARD16 (*Cmap_PixelIdx) (CARD32 );
		pixel = Cmap_PixelIdx (pixel);
	}
	return pixel;
}


#endif __COLORMAP_H__
