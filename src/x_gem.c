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


extern short aes_control[];
extern short aes_intin[];
extern short aes_intout[];


//==============================================================================
short
wind_get_one (int WindowHandle, int What)
{
	aes_intin[0] = WindowHandle;
	aes_intin[1] = What;
   
   aes_control[0] = 104;
   aes_control[1] = 2;
   aes_control[2] = 5;
   aes_control[3] = 0;
   aes_control[4] = 0;
   aes (&aes_params);
	
	return (aes_intout[0] ? aes_intout[1] : -1);
}

//==============================================================================
short
wind_set_proc (int WindowHandle, CICONBLK *icon)
{
	aes_intin[0]                 = WindowHandle;
	aes_intin[1]                 = 201;
	*(CICONBLK**)(&aes_intin[2]) = icon;
	aes_intin[4] = aes_intin[5]  = 0;
	
	aes_control[0] = 105;
	aes_control[1] = 6;
	aes_control[2] = 1;
	aes_control[3] = 0;
	aes_control[4] = 0;
	aes (&aes_params);
	
	return aes_intout[0];
}
