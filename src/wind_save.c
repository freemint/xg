//==============================================================================
//
// wind_save.c -- Save and restore save-under area.
//
// Copyright (C) 2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2001-03-01 - Initial Version.
//==============================================================================
//
#include "window_P.h"
#include "grph.h"
#include "x_gem.h"

#include <stdlib.h>
#include <stdio.h> //printf

#include <X11/Xproto.h>


BOOL         WIND_SaveDone   = xFalse;
PRECT        WIND_SaveArea[3];
CARD32       _WIND_SaveUnder  = 0ul;
static short _WIND_SaveHandle = 0;
static MFDB  _WIND_SaveMfdb   = { NULL, 0,0,0,0,0,0,0,0 };


//==============================================================================
void
WindSaveUnder (CARD32 id, GRECT * rect, short hdl)
{
	size_t width = (rect->w +15) /16;
	void * addr  = malloc (width *2 * rect->h * GRPH_Depth);
	if (addr) {
		MFDB    scrn = { NULL, };
		PRECT * src  = &WIND_SaveArea[0];
		PRECT * dst  = &WIND_SaveArea[1];
		if (_WIND_SaveMfdb.fd_addr) {
			free (_WIND_SaveMfdb.fd_addr);
		}
		WIND_SaveDone             = xFalse;
		_WIND_SaveUnder           = id;
		_WIND_SaveHandle          = -hdl;
		_WIND_SaveMfdb.fd_addr    = addr;
		_WIND_SaveMfdb.fd_w       = rect->w;
		_WIND_SaveMfdb.fd_h       = rect->h;
		_WIND_SaveMfdb.fd_wdwidth = width;
		_WIND_SaveMfdb.fd_nplanes = GRPH_Depth;
		src->rd.x = (src->lu.x = rect->x) + (dst->rd.x = rect->w -1);
		src->rd.y = (src->lu.y = rect->y) + (dst->rd.y = rect->h -1);
		dst->lu.x = 0;
		dst->lu.y = 0;
		v_hide_c    (GRPH_Vdi);
		vro_cpyfm (GRPH_Vdi, S_ONLY,
		           (short*)&WIND_SaveArea[0], &scrn, &_WIND_SaveMfdb);
		v_show_c    (GRPH_Vdi, 0);
		WIND_SaveArea[2] = *src;
	}
}

//==============================================================================
void
WindSaveFlush (BOOL restore)
{
	if (_WIND_SaveMfdb.fd_addr) {
		if (restore) {
			MFDB scrn = { NULL, };
			v_hide_c    (GRPH_Vdi);
			vro_cpyfm_p (GRPH_Vdi, S_ONLY,
			             (PXY*)&WIND_SaveArea[1], &_WIND_SaveMfdb, &scrn);
			v_show_c    (GRPH_Vdi, 0);
			WIND_SaveDone = (_WIND_SaveHandle <= 0);
		} else {
			WIND_SaveDone = xFalse;
		}
		free (_WIND_SaveMfdb.fd_addr);
		_WIND_SaveMfdb.fd_addr = NULL;
	} else if (!restore) {
		WIND_SaveDone = xFalse;
	}
	_WIND_SaveUnder = 0ul;
}
