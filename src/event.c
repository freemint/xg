//==============================================================================
//
// event.c
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-07-01 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "tools.h"
#include "clnt.h"
#include "event.h"
#include "window.h"

#include <stdlib.h>
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xproto.h>


//==============================================================================
BOOL
EvntSet (WINDOW * wind, CLIENT * clnt, CARD32 mask)
{
	CARD16     num;
	WINDEVNT * src, * dst = NULL;
	
	if (wind->u.List.AllMasks < 0) {
		int i = num = wind->u.List.p->Length;
		dst   = src = wind->u.List.p->Event;
		while (i-- && dst->Client != clnt) dst++;
		if (i < 0) dst       =  NULL;
		else       dst->Mask |= mask;
	
	} else {
		src = &wind->u.Event;
		num = 1;
		
		if (src->Client == clnt) {
			dst = src;
		} else if (!src->Client) {
			dst = src;
			dst->Client = clnt;
			clnt->EventReffs++;
		}
	}
	
	if (!dst) {
		size_t                 len = sizeof(WINDEVNT) * num;
		typeof(wind->u.List.p) lst = malloc (sizeof(*lst) + len);
		if (!lst) return xFalse;
		
		lst->Length = num +1;
		memcpy (lst->Event, src, len);
		if (num > 1) free (wind->u.List.p);
		else         wind->u.List.AllMasks |= 0x80000000uL;
		wind->u.List.p         = lst;
		lst->Event[num].Mask   = mask;
		lst->Event[num].Client = clnt;
		clnt->EventReffs++;
	
	} else if (num > 1  && (~wind->u.List.AllMasks & mask)) {
		int i;
		wind->u.List.AllMasks = 0x80000000uL;
		for (i = 0, mask = 0uL; i < num; mask |= src[i++].Mask);
	}
	
	wind->u.List.AllMasks |= mask;
	
	return xTrue;
}

//==============================================================================
BOOL
EvntClr (WINDOW * wind, CLIENT * clnt)
{
	if (wind->u.List.AllMasks < 0) {
		CARD16     num  = wind->u.List.p->Length;
		WINDEVNT * ptr  = wind->u.List.p->Event;
		CARD32     mask = 0uL;
		while (ptr->Client != clnt) {
			mask |= ptr->Mask;
			ptr++;
			if (!--num) break;
		}
		if (num) {
			if (--wind->u.List.p->Length == 1) {
				CLIENT * left = ptr[num == 2 ? +1 : -1].Client;
				free (wind->u.List.p);
				wind->u.Event.Client = left;
			} else {
				mask |= 0x80000000uL;
				while (--num) {
					mask         |=
					ptr[0].Mask   = ptr[1].Mask;
					ptr[0].Client = ptr[1].Client;
					ptr++;
				}
				ptr[0].Mask   = 0uL;
				ptr[0].Client = NULL;
			}
			wind->u.List.AllMasks = mask;
			clnt->EventReffs--;
			return xTrue;
		}
	} else if (wind->u.List.AllMasks  &&  wind->u.Event.Client == clnt) {
		wind->u.Event.Mask   = 0uL;
		wind->u.Event.Client = NULL;
		clnt->EventReffs--;
		return xTrue;
	}
	return xFalse;
}

//==============================================================================
void
EvntDel (WINDOW * wind)
{
	CARD16     num = (wind->u.List.AllMasks < 0 ? wind->u.List.p->Length : 1);
	WINDEVNT * lst = (num > 1 ? wind->u.List.p->Event : &wind->u.Event);
	
	while (num--) {
		if (lst->Mask) {
			lst->Client->EventReffs--;
		}
		lst++;
	}
	if (wind->u.List.AllMasks < 0) {
		free (wind->u.List.p);
	}
	wind->u.Event.Mask   = NoEventMask;
	wind->u.Event.Client = NULL;
}


