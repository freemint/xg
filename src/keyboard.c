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
#include "x_mint.h"

#include <stdio.h>

#include <X11/Xproto.h>
#include <X11/keysym.h>


#define numberof(array)   (sizeof(array) / sizeof(*array))


#define KEYSYM_OFFS   8

#define KC_SHFT_L   0x2A // 42
#define KC_SHFT_R   0x36 // 54
#define KC_CTRL     0x1D // 29
#define KC_ALT      0x38 // 56
#define KC_LOCK     0x3A // 58
#define KC_ALTGR    0x37 // 55 (normally PrintScreen)
#define KC_Modify   0x00 //    (pseudo key)
static KeyCode KYBD_Mod[8][2] = {
	{ KC_SHFT_L + KEYSYM_OFFS, KC_SHFT_R + KEYSYM_OFFS },
	{ KC_LOCK   + KEYSYM_OFFS, 0         },
	{ KC_CTRL   + KEYSYM_OFFS, 0         },
	{ KC_ALT    + KEYSYM_OFFS, 0         },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
static KeySym  KYBD_Map[128][4];
static CARD8   KYBD_Set[256];

const CARD8 KYBD_CodeMin = KEYSYM_OFFS;
CARD8       KYBD_CodeMax = KEYSYM_OFFS + numberof(KYBD_Map) -1;
CARD8       KYBD_PrvMeta = 0;
CARD8       KYBD_Pending = 0;
CARD16      KYBD_Repeat;

#define K_ENGLISH   AESLANG_ENGLISH
#define K_GERMAN    AESLANG_GERMAN
#define K_FRENCH    AESLANG_FRENCH
#define K_SWEDISH   AESLANG_SWEDISH
static char KYBD_Lang  = K_ENGLISH;
static BOOL KYBD_Atari = xFalse;

typedef struct {
	CARD16 scan;
	CARD8  col;
	CARD8  chr;
	KeySym sym;
} SCANTAB;


//------------------------------------------------------------------------------
static KeySym
asc2sym (CARD8 c, BOOL key_pad)
{
	KeySym s;
	switch (c) {
		case '*'  ... '9':
			if (key_pad)   { s = XK_KP_Multiply + c - '*';  break; }
		case ' '  ... ')':
		case ':'  ... '~':  s = XK_space       + c - ' ';  break;
		
		case 0x0D:
			if (key_pad)   { s = XK_KP_Enter;               break; }
		case 0x1B:
		case 0x08 ... 0x0B: s = XK_BackSpace   + c - 0x08; break;
		
		case 0x7F:          s = XK_Delete;                 break;
		
		case '›': s = XK_section;    break;
		case 'û': s = XK_ssharp;     break;
		case 'Ñ': s = XK_adiaeresis; break;   case 'é': s = XK_Adiaeresis; break;
		case 'î': s = XK_odiaeresis; break;   case 'ô': s = XK_Odiaeresis; break;
		case 'Å': s = XK_udiaeresis; break;   case 'ö': s = XK_Udiaeresis; break;
		
		case 'ú': s = XK_FFrancSign; break;
		case 'á': s = XK_cedilla;    break;   case 'Ä': s = XK_Ccedilla; break;
		case 'Ö': s = XK_agrave;     break;   case '∂': s = XK_Agrave;   break;
		case 'Ç': s = XK_eacute;     break;   case 'ê': s = XK_Eacute;   break;
		case 'ä': s = XK_egrave;     break;   case 'ƒ': s = XK_Egrave;   break;
		case 'ó': s = XK_ugrave;     break;   case 'œ': s = XK_Ugrave;   break;
		
		case 'Ü': s = XK_aring;      break;   case 'è': s = XK_Aring;    break;
		
		default:  s = 0;
	}
	return s;
}

//------------------------------------------------------------------------------
static void
set_map (SCANTAB * tab, size_t len)
{
	while (len--) {
		KeySym * k = &KYBD_Map[tab->scan][tab->col];
		if (*k  &&  *k != XK_VoidSymbol) {
			char buf[] = "(_)";
			printf ("    *** can't map code %i:%i to %04lX%s, is %04lX ***\n",
			        tab->col, tab->scan, tab->sym,
			        ((buf[1] = tab->chr) ? buf : ""), *k);
		} else {
			*k = tab->sym;
			if (!(tab->col & 1)) {
				int n = 4 - tab->col;
				while (--n > 0) *(++k) = NoSymbol;
			}
			if (tab->chr) {
				KYBD_Set[tab->chr] = tab->scan;
			}
 		}
		tab++;
	}
}

//------------------------------------------------------------------------------
static int
ins_map (const char * chr)
{
	int key = 1, n = 0;
	while (key <= KYBD_CodeMax) {
		if (KYBD_Set[(unsigned)*chr]) {
			printf ("    *** ''%c'(%04lX) already mapped to keycode %i ***\n",
			        *chr, asc2sym (*chr, xFalse), KYBD_Set[(unsigned)*chr]);
			if (!*(++chr)) return n;
		}
		if (KYBD_Map[key][0] == XK_VoidSymbol) {
			KYBD_Set[(unsigned)*chr] = key;
			KYBD_Map[key][0]         = asc2sym (*chr, xFalse);
			n++;
		//	printf ("    mapped '%c'(%04lX) to keycode %i[0] \n",
		//	        *chr, KYBD_Map[key][0], key);
			if (!chr[1]  ||  *(++chr) == ' ') {
				KYBD_Map[key][1]         = NoSymbol;
			} else {
				KYBD_Set[(unsigned)*chr] = key;
				KYBD_Map[key][1]         = asc2sym (*chr, xFalse);
				n++;
			//	printf ("    mapped '%c'(%04lX) to keycode %i[1] \n",
			//	        *chr, KYBD_Map[key][1], key);
			}
			if (!*(++chr)) return n;
		}
		key++;
	}
	do {
		printf ("    *** no space to map keycode for '%c' ***\n", *chr);
	} while (*(++chr));
	
	return n;
}

//==============================================================================
void
KybdInit (void)
{
	static struct { CARD8 * ushft, * shift; } * keytbl = NULL;
	
	if (!keytbl) {
		int i       = Kbrate (-1, -1);
		KYBD_Repeat = ((unsigned)(i & 0xFF00) >>6) *5;
		keytbl      = Keytbl ((void*)-1, (void*)-1, (void*)-1);
		
		memset (KYBD_Set, 0, sizeof(KYBD_Set));
		for (i = 1; i < 128; i++) {
			CARD8 c = keytbl->ushft[i];
			if (c) KYBD_Set[c] = i;
		}
		if (KYBD_Set[')']) {
			KYBD_Atari = xTrue;
			if      (KYBD_Set['@']) KYBD_Lang = K_FRENCH;
			else if (KYBD_Set['#']) KYBD_Lang = K_GERMAN;
		}
		for (i = 1; i < 128; i++) {
			CARD8 c = keytbl->shift[i];
			if (c) KYBD_Set[c] = i;
		}
		if (!KYBD_Atari) {
			if      (KYBD_Set['Ü']) KYBD_Lang = K_SWEDISH;
			else if (KYBD_Set['á']) KYBD_Lang = K_FRENCH;
			else if (KYBD_Set['û']) KYBD_Lang = K_GERMAN;
		}
		
		printf ("  Keyboard layout %s %s: repeat %ums,\n",
		        (KYBD_Atari ? "Atari" : "PC"),
		        (KYBD_Lang == K_GERMAN ?  "german"  :
		         KYBD_Lang == K_FRENCH ?  "french"  :
		         KYBD_Lang == K_SWEDISH ? "swedish" : "us/english"),
		        KYBD_Repeat);
//	}
//	if () {
{
		SCANTAB vdi_tab[] = {
			{ KC_SHFT_L,0, 0,XK_Shift_L },   { KC_SHFT_R,0, 0,XK_Shift_R },
			{ KC_LOCK,0,   0,XK_Caps_Lock }, { KC_CTRL,0,   0,XK_Control_L },
			{ KC_ALT,0,    0,XK_Alt_L },
			{ 59,0, 0,XK_F1  }, { 84,0, 0,XK_F11 },
			{ 60,0, 0,XK_F2  }, { 85,0, 0,XK_F12 },
			{ 61,0, 0,XK_F3  }, { 86,0, 0,XK_F13 },
			{ 62,0, 0,XK_F4  }, { 87,0, 0,XK_F14 },
			{ 63,0, 0,XK_F5  }, { 88,0, 0,XK_F15 },
			{ 64,0, 0,XK_F6  }, { 89,0, 0,XK_F16 },
			{ 65,0, 0,XK_F7  }, { 90,0, 0,XK_F17 },
			{ 66,0, 0,XK_F8  }, { 91,0, 0,XK_F18 },
			{ 67,0, 0,XK_F9  }, { 92,0, 0,XK_F19 },
			{ 68,0, 0,XK_F10 }, { 93,0, 0,XK_F20 },
			{ 71,0, 0,XK_Home },  { 82,0, 0,XK_Insert },
			{ 72,0, 0,XK_Up },    { 75,0, 0,XK_Left },
			{ 77,0, 0,XK_Right }, { 80,0, 0,XK_Down },
			{ 97,0, 0,XK_Undo },  { 98,0, 0,XK_Help } };
		
		KeySym * k = *KYBD_Map;
		int      n = sizeof(KYBD_Map) / sizeof(KeySym);
		
		KYBD_Map[0][0] = KYBD_Map[0][1] = 
		KYBD_Map[0][2] = KYBD_Map[0][3] = XK_Mode_switch;
		while (--n) *(++k) = XK_VoidSymbol;
		for (i = 1; i < 128; i++) {
			CARD8  c = keytbl->ushft[i];
			KeySym s = asc2sym (c, i >= 73);
			if (c && s) {
				KYBD_Map[i][0] = s;
				if ((c = keytbl->shift[i])) KYBD_Map[i][1] = asc2sym (c, xFalse);
				else                        KYBD_Map[i][1] = NoSymbol;
				KYBD_Map[i][2] = KYBD_Map[i][3] = NoSymbol;
			}
		}
		set_map (vdi_tab, numberof(vdi_tab));
		
		if (KYBD_Atari) {
			SCANTAB page_tab[] = {     { 113,2, 0,XK_End },
			   { 99,2, 0,XK_Page_Up }, { 100,2, 0,XK_Page_Down } };
			set_map (page_tab, numberof(page_tab));
			
			if (KYBD_Lang == K_GERMAN) {
				SCANTAB tab[] = {
					{ KYBD_Set['Å'],2,  '@',XK_at },
					{ KYBD_Set['ö'],3, '\\',XK_backslash },
					{ KYBD_Set['î'],2,  '[',XK_bracketleft },
					{ KYBD_Set['ô'],3,  '{',XK_braceleft },
					{ KYBD_Set['Ñ'],2,  ']',XK_bracketright },
					{ KYBD_Set['é'],3,  '}',XK_braceright } };
				set_map (tab, numberof(tab));
			}
			KYBD_Mod[3][1] = KC_Modify + KEYSYM_OFFS;
			
		} else { // pc keyboard
			SCANTAB page_tab[] = {
				{ KC_ALTGR,0, 0,XK_Super_R }, { 79,0, 0,XK_End },
			   { 73,0, 0,XK_Page_Up },       { 81,0, 0,XK_Page_Down } };
			set_map (page_tab, numberof(page_tab));

			if (!KYBD_Set['@']) {
				SCANTAB tab[] = {
					{ KYBD_Set['q'],2,  '@',XK_at },
					{ KYBD_Set['?'],3, '\\',XK_backslash },
					{ KYBD_Set['<'],2,  '|',XK_bar },
					{ KYBD_Set['+'],3,  '~',XK_asciitilde },
					{ KYBD_Set['8'],2,  '[',XK_bracketleft },
					{ KYBD_Set['7'],3,  '{',XK_braceleft },
					{ KYBD_Set['0'],2,  ']',XK_bracketright },
					{ KYBD_Set['9'],3,  '}',XK_braceright } };
				set_map (tab, numberof(tab));
			}
			KYBD_Mod[4][0] = KC_ALTGR  + KEYSYM_OFFS;
			KYBD_Mod[4][1] = KC_Modify + KEYSYM_OFFS;
		}
		
		if (!KYBD_Set['˝']) {
			SCANTAB tab = { KYBD_Set['2'],2,  '˝',XK_twosuperior };
			set_map (&tab, 1);
		}
		if (!KYBD_Set['˛']) {
			SCANTAB tab = { KYBD_Set['3'],2,  '˛',XK_threesuperior };
			set_map (&tab, 1);
		}
		if (!KYBD_Set['Ê']) {
			SCANTAB tab[] = {{ KYBD_Set['m'],2,  'Ê',XK_mu }};
			set_map (tab, 1);
		}
		i = 0;
		if (!KYBD_Set['›']) i += ins_map("›");
		if (!KYBD_Set['û']) i += ins_map("û");
		if (!KYBD_Set['Ñ']) i += ins_map("ÑéîôÅö");
		if (!KYBD_Set['ú']) i += ins_map("ú");
		if (!KYBD_Set['á']) i += ins_map("áÄÖ∂Çêäƒóœ");
		if (!KYBD_Set['Ü']) i += ins_map("Üè");
		if (i) {
			printf ("    mapped %i extra keycode%s,\n", i, (i == 1 ? "" : "s"));
		}
		while (KYBD_Map[KYBD_CodeMax-8][0] == XK_VoidSymbol) --KYBD_CodeMax;
		printf ("    keycode range: [%i .. %i] \n", KYBD_CodeMin, KYBD_CodeMax);
	//	printf ("    unused:");
	//	for (i = 1; i <= KYBD_CodeMax-8; i++) {
	//		if (KYBD_Map[i][0] == XK_VoidSymbol) printf (" %i ", i +8);
	//	}
	//	printf ("\n");
}		
	}
}


//==============================================================================
short
KybdEvent (CARD16 scan, CARD16 meta)
{
	CARD8   chng = (KYBD_PrvMeta ^ meta);
	KeyCode code = (CARD16)scan >> 8;
	
	if (scan) {
		if     (!code)        code  = KYBD_Set[scan & 0xFF];
		else if (code >= 120) code -= 118;
		
		if (KYBD_Map[code][0] == XK_VoidSymbol) {
			printf ("*** unknown scan code %02X:%02X ***\n", code, scan & 0xFF);
		}
	}
	
	if (_WIND_PointerRoot) {
		CARD32 c_id = _WIND_PointerRoot->Id;
		PXY    r_xy = WindPointerPos (NULL);
		PXY    e_xy = WindPointerPos (_WIND_PointerRoot);
		
		static CARD16 modi[2][6] = {
			{ KC_SHFT_R, KC_SHFT_L, KC_CTRL,     KC_ALT,   KC_LOCK,  KC_ALTGR },
			{ ShiftMask, ShiftMask, ControlMask, Mod1Mask, LockMask, Mod2Mask } };
		CARD16 mask = (meta & (K_LSHIFT|K_RSHIFT) ? ShiftMask : 0);
		int    i    = 0;
		
		if (KYBD_Pending) {
			EvntPropagate (_WIND_PointerRoot, KeyReleaseMask, KeyRelease,
			               c_id, r_xy, e_xy, KYBD_Pending);
			KYBD_Pending = 0;
		}
		while (chng) {
			if (chng & 1) {
				if (meta & (1 << i)) {
					MAIN_KeyButMask |= modi[1][i];
					EvntPropagate (_WIND_PointerRoot, KeyPressMask, KeyPress,
					               c_id, r_xy, e_xy, modi[0][i] + KEYSYM_OFFS);
				} else {
					MAIN_KeyButMask &= ~modi[1][i] | mask;
					EvntPropagate (_WIND_PointerRoot, KeyReleaseMask, KeyRelease,
					               c_id, r_xy, e_xy, modi[0][i] + KEYSYM_OFFS);
				}
			}
			chng >>=1;
			i++;
		}
		if (code) {
			code += KEYSYM_OFFS;
			EvntPropagate (_WIND_PointerRoot, KeyPressMask, KeyPress,
			               c_id, r_xy, e_xy, code);
			if (scan < 0x0100) {
				EvntPropagate (_WIND_PointerRoot, KeyReleaseMask, KeyRelease,
				               c_id, r_xy, e_xy, code);
			} else {
				KYBD_Pending = code;
			}
		}
	
	} else {
		((CARD8*)&MAIN_KeyButMask)[1]
		             = (meta & (K_LSHIFT|K_RSHIFT) ? ShiftMask   : 0)
		             | (meta &  K_LOCK             ? LockMask    : 0)
		             | (meta &  K_CTRL             ? ControlMask : 0)
		             | (meta &  K_ALT              ? Mod1Mask    : 0)
		             | (meta &  K_ALTGR            ? Mod2Mask    : 0);
		KYBD_Pending = 0;
	}
	KYBD_PrvMeta = meta;
	
	return chng;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_GetModifierMapping (CLIENT * clnt, xGetModifierMappingReq * q)
{
	// Returns the keycodes of the modifiers Shift, Lock, Control, Mod1...Mod5
	//
	// Reply:
	// CARD8 numKeyPerModifier
	//...........................................................................
	
	ClntReplyPtr (GetModifierMapping, r);
	
	DEBUG (GetModifierMapping," ");
	
	r->numKeyPerModifier = sizeof(*KYBD_Mod);
	memcpy ((r +1), KYBD_Mod, sizeof(KYBD_Mod));
	
	ClntReply (GetModifierMapping, (8 *2), NULL);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_GetKeyboardMapping (CLIENT * clnt, xGetKeyboardMappingReq * q)
{
	// Returns 'count' keysymbols starting from 'firstKeyCode'.
	//
	// KeyCode firstKeyCode:
	// CARD8   count:
	//
	// Reply:
	// CARD8 keySymsPerKeyCode:
	//...........................................................................
	
	if (q->firstKeyCode < KYBD_CodeMin) {
		Bad(Value, q->firstKeyCode, GetKeyboardMapping,"(): \n"
		    "          firstKeyCode %u < %u \n", q->firstKeyCode, KYBD_CodeMin);
	
	} else if (q->firstKeyCode + q->count -1 > KYBD_CodeMax) {
		Bad(Value, q->count, GetKeyboardMapping,"(): \n"
		    "          firstKeyCode + count -1 %u > %u \n",
		           q->firstKeyCode + q->count -1, KYBD_CodeMax);
	
	} else { //..................................................................
	
		ClntReplyPtr (GetKeyboardMapping, r);
		KeySym * sym = KYBD_Map[q->firstKeyCode - KYBD_CodeMin];
		int      num = q->count * 4;
		size_t   len = num * sizeof(KeySym);
		
		DEBUG (GetKeyboardMapping," %i (%i)", q->firstKeyCode, q->count);
		
		r->keySymsPerKeyCode = 4;
		if (clnt->DoSwap) {
			KeySym * s = (KeySym*)(r +1);
			int      n = num;
			while (n--) *(s++) = Swap32(*(sym++));
		} else {
			memcpy ((r +1), sym, len);
		}
		ClntReply (GetKeyboardMapping, len, NULL);
	}
}

//------------------------------------------------------------------------------
void
RQ_SetModifierMapping (CLIENT * clnt, xSetModifierMappingReq * q)
{
	PRINT (- X_SetModifierMapping," ");
}

//------------------------------------------------------------------------------
void
RQ_ChangeKeyboardMapping (CLIENT * clnt, xChangeKeyboardMappingReq * q)
{
	PRINT (- X_ChangeKeyboardMapping," ");
}

//------------------------------------------------------------------------------
void
RQ_QueryKeymap (CLIENT * clnt, xQueryKeymapReq * q)
{
	PRINT (- X_QueryKeymap," ");
}
