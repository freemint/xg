//==============================================================================
//
// wind_util.c -- Several helper functions for window handling.
//
// Copyright (C) 2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2001-01-22 - Initial Version.
//==============================================================================
//
#include "window_P.h"
#include "Cursor.h"

#include <X11/Xproto.h>


//==============================================================================
BOOL
WindVisible (WINDOW * wind)
{
	// Returns True if the window and all of its anchestors are mapped.
	//...........................................................................
	
	BOOL vis;
	
	while ((vis = wind->isMapped) && (wind = wind->Parent));
	
	return vis;
}


//------------------------------------------------------------------------------
static inline short
_origin (WINDOW * wind, PXY * dst)
{
	WINDOW * w;
	while ((w = wind->Parent)) {
		dst->x += w->Rect.x;
		dst->y += w->Rect.y;
		if (w == &WIND_Root) break;
		else                 wind = w;
	}
	return wind->Handle;
}

//==============================================================================
short
WindOrigin (WINDOW * wind, PXY * dst)
{
	// Sets dst to the origin of the window in absolute screen coordinates and
	// returns the AES handle of its anchestor that is a top window.
	//...........................................................................
	
	*dst = *(PXY*)&wind->Rect;
	
	return _origin (wind, dst);
}

//==============================================================================
short
WindGeometry (WINDOW * wind, GRECT * dst, CARD16 border)
{
	// Sets dst to the rectangle of the window in absolute screen coordinates,
	// including the gven border width, and returns the AES handle of its
	// anchestor that is a top window.
	//...........................................................................
	
	*dst = wind->Rect;
	if (border) {
		dst->x -= border;
		dst->y -= border;
		border <<= 2;
		dst->w += border;
		dst->h += border;
	}
	return _origin (wind, (PXY*)dst);
}


//==============================================================================
PXY
WindPointerPos (WINDOW * wind)
{
	// Returns the mouse pointer position relative to the window.
	//...........................................................................
	
	PXY pos = *MAIN_PointerPos;
	
	if (!wind) {
		pos.x -= WIND_Root.Rect.x;
		pos.y -= WIND_Root.Rect.y;
	
	} else do {
		pos.x -= wind->Rect.x;
		pos.y -= wind->Rect.y;
	} while ((wind = wind->Parent));
	
	return pos;
}


//==============================================================================
//
// The following functions are private to  wind..-modules.

//------------------------------------------------------------------------------
BOOL
_Wind_IsInferior (WINDOW * wind, WINDOW * inferior)
{
	// Returns True if wind is an anchestor of or is itself the inferior.
	//...........................................................................
	
	while (inferior) {
		if (inferior == wind) return xTrue;
		inferior = inferior->Parent;
	}
	return xFalse;
}

//------------------------------------------------------------------------------
void
_Wind_Cursor (WINDOW * wind)
{
	// If window has a mouse cursor struct stored, set the mouse form to this.
	// Else find the next anchestor, which has one stored and uses that.
	//...........................................................................
	
	CURSOR * crsr = NULL;
	
	while (wind && !(crsr = wind->Cursor)) {
		wind = wind->Parent;
	}
	CrsrSelect (crsr);
}

//------------------------------------------------------------------------------
int
_Wind_PathStack (WINDOW ** stack, int * anc, WINDOW * beg, WINDOW * end)
{
	// Flatten the hirachycal relationship from window beg to window end into the
	// stack pointer list and returns the last valid stack entry (not number of!)
	// anc is set to:
	// anc == 0:   beg is an anchestor of end
	// anc == top: beg is an inferior of end
	// else:       stack[anc] is pointing to the least common anchestor of beg
	//             and end
	//...........................................................................
	
	int top = 0;
	
	if (!(stack[0] = beg)) {
		stack[++top] = &WIND_Root;
	} else while ((beg = beg->Parent)) {
		stack[++top] = beg;
	}
	*anc = top;
	
	if (end) {
		WINDOW * r_stk[32];
		int      num = 0;
		do {
			int n = top;
			do if (end == stack[n]) {
				*anc = top = n;
				while (num--) {
					stack[++top] = r_stk[num];
				}
				return top;
			} while (n--);
			r_stk[num++] = end;
		} while ((end = end->Parent));
	}
	return top;
}
