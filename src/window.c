#include "window_P.h"
#include "x_gem.h"
#include "wmgr.h"
#include "event.h"
#include "pixmap.h"
#include "gcontext.h"
#include "Property.h"
#include "Atom.h"
#include "selection.h"
#include "Cursor.h"
#include "grph.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <X11/Xatom.h>


WINDOW WIND_Root = {
	{NULL}, xTrue, ROOT_WINDOW,   // NULL,
	{ 0,0, 0,0 }, 0u, 0, ROOT_DEPTH,
	xTrue, xTrue, NotUseful, CenterGravity, ForgetGravity, xFalse,
	xTrue, xFalse, xFalse, xFalse,  xFalse, xFalse, xFalse,   0u,
	BLACK, {WHITE},   NoEventMask, {{ NoEventMask, NULL }},
	NULL, NULL,NULL, NULL,NULL, NULL, NULL
};

CARD16 _WIND_OpenCounter = 0;

static short _MAP_Inc, _MAP_IncX, _MAP_IncY;
static short _WIND_RootX2, _WIND_RootY2;


//==============================================================================
void
WindInit (BOOL initNreset)
{
	if (initNreset) {
		wind_get_work (0, &WIND_Root.Rect);
		_MAP_Inc = WIND_Root.Rect.y -1;
		_WIND_RootX2 = WIND_Root.Rect.x + WIND_Root.Rect.w -1;
		_WIND_RootY2 = WIND_Root.Rect.y + WIND_Root.Rect.h -1;
		
	} else {
		if (WIND_Root.StackBot) {
			WINDOW * w = WIND_Root.StackBot;
			printf ("\33pFATAL\33q root window has still children!\n");
			while (w) {
				char w_f[16] = "(-)", w_l[16] = "(-)";
				if (w->StackBot) sprintf (w_f, "W:%X", w->StackBot->Id);
				if (w->StackTop) sprintf (w_l, "W:%X", w->StackTop->Id);
				printf ("  W:%X -> %s .. %s %s\n",
				        w->Id, w_f, w_l, (w == WIND_Root.StackTop ? "= Last" : ""));
				w = w->NextSibl;
			}
			exit (1);
		}
		if (WIND_Root.u.List.AllMasks) {
			printf ("\33pFATAL\33q root window event list not empty!\n");
			exit (1);
		}
		if (WIND_Root.Properties) {
			printf ("  remove Propert%s:",
			        (WIND_Root.Properties->Next ? "ies" : "y"));
			while (WIND_Root.Properties) {
				printf (" '%s'(%s)",
				        ATOM_Table[WIND_Root.Properties->Name]->Name,
				        ATOM_Table[WIND_Root.Properties->Type]->Name);
				if (WIND_Root.Properties->Type == XA_STRING) {
					printf ("='%s'", WIND_Root.Properties->Data);
				}
				PropDelete (&WIND_Root.Properties);
			}
			printf (",\n");
		}
		if (WIND_Root.Cursor) {
			CrsrFree (WIND_Root.Cursor, NULL);
			WIND_Root.Cursor = NULL;
		}
	}
	_MAP_IncX = 0;
	_MAP_IncY = 0;
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
	*dst = *(PXY*)&wind->Rect;
	
	return _origin (wind, dst);
}

