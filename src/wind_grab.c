//==============================================================================
//
// wind_grab.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-07-24 - Initial Version.
//==============================================================================
//
#include "window_P.h"
#include "event.h"
#include "Cursor.h"

#include <stdio.h> // printf
#include <stdlib.h>

#include <X11/Xproto.h>


CLIENT * _WIND_PgrabClient  = NULL;
WINDOW * _WIND_PgrabWindow  = NULL;
CURSOR * _WIND_PgrabCursor  = NULL;
CARD32   _WIND_PgrabEvents  = 0ul;
CARD32   _WIND_PgrabTime    = 0ul;
BOOL     _WIND_PgrabOwnrEv  = xFalse;
BOOL     _WIND_PgrabPassive = xFalse;


//------------------------------------------------------------------------------
static void
_Wind_PgrabSet (CLIENT * clnt, WINDOW * wind, p_CURSOR crsr,
                CARD32 mask, BOOL ownr, CARD32 time, BOOL passive)
{
	if (!_WIND_PgrabClient) {
		WindMctrl (xTrue);
	}
	_WIND_PgrabClient  = clnt;
	_WIND_PgrabWindow  = wind;
	_WIND_PgrabEvents  = mask;
	_WIND_PgrabOwnrEv  = ownr;
	_WIND_PgrabTime    = time;
	_WIND_PgrabPassive = passive;
	if (_WIND_PgrabCursor) {
		CrsrFree (_WIND_PgrabCursor, NULL);
	}
	if ((_WIND_PgrabCursor = crsr)) {
		CrsrSelect (crsr);
	} else {
		_Wind_Cursor (_WIND_PgrabWindow);
	}
	
	if (wind != _WIND_PointerRoot) {
		WINDOW * stack[32];
		int anc;
		int top  = _Wind_PathStack (stack, &anc, _WIND_PointerRoot, wind);
		PXY r_xy = WindPointerPos (wind);
		PXY e_xy = WindPointerPos (_WIND_PointerRoot);
		EvntPointer (stack, anc, top, e_xy, r_xy, wind->Id, NotifyGrab);
	}
}

//------------------------------------------------------------------------------
BOOL
_Wind_PgrabMatch (WINDOW * wind, CARD8 but, CARD16 mod)
{
	BTNGRAB * grab = wind->ButtonGrab;
	
	while (grab) {
		if (   (!grab->Button    || grab->Button    == but)
		    && (!grab->Modifiers || grab->Modifiers == mod)) {
			_Wind_PgrabSet (grab->Client, wind, CrsrShare (grab->Cursor),
			                grab->EventMask, grab->OwnrEvents, MAIN_TimeStamp,
			                xTrue);
			return xTrue;
		}
		grab = grab->Next;
	}
	return xFalse;
}

