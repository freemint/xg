//==============================================================================
//
// colormap.h
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
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


#endif __COLORMAP_H__