//==============================================================================
short
WindGeometry (WINDOW * wind, GRECT * dst, CARD16 border)
{
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
BOOL
WindVisible (WINDOW * wind)
{
	BOOL vis;
	
	while ((vis = wind->isMapped) && (wind = wind->Parent));
	
	return vis;
}


//==============================================================================
BOOL
WindButton (CARD16 prev_mask, int count)
{
	WINDOW * wind = _WIND_PointerRoot;
	PXY      w_xy;
	CARD8    butt_r = (prev_mask & Button1Mask ? Button1 :
	                   prev_mask & Button2Mask ? Button2 :
	                   prev_mask & Button3Mask ? Button3 :
	                   None);
	CARD8    butt_p = (MAIN_KeyButMask & Button1Mask ? Button1 :
	                   MAIN_KeyButMask & Button2Mask ? Button2 :
	                   MAIN_KeyButMask & Button3Mask ? Button3 :
	                   None);
	
	if (_WIND_PgrabWindow) {
		CARD32 w_id = 0;
		if (_WIND_OpenCounter && wind) {
			WindOrigin (wind, &w_xy);
			w_xy.x = MAIN_PointerPos->x - w_xy.x;
			w_xy.y = MAIN_PointerPos->y - w_xy.y;
			w_id   = wind->Id;

		} else {
			int hdl = wind_find (MAIN_PointerPos->x, MAIN_PointerPos->y);
			if (hdl >= 0) {
				GRECT curr;
				wind_get_curr (hdl, &curr);
				w_xy.x = MAIN_PointerPos->x - curr.x;
				w_xy.y = MAIN_PointerPos->y - curr.y;
				w_id   = hdl | ROOT_WINDOW;
			}
		}
		if (w_id) {
			WINDOW * wnd_r = NULL, * wnd_p = NULL;
			PXY e_xy;
			if (!wind) {
				wind = &WIND_Root;
			}
			if (butt_r
			    && (!_WIND_PgrabOwnrEv ||
			        !(wnd_r = EvntSearch (wind, _WIND_PgrabClient,
			                              ButtonReleaseMask)))
			    && (_WIND_PgrabEvents & ButtonReleaseMask)) {
				wnd_r = _WIND_PgrabWindow;
			}
			if (butt_p
			    && (!_WIND_PgrabOwnrEv ||
			        !(wnd_p = EvntSearch (wind, _WIND_PgrabClient,
			                              ButtonPressMask)))
			    && (_WIND_PgrabEvents & ButtonPressMask)) {
				wnd_p = _WIND_PgrabWindow;
			}
			if (wnd_r) {
				CARD32   c_id = None;
				WINDOW * w    = wind;
				do if (w->Parent == wnd_r) {
					c_id = w->Id;
					break;
				} while ((w = w->Parent));
				WindOrigin (wnd_r, &e_xy);
				e_xy.x = MAIN_PointerPos->x - e_xy.x;
				e_xy.y = MAIN_PointerPos->y - e_xy.y;
				_evnt_c (_WIND_PgrabClient, ButtonRelease,
				         w_id, wnd_r->Id, c_id,
				         *(CARD32*)&w_xy, *(CARD32*)&e_xy, butt_r);
			}
			if (wnd_p) {
				CARD32   c_id = None;
				WINDOW * w    = wind;
				do if (w->Parent == wnd_p) {
					c_id = w->Id;
					break;
				} while ((w = w->Parent));
				if (wnd_p != wnd_r) {
					WindOrigin (wnd_p, &e_xy);
					e_xy.x = MAIN_PointerPos->x - e_xy.x;
					e_xy.y = MAIN_PointerPos->y - e_xy.y;
				}
				_evnt_c (_WIND_PgrabClient, ButtonPress,
				         w_id, wnd_p->Id, c_id,
				         *(CARD32*)&w_xy, *(CARD32*)&e_xy, butt_p);
			}
		}
		return xFalse;
	
	} else if (!wind) {
		return WmgrButton();
	}
	
	WindOrigin (wind, &w_xy);
	w_xy.x = MAIN_PointerPos->x - w_xy.x;
	w_xy.y = MAIN_PointerPos->y - w_xy.y;
	
	do {
		if (butt_r) {
			EvntPropagate (wind, ButtonReleaseMask, ButtonRelease,
			               wind->Id, None, w_xy, butt_r);
		}
		if (butt_p) {
			EvntPropagate (wind, ButtonPressMask, ButtonPress,
			               wind->Id, None, w_xy, butt_p);
		}
		butt_r = butt_p;
	} while (--count > 0);
	
	return (xFalse);
}


//------------------------------------------------------------------------------
static void
_Wind_setup (CLIENT * clnt, WINDOW * w, CARD32 mask, CARD32 * val, CARD8 req)
{
	if (clnt->DoSwap) {
		CARD32   m = mask;
		CARD32 * p = val;
//		int      i = 0;
		while (m) {
			if (m & 1) {
				*p = Swap32(*p);
				p++;
//				i++;
			}
			m >>= 1;
		}
//		if (i) PRINT (,"+-\n          (%i):", i);
//	
//	} else if (mask) {
//		CARD32 m = mask;
//		int    i = 0;
//		while (m) {
//			if (m & 1) i++;
//			m >>= 1;
//		}
//		PRINT (,"+-\n          (%i):", i);
//	
//	} else {
//		return; // mask == 0, nothing to do
	}
	
	if (mask & CWBackPixmap) {
		if (*val != None  &&  *val != ParentRelative) {
			PIXMAP * pmap = PmapFind (*val);
			if (!pmap) {
//				PRINT(," ");
				Bad(Pixmap, *val, +req, "          invalid backbground.");
				return;
			} else if (pmap->Depth != w->Depth) {
//				PRINT(," ");
				Bad(Match,, +req, "          background depth %u not %u.",
				                  pmap->Depth, w->Depth);
				return;
			} else {
//				PRINT (,"+- bpix=%lX", *val);
				w->Back.Pixmap = PmapShare(pmap);
				w->hasBackPix  = xTrue;
				w->hasBackGnd  = xTrue;
			}
		} else {
//			PRINT (,"+- bpix=<none>");
		}
		val++;
	}
	if (mask & CWBackPixel) {
		// overrides prev
//		PRINT (,"+- bgnd=%lu", *val);
		w->Back.Pixel = *(val++) /*& ((1uL << (w->Depth)) -1)*/;
		w->hasBackGnd = xTrue;
	}
	if (mask & CWBorderPixmap) {
//		PRINT (,"+- fpix=P:%lX", *val);
		val++;
	}
	if (mask & CWBorderPixel) {
		// overrides pref
//		PRINT (,"+- fgnd=%lu", *val);
		w->BorderPixel = *(val++) /*& ((1uL << (w->Depth)) -1)*/;
		w->hasBorder   = xTrue;
	}
	if (mask & CWBitGravity) {
//		PRINT (,"+- bgrv=%u", (CARD8)*val);
		w->BitGravity = *(val++);
	}
	if (mask & CWWinGravity) {
//		PRINT (,"+- wgrv=%u", (CARD8)*val);
		w->WinGravity = *(val++);
	}
	if (mask & CWBackingStore) {
//		PRINT (,"+- bstr=%u", (CARD8)*val);
		w->BackingStore = *(val++);
	}
	if (mask & CWBackingPlanes) {
//		PRINT (,"+- bpln=%lX", *val);
		val++;
	}
	if (mask & CWBackingPixel) {
//		PRINT (,"+- bpix=%lX", *val);
		val++;
	}
	if (mask & CWOverrideRedirect) {
//		PRINT (,"+- rdir=%u", (CARD8)*val);
		w->Override = *(val++);
	}
	if (mask & CWSaveUnder) {
//		PRINT (,"+- save=%u", (CARD8)*val);
		w->SaveUnder = *(val++);
	}
	if (mask & CWEventMask) {
		CARD32 evnt = *val & AllEventMask;
		if (evnt) {
//			PRINT (,"+- evnt=%lX", evnt);
			EvntSet (w, clnt, evnt);
//			#define __V(e) if(evnt & e##Mask) PRINT (,"+-|" #e);
//			__V(ResizeRedirect)
//			__V(SubstructureRedirect)
//			__V(VisibilityChange)
			/*
			__V(ButtonPress)
			__V(ButtonRelease)
			__V(FocusChange)
			__V(KeyPress)
			__V(KeyRelease)
			__V(KeymapState)
			__V(Exposure)
			__V(PropertyChange)
			__V(ColormapChange)
			__V(OwnerGrabButton)
			*/
		} else if (w->u.List.AllMasks) {
//			PRINT (,"+- evnt=<remove>");
			EvntClr (w, clnt);
		}
		val++;
	}
	if (mask & CWDontPropagate) {
		w->PropagateMask = ~*(val++) & AllEventMask;
//		PRINT (,"+- dprp=%lX", ~w->PropagateMask & AllEventMask);
	}
	if (mask & CWColormap) {
//		PRINT (,"+- cmap=M:%lX", (CARD32)*val);
		val++;
	}
	if (mask & CWCursor) {
//		PRINT (,"+- crsr=");
		if (*val != None) {
			if (!(w->Cursor = CrsrGet (*val))) {
//				PRINT (,"+-<invalid>");
			} else {
//				PRINT (,"+-%lX", (CARD32)*val);
			}
		} else {
//			PRINT (,"+-<none>");
		}
		if (w == _WIND_PointerRoot) {
			CrsrSelect (w->Cursor);
		}
		val++;
	}
}


//==============================================================================
BOOL
WindMap (WINDOW * wind, BOOL visible)
{
	BOOL redraw    = xFalse;
	wind->isMapped = xTrue;
	
	if (wind->u.List.AllMasks & StructureNotifyMask) {
		EvntMapNotify (wind, StructureNotifyMask, wind->Id, wind->Override);
	}
	if (wind->Parent->u.List.AllMasks & SubstructureNotifyMask) {
		EvntMapNotify (wind->Parent,
		               SubstructureNotifyMask, wind->Id, wind->Override);
	}
	if (visible) {
		WINDOW * w = wind;
		BOOL     b = xTrue;
		while (1) {
			if (b) {
				if (w->isMapped && w->ClassInOut) {
					redraw = xTrue;
					if (w->u.List.AllMasks & VisibilityChangeMask) {
						EvntVisibilityNotify (w, VisibilityUnobscured);
					}
					if (w->StackTop) {
						w = w->StackTop;
						continue;
					}
				}
				if (w == wind) {
					break;
				}
			}
			if (w->PrevSibl) {
				w = w->PrevSibl;
				b = xTrue;
			} else if ((w = w->Parent) == wind) {
				break;
			} else {
				b = xFalse;
			}
		}
	}
	return redraw;
}

//==============================================================================
void
WindUnmap (WINDOW * wind, BOOL by_conf)
{
	wind->isMapped = xFalse;
	
	if (wind->u.List.AllMasks & StructureNotifyMask) {
		EvntUnmapNotify (wind, StructureNotifyMask, wind->Id, by_conf);
	}
	if (wind->Parent->u.List.AllMasks & SubstructureNotifyMask) {
		EvntUnmapNotify (wind->Parent,
		                 SubstructureNotifyMask, wind->Id, by_conf);
	}
}

//==============================================================================
void
WindDelete (WINDOW * wind, CLIENT * clnt)
{
	BOOL     enter = xFalse;
	WINDOW * pwnd  = wind->Parent, * next = NULL;
	CLIENT * owner;
	PXY      r_p, e_p;
	CARD16   n     = 0;
	
	if (clnt && pwnd->u.List.AllMasks) {
		EvntClr (pwnd, clnt);
	}
	
	while(1) {	
		if (clnt) {
			if (wind->u.List.AllMasks) {
				EvntClr (pwnd, clnt);
			}
			
			if (!RID_Match (clnt->Id, wind->Id)) {
				// this point can't be reached in the very first loop (n == 0),
				// else the calling function has a serious bug
				
				next = wind->NextSibl;
				if (RID_Match (clnt->Id, pwnd->Id)) {
					
					// reparent window, move wind to closest anchestor
				
				}
				if ((wind = next)) {
					continue;
				
				} else {
					// that's save, if pwnd will be deleted, wind was moved lower in
					// the hirachy
					wind = pwnd;
					pwnd = pwnd->Parent;
					n--;
				}
			}
		}
		if (wind->StackBot) {
			pwnd = wind;
			wind = wind->StackBot;
			n++;
			continue;
		}
		
		// now  wind is guaranteed not to have childs anymore and aren't be
		// excluded from deleting
		
		if (clnt && !RID_Match (clnt->Id, wind->Id)) {
			printf ("\33pPANIC\33q wind_destroy()"
			        " W:%X must be excluded from deleting!\n", wind->Id);
			exit(1);
		}
		if (wind->StackBot || wind->StackTop) {
			printf ("\33pPANIC\33q wind_destroy()"
			        " W:%X has still children: bot=W:%X top=W:%X!\n",
			        wind->Id, (wind->StackBot ? wind->StackBot->Id : 0),
			        (wind->StackTop ? wind->StackTop->Id : 0));
			exit(1);
		}
		
		if (wind->isMapped) {
			if (wind->u.List.AllMasks & StructureNotifyMask) {
				EvntUnmapNotify (wind, StructureNotifyMask, wind->Id, xFalse);
			}
			if (pwnd->u.List.AllMasks & SubstructureNotifyMask) {
				EvntUnmapNotify (pwnd, SubstructureNotifyMask, wind->Id, xFalse);
			}
		}
		if (wind == _WIND_PgrabWindow) {
			_Wind_PgrabClear (NULL);
		}
		if (wind == _WIND_PointerRoot) {
			if (!enter) {
				WindOrigin (pwnd, &r_p);
				r_p.x = MAIN_PointerPos->x - r_p.x;
				r_p.y = MAIN_PointerPos->y - r_p.y;
				WindOrigin (pwnd, &r_p);
				e_p.x = MAIN_PointerPos->x - e_p.x;
				e_p.y = MAIN_PointerPos->y - e_p.y;
				enter = xTrue;
			}
			if (wind->u.List.AllMasks & LeaveWindowMask) {
				EvntLeaveNotify (wind, pwnd->Id, None, r_p, e_p,
				                 NotifyNormal, ELFlagSameScreen, NotifyAncestor);
			}
			if (wind->u.List.AllMasks & FocusChangeMask) {
				EvntFocusOut (wind, NotifyNormal, NotifyAncestor);
			}
			e_p.x += wind->Rect.x;
			e_p.y += wind->Rect.y;
			_WIND_PointerRoot = pwnd;
			CrsrSelect (pwnd->Cursor);
		}
		if (wind->u.List.AllMasks & StructureNotifyMask) {
			EvntDestroyNotify (wind, StructureNotifyMask, wind->Id);
		}
		if (wind->Parent->u.List.AllMasks & SubstructureNotifyMask) {
			EvntDestroyNotify (wind->Parent, SubstructureNotifyMask, wind->Id);
		}
		EvntDel (wind);
		
		if (wind->nSelections) SlctClear (wind);
		if (wind->Cursor)      CrsrFree  (wind->Cursor, NULL);
		if (wind->hasBackPix)  PmapFree  (wind->Back.Pixmap, NULL);
		
		if (wind->Handle > 0) {
			if (wind->isMapped) {
				wind_close  (wind->Handle);
				_WIND_OpenCounter--;
			}
			wind_delete (wind->Handle);
		
		} else {
			next = wind->NextSibl;
		}
		owner = (clnt ? clnt : ClntFind(wind->Id));
		
		if (wind->PrevSibl) wind->PrevSibl->NextSibl = wind->NextSibl;
		else                pwnd->StackBot           = wind->NextSibl;
		if (wind->NextSibl) wind->NextSibl->PrevSibl = wind->PrevSibl;
		else                pwnd->StackTop           = wind->PrevSibl;
		XrscDelete (owner->Drawables, wind);
		
		if (!n) break;
		
		if (!(wind = next)) {
			wind = pwnd;
			pwnd = pwnd->Parent;
			n--;
		}
	}
	
	if (enter) {
		if (pwnd->u.List.AllMasks & EnterWindowMask) {
			EvntEnterNotify (pwnd, pwnd->Id, None, r_p, e_p,
			                 NotifyNormal, ELFlagSameScreen, NotifyInferior);
		}
		if (pwnd->u.List.AllMasks & FocusChangeMask) {
			EvntFocusIn (pwnd, NotifyNormal, NotifyInferior);
		}
		if (_WIND_OpenCounter) {
			WindPointerWatch (xFalse); // correct watch rectangle
		} else {
			_WIND_PointerRoot = NULL;
			MainClrWatch(0);
			MainSetMove (xFalse);
		}
	}
}


//==============================================================================
BOOL
WindCirculate (WINDOW * wind, CARD8 place)
{
	BOOL notify = xFalse;
	BOOL redraw = xFalse;
	
	if (place == PlaceOnTop) {
		if (wind->NextSibl) {
			if ((wind->NextSibl->PrevSibl = wind->PrevSibl)) {
				  wind->PrevSibl->NextSibl = wind->NextSibl;
			} else { // (!wind->PrevSibl)
				  wind->Parent->StackBot   = wind->NextSibl;
			}
			wind->NextSibl         = NULL;
			wind->PrevSibl         = wind->Parent->StackTop;
			wind->Parent->StackTop = wind->PrevSibl->NextSibl = wind;
			notify = xTrue;
		}
		if (wind->Handle > 0) wind_set (wind->Handle, WF_TOP, 0,0,0,0);
		else                  redraw = notify;
	
	} else  { // (place == PlaceOnBottom)
		if (wind->NextSibl) {
			if ((wind->PrevSibl->NextSibl = wind->NextSibl)) {
				  wind->NextSibl->PrevSibl = wind->PrevSibl;
			} else { // (!wind->PrevSibl)
				  wind->Parent->StackTop   = wind->PrevSibl;
			}
			wind->PrevSibl         = NULL;
			wind->NextSibl         = wind->Parent->StackBot;
			wind->Parent->StackBot = wind->NextSibl->PrevSibl = wind;
			notify = xTrue;
		}
		if (wind->Handle > 0) wind_set (wind->Handle, WF_BOTTOM, 0,0,0,0);
		else                  redraw = notify;
	}
	
	if (notify) {
		if (wind->u.List.AllMasks & StructureNotifyMask) {
			EvntCirculateNotify (wind, StructureNotifyMask, wind->Id, place);
		}
		if (wind->Parent->u.List.AllMasks & SubstructureNotifyMask) {
			EvntCirculateNotify (wind->Parent, SubstructureNotifyMask,
			                     wind->Id, place);
		}
	}
	WindPointerWatch (xFalse);
	
	return redraw;
}


//------------------------------------------------------------------------------
static void
_Wind_Resize (WINDOW * wind, GRECT * diff)
{
	CARD32 above = (wind->PrevSibl ? wind->PrevSibl->Id : None);
	GRECT  work;
	
	work.x = (wind->Rect.x += diff->x) - wind->BorderWidth;
	work.y = (wind->Rect.y += diff->y) - wind->BorderWidth;
	work.w =  wind->Rect.w += diff->w;
	work.h =  wind->Rect.h += diff->h;
	if (wind->u.List.AllMasks & StructureNotifyMask) {
		EvntConfigureNotify (wind, StructureNotifyMask,
		                     wind->Id, above, work,
		                     wind->BorderWidth, wind->Override);
	}
	if (wind->Parent->u.List.AllMasks & SubstructureNotifyMask) {
		EvntConfigureNotify (wind->Parent, SubstructureNotifyMask,
		                     wind->Id, above, work,
		                     wind->BorderWidth, wind->Override);
	}
	if ((wind = wind->StackBot)) do {
		BOOL notify = xFalse;
		if       (wind->WinGravity == NorthGravity  ||
		          wind->WinGravity == CenterGravity ||
		          wind->WinGravity == SouthGravity) {
			wind->Rect.x += diff->w /2;
			notify       =  xTrue;
		} else if (wind->WinGravity == NorthEastGravity ||
		           wind->WinGravity == EastGravity      ||
		           wind->WinGravity == SouthEastGravity) {
			wind->Rect.x += diff->w;
			notify       =  xTrue;
		}
		if       (wind->WinGravity == WestGravity   ||
		          wind->WinGravity == CenterGravity ||
		          wind->WinGravity == EastGravity) {
			wind->Rect.y += diff->h /2;
			notify       =  xTrue;
		} else if (wind->WinGravity == SouthWestGravity ||
		           wind->WinGravity == SouthGravity     ||
		           wind->WinGravity == SouthEastGravity) {
			wind->Rect.y += diff->h;
			notify       =  xTrue;
		}
		if (notify) {
			PXY pos = { wind->Rect.x - wind->BorderWidth,
			            wind->Rect.y - wind->BorderWidth };
			if (wind->u.List.AllMasks & StructureNotifyMask) {
				EvntGravityNotify (wind, StructureNotifyMask,
				                   wind->Id, pos);
			}
			if (wind->Parent->u.List.AllMasks & SubstructureNotifyMask) {
				EvntGravityNotify (wind->Parent, SubstructureNotifyMask,
				                   wind->Id, pos);
			}
		} else if (wind->isMapped  &&  wind->WinGravity == UnmapGravity) {
			WindUnmap (wind, xTrue);
		}
	} while ((wind = wind->NextSibl));
}

//==============================================================================
void
WindResize (WINDOW * wind, GRECT * diff)
{
	GRECT curr;
	
	WmgrCalcBorder (&curr, wind);
	if ((curr.x += diff->x) < WIND_Root.Rect.x) {
		diff->x += WIND_Root.Rect.x - curr.x;
		curr.x  =  WIND_Root.Rect.x;
	}
	if ((curr.y += diff->y) < WIND_Root.Rect.y) {
		diff->y += WIND_Root.Rect.y - curr.y;
		curr.y  =  WIND_Root.Rect.y;
	}
	curr.w += diff->w;
	curr.h += diff->h;
	
	_Wind_Resize (wind, diff);
	
	if (wind->isMapped) {
		wind_set_curr (wind->Handle, &curr);
		if (!diff->x && !diff->y) {
			short decor = (wind->GwmDecor ? WMGR_Decor : 0);
			if (diff->w < decor || diff->h < decor) {
				WindDrawSection (wind, NULL);
			} else {
				GRECT work = wind->Rect;
				work.x += WIND_Root.Rect.x;
				work.y += WIND_Root.Rect.y;
				work.w -= diff->w - decor;
				work.h -= diff->h - decor;
				WindDrawSection (wind, &work);
			}
		}
	}
	
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_CreateWindow (CLIENT * clnt, xCreateWindowReq * q)
{
	WINDOW * pwnd = NULL;
	WINDOW * wind = NULL;
	
	if (DrawFind (q->wid).p) {
		Bad(IDChoice, q->wid, CreateWindow,);
		
	} else if (!(pwnd = WindFind(q->parent))) {
		Bad(Window, q->parent, CreateWindow,);
		
	/* check visual */
	/* check depth */
	
	} else if (!(wind = XrscCreate(WINDOW, q->wid, clnt->Drawables))) {
		Bad(Alloc,, CreateWindow,"(W:%lX)", q->wid);
	
	} else {
//		PRINT (CreateWindow,"-(%u) W:%lX [%i,%i/%u,%u/%u:%u] on W:%lX with V:%lX",
//		       q->class, q->wid, q->x, q->y, q->width, q->height, q->borderWidth,
//		       q->depth, q->parent, q->visual);
		
		wind->isWind = xTrue;
		
		wind->ClassInOut = (q->class == CopyFromParent ? pwnd->ClassInOut :
		                    q->class == InputOutput    ? xTrue : xFalse);
		
		wind->Rect.x      = q->x + q->borderWidth;
		wind->Rect.y      = q->y + q->borderWidth;
		wind->Rect.w      = q->width;
		wind->Rect.h      = q->height;
		wind->BorderWidth = q->borderWidth;
		wind->Depth       = (!q->depth && wind->ClassInOut
		                     ? pwnd->Depth : q->depth);
		
		wind->Override     = xFalse;
		wind->BackingStore = NotUseful,
		wind->WinGravity   = NorthWestGravity;
		wind->BitGravity   = ForgetGravity;
		wind->SaveUnder    = xFalse;
		
		wind->isMapped   = xFalse;
		wind->hasBorder  = xFalse;
		wind->hasBackGnd = xFalse;
		wind->hasBackPix = xFalse;
		wind->GwmParent  = xFalse;
		wind->GwmDecor   = xFalse;
		wind->GwmIcon    = xFalse;
		
		wind->nSelections = 0;
		
		wind->BorderPixel = BLACK;
		wind->Back.Pixel  = WHITE;
		
		wind->PropagateMask  = AllEventMask;
		wind->u.Event.Mask   = 0uL;
		wind->u.Event.Client = NULL;
		
		wind->Parent     = pwnd;
		wind->PrevSibl   = NULL;
		wind->NextSibl   = NULL;
		wind->StackBot   = NULL;
		wind->StackTop   = NULL;
		wind->Cursor     = (q->mask & CWCursor ? NULL : CrsrShare (pwnd->Cursor));
		wind->Properties = NULL;
		
		if (pwnd->StackTop) {
			pwnd->StackTop->NextSibl = wind;
			wind->PrevSibl           = pwnd->StackTop;
		} else {
			pwnd->StackBot = wind;
		}
		pwnd->StackTop = wind;
		
		_Wind_setup (clnt, wind, q->mask, (CARD32*)(q +1), X_CreateWindow);
		
//		PRINT (,"+");
		
		if (pwnd != &WIND_Root) {
			wind->Handle = (pwnd->Handle < 0 ? pwnd->Handle : -pwnd->Handle);
		
		} else if (!WmgrWindHandle (wind)) {
			Bad(Alloc,, CreateWindow,"(W:%lX): AES", q->wid);
			WindDelete (wind, clnt);
			return;
		}
		
		if (pwnd->u.List.AllMasks & SubstructureNotifyMask) {
			EvntCreateNotify (pwnd, SubstructureNotifyMask, wind->Id,
			                  wind->Rect, wind->BorderWidth, wind->Override);
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_MapWindow (CLIENT * clnt, xMapWindowReq * q)//, WINDOW * wind)
{
	WINDOW * wind = WindFind(q->id);

	if (!wind) {
		Bad(Window, q->id, MapWindow,);
	
	} else if (wind == &WIND_Root) {
	#	ifndef NODEBUG
		PRINT (,"\33pWARNING\33q MapWindow(W:%lX) ignored.", ROOT_WINDOW);
	#	endif NODEBUG
		
	} else if (!wind->isMapped) {
		
		DEBUG (MapWindow," W:%lX (%i)", q->id, wind->Handle);
		
		if (wind->Handle < 0) {
			if (WindMap (wind, WindVisible (wind->Parent))) {
				GRECT curr;
				WindGeometry (wind, &curr, wind->BorderWidth);
				WindDrawSection (wind, &curr);
			}
			if (wind->Parent == _WIND_PointerRoot) {
				WindPointerWatch (xFalse);
			}
		
		} else {
			GRECT curr;
			
			WmgrWindMap (wind, &curr);
			if (!_WIND_OpenCounter++) {
				MainSetWatch (&curr, MO_ENTER);
			} else {
				WindPointerWatch (xFalse);
			}
			WindMap (wind, xTrue);
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_MapSubwindows (CLIENT * clnt, xMapSubwindowsReq * q)
{
	WINDOW * wind = WindFind (q->id);
	
	if (!wind) {
		Bad(Window, q->id, MapSubwindows,);
	
	} else if (wind == &WIND_Root) {
	#	ifndef NODEBUG
		PRINT (,"\33pWARNING\33q MapSubwindows(W:%lX) ignored.", ROOT_WINDOW);
	#	endif NODEBUG
		
	} else if (wind->StackTop) {
		BOOL     visible = WindVisible (wind);
		WINDOW * w       = wind->StackTop;
		
		DEBUG (MapSubwindows," W:%lX", q->id);
		
		do {
			if (!w->isMapped && WindMap (w, visible)) {
				GRECT curr;
				WindGeometry (w, &curr, w->BorderWidth);
				WindDrawSection (w, &curr);
			}
		} while ((w = w->PrevSibl));
		
		if (wind == _WIND_PointerRoot) {
			WindPointerWatch (xFalse);
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_UnmapWindow (CLIENT * clnt, xUnmapWindowReq * q)
{
	WINDOW * wind = WindFind (q->id);
	
	if (!wind) {
		Bad(Window, q->id, UnmapWindow,);
	
	} else if (wind->isMapped) {
		
		DEBUG (UnmapWindow," 0x%lX", q->id);
		
		WindUnmap (wind, xFalse);
		
		if (wind->Handle > 0) {
			wind_close (wind->Handle);
			if (--_WIND_OpenCounter) {
				WindPointerWatch (xFalse);
			} else {
				_WIND_PointerRoot = NULL;
				MainClrWatch(0);
			}
		} else if (WindVisible (wind->Parent)) {
			GRECT sect;
			WindGeometry (wind, &sect, wind->BorderWidth);
			WindDrawSection (wind->Parent, &sect);
			if (PXYinRect (MAIN_PointerPos, &sect)) {
				WindPointerWatch (xFalse);
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_UnmapSubwindows (CLIENT * clnt, xUnmapSubwindowsReq * q)
{
	PRINT (- X_UnmapSubwindows," W:%lX", q->id);
}

//------------------------------------------------------------------------------
void
RQ_ConfigureWindow (CLIENT * clnt, xConfigureWindowReq * q)
{

	WINDOW * wind = WindFind (q->window);
	
	if (!wind) {
		Bad(Window, q->window, ConfigureWindow,);
	
	} else {
		CARD32 * val = (CARD32*)(q +1);
		GRECT    d   = { 0,0, 0,0 }, * r = &wind->Rect;
		short    db  = wind->BorderWidth;
		
//		PRINT (ConfigureWindow,"- W:%lX:", q->window);
		
		if (clnt->DoSwap) {
			CARD16 mask = q->mask;
			do if (mask & 1) {
				*val = Swap32(*val);
				val++;
			} while ((mask >>= 1));
			val = (CARD32*)(q +1);
		}
		
		if (q->mask & CWX)      { d.x = (short)*(val++) + db - r->x;
}//		                          PRINT (,"+- x=%+i", d.x); }
		if (q->mask & CWY)      { d.y = (short)*(val++) + db - r->y;
}//		                          PRINT (,"+- y=%+i", d.y); }
		if (q->mask & CWWidth)  { d.w = (short)*(val++) - r->w;
}//		                          PRINT (,"+- w=%+i", d.w); }
		if (q->mask & CWHeight) { d.h = (short)*(val++) - r->h;
}//		                          PRINT (,"+- h=%+i", d.h); }
		if (q->mask & CWBorderWidth) {
			db = (wind->BorderWidth = (short)*(val++)) - db;
			r->x += db;
			r->y += db;
//			PRINT (,"+- brdr=%+i", db);
		}
		if (q->mask & CWSibling) {
//			PRINT (,"+- sibl=0x%lX", (CARD32)*val);
			val++;
		}
			if (q->mask & CWStackMode) {
//				PRINT (,"+- stck=%i", (int)(CARD8)*val);
				val++;
		}
//		PRINT (,"+");
		
		if (wind->Handle > 0) {
			WindResize (wind, &d);
			
		} else {
			_Wind_Resize (wind, &d);
			if (d.x || d.y || d.w || d.h) {
				GRECT clip;
				WindGeometry (wind, &clip, wind->BorderWidth);
				if (d.x < 0) d.x    = -d.x;
				else         clip.x -= d.x;
				clip.w += (d.x > d.w ? d.x : d.w);
				if (d.y < 0) d.y    = -d.y;
				else         clip.y -= d.y;
				clip.h += (d.y > d.h ? d.y : d.h);
				WindDrawSection (wind->Parent, &clip);
			}
		}
		WindPointerWatch (xFalse);
	}
}

//------------------------------------------------------------------------------
void
RQ_ChangeWindowAttributes (CLIENT * clnt, xChangeWindowAttributesReq * q)
{
	WINDOW * wind = WindFind (q->window);
	
	if (!wind) {
		Bad(Window, q->window, ChangeWindowAttributes,);
	
	} else {
//		PRINT (ChangeWindowAttributes,"- W:%lX ", q->window);
		
		if ((q->valueMask & CWCursor) && wind->Cursor) {
			CrsrFree (wind->Cursor, NULL);
			wind->Cursor = NULL;
		}
		if ((q->valueMask & (CWBackPixmap|CWBackPixel)) && wind->hasBackPix) {
			PmapFree (wind->Back.Pixmap, NULL);
			wind->Back.Pixmap = NULL;
			wind->hasBackPix  = xFalse;
			wind->hasBackGnd  = xFalse;
		}
		_Wind_setup (clnt, wind, q->valueMask, (CARD32*)(q +1),
		             X_ChangeWindowAttributes);
		
//		PRINT (,"+");
	}
}

//------------------------------------------------------------------------------
void
RQ_CirculateWindow (CLIENT * clnt, xCirculateWindowReq * q)
{
	PRINT (- X_CirculateWindow," W:%lX %s",
	       q->window, (q->direction ? "RaiseLowest" : "LowerHighest"));
}

//------------------------------------------------------------------------------
void
RQ_GetWindowAttributes (CLIENT * clnt, xGetWindowAttributesReq * q)
{
	WINDOW * wind = NULL;
	
	if ((q->id & ~RID_MASK) && !(wind = WindFind(q->id))) {
		Bad(Window, q->id, GetWindowAttributes,);
	
	} else {
		ClntReplyPtr (GetWindowAttributes, r);
		
		PRINT (GetWindowAttributes," W:%lX", q->id);
		
		if (wind) {
			r->backingStore     = NotUseful;
			r->visualID         = DFLT_VISUAL;
			r->class            = wind->ClassInOut;
			r->bitGravity       = ForgetGravity;
			r->winGravity       = NorthWestGravity;
			r->backingBitPlanes = 0x1uL;
			r->backingPixel     = 0;
			r->saveUnder        = xFalse;
			r->mapInstalled     = xTrue;
			r->mapState         = (WindVisible (wind) ? IsViewable : IsUnmapped);
			r->override         = wind->Override;
			r->colormap         = None;
			r->allEventMasks    = wind->u.List.AllMasks & 0x7FFFFFFF;
			r->yourEventMask      = NoEventMask;
			r->doNotPropagateMask = ~wind->PropagateMask;
			
			if (wind->u.List.AllMasks > 0  &&  wind->u.Event.Client == clnt) {
				r->yourEventMask = wind->u.Event.Mask;
			
			} else if (wind->u.List.AllMasks < 0) {
				int        num = wind->u.List.p->Length;
				WINDEVNT * lst = wind->u.List.p->Event;
				while (--num) {
					if (lst->Client == clnt) {
						r->yourEventMask = lst->Mask;
						break;
					}
					lst++;
				}
			}

		} else { // Id is a AES-Window
			r->backingStore     = NotUseful;
			r->visualID         = DFLT_VISUAL;
			r->class            = InputOutput;
			r->bitGravity       = ForgetGravity;
			r->winGravity       = NorthWestGravity;
			r->backingBitPlanes = 0x1uL;
			r->backingPixel     = 0;
			r->saveUnder        = xFalse;
			r->mapInstalled     = xTrue;
			r->mapState         = IsViewable;
			r->override         = xTrue;
			r->colormap         = None;
			r->allEventMasks    = KeyPressMask|ButtonPressMask|ExposureMask|
			                      VisibilityChangeMask|StructureNotifyMask|
			                      ResizeRedirectMask;
			r->yourEventMask      = NoEventMask;
			r->doNotPropagateMask = NoEventMask;
		}
		
		ClntReply (GetWindowAttributes,, "v.2ll4mss.");
	}
}

//------------------------------------------------------------------------------
void
RQ_DestroyWindow (CLIENT * clnt, xDestroyWindowReq * q)
{
	WINDOW * wind = WindFind (q->id);
	
	if (!wind) {
		Bad(Window, q->id, DestroyWindow,);
	
	} else {
		WINDOW * pwnd = (wind->Handle < 0  &&  WindVisible (wind)
		                 ? wind->Parent : NULL);
		GRECT    curr;
		
		DEBUG (DestroyWindow," W:%lX", q->id);
		
		if (pwnd)  WindGeometry (wind, &curr, wind->BorderWidth);
		
		WindDelete (wind, NULL);
		
		if (pwnd) WindDrawSection (pwnd, &curr);
	}
}

//------------------------------------------------------------------------------
void
RQ_DestroySubwindows (CLIENT * clnt, xDestroySubwindowsReq * q)
{
	PRINT (- X_DestroySubwindows," W:%lX", q->id);
}

//------------------------------------------------------------------------------
void
RQ_ReparentWindow (CLIENT * clnt, xReparentWindowReq * q)
{
	// Window window:
	// Window parent:
	// INT16  x, y:
	//...........................................................................
	
	WINDOW * wind = WindFind (q->window);
	WINDOW * pwnd = WindFind (q->parent);
	
	if (!wind) {
		Bad(Window, q->window, ReparentWindow,"(): invalid child.");
	
	} else if (!pwnd) {
		Bad(Window, q->parent, ReparentWindow,"(): invalid parent.");
	
	} else {
		BOOL     map;
		WINDOW * w = pwnd;
		do if (w == wind) {
			Bad(Match,, ReparentWindow,"(): parent is inferior.");
			return;
		} while ((w = w->Parent));
		
		PRINT (ReparentWindow," W:%lX for W:%lX", q->window, q->parent);
		
		if ((map = wind->isMapped)) {
			WindUnmap (wind, xFalse);
			
			if (wind->Handle > 0) {
				wind_close (wind->Handle);
				if (--_WIND_OpenCounter) {
					WindPointerWatch (xFalse);
				} else {
					_WIND_PointerRoot = NULL;
					MainClrWatch(0);
				}
				
			} else if (WindVisible (wind->Parent)) {
				GRECT sect;
				WindGeometry (wind, &sect, wind->BorderWidth);
				WindDrawSection (wind->Parent, &sect);
				if (PXYinRect (MAIN_PointerPos, &sect)) {
					WindPointerWatch (xFalse);
				}
			}
		}
		if (wind->Handle > 0) {
			wind_delete (wind->Handle);
			wind->Handle    = -1;
			wind->GwmParent = xFalse;
			wind->GwmDecor  = xFalse;
			wind->GwmIcon   = xFalse;
		}
		if (wind->PrevSibl) wind->PrevSibl->NextSibl = wind->NextSibl;
		else                wind->Parent->StackBot   = wind->NextSibl;
		if (wind->NextSibl) wind->NextSibl->PrevSibl = wind->PrevSibl;
		else                wind->Parent->StackTop   = wind->PrevSibl;
		wind->NextSibl = NULL;
		wind->Parent   = pwnd;
		if ((wind->PrevSibl = pwnd->StackTop)) {
			pwnd->StackTop->NextSibl = wind;
		} else {
			pwnd->StackBot = wind;
		}
		pwnd->StackTop = wind;
		wind->Rect.x   = q->x;
		wind->Rect.y   = q->y;
		
		if (wind->u.List.AllMasks & StructureNotifyMask) {
			EvntReparentNotify (wind, StructureNotifyMask,
			                    wind->Id, pwnd->Id,
			                    wind->Rect, wind->Override);
		}
		if (pwnd->u.List.AllMasks & SubstructureNotifyMask) {
			EvntReparentNotify (pwnd, SubstructureNotifyMask,
			                    wind->Id, pwnd->Id,
			                    wind->Rect, wind->Override);
		}
		
		if (map && WindMap (wind, WindVisible (pwnd))) {
			GRECT curr;
			WindGeometry (wind, &curr, wind->BorderWidth);
			WindDrawSection (wind, &curr);
			WindPointerWatch (xFalse);
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_QueryTree (CLIENT * clnt, xQueryTreeReq * q)
{
	WINDOW * wind = WindFind (q->id);
	
	ClntReplyPtr (QueryTree, r);
	
	if (!wind) {
		Bad(Window, q->id, QueryTree,);
	
	} else {
		WINDOW * chld = wind->StackBot;
		CARD32 * c_id = (CARD32*)(r +1);
		
		PRINT (QueryTree," W:%lX", q->id);
		
		r->root      = ROOT_WINDOW;
		r->parent    = (wind->Parent ? wind->Parent->Id : None);
		r->nChildren = 0;
		
		if (clnt->DoSwap) while (chld) {
			*(c_id++) = Swap32(chld->Id);
			r->nChildren++;
			chld = chld->NextSibl;
		} else while (chld) {
			*(c_id++) = chld->Id;
			r->nChildren++;
			chld = chld->NextSibl;
		}
		ClntReply (QueryTree, (r->nChildren *4), "ww.");
	}
}


//------------------------------------------------------------------------------
void
RQ_ChangeSaveSet (CLIENT * clnt, xChangeSaveSetReq * q)//, WINDOW * wind)
{
	PRINT (- X_ChangeSaveSet," W:%lX", q->window);
}
