//==============================================================================
//
// keyboard.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-11-15 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "clnt.h"
#include "tools.h"
#include "keyboard.h"
#include "window_P.h"
#include "event.h"
#include "x_gem.h"

#include <stdio.h>

#include <X11/keysym.h>


KeySym KYBD_Map[][2] = {
#	include "keyb_map.c"
};
#define     KEYSYM_OFFS   8
const CARD8 KYBD_MapMin = KEYSYM_OFFS + 1;
const CARD8 KYBD_MapMax = KEYSYM_OFFS + (sizeof(KYBD_Map) / sizeof(*KYBD_Map));


//==============================================================================
void
KybdEvent (short scan, short prev_meta)
{
	static BYTE pndg = 0;
	
	if (_WIND_PointerRoot) {
		CARD8  chng = (MAIN_KeyButMask ^ prev_meta) >> 1;
		CARD32 c_id = _WIND_PointerRoot->Id;
		PXY    r_xy = WindPointerPos (NULL);
		PXY    e_xy = WindPointerPos (_WIND_PointerRoot);
		
		if (pndg) {
			EvntPropagate (_WIND_PointerRoot, KeyReleaseMask, KeyRelease,
			               _WIND_PointerRoot->Id, r_xy, e_xy, pndg);
			pndg = 0;
		}
		if (chng) {
			BYTE code[] = { 0, KEY_LOCK, KEY_CTRL, KEY_ALT,
			                   KEY_SHIFT_R, KEY_SHIFT_L };
			int  i      = 1;
			do {
				if (chng & 1) {
					if (prev_meta & (1 << i)) {
						EvntPropagate (_WIND_PointerRoot, KeyReleaseMask, KeyRelease,
						               c_id, r_xy, e_xy, code[i] + KEYSYM_OFFS);
					} else {
						EvntPropagate (_WIND_PointerRoot, KeyPressMask, KeyPress,
						               c_id, r_xy, e_xy, code[i] + KEYSYM_OFFS);
					}
				}
				i++;
			} while ((chng >>= 1));
			
		}
		if (scan) {
			pndg = scan >>8;
			if (MAIN_KeyButMask & K_ALT) switch (pndg) {
				case SCN_A_DIAER: pndg = KEY_BRAC_R;  break;
				case SCN_O_DIAER: pndg = KEY_BRAC_L;  break;
				case SCN_U_DIAER: pndg = KEY_AT_BKSL; break;
				case SCN_PAREN_L: pndg = KEY_PAGE_UP; break;
				case SCN_PAREN_R: pndg = KEY_PAGE_DN; break;
			}
			pndg += KEYSYM_OFFS;
			EvntPropagate (_WIND_PointerRoot, KeyPressMask, KeyPress,
			               c_id, r_xy, e_xy, pndg);
		}
	}
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_SetModifierMapping (CLIENT * clnt, xSetModifierMappingReq * q)
{
	PRINT (- X_SetModifierMapping," ");
}

//------------------------------------------------------------------------------
void
RQ_GetModifierMapping (CLIENT * clnt, xGetModifierMappingReq * q)
{
	// Returns the keycodes of the modifiers Shift, Lock, Control, Mod1...Mod5
	//
	// Reply:
	// CARD8 numKeyPerModifier
	//...........................................................................
	
	ClntReplyPtr (GetModifierMapping, r);
	CARD16 * k = (CARD16*)(r +1);
	
	DEBUG (GetModifierMapping," ");
	
	r->numKeyPerModifier = 2;
	k[0] = ((KEY_SHIFT_R + KEYSYM_OFFS) << 8) | (KEY_SHIFT_L + KEYSYM_OFFS);
	k[1] = ((KEY_LOCK    + KEYSYM_OFFS) << 8) | 0;
	k[2] = ((KEY_CTRL    + KEYSYM_OFFS) << 8) | 0;
	k[3] = ((KEY_ALT     + KEYSYM_OFFS) << 8) | 0;
	k[4] = ((KEY_SHIFT_L + KEYSYM_OFFS) << 8) | 0;
	k[5] = ((KEY_SHIFT_R + KEYSYM_OFFS) << 8) | 0;
	k[6] = k[7]               = 0;
	ClntReply (GetModifierMapping, (8 *2), NULL);
}


//------------------------------------------------------------------------------
void
RQ_ChangeKeyboardMapping (CLIENT * clnt, xChangeKeyboardMappingReq * q)
{
	PRINT (- X_ChangeKeyboardMapping," ");
}

//------------------------------------------------------------------------------
void
RQ_GetKeyboardMapping (CLIENT * clnt, xGetKeyboardMappingReq * q)
{
	ClntReplyPtr (GetKeyboardMapping, r);
	KeySym * k = KYBD_Map[q->firstKeyCode - KYBD_MapMin];
	int      n = q->count * (sizeof(*KYBD_Map) / sizeof(KeySym)), num = n;
	KeySym * s = (KeySym*)(r +1);
	
	PRINT (GetKeyboardMapping," %i (%i)", q->firstKeyCode, q->count);
	
	r->keySymsPerKeyCode = sizeof(*KYBD_Map) / sizeof(KeySym);
	if (clnt->DoSwap) while (n--) *(s++) = Swap32(*(k++));
	else              while (n--) *(s++) =        *(k++);
	
	ClntReply (GetKeyboardMapping, (num * sizeof(KeySym)), NULL);
}
