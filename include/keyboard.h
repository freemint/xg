//==============================================================================
//
// keyboard.c
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-11-15 - Initial Version.
//==============================================================================
//
#ifndef __KEYBOARD_H__
# define __KEYBOARD_H__


#define K_LOCK   0x10   // bitmask returned by Kbshift()


#define KEY_SHIFT_L   0x2A
#define KEY_SHIFT_R   0x36
#define KEY_CTRL      0x1D
#define KEY_ALT       0x38
#define KEY_LOCK      0x3A

#define SCN_A_DIAER   0x28
#define SCN_O_DIAER   0x27
#define SCN_U_DIAER   0x1A
#define SCN_PAREN_L   0x63
#define SCN_PAREN_R   0x64

#define KEY_BRAC_L    0x45
#define KEY_BRAC_R    0x46
#define KEY_AT_BKSL   0x37
#define KEY_PAGE_UP   0x49
#define KEY_PAGE_DN   0x51


void KybdEvent (short scan, short prev);

extern const CARD8 KYBD_MapMin, KYBD_MapMax;


#endif __KEYBOARD_H__
