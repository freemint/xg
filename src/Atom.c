//==============================================================================
//
// Atom.c -- Implementation of struct 'ATOM' related functions.
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-07 - Module released for beta state.
// 2000-06-18 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "tools.h"
#include "Atom.h"
#include "clnt.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xatom.h>


#define MAX_ATOM   200

ATOM * ATOM_Table[MAX_ATOM +2] = {
#	define ENTRY(e) \
               (ATOM*) "\0\0\0\0" "\0\0\0\0" "\0\0\0\0" "\0\0\0\0" "\0\0\0\0" #e
	ENTRY (None),
	ENTRY (XA_PRIMARY),			ENTRY (XA_SECONDARY),	ENTRY (XA_ARC),
	ENTRY (XA_ATOM),				ENTRY (XA_BITMAP), 		ENTRY (XA_CARDINAL),
	ENTRY (XA_COLORMAP),			ENTRY (XA_CURSOR),		ENTRY (XA_CUT_BUFFER0),
	ENTRY (XA_CUT_BUFFER1),		ENTRY (XA_CUT_BUFFER2),	ENTRY (XA_CUT_BUFFER3),
	ENTRY (XA_CUT_BUFFER4),		ENTRY (XA_CUT_BUFFER5),	ENTRY (XA_CUT_BUFFER6),
	ENTRY (XA_CUT_BUFFER7),		ENTRY (XA_DRAWABLE),		ENTRY (XA_FONT),
	ENTRY (XA_INTEGER),			ENTRY (XA_PIXMAP),		ENTRY (XA_POINT),
	ENTRY (XA_RECTANGLE),		ENTRY (XA_RESOURCE_MANAGER),
	ENTRY (XA_RGB_COLOR_MAP),	ENTRY (XA_RGB_BEST_MAP),ENTRY (XA_RGB_BLUE_MAP),
	ENTRY (XA_RGB_DEFAULT_MAP),ENTRY (XA_RGB_GRAY_MAP),ENTRY (XA_RGB_GREEN_MAP),
	ENTRY (XA_RGB_RED_MAP),		ENTRY (XA_STRING),		ENTRY (XA_VISUALID),
	ENTRY (XA_WINDOW),			ENTRY (XA_WM_COMMAND),	ENTRY (XA_WM_HINTS),
	ENTRY (XA_WM_CLIENT_MACHINE),								ENTRY (XA_WM_ICON_NAME),
	ENTRY (XA_WM_ICON_SIZE),	ENTRY (XA_WM_NAME),		ENTRY (XA_WM_NORMAL_HINTS),
	ENTRY (XA_WM_SIZE_HINTS),	ENTRY (XA_WM_ZOOM_HINTS),	ENTRY (XA_MIN_SPACE),
	ENTRY (XA_NORM_SPACE),		ENTRY (XA_MAX_SPACE),	ENTRY (XA_END_SPACE),
	ENTRY (XA_SUPERSCRIPT_X),	ENTRY (XA_SUPERSCRIPT_Y),	ENTRY (XA_SUBSCRIPT_X),
	ENTRY (XA_SUBSCRIPT_Y),		ENTRY (XA_UNDERLINE_POSITION),
	ENTRY (XA_UNDERLINE_THICKNESS),	ENTRY (XA_STRIKEOUT_ASCENT),
	ENTRY (XA_STRIKEOUT_DESCENT),	ENTRY (XA_ITALIC_ANGLE),ENTRY (XA_X_HEIGHT),
	ENTRY (XA_QUAD_WIDTH),		ENTRY (XA_WEIGHT),		ENTRY (XA_POINT_SIZE),
	ENTRY (XA_RESOLUTION),		ENTRY (XA_COPYRIGHT),	ENTRY (XA_NOTICE),
	ENTRY (XA_FONT_NAME),		ENTRY (XA_FAMILY_NAME),	ENTRY (XA_FULL_NAME),
	ENTRY (XA_CAP_HEIGHT),		ENTRY (XA_WM_CLASS),	ENTRY (XA_WM_TRANSIENT_FOR),
	//--- Extra Atom Definitions
	ENTRY (WM_PROTOCOLS),		ENTRY (WM_DELETE_WINDOW),
};
CARD32 ATOM_Count = LAST_PREDEF_ATOM;


