//==============================================================================
//
// wmgr.c -- Build-in window manager.
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-09-01 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "clnt.h"
#include "tools.h"
#include "wmgr.h"
#include "window.h"
#include "pixmap.h"
#include "event.h"
#include "Cursor.h"
#include "grph.h"
#include "colormap.h"
#include "Atom.h"
#include "Property.h"
#include "Request.h"
#include "ICCC.h"
#include "x_gem.h"
#include "x_mint.h"
#include "Xapp.h"

#include <stdio.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/cursorfont.h>


extern const short _app;

BOOL    WMGR_Active = xFalse;;
CARD16  WMGR_Cursor = 0;
#define WMGR_DECOR    5
short   WMGR_Decor  = WMGR_DECOR;
#define A_WIDGETS     (NAME|MOVER|CLOSER|SMALLER)
#define P_WIDGETS     0

CARD16       WMGR_OpenCounter  = 0;
short        WMGR_Focus        = 0;
short        _WMGR_FocusHolder = 0;
static short _WMGR_HasFocus    = 0;
static short _WMGR_WidgetColor[][3] = { {W_NAME,}, {0} };
static short _WMGR_WidgetFgnd, _WMGR_WidgetBgnd;

#include "intro.c"
static MFDB  _WMGR_Logo = { x_bits, x_width,x_height, (x_width +15) /16, 0, 1 };
static MFDB  _WMGR_Qbuf = { NULL,   x_width,x_height, (x_width +15) /16, 0, 0 };
static short _WMGR_P[8] = { 0,0, x_width -1, x_height -1 };

static MFDB _WMGR_Icon = { NULL, 0,0, 0, 0, 1 };

static OBJECT * _WMGR_Menu        = NULL;
static char     _WMGR_MenuEmpty[] = "   ";

static struct { CURSOR L, LD, D, RD, R, X; } _WMGR_Cursor = { 
	{ NULL,0, XC_left_side,           1uL, {0,0},0, G_WHITE, G_RED },
	{ NULL,0, XC_bottom_left_corner,  1uL, {0,0},0, G_WHITE, G_RED },
	{ NULL,0, XC_bottom_side,         1uL, {0,0},0, G_WHITE, G_RED },
	{ NULL,0, XC_bottom_right_corner, 1uL, {0,0},0, G_WHITE, G_RED },
	{ NULL,0, XC_right_side,          1uL, {0,0},0, G_WHITE, G_RED },
	{ NULL,0, XC_X_cursor,            1uL, {0,0},0, G_WHITE, G_RED }
};

static void FT_Wmgr_reply (p_CLIENT , CARD32 size, const char * form);
static void FT_Wmgr_error (p_CLIENT , CARD8 code, CARD8 , CARD16 , CARD32 val);
static void FT_Wmgr_event (p_CLIENT , p_WINDOW , CARD16 evnt, va_list);
static xReply _WMGR_Obuf;
static FUNCTABL _WMGR_Table = {
	FT_Wmgr_reply, FT_Wmgr_error, FT_Wmgr_event,
	FT_Grph_ShiftArc_MSB, FT_Grph_ShiftPnt_MSB, FT_Grph_ShiftR2P_MSB
};
static CLIENT _WMGR_Client = {
	NULL, xFalse, 0x00000ul,            //NULL,
	0u, (char*)"localhost", (char*)"127.0.0.1", 0, -1,
	{ 0ul, 0ul, (char*)&_WMGR_Obuf }, { 0ul, 0ul, NULL },
	NULL, &_WMGR_Table, xFalse, RetainPermanent, 0ul,
	{{0}}, {{0}}, {{0}}, "  wm"
};


//==============================================================================
void
WmgrIntro (BOOL onNoff)
{
	static BOOL on_screen = xFalse;
	
	if (onNoff) {
		
		if (GrphInit() && !on_screen) {
			short pxy[8] = { (_WMGR_P[4] = (GRPH_ScreenW - x_width)  /2),
			                 (_WMGR_P[5] = (GRPH_ScreenH - x_height) /2),
			                 (_WMGR_P[6] = _WMGR_P[4] + x_width  -1),
			                 (_WMGR_P[7] = _WMGR_P[5] + x_height -1),
			                 _WMGR_P[0], _WMGR_P[1], _WMGR_P[2], _WMGR_P[3] };
			MFDB  scrn   = { NULL };
			short clr[2] = { G_BLACK, G_YELLOW };
			short wg[4];
			
			graf_mouse (BUSYBEE, NULL);
			WindUpdate (xTrue);
			
			wind_get (0, WF_SCREEN, wg, wg +1, wg +2, wg +3);
			_WMGR_Qbuf.fd_addr    = (void*)((wg[0] <<16) | (wg[1] & 0xFFFF));
			_WMGR_Qbuf.fd_nplanes = GRPH_Depth;
			
			vst_alignment (GRPH_Vdi, TA_LEFT, TA_TOP, wg,wg);
			vst_effects   (GRPH_Vdi, TXT_UNDERLINED);
			vswr_mode     (GRPH_Vdi, MD_XOR);
			
			v_hide_c  (GRPH_Vdi);
			vro_cpyfm (GRPH_Vdi, S_ONLY,   pxy,     &scrn,       &_WMGR_Qbuf);
			vrt_cpyfm (GRPH_Vdi, MD_TRANS, _WMGR_P, &_WMGR_Logo, &scrn, clr);
			v_gtext   (GRPH_Vdi, pxy[0] +27, pxy[1], "V11 R6.4");
			v_show_c  (GRPH_Vdi, 1);
			
			vst_alignment (GRPH_Vdi, TA_LEFT, TA_BASE, wg,wg);
			vst_effects   (GRPH_Vdi, TXT_NORMAL);
			
			on_screen = xTrue;
		}
		
	} else {
		if (on_screen) {
			MFDB scrn = { NULL };
			
			v_hide_c  (GRPH_Vdi);
			vro_cpyfm (GRPH_Vdi, S_ONLY, _WMGR_P, &_WMGR_Qbuf, &scrn);
			v_show_c  (GRPH_Vdi, 1);
			
			WindUpdate (xFalse);
			
			on_screen = xFalse;
		}
		graf_mouse (ARROW, NULL);
	}
}

