//==============================================================================
//
// event.h
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-07-01 - Initial Version.
//==============================================================================
//
#ifndef __EVENT_H__
# define __EVENT_H__

# include <stdarg.h>


#ifndef __PXY
# define __PXY
typedef struct s_PXY {
	short x;
	short y;
} PXY;
#endif


#define AllEventMask   0x01FFFFFFuL

BOOL EvntSet (p_WINDOW wind, p_CLIENT clnt, CARD32 mask);
BOOL EvntClr (p_WINDOW wind, p_CLIENT clnt);
void EvntDel (p_WINDOW wind);

// KeyPress/KeyRelease, ButtonPress/ButtonRelease
BOOL EvntPropagate (p_WINDOW wind, CARD32 mask, BYTE event,
                    CARD32 rid, CARD32 cid, PXY * rxy, BYTE detail);

#define EvntCirculateNotify(w,m, wid, plc)\
                                         _evnt_w (w,m,CirculateNotify, wid, plc)
#define EvntCirculateRequest(w,m, wid, plc)\
                 _evnt_w (w,SubstructureRedirectMask,CirculateRequest, wid, plc)
void    EvntClientMsg (p_CLIENT, CARD32 id, CARD32 type, BYTE format, void *);
#define EvntConfigureNotify(w,m, wid, abv, rct, brd, ovr)\
                         _evnt_w (w,m,ConfigureNotify, wid, abv, &rct, brd, ovr)
#define EvntCreateNotify(   w,m, wid, rct, brd, ovr)\
                                 _evnt_w (w,m,CreateNotify, wid, &rct, brd, ovr)
#define EvntDestroyNotify(  w,m, wid)           _evnt_w (w,m,DestroyNotify, wid)
#define EvntEnterNotify(    w,   wid, cid, rxy, exy, mod, foc, dtl)\
                              _evnt_w (w,EnterWindowMask,EnterNotify, wid, cid,\
                                            *(CARD32*)&(rxy), *(CARD32*)&(exy),\
                                                                  mod, foc, dtl)
void    EvntExpose (p_WINDOW, short len, const p_GRECT rect);
#define EvntFocusIn(        w,   mod, dtl) \
                                   _evnt_w (w,FocusChangeMask,FocusIn, mod, dtl)
#define EvntFocusOut(       w,   mod, dtl) \
                                  _evnt_w (w,FocusChangeMask,FocusOut, mod, dtl)
#define EvntGravityNotify(  w,m, wid, wxy)\
                                  _evnt_w (w,m,MapNotify, wid, *(CARD32*)&(wxy))
#define EvntLeaveNotify(    w,   wid, cid, rxy, exy, mod, foc, dtl)\
                              _evnt_w (w,LeaveWindowMask,LeaveNotify, wid, cid,\
                                            *(CARD32*)&(rxy), *(CARD32*)&(exy),\
                                                                  mod, foc, dtl)
#define EvntMapNotify(      w,m, wid, ovr)     _evnt_w (w,m,MapNotify, wid, ovr)
#define EvntMapRequest(     w,m, wid)              _evnt_w (w,m,MapRequest, wid)
#define EvntMotionNotify(   w,m, wid, cid, rxy, exy)\
                                           _evnt_w (w,m,MotionNotify, wid, cid,\
                                             *(CARD32*)&(rxy), *(CARD32*)&(exy))
#define EvntNoExposure(     c,   drw, req)        _evnt_c (c,NoExpose, drw, req)
#define EvntPropertyNotify( w,   atm, del)\
                         _evnt_w (w,PropertyChangeMask,PropertyNotify, atm, del)
#define EvntReparentNotify( w,m, wid, pid, wxy, ovr)\
                                         _evnt_w (w,m,ReparentNotify, wid, pid,\
                                                          *(CARD32*)&(wxy), ovr)
#define EvntSelectionClear( c,   wid, sel)  _evnt_c (c,SelectionClear, wid, sel)
#define EvntSelectionNotify(c,   tim, wid, sel, trg, prp)\
                            _evnt_c (c,SelectionNotify, tim, wid, sel, trg, prp)
#define EvntSelectionRequest(c,  tim, own, req, sel, trg, prp)\
                                              _evnt_c (c,SelectionRequest, tim,\
                                                        own, req, sel, trg, prp)
#define EvntUnmapNotify(    w,m, wid, cfg)   _evnt_w (w,m,UnmapNotify, wid, cfg)
#define EvntVisibilityNotify(w,  obs) \
                          _evnt_w (w,VisibilityChangeMask,VisibilityNotify, obs)


//___________to_do_
//KeyPress
//KeyRelease
//KeymapNotify
//GraphicsExpose
//NoExpose
//VisibilityNotify
//ConfigureRequest
//GravityNotify
//ResizeRequest
//CirculateNotify
//CirculateRequest
//ColormapNotify
//ClientMessage
//MappingNotify


void _evnt_w (p_WINDOW wind, CARD32 mask, CARD16 evnt, ...);
void _evnt_c (p_CLIENT clnt, CARD16 evnt, ...);

void FT_Evnt_send_MSB (p_CLIENT , p_WINDOW , CARD16 evnt, va_list);
void FT_Evnt_send_LSB (p_CLIENT , p_WINDOW , CARD16 evnt, va_list);


#endif __EVENT_H__