//==============================================================================
void
AtomInit (BOOL initNreset)
{
	int i;
	
	// first call from Server initialization, fill in the name length for each
	// predefined atom
	if (initNreset) {
		for (i = 1; i <= LAST_PREDEF_ATOM; ++i) {
			ATOM_Table[i]->Id     = i;
			ATOM_Table[i]->Length = strlen (ATOM_Table[i]->Name);
		}
	
	// later calls from server reset, delete all non-predefined atoms
	} else {
		for (i = LAST_PREDEF_ATOM +1; i <= ATOM_Count; ++i) {
			free (ATOM_Table[i]);
		}
	#	ifdef TRACE
		if ((i = ATOM_Count - LAST_PREDEF_ATOM) > 0) {
			printf ("  remove %i Atom%s.\n", i, (i == 1 ? "" : "s"));
		}
	#	endif TRACE
	}
	
	// always clear all empty table entries
	memset (ATOM_Table + LAST_PREDEF_ATOM +1, 0,
	         (MAX_ATOM - LAST_PREDEF_ATOM) * sizeof(ATOM*));
	ATOM_Count = LAST_PREDEF_ATOM;
}



//==============================================================================
//
// Callback Functions

#include "Request.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_InternAtom (CLIENT * clnt, xInternAtomReq * q)
{
	// Returns the atom for a given name
	//
	// BOOL   onlyIfExists:      TRUE: a nonexisting atom won't be created
	// CARD16 nbytes:            number of bytes in string
	char * name = (char*)(q +1); // name to be found
	//
	// Reply:
	// Atom atom:   Atom id or 'None' if not existing and onlyIfExists = TRUE
	//...........................................................................
	
	if (!q->nbytes) {
		Bad(Value, 0, InternAtom, "():\n          zero length string.");
	
	} else { //..................................................................
		
		ClntReplyPtr (InternAtom, r);
		ATOM * atom = NULL;
		int    n    = 1;
		
		do {
			if (ATOM_Table[n]->Length == q->nbytes &&
			    !strncmp (ATOM_Table[n]->Name, name, q->nbytes)) {
				atom = ATOM_Table[n];
				break;
			}
		} while (++n <= ATOM_Count);
		
		if (!q->onlyIfExists && !atom) {
			BOOL full = (ATOM_Count >= MAX_ATOM);
			if (!full && (atom = malloc (sizeof(ATOM) + q->nbytes))) {
				ATOM_Table[++ATOM_Count] = atom;
				memcpy (atom->Name,name, q->nbytes);
				atom->Name[q->nbytes] = '\0';
				atom->Length          = q->nbytes;
				atom->Id              = ATOM_Count;
				atom->SelOwner        = NULL;
				atom->SelWind         = NULL;
				atom->SelTime         = 0uL;
				n =  -1; // only for debug output
			
			} else {
				Bad(Alloc,, InternAtom, "('%.*s'):\n"
				            "          %s.", q->nbytes, name,
				            (full ? "maximum number reached" : "memory exhausted"));
			}
		}
		if (r) {
			r->atom = (atom ? atom->Id : None);
			DEBUG (InternAtom, "('%.*s') -> %lu%s",
			       q->nbytes, name, r->atom, (n < 0 ? "+" : ""));
			ClntReply (InternAtom,, "a");
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_GetAtomName (CLIENT * clnt, xGetAtomNameReq * q)
{
	// Returns the name for a given atom
	//
	// CARD32 id: requested atom
	//
	// Reply:
	// CARD16 nameLength: # of characters in name
	// (char*)(r +1):     name
	//...........................................................................
	
	if (!AtomValid(q->id)) {
		Bad(Atom, q->id, GetAtomName,);
	
	} else { //..................................................................
		
		ClntReplyPtr (GetAtomName, r);
		size_t len = r->nameLength = ATOM_Table[q->id]->Length;
		
		DEBUG (GetAtomName, "(%lu) ->'%.*s'",
		       q->id, (int)len, ATOM_Table[q->id]->Name);
		
		memcpy (r +1, ATOM_Table[q->id]->Name, len);
		ClntReply (GetAtomName, len, ".");
	}
}