//------------------------------------------------------------------------------
/*
static void
_Wmgr_request (void * buf, size_t len)
{
	xReq * q  = buf;
	q->length = Units(len);
	_WMGR_Client.iBuf.Mem  = buf;
	_WMGR_Client.iBuf.Done = len;
	_WMGR_Client.SeqNum++;
}
*/

//==============================================================================
BOOL
WmgrInit (BOOL initNreset)
{
	BOOL ok = xTrue;
	
	if (initNreset) {
		XrscPoolInit (_WMGR_Client.Drawables);
		XrscPoolInit (_WMGR_Client.Fontables);
		XrscPoolInit (_WMGR_Client.Cursors);
		
		if (!rsrc_load ((char*)"Xapp.rsc")) {
			WmgrIntro (xFalse);
			form_alert (1, (char*)"[3][Can't load RSC file!][ Quit ]");
			ok = xFalse;
			
		} else {
			OBJECT * tree;
			short i;
			
			rsrc_gaddr (R_TREE, ABOUT, &tree);
			_WMGR_Icon.fd_addr    = tree[ABOUT_LOGO].ob_spec.bitblk->bi_pdata;
			_WMGR_Icon.fd_w       = tree[ABOUT_LOGO].ob_height;
			_WMGR_Icon.fd_h       = tree[ABOUT_LOGO].ob_spec.bitblk->bi_hl;
			_WMGR_Icon.fd_wdwidth =(tree[ABOUT_LOGO].ob_spec.bitblk->bi_wb +1) /2;
			
			rsrc_gaddr (R_TREE, MENU, &_WMGR_Menu);
			if (_app) {
				for (i = MENU_CLNT_FRST; i <= MENU_CLNT_LAST; ++i) {
					_WMGR_Menu[i].ob_spec.free_string = _WMGR_MenuEmpty;
					_WMGR_Menu[i].ob_flags |= OF_HIDETREE;
				}
				_WMGR_Menu[MENU_CLNT].ob_state |= OS_DISABLED;
				menu_bar     (_WMGR_Menu, 1);
				menu_ienable (_WMGR_Menu, MENU_GWM, 1);
			}
			i = W_NAME;
			wind_get (0, WF_DCOLOR, &i, &_WMGR_WidgetFgnd, &_WMGR_WidgetBgnd, &i);
			i = 0;
			do {
				short n = _WMGR_WidgetColor[i][0];
				wind_get (0, WF_DCOLOR, &n,
				          &_WMGR_WidgetColor[i][1], &_WMGR_WidgetColor[i][2], &n);
			} while (_WMGR_WidgetColor[++i][0]);
			_WMGR_FocusHolder = wind_create (0, -100, -100, 99, 99);
		}
	
	} else {
		// something to reset ...
		WmgrIntro (xFalse);
	}
	
	return ok;
}

//==============================================================================
void
WmgrExit (void)
{
	WmgrSetDesktop (xFalse);
	ClntInit (xFalse);
	
	if (_WMGR_Menu) {
		menu_bar (_WMGR_Menu, 0);
		rsrc_free();
		_WMGR_Menu = NULL;
	}
	WmgrIntro (xFalse);
	GrphExit();
}


//==============================================================================
void
WmgrActivate (BOOL onNoff)
{
	WINDOW * w = WIND_Root.StackBot;
	
	static BOOL __init = xTrue;
	
	if (__init) {
		// init cursors
		CrsrSetGlyph (&_WMGR_Cursor.L,  _WMGR_Cursor.L.Id);
		CrsrSetGlyph (&_WMGR_Cursor.LD, _WMGR_Cursor.LD.Id);
		CrsrSetGlyph (&_WMGR_Cursor.D,  _WMGR_Cursor.D.Id);
		CrsrSetGlyph (&_WMGR_Cursor.RD, _WMGR_Cursor.RD.Id);
		CrsrSetGlyph (&_WMGR_Cursor.R,  _WMGR_Cursor.R.Id);
		CrsrSetGlyph (&_WMGR_Cursor.X,  _WMGR_Cursor.X.Id);
		
		while (Kbshift(-1) & (K_LSHIFT|K_RSHIFT)) ;
		WmgrIntro (xFalse);
		
		__init = xFalse;
	
	} else if (onNoff == WMGR_Active) {
		puts("*BOINK*");
		return;
	}
	
	if (onNoff) {
		WMGR_Active = xTrue;
		while (w) {
			short hdl = w->Handle;
			if (!w->Override && WmgrWindHandle (w)) {
				char * title = PropValue (w, XA_WM_NAME, XA_STRING, 1);
				if (title) {
					wind_set_str (w->Handle, WF_NAME, title);
				}
				WindSetHandles (w);
				if (w->isMapped) {
					GRECT dummy;
					WindUnmap   (w, xFalse);
					WmgrWindMap (w, &dummy);
					WindMap     (w, xTrue);
				} else {
				#	define clnt &_WMGR_Client
					PRINT (ReparentWindow, "(W:%X) active", w->Id);
				#	undef clnt
				}
				wind_delete (hdl);
			}
			w = w->NextSibl;
		}
		if (_app) menu_icheck (_WMGR_Menu, MENU_GWM, 1);
	
	} else {
		WmgrCursorOff (NULL);
		WMGR_Active = xFalse;
		while (w) {
			short hdl = w->Handle;
			if (w->GwmDecor && WmgrWindHandle (w)) {
				WindSetHandles (w);
				if (w->isMapped || w->GwmIcon) {
					GRECT curr;
					char * title = PropValue (w, XA_WM_NAME, XA_STRING, 1);
					if (title) {
						wind_set_str (w->Handle, WF_NAME, title);
					}
					if (w->isMapped) {
						WindUnmap (w, xFalse);
						wind_get_curr (hdl, &curr);
					} else {
						w->GwmIcon = xFalse;
						wind_get_r (hdl, WF_UNICONIFY, &curr);
					}
				#	define clnt &_WMGR_Client
					PRINT (ReparentWindow, "(W:%X) revert", w->Id);
				#	undef clnt
					if       (w->WinGravity == NorthWestGravity ||
					          w->WinGravity == NorthGravity     ||
					          w->WinGravity == NorthEastGravity) {
						 w->Rect.y = curr.y - WIND_Root.Rect.y + w->BorderWidth;
					} else if (w->WinGravity == SouthWestGravity ||
					           w->WinGravity == SouthGravity     ||
					           w->WinGravity == SouthEastGravity) {
						 w->Rect.y = curr.y - WIND_Root.Rect.y - w->BorderWidth *2
						           + curr.h - w->Rect.h;
					}
					if       (w->WinGravity == NorthWestGravity ||
					          w->WinGravity == WestGravity      ||
					          w->WinGravity == SouthWestGravity) {
						w->Rect.x = curr.x - WIND_Root.Rect.x + w->BorderWidth;
					} else if (w->WinGravity == NorthEastGravity ||
					           w->WinGravity == EastGravity      ||
					           w->WinGravity == SouthEastGravity) {
						w->Rect.x = curr.x - WIND_Root.Rect.x - w->BorderWidth *2
						          + curr.w - w->Rect.w;
					}
					WmgrWindMap (w, &curr);
					if (w->u.List.AllMasks & StructureNotifyMask) {
						EvntReparentNotify (w, StructureNotifyMask,
						                    w->Id, ROOT_WINDOW,
						                    *(PXY*)&w->Rect, w->Override);
					}
					if (WIND_Root.u.List.AllMasks & SubstructureNotifyMask) {
						EvntReparentNotify (&WIND_Root, SubstructureNotifyMask,
						                    w->Id, ROOT_WINDOW,
						                    *(PXY*)&w->Rect, w->Override);
					}
					WindMap (w, xTrue);
				} else {
				#	define clnt &_WMGR_Client
					PRINT (ReparentWindow, "(W:%X) passive", w->Id);
				#	undef clnt
				}
				wind_delete (hdl);
			}
			w = w->NextSibl;
		}
		if (_app) menu_icheck (_WMGR_Menu, MENU_GWM, 0);
		
	}
}


