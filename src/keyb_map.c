//==============================================================================
//
// keyb_map.c
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-11-17 - Initial Version.
//==============================================================================
//
	{ XK_Escape,      0 },
	{ XK_1,           XK_exclam },
	{ XK_2,           XK_quotedbl },
	{ XK_3,           XK_section },
	{ XK_4,           XK_dollar },
	{ XK_5,           XK_percent },
	{ XK_6,           XK_ampersand },
	{ XK_7,           XK_slash },
	{ XK_8,           XK_parenleft },
	{ XK_9,           XK_parenright },
	{ XK_0,           XK_equal },
	{ XK_ssharp,      XK_question },
	{ XK_apostrophe,  XK_grave },
	{ XK_BackSpace,   0 },
	{ XK_Tab,         0 },
	{ XK_q,           XK_Q },
	{ XK_w,           XK_W },
	{ XK_e,           XK_E },
	{ XK_r,           XK_R },
	{ XK_t,           XK_T },
	{ XK_z,           XK_Z },
	{ XK_u,           XK_U },
	{ XK_i,           XK_I },
	{ XK_o,           XK_O },
	{ XK_p,           XK_P },
	{ XK_udiaeresis,  XK_Udiaeresis }, // 0x1A SCN_U_DIAER
	{ XK_plus,        XK_asterisk },
	{ XK_Return,      0 },
	{ XK_Control_L,   0 },             // 0x1D KEY_CTRL
	{ XK_a,           XK_A },
	{ XK_s,           XK_S },
	{ XK_d,           XK_D },
	{ XK_f,           XK_F },
	{ XK_g,           XK_G },
	{ XK_h,           XK_H },
	{ XK_j,           XK_J },
	{ XK_k,           XK_K },
	{ XK_l,           XK_L },
	{ XK_odiaeresis,  XK_Odiaeresis }, // 0x27 SCN_O_DIAER
	{ XK_adiaeresis,  XK_Adiaeresis }, // 0x28 SCN_A_DIAER
	{ XK_numbersign,  XK_asciicircum},
	{ XK_Shift_L,     0 },             // 0x2A KEY_SHIFT_L
	{ XK_asciitilde,  XK_bar },
	{ XK_y,           XK_Y },
	{ XK_x,           XK_X },
	{ XK_c,           XK_C },
	{ XK_v,           XK_V },
	{ XK_b,           XK_B },
	{ XK_n,           XK_N },
	{ XK_m,           XK_M },
	{ XK_comma,       XK_semicolon },
	{ XK_period,      XK_colon },
	{ XK_minus,       XK_underscore },
	{ XK_Shift_R,     0 },             // 0x36 KEY_SHIFT_R
	{ XK_at,          XK_backslash },  // 0x37 KEY_AT_BKSL (remapped)
	{ XK_Alt_L,       0 },             // 0x38 KEY_ALT
	{ XK_space,       0 },
	{ XK_Caps_Lock,   0 },             // 0x3A KEY_LOCK
	{ XK_F1,          0 },
	{ XK_F2,          0 },
	{ XK_F3,          0 },
	{ XK_F4,          0 },
	{ XK_F5,          0 },
	{ XK_F6,          0 },
	{ XK_F7,          0 },
	{ XK_F8,          0 },
	{ XK_F9,          0 },
	{ XK_F10,         0 },
	{ XK_bracketleft, XK_braceleft  }, // 0x45 KEY_BRAC_L (remapped)
	{ XK_bracketright,XK_braceright }, // 0x46 KEY_BRAC_R (remapped)
	{ XK_Home,        0 },
	{ XK_Up,          0 },
	{ XK_Page_Up,     0 },             // 0x49 KEY_PAGE_UP (remapped)
	{ XK_KP_Subtract, 0 },
	{ XK_Left,        0 },
	{ XK_Alt_R,       0 },             // 0x4C KEY_ALTGR (milan)
	{ XK_Right,       0 },
	{ XK_KP_Add,      0 },
	{ XK_End,         0 },             // 0x4F (milan)
	{ XK_Down,        0 },
	{ XK_Page_Down,   0 },             // 0x51 KEY_PAGE_DN (remapped)
	{ XK_Insert,      0 },
	{ XK_Delete,      0 },
	{ XK_F11,         0 },
	{ XK_F12,         0 },
	{ XK_F13,         0 },
	{ XK_F14,         0 },
	{ XK_F15,         0 },
	{ XK_F16,         0 },
	{ XK_F17,         0 },
	{ XK_F18,         0 },
	{ XK_F19,         0 },
	{ XK_F20,         0 },
	{ XK_at,          XK_backslash },  // 0x5E KEY_AT_BKSL (remapped)
	{ XK_VoidSymbol }, // 0x5F
	{ XK_less,        XK_greater },
	{ XK_Undo,        0 },
	{ XK_Help,        0 },
	{ XK_parenleft,   0 },        // _KP_ 0x63 SCN_PAREN_L
	{ XK_parenright,  0 },        // _KP_ 0x64 SCN_PAREN_R
	{ XK_KP_Divide,   0 },
	{ XK_KP_Multiply, 0 },
	{ XK_KP_7,        0 },
	{ XK_KP_8,        0 },
	{ XK_KP_9,        0 },
	{ XK_KP_4,        0 },
	{ XK_KP_5,        0 },
	{ XK_KP_6,        0 },
	{ XK_KP_1,        0 },
	{ XK_KP_2,        0 },
	{ XK_KP_3,        0 },
	{ XK_KP_0,        0 },
	{ XK_KP_Decimal,  0 },
	{ XK_KP_Enter,    0 },
	
// milan mapped codes start at 0x73

	{ XK_backslash,  0 }, // MIL_BKSL
	{ XK_bar,        0 }, // MIL_PIPE
	{ XK_braceleft,  0 }, // MIL_BRCE_L
	{ XK_braceright, 0 }, // MIL_BRCE_R
	{ XK_Greek_mu,   0 }  // MIL_MU
	