//==============================================================================
void
EvntExpose (WINDOW * wind, short len, const p_GRECT rect)
{
	CARD16     num = (wind->u.List.AllMasks < 0 ? wind->u.List.p->Length : 1);
	WINDEVNT * lst = (num > 1 ? wind->u.List.p->Event : &wind->u.Event);
	
	while (num--) {
		if (lst->Mask & ExposureMask) {
			NETBUF      * buf = &lst->Client->oBuf;
			xEvent      * evn = (xEvent*)(buf->Mem + buf->Left + buf->Done);
			const GRECT * rct = rect;
			int           cnt = len;
			if (lst->Client->DoSwap) while (cnt--) {
				evn->u.u.type           = Expose;
				evn->u.u.sequenceNumber = Swap16(lst->Client->SeqNum);
				evn->u.expose.window    = Swap32(wind->Id);
				evn->u.expose.count     = Swap16(cnt);
				SwapRCT ((GRECT*)&evn->u.expose.x, rct);
				rct++;
				evn++;
			} else while (cnt--) {
				evn->u.u.type             = Expose;
				evn->u.u.sequenceNumber   = lst->Client->SeqNum;
				evn->u.expose.window      = wind->Id;
				evn->u.expose.count       = cnt;
				*(GRECT*)&evn->u.expose.x = *rct;
				rct++;
				evn++;
			}
			buf->Left     += sizeof(xEvent) * len;
			MAIN_FDSET_wr |= 1uL << lst->Client->Fd;
		}
		lst++;
	}
}

//==============================================================================
void
EvntClientMsg (CLIENT * clnt, Window id, Atom type, BYTE format, void * data)
{
	NETBUF * buf = &clnt->oBuf;
	xEvent * evn = (xEvent*)(buf->Mem + buf->Left + buf->Done);
	
	evn->u.u.type   = ClientMessage;
	evn->u.u.detail = format;
	if (clnt->DoSwap) {
		evn->u.u.sequenceNumber       = Swap16(clnt->SeqNum);
		evn->u.clientMessage.window   = Swap32(id);
		evn->u.clientMessage.u.l.type = Swap32(type);
	} else {
		evn->u.u.sequenceNumber       = clnt->SeqNum;
		evn->u.clientMessage.window   = id;
		evn->u.clientMessage.u.l.type = type;
		format = 0;
	}
	if (format == 32) {
		CARD32 * src = data;
		CARD32 * dst = &evn->u.clientMessage.u.l.longs0;
		int      num = 5;
		do { *(dst++) = Swap32(*(src++)); } while (--num);
	} else if (format == 16) {
		CARD16 * src = data;
		CARD16 * dst = &evn->u.clientMessage.u.s.shorts0;
		int      num = 10;
		do { *(dst++) = Swap16(*(src++)); } while (--num);
	} else {
		memcpy (evn->u.clientMessage.u.b.bytes, data, 20);
	}
	buf->Left     += sizeof(xEvent);
	MAIN_FDSET_wr |= 1uL << clnt->Fd;
}

//==============================================================================
BOOL
EvntPropagate (WINDOW * wind, CARD32 mask, BYTE event,
               Window rid, Window cid, PXY * r_xy, BYTE detail)
{
	BOOL exec = xFalse;
	PXY  rxy  = *r_xy;
	PXY  exy  = *r_xy;
	
	do {
		if (mask & wind->u.List.AllMasks) {
			_evnt_w (wind, mask, event,
			         rid, cid, *(CARD32*)&rxy, *(CARD32*)&exy, detail);
			exec  = xTrue;
			mask &= !wind->u.List.AllMasks;
		}
		if (mask &= wind->PropagateMask) {
			exy.x += wind->Rect.x;
			exy.y += wind->Rect.y;
		} else {
			break;
		}
	} while ((wind = wind->Parent));
	
	return exec;
}


//==============================================================================
// w   - Window
// a   - ATOM
// l   - CARD32 ('long')
// s   - CARD16 ('short')
// b/c - BOOL / CARD8 ('char')
// p   - PXY, casted to one single CARD32
// r   - GRECT*
// d   - detail, CARD8
// S   - global TIMESTAMP
// M   - global SETofKEYBUTMASK
// W   - Id from parameter WINDOW*
// T/F - xTrue / xFalse, constant CARD8 (mainly for 'same-screen')