//==============================================================================
void
WmgrClntInsert (CLIENT * client)
{
	char * ent = client->Entry;
	
	sprintf (ent, "  ---: [%s:%i]", client->Addr, client->Port);
	
	if (_app) {
		int    n  = MENU_CLNT_FRST;
		while (n <= MENU_CLNT_LAST) {
			char * tmp = _WMGR_Menu[n].ob_spec.free_string;
			_WMGR_Menu[n].ob_spec.free_string = ent;
			if (_WMGR_Menu[n].ob_flags & OF_HIDETREE) {
				_WMGR_Menu[n].ob_flags         &= ~OF_HIDETREE;
				_WMGR_Menu[MENU_CLNT].ob_state &= ~OS_DISABLED;
				_WMGR_Menu[MENU_CLNT_FACE].ob_height
				                     = _WMGR_Menu[n].ob_y + _WMGR_Menu[n].ob_height;
				break;
			}
			ent = tmp;
			n++;
		}
	}
}

//==============================================================================
void
WmgrClntUpdate (CLIENT * client, const char * text)
{
	if (text) {
		strncpy (client->Entry +7, text, CLNTENTLEN -8);
		client->Entry[CLNTENTLEN-1] = ' ';
		client->Entry[CLNTENTLEN]   = '\0';
		
	} else {
		char buf[8];
		int  len = sprintf (buf, "%03X", client->Id >> (RID_MASK_BITS & 0x1C));
		strncpy (client->Entry +2, buf, len);
	}
}

//==============================================================================
void
WmgrClntRemove (CLIENT * client)
{
	if (_app) {
		int n = MENU_CLNT_FRST;
		
		while (n < MENU_CLNT_LAST
		       && _WMGR_Menu[n].ob_spec.free_string != client->Entry) n++;
		while (n < MENU_CLNT_LAST && !(_WMGR_Menu[n+1].ob_flags & OF_HIDETREE)) {
			_WMGR_Menu[n].ob_spec.free_string = _WMGR_Menu[n+1].ob_spec.free_string;
			n++;
		}
		_WMGR_Menu[n].ob_spec.free_string = _WMGR_MenuEmpty;
		_WMGR_Menu[n].ob_flags            |= OF_HIDETREE;
		if (n == MENU_CLNT_FRST) {
			_WMGR_Menu[MENU_CLNT].ob_state |= OS_DISABLED;
		} else {
			_WMGR_Menu[MENU_CLNT_FACE].ob_height = _WMGR_Menu[n].ob_y;
		}
	}
}


//==============================================================================
#define A_WIDGETS (NAME|MOVER|CLOSER|SMALLER)
#define P_WIDGETS 0

void
WmgrCalcBorder (GRECT * curr, WINDOW * wind)
{
	GRECT work = wind->Rect;
	work.x += WIND_Root.Rect.x;
	work.y += WIND_Root.Rect.y;
	
	if (wind->GwmDecor) {
		work.x -= WMGR_DECOR;
		work.w += WMGR_DECOR *2;
		work.h += WMGR_DECOR;
		wind_calc_r (WC_BORDER, A_WIDGETS, &work, curr);
	
	} else {
		CARD16 b = wind->BorderWidth;
		if (b) {
			if (wind->hasBorder &&  wind->BorderPixel == G_BLACK) {
				b--;
			}
			work.x -= b;
			work.y -= b;
			work.w += b *2;
			work.h += b *2;
		}
		wind_calc_r (WC_BORDER, P_WIDGETS, &work, curr);
	}
}


