//==============================================================================
//
// Property.c -- Implementation of struct 'PROPERTY' related functions.
//
// Even  if  the  server normally doesn't interprete property contents, some of
// it will be reported to the built-in window manager if changed or created.
// These are:  XA_WM_COMMAND, XA_WM_NAME, XA_WM_ICON_NAME.
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-07 - Module released for beta state.
// 2000-06-19 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "tools.h"
#include "Property.h"
#include "clnt.h"
#include "window.h"
#include "Atom.h"
#include "event.h"
#include "wmgr.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h> // debug

#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>


//------------------------------------------------------------------------------
static p_PROPERTY *
_Prop_Find (WINDOW * wind, Atom prop)
{
	p_PROPERTY * pProp = &wind->Properties;
	
	while (*pProp) {
		if ((*pProp)->Name == prop) {
			break;
		}
		pProp = &(*pProp)->Next;
	}
	return pProp;
}


//==============================================================================
void *
PropValue (WINDOW * wind, Atom name, Atom type, size_t min_len)
{
	PROPERTY * prop = *_Prop_Find (wind, name);
	
	if (!prop || (type != None && type != prop->Type)
	          || (min_len && min_len > prop->Length)) return NULL;
	
	return prop->Data;
}


//==============================================================================
BOOL
PropHasAtom (WINDOW * wind, Atom name, Atom which)
{
	PROPERTY * prop = *_Prop_Find (wind, name);
	BOOL       has  = xFalse;
	
	if (prop  &&  prop->Type == XA_ATOM) {
		Atom * list = (Atom*)prop->Data;
		int    num  = prop->Length /4;
		int    i;
		for (i = 0; i < num; ++i) {
			if (list[i] == which) {
				has = xTrue;
				break;
			}
		}
	}
	return has;
}