//------------------------------------------------------------------------------
BOOL
_Wind_PgrabClear (CLIENT * clnt)
{
	if (!clnt  ||  clnt == _WIND_PgrabClient) {
		_WIND_PgrabClient = NULL;
		_WIND_PgrabWindow = NULL;
		if (_WIND_PgrabCursor) {
			CrsrFree (_WIND_PgrabCursor, NULL);
			_WIND_PgrabCursor = NULL;
		}
		WindMctrl (xFalse);
		
		return xTrue;
	}
	return xFalse;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_GrabPointer (CLIENT * clnt, xGrabPointerReq * q)
{
	// Grabs actively the control of the pointer.
	//
	// BOOL   ownerEvents:
	// CARD16 eventMask:
	// Window grabWindow:
	// Window confineTo:   (ignored)
	// Cursor cursor:
	// Time   time:
	// BYTE   pointerMode, keyboardMode:
	//
	// Reply:
	// BYTE status:
	//...........................................................................
	
	WINDOW * wind = WindFind (q->grabWindow);
	CURSOR * crsr = NULL;
	
	if (!wind) {
		Bad(Window, q->grabWindow, GrabPointer,);
	
	} else if (q->cursor && !(crsr = CrsrGet(q->cursor))) {
		Bad(Cursor, q->cursor, GrabPointer,);
	
	} else { //..................................................................
	
		ClntReplyPtr (GrabPointer, r);
		CARD32 time = (q->time ? q->time : MAIN_TimeStamp);
		
		PRINT (GrabPointer," W:%lX(W:%lX) evnt=0x%04X/%i mode=%i/%i"
		                       " C:%lX T:%lX",
		       q->grabWindow, q->confineTo, q->eventMask, q->ownerEvents,
		       q->pointerMode, q->keyboardMode, q->cursor, q->time);
		
		if (_WIND_PgrabClient  &&  _WIND_PgrabClient != clnt) {
			r->status = AlreadyGrabbed;
		
		} else if (!WindVisible (wind)) {
			r->status = GrabNotViewable;
		
		} else if (time < _WIND_PgrabTime || time > MAIN_TimeStamp) {
			r->status = GrabInvalidTime;
		
		} else {
			r->status = GrabSuccess;
		}
		ClntReply (GrabPointer,, NULL);
		
		if (r->status == GrabSuccess) {
			_Wind_PgrabSet (clnt, wind, crsr,
			                q->eventMask, q->ownerEvents, time, xFalse);
		
		} else if (crsr) {
			CrsrFree (crsr, NULL);
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_ChangeActivePointerGrab (CLIENT * clnt, xChangeActivePointerGrabReq * q)
{
	// Changes dynamic parameters if the pointer is actively grabbed by this
	// client.
	//
	// Cursor cursor:
	// Time   time:
	// CARD16 eventMask:
	//...........................................................................
	
	CURSOR * crsr = NULL;
	CARD32   time = (q->time ? q->time : MAIN_TimeStamp);
	
	if (q->cursor && !(crsr = CrsrGet(q->cursor))) {
		Bad(Cursor, q->cursor, ChangeActivePointerGrab,);
	
	} else { //..................................................................
	
		if (clnt == _WIND_PgrabClient
		    &&  time >= _WIND_PgrabTime  &&  time <= MAIN_TimeStamp) {
			
			PRINT (ChangeActivePointerGrab," C:%lX T:%lX evnt=%04X",
			       q->cursor, q->time, q->eventMask);
			
			_WIND_PgrabEvents = q->eventMask;
			_WIND_PgrabTime   = time;
			if (_WIND_PgrabCursor) {
				CrsrFree (_WIND_PgrabCursor, NULL);
			}
			if ((_WIND_PgrabCursor = crsr)) {
				CrsrSelect (crsr);
			} else {
				_Wind_Cursor (_WIND_PgrabWindow);
			}
		
		} else {
			
			PRINT (ChangeActivePointerGrab," C:%lX T:%lX evnt=%04X ignored",
			       q->cursor, q->time, q->eventMask);
			
			if (crsr) CrsrFree (crsr, NULL);
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_UngrabPointer (CLIENT * clnt, xUngrabPointerReq * q)
{
	// Releases the pointer if this client has it grabbed.
	//
	// CARD32 id: time or CurrentTime
	//...........................................................................
	
	CARD32 wid = (_WIND_PgrabWindow ? _WIND_PgrabWindow->Id : 0);
	
	if ((!q->id || (q->id >= _WIND_PgrabTime && q->id <= MAIN_TimeStamp))
	    && _Wind_PgrabClear (clnt)) {
		
		PRINT (UngrabPointer," W:%lX T:%lX", wid, q->id);
		
		_Wind_Cursor (_WIND_PointerRoot);
	
	} else {
		PRINT (UngrabPointer," W:%lX T:%lX ignored", wid, q->id);
	}
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static BTNGRAB *
find_grab (BTNGRAB * grab, CARD8 but, CARD16 mod)
{
	while (grab) {
		if (   (!but || !grab->Button    || but == grab->Button)
		    && (!mod || !grab->Modifiers || mod == grab->Modifiers)) break;
		grab = grab->Next;
	}
	return grab;
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void
RQ_GrabButton (CLIENT * clnt, xGrabButtonReq * q)
{
	// Establishes a passive pointer grab
	//
	// CARD8  button:
	// CARD16 modifiers:
	// BOOL   ownerEvents:
	// CARD16 eventMask:
	// Window grabWindow:
	// Window confineTo:   (ignored)
	// Cursor cursor:
	// BYTE   pointerMode, keyboardMode:
	//...........................................................................
	
	WINDOW  * wind = WindFind (q->grabWindow);
	CURSOR  * crsr = NULL;
	BTNGRAB * grab = NULL, * reuse;
	
	if (!wind) {
		Bad(Window, q->grabWindow, GrabButton,);
	
	} else if (q->cursor && !(crsr = CrsrGet(q->cursor))) {
		Bad(Cursor, q->cursor, GrabButton,);
	
	} else if ((reuse = find_grab (wind->ButtonGrab, q->button, q->modifiers))
	           &&  reuse->Client != clnt) {
		Bad(Access,, GrabButton,);
	
	} else if (!reuse && !(grab = malloc (sizeof(BTNGRAB)))) {
		Bad(Alloc,, GrabButton,);
	
	} else { //..................................................................
		PRINT (GrabButton," W:%lX(W:%lX,C:%lX) evnt=0x%04X/%i mode=%i/%i"
		                  " but=%i/%04X",
		       q->grabWindow, q->confineTo, q->cursor, q->eventMask,
		       q->ownerEvents, q->pointerMode, q->keyboardMode, q->button,
		       q->modifiers);
		
		if (reuse) {
			if (reuse->Cursor  &&  reuse->Cursor != crsr) {
				CrsrFree (reuse->Cursor, NULL);
			}
			grab = reuse;
		} else {
			grab->Next       = wind->ButtonGrab;
			wind->ButtonGrab = grab;
			grab->Client     = clnt;
			clnt->EventReffs++;
		}
		grab->PntrAsync  = q->pointerMode;
		grab->KybdAsync  = q->keyboardMode;
		grab->OwnrEvents = q->ownerEvents;
		grab->Button     = q->button;
		grab->Modifiers  = q->modifiers;
		grab->EventMask  = q->eventMask;
		grab->Cursor     = crsr;
		crsr             = NULL;
	}
	if (crsr) {
		CrsrFree (crsr, NULL);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_UngrabButton (CLIENT * clnt, xUngrabButtonReq * q)
{
	// Releases passive grab
	//
	// CARD8  button:
	// CARD16 modifiers:
	// Window grabWindow:
	//...........................................................................
	
	WINDOW  * wind = WindFind (q->grabWindow);
	
	if (!wind) {
		Bad(Window, q->grabWindow, GrabButton,);
	
	} else { //..................................................................
		BTNGRAB ** pGrab = &wind->ButtonGrab;
		
		PRINT (UngrabButton," W:%lX but=%i/%04X",
		       q->grabWindow, q->button, q->modifiers);
		
		while (*pGrab) {
			if ((*pGrab)->Client == clnt
			    && (!q->button    || q->button    == (*pGrab)->Button)
			    && (!q->modifiers || q->modifiers == (*pGrab)->Modifiers)) {
				BTNGRAB * grab = *pGrab;
				*pGrab         = grab->Next;
				if (grab->Cursor) CrsrFree (grab->Cursor, NULL);
				grab->Client->EventReffs++;
				free (grab);
				if (q->button && q->modifiers) break;
			}
			pGrab = &(*pGrab)->Next;
		}
	}
}


//------------------------------------------------------------------------------
void
RQ_GrabKeyboard (CLIENT * clnt, xGrabKeyboardReq * q)
{
	WINDOW * wind = WindFind (q->grabWindow);
	
	if (!wind) {
		Bad(Window, q->grabWindow, GrabKeyboard,);
	
	} else {
		ClntReplyPtr (GrabKeyboard, r);
		
		PRINT (- X_GrabKeyboard," W:%lX ownr=%i mode=%i/%i T:%lX",
		       q->grabWindow, q->ownerEvents,
		       q->pointerMode, q->keyboardMode, q->time);
		
		r->status = GrabSuccess;
		
		ClntReply (GrabKeyboard,, NULL);
	}
}

//------------------------------------------------------------------------------
void
RQ_UngrabKeyboard (CLIENT * clnt, xUngrabKeyboardReq * q)
{
	PRINT (- X_UngrabKeyboard," T:%lX", q->id);
}

//------------------------------------------------------------------------------
void
RQ_GrabKey (CLIENT * clnt, xGrabKeyReq * q)
{
	PRINT (- X_GrabKey," W:%lX key=%i/%04x",
	       q->grabWindow, q->key, q->modifiers);
}

//------------------------------------------------------------------------------
void
RQ_UngrabKey (CLIENT * clnt, xUngrabKeyReq * q)
{
	PRINT (- X_UngrabKey," W:%lX key=%i/%04x",
	       q->grabWindow, q->key, q->modifiers);
}


//------------------------------------------------------------------------------
void
RQ_SetInputFocus (CLIENT * clnt, xSetInputFocusReq * q)
{
	PRINT (- X_SetInputFocus," W:%lX(%i) T:%lX", q->focus, q->revertTo, q->time);
}

//------------------------------------------------------------------------------
void
RQ_GetInputFocus (CLIENT * clnt, xGetInputFocusReq * q)
{
	ClntReplyPtr (GetInputFocus, r);
	
//	PRINT (- X_GetInputFocus," ");
	
	r->revertTo = None;
	r->focus    = PointerRoot; //None;
	
	ClntReply (GetInputFocus,, "w");
}