//------------------------------------------------------------------------------
static void
_Wmgr_SetWidgets (short hdl, int state)
{
	// state ==  0: normal
	//       >= +1: topped
	//       <= -1: bottom
	
	int a = (state >= 0 ? 1 : 2);
	int b = (state <= 0 ? 2 : 1);
	int n = 0;
	do {
		wind_set (hdl, WF_COLOR, _WMGR_WidgetColor[n][0],
		          _WMGR_WidgetColor[n][a], _WMGR_WidgetColor[n][b], -1);
	} while (_WMGR_WidgetColor[++n][0]);
}

//==============================================================================
BOOL
WmgrWindHandle (WINDOW * wind)
{
	BOOL decor = WMGR_Active && !wind->Override;
	short hdl = wind_create_r ((decor ? A_WIDGETS : P_WIDGETS), &WIND_Root.Rect);
	if (hdl > 0) {
		OBJECT *icons;
		wind_set (hdl, WF_BEVENT, 0x0001, 0,0,0);
		wind->Handle    = hdl;
		wind->GwmDecor  = decor;
		wind->GwmParent = xFalse;
		if(rsrc_gaddr (R_TREE, ICONS, &icons)) {
			wind_set_proc(hdl, icons[ICONS_NAME_X].ob_spec.ciconblk);
		}
		_Wmgr_SetWidgets (hdl, -1);
		
		return xTrue;
	}
	return xFalse;
}

//==============================================================================
BOOL
WmgrWindMap (WINDOW * wind, GRECT * curr)
{
	WmgrCalcBorder (curr, wind);
	
	if (wind->GwmIcon) {
			char * title = PropValue (wind, XA_WM_NAME, XA_STRING, 1);
			if (title) {
				wind_set_str (wind->Handle, WF_NAME, title);
			}
			wind_set_r (wind->Handle, WF_UNICONIFY, curr);
			wind->GwmIcon = xFalse;
	
	} else if (wind->GwmDecor) {
		if (!wind->GwmParent) {
			CARD16 b  = wind->BorderWidth;
			int    dx = wind->Rect.x - curr->x, dy = wind->Rect.y - curr->y, d;
			PXY    pos;
		#	define clnt &_WMGR_Client
			PRINT (ReparentWindow, "(W:%X) mapped", wind->Id);
		#	undef clnt
			if       (wind->WinGravity == NorthWestGravity ||
			          wind->WinGravity == WestGravity      ||
			          wind->WinGravity == SouthWestGravity) {
				curr->x = wind->Rect.x + WIND_Root.Rect.x - b;
			} else if (wind->WinGravity == NorthEastGravity ||
			           wind->WinGravity == EastGravity      ||
			           wind->WinGravity == SouthEastGravity) {
				curr->x = wind->Rect.x + WIND_Root.Rect.x + wind->Rect.w + b *2
				        - curr->w;
			}
			d = (curr->x + curr->w) - (WIND_Root.Rect.x + WIND_Root.Rect.w);
			if (d > 0)                      curr->x -= d;
			if (curr->x < WIND_Root.Rect.x) curr->x  = WIND_Root.Rect.x;
			wind->Rect.x = curr->x + dx;
			
			if       (wind->WinGravity == NorthWestGravity ||
			          wind->WinGravity == NorthGravity     ||
			          wind->WinGravity == NorthEastGravity) {
				curr->y = wind->Rect.y + WIND_Root.Rect.y - b;
			} else if (wind->WinGravity == SouthWestGravity ||
			           wind->WinGravity == SouthGravity     ||
			           wind->WinGravity == SouthEastGravity) {
				curr->y = wind->Rect.y + WIND_Root.Rect.y + wind->Rect.h + b *2
				        - curr->h;
			}
			d = (curr->y + curr->h) - (WIND_Root.Rect.y + WIND_Root.Rect.h);
			if (d > 0)                      curr->y -= d;
			if (curr->y < WIND_Root.Rect.y) curr->y  = WIND_Root.Rect.y;
			wind->Rect.y = curr->y + dy;
			
			pos.x = wind->Rect.x - b;
			pos.y = wind->Rect.y - b;
			if (wind->u.List.AllMasks & StructureNotifyMask) {
				EvntReparentNotify (wind, StructureNotifyMask,
				                    wind->Id, ROOT_WINDOW, pos, wind->Override);
			}
			if (WIND_Root.u.List.AllMasks & SubstructureNotifyMask) {
				EvntReparentNotify (&WIND_Root, SubstructureNotifyMask,
				                    wind->Id, ROOT_WINDOW, pos, wind->Override);
			}
			
			wind->GwmParent = xTrue;
		}
	
	} else {
		if (curr->y < 0) {
			CARD32 above = (wind->PrevSibl ? wind->PrevSibl->Id : None);
			GRECT  work;
			wind->Rect.y -= curr->y;
			curr->y       = 0;
			WmgrCalcBorder (curr, wind);
			work.x = wind->Rect.x - wind->BorderWidth;
			work.y = wind->Rect.y - wind->BorderWidth;
			work.w = wind->Rect.w;
			work.h = wind->Rect.h;
			EvntConfigureNotify (wind, wind->Id, above,
			                     &work, wind->BorderWidth, wind->Override);
		}
		if (wind->SaveUnder && wind->Override) {
			GRECT rect = *curr;
			rect.w += 2;
			rect.h += 2;
			WindSaveUnder (wind->Id, &rect, 0);
		}
		wind->GwmParent = xFalse;
	}
	
	if (!WMGR_OpenCounter++) {
		wind_open   (_WMGR_FocusHolder, -100, -100, 10, 10);
		_WMGR_HasFocus = 1;
	} else {
		_WMGR_HasFocus = 2;
	}
	wind_open_r (wind->Handle, curr);
	
	return (WMGR_OpenCounter > 1);
}

//==============================================================================
BOOL
WmgrWindUnmap (WINDOW * wind, BOOL no_check)
{
	if (no_check || wind->isMapped || wind->GwmIcon) {
		wind_close (wind->Handle);
		if (!--WMGR_OpenCounter) {
			wind_close (_WMGR_FocusHolder);
			
			return xFalse;
		}
	}
	return xTrue;
}

