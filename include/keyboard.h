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


#define K_LOCK    0x10   // bitmask returned by Kbshift()
#define K_ALTGR   0x80


#define KEY_SHIFT_L   0x2A
#define KEY_SHIFT_R   0x36
#define KEY_CTRL      0x1D
#define KEY_ALT       0x38
#define KEY_LOCK      0x3A
#define KEY_ALTGR     0x4C

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

// milan

#define SCN_BRCE_L    0x08
#define SCN_BRCE_R    0x0B
#define SCN_BRCK_L    0x09
#define SCN_BRCK_R    0x0A
#define SCN_AT        0x10
#define SCN_BKSL      0x0C
#define SCN_TLDE      0x1B
#define SCN_PIPE      0x60
#define SCN_MU        0x32

#define KEY_TLDE      0x2B
#define MIL_BKSL      0x73
#define MIL_PIPE      0x74
#define MIL_BRAC_L    0x75
#define MIL_BRAC_R    0x76
#define MIL_MU        0x77


void KybdEvent (short scan, short prev);

extern const CARD8 KYBD_MapMin, KYBD_MapMax;


#endif __KEYBOARD_H__
