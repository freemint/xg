//==============================================================================
//
// x_gem.c -- extensions to the gem-lib.
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-05 - Initial Version.
//==============================================================================
//
#include "x_gem.h"

#include <stddef.h>



//==============================================================================
short
wind_get_one (int WindowHandle, int What)
{
	short value;
	
	if ( wind_get( WindowHandle, What, &value, NULL, NULL, NULL) )
		return value;
	
	return -1;
}

//==============================================================================
short
wind_set_proc (int WindowHandle, CICONBLK *icon)
{
	return wind_set_str( WindowHandle, 201, (char *)icon);
}