//==============================================================================
void
WmgrWindName (WINDOW * wind, const char * name, BOOL windNicon)
{
	if (/*wind->isIcon !=*/ windNicon) {
		wind_set_str (wind->Handle, WF_NAME, (char*)name);
	}
}


//==============================================================================
void
WmgrCursor (WINDOW * wind, p_PXY pos)
{
	CARD16 type = 0x000;
	
	if (pos->y >= 0) {
		if      (pos->x <  0)            type =  0x100;
		else if (pos->x >= wind->Rect.w) type =  0x001;
		if      (pos->y >= wind->Rect.h) type |= 0x010;
	}
	if (type != WMGR_Cursor) {
		switch (type) {
			case 0x100: CrsrSelect (&_WMGR_Cursor.L);  break;
			case 0x110: CrsrSelect (&_WMGR_Cursor.LD); break;
			case 0x010: CrsrSelect (&_WMGR_Cursor.D);  break;
			case 0x011: CrsrSelect (&_WMGR_Cursor.RD); break;
			case 0x001: CrsrSelect (&_WMGR_Cursor.R);  break;
			default:    CrsrSelect (NULL);
		}
		WMGR_Cursor = type;
	}
}

//==============================================================================
void
WmgrCursorOff (CURSOR * new_crsr)
{
	CrsrSelect (new_crsr);
	WMGR_Cursor = 0;
}


//------------------------------------------------------------------------------
static short
desktop_fill (PARMBLK *pblk)
{
	pblk->pb_w  += pblk->pb_x  -1;
	pblk->pb_h  += pblk->pb_y  -1;
	pblk->pb_wc += pblk->pb_xc -1;
	pblk->pb_hc += pblk->pb_yc -1;
	
	if (GrphIntersectP ((PRECT*)&pblk->pb_xc, (PRECT*)&pblk->pb_x)) {
		vswr_mode (GRPH_Vdi, MD_REPLACE);
		vsf_color (GRPH_Vdi, pblk->pb_parm);
		v_bar     (GRPH_Vdi, (short*)&pblk->pb_xc);
	}
	return pblk->pb_currstate;
}

//------------------------------------------------------------------------------
static short
desktop_pmap (PARMBLK *pblk)
{
	pblk->pb_w  += pblk->pb_x  -1;
	pblk->pb_h  += pblk->pb_y  -1;
	pblk->pb_wc += pblk->pb_xc -1;
	pblk->pb_hc += pblk->pb_yc -1;
	
	if (GrphIntersectP ((PRECT*)&pblk->pb_xc, (PRECT*)&pblk->pb_x)) {
		WindDrawPmap (WIND_Root.Back.Pixmap,
		              *(PXY*)&pblk->pb_x, (PRECT*)&pblk->pb_xc);
	}
	return pblk->pb_currstate;
}

//==============================================================================
OBJECT  WMGR_Desktop = { -1,-1,-1, 000, OF_LASTOB, OS_NORMAL, {0l}, 0,0, };

void
WmgrSetDesktop (BOOL onNoff)
{
	static USERBLK ublk;
	
	if (onNoff) {
		WIND_UPDATE_BEG;
		WMGR_Desktop.ob_width  = GRPH_ScreenW;
		WMGR_Desktop.ob_height = GRPH_ScreenH;
		if (WIND_Root.hasBackPix) {
			ublk.ub_code = desktop_pmap;
			WMGR_Desktop.ob_type         = G_USERDEF;
			WMGR_Desktop.ob_spec.userblk = &ublk;
		} else if (WIND_Root.Back.Pixel >= 16) {
			ublk.ub_code = desktop_fill;
			ublk.ub_parm = WIND_Root.Back.Pixel;
			WMGR_Desktop.ob_type         = G_USERDEF;
			WMGR_Desktop.ob_spec.userblk = &ublk;
		} else {
			WMGR_Desktop.ob_type       = G_BOX;
			WMGR_Desktop.ob_spec.index = 0x000011F0L | WIND_Root.Back.Pixel;
		}
		wind_set (0, WF_NEWDESK, (int)&WMGR_Desktop, 0, 0,0);
		WIND_UPDATE_END;
	
	} else if (WMGR_Desktop.ob_type) {
		wind_set (0, WF_NEWDESK, 0,0, 0, 0);
		WMGR_Desktop.ob_type = 000;
	}
}


//==============================================================================
BOOL
WmgrMenu (short title, short entry, short meta)
{
	BOOL run = xTrue;
	
	switch (entry) {
		
		case MENU_GWM:
			WmgrActivate (!WMGR_Active);
			WindPointerWatch (xFalse);
			break;
		
		case MENU_QUIT:
			run = (!(_WMGR_Menu[MENU_CLNT].ob_state & OS_DISABLED) &&
			       form_alert (1, (char*)"[2]"
			                   "[Really quit the server|and all runng clients?]"
			                   "[ quit |continue]")
			       != 1);
			break;
		
		case MENU_CLNT_FRST ... MENU_CLNT_LAST: {
			char * str = _WMGR_Menu[entry].ob_spec.free_string;
			if (str != _WMGR_MenuEmpty) {
				CLIENT * clnt = (CLIENT*)(str - offsetof (CLIENT, Entry));
				if (meta & K_CTRL) {
					menu_tnormal (_WMGR_Menu, title, 1);
					title = 0;
					ClntDelete (clnt);
				}
			}
		}	break;
	}
	if (title) {
		menu_tnormal (_WMGR_Menu, title, 1);
	}
	return run;
}


//------------------------------------------------------------------------------
static WINDOW *
_Wmgr_WindByHandle (short hdl)
{
	WINDOW * wind;
	
	if (!hdl) {
		wind = &WIND_Root;
	
	} else {
		wind = WIND_Root.StackBot;
		while (wind  &&  wind->Handle != hdl) wind = wind->NextSibl;
	}
	return wind;
}

