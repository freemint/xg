//==============================================================================
//
// Property.h -- Declaration of struct 'PROPERTY' and related functions.
//
// Properties  are  intended  as a general-purpose naming mechanism for clients
// and  haven't  placed  some  interpretation  on it by the protocol.  Only the
// format value is needed by the server to do byte swapping, if neccessary.
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-07 - Module released for beta state.
// 2000-06-24 - Initial Version.
//==============================================================================
//
#ifndef __PROPERTY_H__
#	define __PROPERTY_H__

#include <X11/Xdefs.h>


typedef struct s_PROPERTY {
	p_PROPERTY Next;
	Atom       Name;
	Atom       Type;
	CARD8      Format : 8; // 8/16/32 bit
	size_t     Length :24; // in bytes
	char       Data[4];
} PROPERTY;

void PropDelete (p_PROPERTY * pProp);

void * PropValue   (p_WINDOW wind, Atom name, Atom type, size_t min_len);
BOOL   PropHasAtom (p_WINDOW wind, Atom name, Atom which);


#endif __PROPERTY_H__