static const char * _EVNT_Form[LASTEvent] = {
	NULL, NULL,
	"SwWwppMTd","SwWwppMTd",              // KeyPress,    KeyRelease
	"SwWwppMTd","SwWwppMTd",              // ButtonPress, ButtonRelease
	"SwWwppMT", "SwWwppMccd","SwWwppMccd",// MotionNotify,EnterNotify,LeaveNotify
	"Wcd",      "Wcd",                    // FocusIn,     FocusOut
	NULL, //KeymapNotify
	NULL, //Expose
	NULL, //GraphicsExpose
	NULL, //NoExpose
	"Wc",                                 // VisibilityNotify
	"Wwrsb",    "Ww",                     // CreateNotify,  DestroyNotify
	"Wwb",      "Wwb",       "Ww",        // UnmapNotify,   MapNotify, MapRequest
	"Wwwpb",    "Wwwrsb",                 // ReparentNotify, ConfigureNotify
	NULL, //ConfigureRequest
	"Wwp",                                // GravityNotify
	NULL, //ResizeRequest
	"Wwlc",     "Wwlc",                   // CirculateNotify, CirculateRequest
	"WaSb",     "Swa",                    // PropertyNotify,  SelectionClear
	"lwwaaa",   "lwaaa",                  // SelectionRequest,SelectionNotify
	NULL, //ColormapNotify
	NULL, //ClientMessage
	NULL, //MappingNotify
};
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void
FT_Evnt_send_MSB (CLIENT * clnt, WINDOW * wind, CARD16 evnt, va_list vap)
{
	const char * frm = _EVNT_Form[evnt];
	NETBUF     * buf = &clnt->oBuf;
	xEvent     * evn = (xEvent*)(buf->Mem + buf->Left + buf->Done);
	char       * ptr = ((char*)evn) +4;
	#define ARG(t)   { *(t*)ptr = va_arg (vap, t); ptr += sizeof(t); }
	
	evn->u.u.type = evnt;
	evn->u.u.sequenceNumber = clnt->SeqNum;
	
	while (*frm) switch (*(frm++)) {
		case 'w': ARG (Window); break;
		case 'a': ARG (Atom);   break;
		case 'l': ARG (CARD32); break;
		case 's': ARG (CARD16); break;
		case 'c': ARG (CARD8);  break;
		case 'b': ARG (BOOL);   break;
		case 'p':
			*(PXY*)ptr = *(PXY*)&va_arg (vap, CARD32);
			ptr += 4;
			break;
		case 'r':
			*(GRECT*)ptr = *va_arg (vap, GRECT*);
			ptr += 8;
			break;
		case 'd': evn->u.u.detail   = va_arg (vap, CARD8);       break;
		case 'W': *(Window*)ptr     = wind->Id;        ptr += 4; break;
		case 'S': *(Time*)ptr       = MAIN_TimeStamp;  ptr += 4; break;
		case 'M': *(KeyButMask*)ptr = MAIN_KeyButMask; ptr += 2; break;
		case 'T': *ptr              = xTrue;           ptr += 1; break;
		case 'F': *ptr              = xFalse;          ptr += 1; break;
	}
	buf->Left     += sizeof(xEvent);
	MAIN_FDSET_wr |= 1uL << clnt->Fd;
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void
FT_Evnt_send_LSB (CLIENT * clnt, WINDOW * wind, CARD16 evnt, va_list vap)
{
	const char * frm = _EVNT_Form[evnt];
	NETBUF     * buf = &clnt->oBuf;
	xEvent     * evn = (xEvent*)(buf->Mem + buf->Left + buf->Done);
	char       * ptr = ((char*)evn) +4;
	#define ARG32(t) { *(t*)ptr = Swap32(va_arg (vap, t)); ptr += 4; }
	#define ARG16(t) { *(t*)ptr = Swap16(va_arg (vap, t)); ptr += 2; }
	
	evn->u.u.type = evnt;
	evn->u.u.sequenceNumber = Swap16(clnt->SeqNum);
	
	while (*frm) switch (*(frm++)) {
		case 'w': ARG32 (Window); break;
		case 'a': ARG32 (Atom);   break;
		case 'l': ARG32 (CARD32); break;
		case 's': ARG16 (CARD16); break;
		case 'c': ARG   (CARD8);  break;
		case 'b': ARG   (BOOL);   break;
		case 'p':
			SwapPXY ((PXY*)ptr, (PXY*) &va_arg (vap, CARD32));  ptr += 4; break;
		case 'r':
			SwapRCT ((GRECT*)ptr, va_arg (vap, GRECT*));        ptr += 8; break;
		case 'd': evn->u.u.detail   = va_arg (vap, CARD8);               break;
		case 'W': *(Window*)ptr     = Swap32(wind->Id);        ptr += 4; break;
		case 'S': *(Time*)ptr       = Swap32(MAIN_TimeStamp);  ptr += 4; break;
		case 'M': *(KeyButMask*)ptr = Swap16(MAIN_KeyButMask); ptr += 2; break;
		case 'T': *ptr              = xTrue;                   ptr += 1; break;
		case 'F': *ptr              = xFalse;                  ptr += 1; break;
	}
	buf->Left     += sizeof(xEvent);
	MAIN_FDSET_wr |= 1uL << clnt->Fd;
}

//------------------------------------------------------------------------------
void
_evnt_w (WINDOW * wind, CARD32 mask, CARD16 evnt, ...)
{
	CARD16     num = (wind->u.List.AllMasks < 0 ? wind->u.List.p->Length : 1);
	WINDEVNT * lst = (num > 1 ? wind->u.List.p->Event : &wind->u.Event);

	if (evnt < 2 || evnt >= LASTEvent) {
		printf ("\33pERROR\33q invalid event value %u for W:%X.\n",
		        evnt, wind->Id);
		return;
	}
	if (!_EVNT_Form[evnt]) {
		printf ("\33pERROR\33q undefined event set at %u for W:%X.\n",
		        evnt, wind->Id);
		return;
	}
	if (mask & ~AllEventMask) {
		printf ("\33pWARNING\33q invalid event mask %lX for W:%X.\n",
		        mask, wind->Id);
		mask &= AllEventMask;
	}
	
	while (num--) {
		if (lst->Mask & mask) {
			va_list   vap;
			va_start (vap, evnt);
			lst->Client->Fnct->event (lst->Client, wind, evnt, vap);
			va_end (vap);
		}
		lst++;
	}
}

//------------------------------------------------------------------------------
void
_evnt_c (CLIENT * clnt, CARD16 evnt, ...)
{
	if (evnt < 2 || evnt >= LASTEvent) {
		printf ("\33pERROR\33q invalid event value %u.\n", evnt);
		
	} else if (!_EVNT_Form[evnt]) {
		printf ("\33pERROR\33q undefined event set at %u.\n", evnt);
		
	} else {
		va_list   vap;
		va_start (vap, evnt);
		clnt->Fnct->event (clnt, NULL, evnt, vap);
		va_end (vap);
	}
}
//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_SendEvent (CLIENT * clnt, xSendEventReq * q)
{
	// BOOL   propagate:
	// Window destination:
	// CARD32 eventMask:
	// xEvent event
	//...........................................................................
	
	extern WINDOW * _WIND_PointerRoot;
	
	WINDOW * wind = NULL;
	
	if (q->event.u.u.type < 2 || q->event.u.u.type >= LASTEvent) {
		Bad(Value, q->event.u.u.type, SendEvent,);
	
	} else if (q->destination == PointerWindow) {
		wind = _WIND_PointerRoot;
	
	} else if (q->destination == InputFocus) {
		
		// this is only correct if PointerRoot is an inferior of InputFocus !!!
		//
		wind = _WIND_PointerRoot;
	
	} else if (!(wind = WindFind (q->destination))) {
		Bad(Window, q->destination, SendEvent,);
	}
	//...........................................................................
	
	if (wind) {
		WINDEVNT _lst, * lst = &_lst;
		CARD16   num  = 0;
		CARD32   mask = q->eventMask;
		
		PRINT (SendEvent," W:%lX mask=%lX #%i %s",
		       q->destination, q->eventMask, q->event.u.u.type,
		       (q->propagate ? "prop." : "first"));
	
		if (!q->eventMask) {
			if ((_lst.Client = ClntFind (wind->Id))) {
				mask = _lst.Mask = ~0uL;
				num  = 1;
			}
		} else do {
			if (wind->u.List.AllMasks & q->eventMask) {
				num = (wind->u.List.AllMasks < 0 ? wind->u.List.p->Length : 1);
				lst = (num > 1 ? wind->u.List.p->Event : &wind->u.Event);
				break;
			} else if (!q->propagate) {
				break;
			}
		} while ((wind = wind->Parent));
		
		while (num--) {
			if (lst->Mask & mask) {
				NETBUF * buf  = &lst->Client->oBuf;
				xEvent * evn  = (xEvent*)(buf->Mem + buf->Left + buf->Done);
				
				evn->u.u.type   = q->event.u.u.type | 0x80;
				evn->u.u.detail = q->event.u.u.detail;
				if (lst->Client->DoSwap) {
					evn->u.u.sequenceNumber     = Swap16(lst->Client->SeqNum);
					evn->u.clientMessage.window = Swap32(wind->Id);
				} else {
					evn->u.u.sequenceNumber     = lst->Client->SeqNum;
					evn->u.clientMessage.window = wind->Id;
				}
				memcpy (evn->u.clientMessage.u.b.bytes,
				        q->event.u.clientMessage.u.b.bytes, 20);
				
				buf->Left     += sizeof(xEvent);
				MAIN_FDSET_wr |= 1uL << lst->Client->Fd;
			}
			lst++;
		}
	}
}