//------------------------------------------------------------------------------
static WINDOW *
_Wmgr_WindByPointer (void)
{
	WINDOW * wind = NULL;
	short    hdl  = wind_find (MAIN_PointerPos->x, MAIN_PointerPos->y);
	
	if (hdl >= 0) {
		wind = _Wmgr_WindByHandle (hdl);
	}
	return wind;
}

//------------------------------------------------------------------------------
static void
_Wmgr_DrawIcon (WINDOW * wind, GRECT * clip)
{
	GRECT  work, sect;
	MFDB   screen = { NULL };
	MFDB * icon   = &_WMGR_Icon;
	MFDB * mask   = NULL;
	short  pxy[8] = { 0, 0, };
	short  col[3] = { G_BLACK, G_WHITE, 0 };
	PXY    rec[2];
	typeof(vrt_cpyfm) * cpyfm = vrt_cpyfm;
	int                 mode  = MD_TRANS;
	
	WmHints * hints = PropValue (wind,
	                             XA_WM_HINTS, XA_WM_HINTS, sizeof(WmHints));
	if (hints && hints->icon_pixmap) {
		PIXMAP * pmap = PmapFind (hints->icon_pixmap);
		if (pmap) {
			icon = PmapMFDB (pmap);
			if (icon->fd_nplanes != 1) {
				cpyfm = (typeof(vrt_cpyfm)*)vro_cpyfm;
				mode  = S_OR_D;
			}
			if (hints->icon_mask
			    && (pmap = PmapFind (hints->icon_mask)) && pmap->Depth == 1) {
				mask = PmapMFDB (pmap);
			}
		}
	}
	WindUpdate (xTrue);
	wind_get_work (wind->Handle, &work);
	pxy[2] = icon->fd_w -1;
	pxy[3] = icon->fd_h -1;
	pxy[4] = work.x + ((work.w - icon->fd_w) /2);
	pxy[5] = work.y + ((work.h - icon->fd_h) /2);
	pxy[6] = pxy[4] + icon->fd_w -1;
	pxy[7] = pxy[5] + icon->fd_h -1;
	rec[1].x = (rec[0].x = work.x) + work.w -1;
	rec[1].y = (rec[0].y = work.y) + work.h -1;
	
	vswr_mode (GRPH_Vdi, MD_REPLACE);
	vsf_color (GRPH_Vdi, G_LWHITE);
	v_hide_c  (GRPH_Vdi);
	wind_get_first (wind->Handle, &sect);
	while (sect.w > 0  &&  sect.h > 0) {
		if (GrphIntersect (&sect, &work)) {
			vs_clip_r (GRPH_Vdi, &sect);
			v_bar     (GRPH_Vdi, (short*)rec);
			if (mask) vrt_cpyfm (GRPH_Vdi, MD_TRANS, pxy, mask, &screen, col +1);
			(*cpyfm) (GRPH_Vdi, mode, pxy, icon, &screen, col);
		}
		wind_get_next (wind->Handle, &sect);
	}
	vs_clip_r (GRPH_Vdi, NULL);
	v_show_c  (GRPH_Vdi, 1);
	WindUpdate (xFalse);
}

//==============================================================================
void
WmgrSetFocus (short focus)
{
	short place = WF_BOTTOM;
	
	if (WMGR_Focus) {
		_Wmgr_SetWidgets (WMGR_Focus, -1);
	}
	if (focus) {
		_Wmgr_SetWidgets (focus, +1);
		if (!_Wmgr_WindByHandle (wind_get_top())) {
			place = WF_TOP;
		}
	}
	wind_set (_WMGR_FocusHolder, place, 0,0,0,0);
	
	WMGR_Focus = focus;
}

