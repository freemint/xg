//==============================================================================
//
// wind_grab.c
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-07-24 - Initial Version.
//==============================================================================
//
#include "window_P.h"
#include "Request.h"

#include <stdio.h> // printf


//==============================================================================
//
// Callback Functions


//------------------------------------------------------------------------------
void
RQ_GrabPointer (CLIENT * clnt, xGrabPointerReq * q)//, WINDOW * wind)
{
	WINDOW * wind = WindFind (q->grabWindow);
	
	if (!wind) {
		Bad(Window, q->grabWindow, GrabPointer,);
	
	} else {
		ClntReplyPtr (GrabPointer, r);
		
		PRINT (- X_GrabPointer," W:%lX(W:%lX) evnt=0x%04X/%i mode=%i/%i"
		                       " C:%lX T:%lX",
		       q->grabWindow, q->confineTo, q->eventMask, q->ownerEvents,
		       q->pointerMode, q->keyboardMode, q->cursor, q->time);
		
		r->status = AlreadyGrabbed;
		
		ClntReply (GrabPointer,, NULL);
	}
}

//------------------------------------------------------------------------------
void
RQ_ChangeActivePointerGrab (CLIENT * clnt, xChangeActivePointerGrabReq * q)
{
	PRINT (- X_ChangeActivePointerGrab," C:%lX T:%lX evnt=%04X",
	       q->cursor, q->time, q->eventMask);
}

//------------------------------------------------------------------------------
void
RQ_UngrabPointer (CLIENT * clnt, xUngrabPointerReq * q)
{
	if (q->id != CurrentTime) {
	//	if (q->id < MAIN_GrabPointer) {
	//		printf ("### %lu earlier than grab %lu \n", q->id, MAIN_GrabPointer);
	//	} else if (q->id > MAIN_TimeStamp) {
	//		printf ("### %lu later than grab %lu \n", q->id, MAIN_TimeStamp);
	//	}
	}
}

//------------------------------------------------------------------------------
void
RQ_GrabButton (CLIENT * clnt, xGrabButtonReq * q)//, WINDOW * wind)
{
	PRINT (- X_GrabButton," W:%lX(W:%lX) evnt=0x%04X/%i mode=%i/%i"
	                      " crsr=0x%lX but=%i/%04X",
	       q->grabWindow, q->confineTo, q->eventMask, q->ownerEvents,
	       q->pointerMode, q->keyboardMode, q->cursor, q->button, q->modifiers);
}

//------------------------------------------------------------------------------
void
RQ_UngrabButton (CLIENT * clnt, xUngrabButtonReq * q)//, WINDOW * wind)
{
	PRINT (- X_UngrabButton," W:%lX but=%i/%04X",
	       q->grabWindow, q->button, q->modifiers);
}

//------------------------------------------------------------------------------
void
RQ_GrabKeyboard (CLIENT * clnt, xGrabKeyboardReq * q)//, WINDOW * wind)
{
	PRINT (- X_GrabKeyboard," W:%lX T:%lX", q->grabWindow, q->time);
}

//------------------------------------------------------------------------------
void
RQ_UngrabKeyboard (CLIENT * clnt, xUngrabKeyboardReq * q)
{
	PRINT (- X_UngrabKeyboard," T:%lX", q->id);
}

//------------------------------------------------------------------------------
void
RQ_GrabKey (CLIENT * clnt, xGrabKeyReq * q)//, WINDOW * wind)
{
	PRINT (- X_GrabKey," W:%lX key=%i/%04x",
	       q->grabWindow, q->key, q->modifiers);
}

//------------------------------------------------------------------------------
void
RQ_UngrabKey (CLIENT * clnt, xUngrabKeyReq * q)//, WINDOW * wind)
{
	PRINT (- X_UngrabKey," W:%lX key=%i/%04x",
	       q->grabWindow, q->key, q->modifiers);
}


//------------------------------------------------------------------------------
void
RQ_SetInputFocus (CLIENT * clnt, xSetInputFocusReq * q)//, WINDOW * wind)
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