//==============================================================================
void
PropDelete (p_PROPERTY * pProp)
{
	PROPERTY * prop = *pProp;
	if (prop) {
		*pProp = prop->Next;
		free (prop);
	}
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_ChangeProperty (CLIENT * clnt, xChangePropertyReq * q)
{
	// Alters or creates a property of the given window.
	//
	// Window window:   location for the property
	// Atom   property: to be altered (or created)
	// Atom   type:     uninterpreted, but must match for existing
	// CARD8  mode:     Replace|Prepend|Append
	// CARD8  format:   8/16/32-bit per unit
	// CARD32 nUnits:   length of stuff following, depends on format
	// void   (q +1):   data to be applied
	//...........................................................................
	
	WINDOW * wind = WindFind (q->window);
	
	PROPERTY * prop, ** pProp;
	int factor = (q->format == 8  ? 1 : // number of bytes per unit
	              q->format == 16 ? 2 :
	              q->format == 32 ? 4 : 0);
	
	if (!wind) {
		Bad(Window, q->window, ChangeProperty,"():\n          not %s.",
		                       (DBG_XRSC_TypeError ? "a window" : "found"));
		
	} else if (!AtomValid(q->property)) {
		Bad(Atom, q->property, ChangeProperty,"(W:%lX):\n"
		                        "          invalid property.", q->window);
	
	} else if (!AtomValid(q->type)) {
		Bad(Atom, q->type, ChangeProperty,"(W:%lX,'%s'):\n"
		                   "          undefined type.",
		                   q->window, ATOM_Table[q->property]->Name);
	
	} else if (!factor) {
		Bad(Value, q->format, ChangeProperty,"(W:%lX,'%s'):\n"
		                      "          illegal format.",
		                      q->window, ATOM_Table[q->property]->Name);
	
	} else if ((prop = *(pProp = _Prop_Find (wind, q->property))) &&
	            q->mode != PropModeReplace                        &&
	           (q->type != prop->Type  ||  q->format != prop->Format)) {
		Bad(Match,, ChangeProperty,"(W:%lX,'%s'):\n"
		            "          type = A:%lu/A:%lu, format = %i/%i.",
		            q->window, ATOM_Table[q->property]->Name,
		            q->type, prop->Type, q->format, prop->Format);
	
	} else { //..................................................................
		
		size_t len  = q->nUnits * factor;
		size_t size = (len +3) & ~3ul;
		size_t have = (prop ? (prop->Length +3) & ~3ul : 0);
		CARD8  mode = (prop && prop->Length ? q->mode : PropModeReplace);
		BOOL   rplc = (prop && size != have);
		
		if (!prop ||  size != have) {
			size_t need = (mode == PropModeReplace ? len : len + prop->Length);
			size_t byte = sizeof(PROPERTY) -4 + ((need +3) & ~3ul);
			CARD8  xtra = (need & 3 ? 0 : 1);
			// reserve an extra char space for a trailing '\0'
			prop        = malloc (byte + xtra);
			if (prop) {
				prop->Next   = (rplc ? (*pProp)->Next : *pProp);
				prop->Name   = q->property;
				prop->Type   = q->type;
				prop->Format = q->format;
				prop->Length = need;
			
			} else if (*pProp && size < have) {
				// reuse old property
			#	ifndef NODEBUG
				PRINT (,"\33pWARNING\33q ChangeProperty(W:%lX,A:%lu):\n"
		             "          memory exhausted (%lu bytes), reuse previous.",
		             q->window, q->property, byte);
			#	endif NODEBUG
				prop         = *pProp;
				prop->Type   = q->type;
				prop->Format = q->format;
				prop->Length = need;
				rplc         = xFalse; // must not be deleted now!
			
			} else {
				Bad(Alloc,, ChangeProperty,"(W:%lX,A:%lu):\n"
				            "          %lu bytes.", q->window, q->property, byte);
			}
		
		} else if (q->mode == PropModeReplace) {
			prop->Length = len;
		} else {
			prop->Length += len;
		}
		
		if (prop) {
			char * data = prop->Data;
			
			if (mode == PropModePrepend) {
				memmove (data + len, (*pProp)->Data, (*pProp)->Length);
			
			} else if (mode == PropModeAppend) {
				if (prop != *pProp) {
					memcpy (data, (*pProp)->Data, (*pProp)->Length);
				}
				data += (*pProp)->Length;
			}
			
			if (len) {
				if (!clnt->DoSwap  ||  q->format == 8) {
					memcpy (data, (q +1), len);
				
				} else if (q->format == 16) {
					CARD16 * src = (CARD16*)(q +1);
					CARD16 * dst = (CARD16*)data;
					size_t   num = q->nUnits;
					while (num--) {
						*(dst++) = Swap16(*(src++));
					}
				
				} else { // q->format == 32
					CARD32 * src = (CARD32*)(q +1);
					CARD32 * dst = (CARD32*)data;
					size_t   num = q->nUnits;
					while (num--) {
						*(dst++) = Swap32(*(src++));
					}
				}
			}
			
			prop->Data[prop->Length] = '\0';
			
			if (rplc) {
				free (*pProp);
			}
			*pProp = prop;
			
			if (wind->Handle > 0) switch (prop->Name) {
				case XA_WM_COMMAND:
					WmgrClntUpdate (clnt, prop->Data);
					break;
				case XA_WM_NAME:
					WmgrWindName (wind, prop->Data, xTrue);
					break;
				case XA_WM_ICON_NAME:
					WmgrWindName (wind, prop->Data, xFalse);
					break;
			}
			
		#	ifdef TRACE
			PRINT (ChangeProperty, "- '%s'(%s,%i*%li) of W:%lX",
	      		 ATOM_Table[prop->Name]->Name, ATOM_Table[prop->Type]->Name,
	      		 factor, q->nUnits, q->window);
			if (prop->Type == XA_STRING) {
				PRINT (,"+\n          '%s'", prop->Data);
			} else {
				PRINT (,"+");
			}
		#	endif TRACE
			if (wind->u.List.AllMasks & PropertyChangeMask) {
				EvntPropertyNotify (wind, prop->Name, PropertyNewValue);
			}
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_DeleteProperty (CLIENT * clnt, xDeletePropertyReq * q)
{
	// Deletes an (existing) property from the given window.
	//
	// Window window:   location for the property
	// Atom   property: to be altered (or created)
	//...........................................................................
	
	WINDOW * wind = WindFind (q->window);
	
	PROPERTY ** pProp;
	
	if (!wind) {
		Bad(Window, q->window, DeleteProperty,"():\n          not %s.",
		                       (DBG_XRSC_TypeError ? "a window" : "found"));
		
	} else if (!AtomValid(q->property)) {
		Bad(Atom, q->property, DeleteProperty,"(W:%lX)", q->window);
	
	} else { //..................................................................
		
		PROPERTY * prop = * (pProp = _Prop_Find (wind, q->property));
		
		DEBUG (DeleteProperty," '%s'(%s) from W:%lX",
		       ATOM_Table[q->property]->Name,
		       (prop ? ATOM_Table[prop->Type]->Name : "n/a"), q->window);
		
		if (prop) {
			if (wind->u.List.AllMasks & PropertyChangeMask) {
				EvntPropertyNotify (wind, prop->Name, PropertyDelete);
			}
			*pProp = prop->Next;
			free (prop);
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_GetProperty (CLIENT * clnt, xGetPropertyReq * q)
{
	// Returns the full or partial content of a property.
	//
	// Window window:     location for the property
	// Atom   property:   source to get content from
	// Atom   type:       AnyPropertyType or must match
	// CARD32 longOffset: offset in 32bit units, indipendent of property format
	// CARD32 longLength: length to read in 32bit units
	// BOOL   delete:     remove property if all date were read
	//
	// Reply:
	// Atom   propertyType: None, if property doesn't exist
	// CARD8  format:       0|8|16|32
	// CARD32 bytesAfter:   length of data behind the part read
	// CARD32 nItems:       # of 8, 16, or 32-bit entities in reply
	// (char*)(r +1):       content
	//...........................................................................
	
	WINDOW * wind = WindFind (q->window);
	CARD32   offs = q->longOffset *4;
	
	PROPERTY ** pProp;
	
	if (!wind) {
		Bad(Window, q->window, GetProperty,"():\n          not %s.",
			                    (DBG_XRSC_TypeError ? "a window" : "found"));
		
	} else if (!AtomValid(q->property)) {
		Bad(Atom, q->property ,GetProperty,"(W:%lX)", q->window);
	
	} else if (*(pProp = _Prop_Find (wind, q->property)) &&
	           offs > (*pProp)->Length) {
		Bad(Value, q->longOffset, GetProperty,"(W:%lX,A:%lu)\n"
		                          "          offset %lu > property length %u.",
		                          q->window, q->property, offs, (*pProp)->Length);
	
	} else { //..................................................................
		
		ClntReplyPtr (GetProperty, r);
		PROPERTY * prop = *pProp;
		size_t     len  = 0;
		
		if (!prop) {
			r->propertyType = None;
			r->format       = 0;
			r->bytesAfter   = 0;
			r->nItems       = 0;
		
		} else if (q->type != AnyPropertyType  &&  q->type != prop->Type ) {
			r->propertyType = prop->Type;
			r->format       = prop->Format;
			r->bytesAfter   = prop->Length;
			r->nItems       = 0;
			
		} else {
			CARD8  byte = prop->Format /8;
			len = (q->longLength *4) +offs;
			len = (len <= prop->Length ? len : prop->Length) - offs;
			
			r->propertyType = prop->Type;
			r->format       = prop->Format;
			r->bytesAfter   = (prop->Length - offs) - len;
			
			if (!len) {
				r->nItems = 0;
				
			} else if (!clnt->DoSwap  ||  prop->Format == 8) {
				r->nItems = (byte == 1 ? len : (len + byte -1) / byte);
				memcpy ((r +1), prop->Data + offs, len);
				
			} else if (prop->Format == 16) {
				CARD16 * src = (CARD16*)(prop->Data + offs);
				CARD16 * dst = (CARD16*)(r +1);
				size_t   num = r->nItems = (len +1) /2;
				while (num--) {
					*(dst++) = Swap16(*(src++));
				}
				
			} else { // prop->Format == 32
				CARD32 * src = (CARD32*)(prop->Data + offs);
				CARD32 * dst = (CARD32*)(r +1);
				size_t   num =  r->nItems = (len +3) /4;
				while (num--) {
					*(dst++) = Swap32(*(src++));
				}
			}
		}
		
		DEBUG (GetProperty," '%s'(%s,%u) %lu-%lu from W:%lX",
		       ATOM_Table[q->property]->Name,
		       ATOM_Table[r->propertyType]->Name,
		       r->format, offs, len, q->window);
		
		ClntReply (GetProperty, len, "all");
		
		if (prop && !r->bytesAfter && q->delete) {
			if (wind->u.List.AllMasks & PropertyChangeMask) {
				EvntPropertyNotify (wind, prop->Name, PropertyDelete);
			}
			*pProp = prop->Next;
			free (prop);
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_ListProperties (CLIENT * clnt, xListPropertiesReq * q)
{
	// Returns a list of the atoms of all properties currently defined on the
	// window.
	//
	// CARD32 id: window
	// 
	// Reply:
	// CARD16 nProperties: # of atoms
	// (Atom*)(r +1)       list of atoms
	//...........................................................................
	
	WINDOW * wind = WindFind (q->id);
	
	if (!wind) {
		Bad(Window, q->id, ListProperties,"():\n          not %s.",
		                   (DBG_XRSC_TypeError ? "a window" : "found"));
	
	} else { //..................................................................
		
		ClntReplyPtr (ListProperties, r);
		PROPERTY * prop = wind->Properties;
		Atom     * atom = (Atom*)(r +1);
		
		r->nProperties = 0;
		
		if (clnt->DoSwap) while (prop) {
			*(atom++) = Swap32(prop->Name);
			r->nProperties++;
			prop = prop->Next;
		
		} else while (prop) { // (!clnt->DoSwap)
			*(atom++) = prop->Name;
			r->nProperties++;
			prop = prop->Next;
		}
		
		DEBUG (ListProperties," of W:%lX (%u)", q->id, r->nProperties);
		
		ClntReply (ListProperties, (r->nProperties *4), ".");
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_RotateProperties (CLIENT * clnt, xRotatePropertiesReq * q)
{
	// Every property named by the given atom list gets the value from the
	// property that is delta list entries apart.
	// In fact, all properties get the nameing atom from the property that is
	// -delta entries apart, same result.
	//
	// Window window:     location for the properties
	// CARD16 nAtoms:     # of properties to be rotated
	// INT16  nPositions: delta value
	// (Atom*)(q +1)      list of properties
	//...........................................................................
	
	WINDOW * wind = WindFind (q->window);
	
	if (!wind) {
		Bad(Window, q->window, RotateProperties,"():\n          not %s.",
		                       (DBG_XRSC_TypeError ? "a window" : "found"));
	
	} else if (q->nAtoms) { //...................................................
		
		PROPERTY ** list = (PROPERTY**)(q +1);
		BOOL        ok   = xTrue;
		Atom        name;
		int i, j;
		
		for (i = 0; i < q->nAtoms; ++i) {
			name = ((Atom*)(q +1))[i];
			if (!AtomValid (name)) {
				Bad(Atom, name, RotateProperties,"(W:%lX)", q->window);
				ok = xFalse;
				break;
	
			} else if (!(list[i] = *_Prop_Find (wind, name))) {
				Bad(Match,, RotateProperties,"(W:%lX):\n"
				            "          property A:%lX not found.", q->window, name);
				ok = xFalse;
				break;
			
			} else for (j = 0; j < i; ++j) {
				if (list[j]->Name == name) {
					Bad(Match,, RotateProperties,"(W:%lX):\n"
					            "          property A:%lX occured more than once.",
					            q->window, name);
					ok = xFalse;
					i  = q->nAtoms;
					break;
				}
			}
		}
		if (ok) {
			Atom first = list[0]->Name;
			int  delta = q->nPositions % q->nAtoms;
			
			for (i = 0; i < q->nAtoms; ++i) {
				if (!(j = i + delta)) {
					name = first;
				} else {
					if      (j <  0)         j += q->nAtoms;
					else if (j >= q->nAtoms) j -= q->nAtoms;
					name = list[j]->Name;
				}
				list[i]->Name = name;
				
				if (wind->u.List.AllMasks & PropertyChangeMask) {
					EvntPropertyNotify (wind, name, PropertyNewValue);
				}
			}
	
			DEBUG (RotateProperties,"(W:%lX,%u,%+i)",
			       q->window, q->nAtoms, q->nPositions);
		}
	}
}