//==============================================================================
BOOL
WmgrMessage (short * msg)
{
	static BOOL color_changed = xFalse;
	
	BOOL     reset    = xFalse;
	BOOL     inv_save = WIND_SaveDone;
	WINDOW * wind;
	
	switch (msg[0]) {
		
		case WM_REDRAW: if ((wind = _Wmgr_WindByHandle(msg[3]))) {
			GRECT * rect = (GRECT*)(msg +4);
			if (WIND_SaveDone
			    &&  rect->x              >= WIND_SaveArea->lu.x
			    &&  rect->y              >= WIND_SaveArea->lu.y
			    &&  rect->x + rect->w -1 <= WIND_SaveArea->rd.x
			    &&  rect->y + rect->h -1 <= WIND_SaveArea->rd.y) {
				inv_save = xFalse;
			} else {
				if (wind->GwmIcon) _Wmgr_DrawIcon  (wind, rect);
				else               WindDrawSection (wind, rect);
			}
		}	break;
		
		case WM_MOVED: if ((wind = _Wmgr_WindByHandle(msg[3]))) {
			wind_set_r (msg[3], WF_CURRXYWH, (GRECT*)(msg +4));
			if (!wind->GwmIcon) {
				CARD32 above = (wind->PrevSibl ? wind->PrevSibl->Id : None);
				GRECT work;
				wind_get_work (msg[3], &work);
				work.x -= WIND_Root.Rect.x - WMGR_DECOR;
				work.y -= WIND_Root.Rect.y;
				*(PXY*)&wind->Rect = *(PXY*)&work;
				work.x -= wind->BorderWidth;
				work.y -= wind->BorderWidth;
				work.w =  wind->Rect.w;
				work.h =  wind->Rect.h;
				EvntConfigureNotify (wind, wind->Id, above,
					                  &work, wind->BorderWidth, wind->Override);
			}
			WindPointerWatch (xFalse);
		}	break;
		
		case WM_CLOSED: if ((wind = _Wmgr_WindByHandle(msg[3]))) {
			CLIENT * clnt = ClntFind (wind->Id);
			if (clnt) {
				if (PropHasAtom (wind, WM_PROTOCOLS, WM_DELETE_WINDOW)) {
					Atom data[5] = { WM_DELETE_WINDOW };
					EvntClientMsg (clnt, wind->Id, WM_PROTOCOLS, 32, data);
				} else {
					reset = (ClntDelete (clnt) == 0);
				}
			}
		}	break;
		
		case WM_TOPPED: if ((wind = _Wmgr_WindByHandle(msg[3]))) {
			if (color_changed) {
				CmapPalette();
				color_changed = xFalse;
			}
			_WMGR_HasFocus = (WMGR_OpenCounter > 1 ? 2 : 1);
			WindCirculate (wind, PlaceOnTop);
		}	break;
		
		case WM_BOTTOMED: if ((wind = _Wmgr_WindByHandle(msg[3]))) {
			_WMGR_HasFocus = 0;
			WindCirculate (wind, PlaceOnBottom);
		}	break;
		
		case WM_ONTOP:
			if (color_changed) {
				CmapPalette();
				color_changed = xFalse;
			}
			if (msg[3] == _WMGR_FocusHolder) {
				wind_set (_WMGR_FocusHolder, WF_BOTTOM, 0,0,0,0);
			} else {
				_WMGR_HasFocus++;
			}
			WindPointerWatch (xFalse);
			break;
		
		case WM_UNTOPPED:
			if (msg[3] != _WMGR_FocusHolder) {
				_WMGR_HasFocus--;
			}
		case 22360: // winshade
		case 22361: // winunshade
			WindPointerWatch (xFalse);
			break;
		
		case WM_ICONIFY: if ((wind = _Wmgr_WindByHandle(msg[3]))) {
			char * title = PropValue (wind, XA_WM_ICON_NAME, XA_STRING, 1);
			if (title) {
				wind_set_str (wind->Handle, WF_NAME, title);
			}
			if (msg[3] == WMGR_Focus) {
				WMGR_Focus = 0;
			}
			_Wmgr_SetWidgets (msg[3], 0);
			WindUnmap (wind, xFalse);
			wind_set_r (msg[3], WF_ICONIFY, (GRECT*)(msg +4));
			wind_set   (msg[3], WF_BEVENT, 0x0000, 0,0,0);
			wind_set   (_WMGR_FocusHolder, WF_BOTTOM, 0,0,0,0);
			wind->GwmIcon = xTrue;
			WindPointerWatch (xFalse);
		}	break;
		
		case WM_UNICONIFY: if ((wind = _Wmgr_WindByHandle(msg[3]))) {
			char * title = PropValue (wind, XA_WM_NAME, XA_STRING, 1);
			GRECT  curr;
			if (title) {
				wind_set_str (wind->Handle, WF_NAME, title);
			}
			WmgrCalcBorder (&curr, wind);
			_Wmgr_SetWidgets (msg[3], -1);
			wind_set_r (msg[3], WF_UNICONIFY, &curr);
			wind_set   (msg[3], WF_BEVENT, 0x0001, 0,0,0);
			wind->GwmIcon = xFalse;
			WindMap (wind, xTrue);
			WindPointerWatch (xFalse);
		}	break;
		
		case COLORS_CHANGED:
			color_changed = xTrue;
			break;

		default:
			printf ("event #%i %X \n", msg[0], MAIN_KeyButMask);
	}
	
	if (inv_save) WindSaveFlush (xFalse);
	
	return reset;
}

//==============================================================================
BOOL
WmgrButton (void)
{
	WINDOW * wind = (WMGR_Cursor ? _Wmgr_WindByPointer() : NULL);
	
	if (!wind) {
		return xFalse;
	
	} else {
		EVMULTI_IN  ev_i = {
			MU_BUTTON|MU_M1|MU_TIMER, 1,0x03,0x00,
		   MO_LEAVE, { MAIN_PointerPos->x, MAIN_PointerPos->y, 1,1 },
		   0, {0,0, 0,0}, 20,0 };
		EVMULTI_OUT ev_o;
		short       ev, dummy[8];
		PXY         pc[5],   pw[5];
		CARD16      cursor = WMGR_Cursor;
		int         ch, cv,  wh, wv,  mx, my;
		int         ml = 0, mu = 0, mr = WIND_Root.Rect.w, md = WIND_Root.Rect.h;
		
		SizeHints * s_hints = PropValue (wind,
		                                 XA_WM_NORMAL_HINTS, XA_WM_SIZE_HINTS,
		                                 4/*sizeof(SizeHints)*/);
		if (s_hints) {
			printf("%03lX:", s_hints->flags);
			if (s_hints->flags & PMinSize)
				printf ("   min = %li,%li", s_hints->min_width,s_hints->min_height);
			if (s_hints->flags & PMaxSize)
				printf ("   max = %li,%li", s_hints->max_width,s_hints->max_height);
			if (s_hints->flags & PResizeInc)
				printf ("   inc = %li,%li", s_hints->inc_width,s_hints->inc_height);
			if (s_hints->flags & PAspect)
				printf ("   aspect = %li,%li/%li,%li",
				        s_hints->min_aspect.x, s_hints->min_aspect.y,
				        s_hints->max_aspect.x, s_hints->max_aspect.y);
			if (s_hints->flags & PBaseSize)
				printf ("   base = %li,%li",
				        s_hints->base_width,s_hints->base_height);
			if (s_hints->flags & PWinGravity)
				printf ("   gravity = %li", s_hints->win_gravity);
			printf("\n");
			
			if (s_hints->flags & PBaseSize) {
				ml = s_hints->base_width;
				mu = s_hints->base_height;
				if (s_hints->flags & PResizeInc) {
					ml += s_hints->inc_width;
					mu += s_hints->inc_height;
				}
			} else if (s_hints->flags & PMinSize) {
				ml = s_hints->min_width;
				mu = s_hints->min_height;
			}
			if (!(MAIN_KeyButMask & K_CTRL)) {
				if (s_hints->flags & PMaxSize) {
					mr = s_hints->max_width;
					md = s_hints->max_height;
				}
				if (ml >= mr  &&  mu >= md) {
					ev_i.evi_flags &= ~MU_M1;
					cursor         =  0x000;
					CrsrSelect (&_WMGR_Cursor.X);
				}
			}
		}
		
		wind_get_curr (wind->Handle, (GRECT*)pc);
		pc[2].x = pc[1].x += (pc[3].x = pc[4].x = pc[0].x) -1;
		pc[2].y = pc[3].y =   pc[0].y + pc[1].y            -1;
		pc[4].y =            (pc[1].y = pc[0].y)           +1;
		pw[1].x =  pw[2].x =           wind->Rect.x + WIND_Root.Rect.x;
		pw[4].y = (pw[0].y = pw[1].y = wind->Rect.y + WIND_Root.Rect.y) +1;
		pw[0].x =  pw[3].x = pw[4].x = pw[1].x + wind->Rect.w -1;
		pw[2].y =  pw[3].y =           pw[1].y + wind->Rect.h -1;
		
		if (MAIN_KeyButMask & 0x01) {
			if (cursor == 0x100) {
				cursor = 0x110;
				CrsrSelect (&_WMGR_Cursor.LD);
			} else if (cursor == 0x001) {
				cursor = 0x011;
				CrsrSelect (&_WMGR_Cursor.RD);
			}
		}
		if (cursor & 0x100) {
			int _l = ml;
			ch = pc[0].x - MAIN_PointerPos->x;
			wh = pw[1].x - MAIN_PointerPos->x;
			ml = pw[0].x - mr - wh;
			mr = pw[0].x - _l - wh;
		} else if (cursor & 0x001) {
			ch = pc[1].x - MAIN_PointerPos->x;
			wh = pw[0].x - MAIN_PointerPos->x;
			ml = pw[1].x + ml - wh;
			mr = pw[1].x + mr - wh;
		} else {
			ch = wh = 0;
		}
		if (cursor & 0x010) {
			cv = pc[2].y - MAIN_PointerPos->y;
			wv = pw[3].y - MAIN_PointerPos->y;
			mu = pw[0].y + mu - wv;
			md = pw[0].y + md - wv;
		} else {
			cv = wv = 0;
		}
		
		vswr_mode (GRPH_Vdi, MD_XOR);
		vsl_color (GRPH_Vdi, G_BLACK);
		vsl_udsty (GRPH_Vdi, 0xAAAA);
		vsl_type  (GRPH_Vdi, USERLINE);
		v_hide_c (GRPH_Vdi);
		do {
			v_pline (GRPH_Vdi, 5, (short*)pc);
			v_pline (GRPH_Vdi, 5, (short*)pw);
			v_show_c (GRPH_Vdi, 1);
			ev = evnt_multi_s (&ev_i, dummy, &ev_o);
			mx = (ev_o.evo_mouse.x <= ml ? ml :
			      ev_o.evo_mouse.x >= mr ? mr : ev_o.evo_mouse.x);
			my = (ev_o.evo_mouse.y <= mu ? mu :
			      ev_o.evo_mouse.y >= md ? md : ev_o.evo_mouse.y);
			v_hide_c (GRPH_Vdi);
			v_pline (GRPH_Vdi, 5, (short*)pw);
			v_pline (GRPH_Vdi, 5, (short*)pc);
			if (ev & MU_TIMER) {
				ev_i.evi_flags &= ~MU_TIMER;
			}
			if (ev & MU_M1) {
				if (cursor & 0x100) {
					pc[0].x = pc[3].x = pc[4].x = mx + ch;
					pw[1].x = pw[2].x           = mx + wh;
				} else if (cursor & 0x001) {
					pc[1].x = pc[2].x           = mx + ch;
					pw[0].x = pw[3].x = pw[4].x = mx + wh;
				}
				if (cursor & 0x010) {
					pc[2].y = pc[3].y = my + cv;
					pw[2].y = pw[3].y = my + wv;
				}
				*(PXY*)&ev_i.evi_m1 = ev_o.evo_mouse;
			}
		} while (!(ev & MU_BUTTON));
		v_show_c (GRPH_Vdi, 1);
		vsl_type (GRPH_Vdi, SOLID);
		
		if (ev_i.evi_flags & MU_TIMER) {
			WindCirculate (wind, PlaceOnTop);
		
		} else {
			pw[2].x  = pw[1].x - WIND_Root.Rect.x - wind->Rect.x;
			pw[2].y  = 0;
			pw[3].x -= pw[1].x -1 + wind->Rect.w;
			pw[3].y -= pw[1].y -1 + wind->Rect.h;
			if (pw[2].x || pw[3].x || pw[3].y) {
				WindResize (wind, (GRECT*)(pw +2));
			}
			WindPointerWatch (xFalse);
		}
	}
	return xTrue;
}


//------------------------------------------------------------------------------
static void
FT_Wmgr_reply (p_CLIENT clnt, CARD32 size, const char * form)
{
	int req = (_WMGR_Client.iBuf.Mem
	           ? ((xReq*)_WMGR_Client.iBuf.Mem)->reqType : -1);
	
	_WMGR_Obuf.generic.type           = X_Reply;
	_WMGR_Obuf.generic.sequenceNumber = _WMGR_Client.SeqNum;
	_WMGR_Obuf.generic.length         = Units(size - sizeof(xReply));
	
	_WMGR_Client.oBuf.Done = Align(size);
	
	PRINT (,"reply to '%s'",
	       (req < 0  ? "<n/a>" :
	        req > 0 && req < FirstExtensionError ? RequestTable[req].Name :
	        "<invalid>"));
}

//------------------------------------------------------------------------------
static void
FT_Wmgr_error (p_CLIENT c, CARD8 code, CARD8 majOp, CARD16 minOp, CARD32 val)
{
}

//------------------------------------------------------------------------------
static void
FT_Wmgr_event (p_CLIENT c, p_WINDOW w, CARD16 evnt, va_list vap)
{
	FT_Evnt_send_MSB (&_WMGR_Client, w, evnt, vap);
	_WMGR_Client.oBuf.Done = _WMGR_Client.oBuf.Left;
	_WMGR_Client.oBuf.Left = 0;
}
