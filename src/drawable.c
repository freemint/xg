//==============================================================================
//
// drawable.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-05 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "tools.h"
#include "clnt.h"
#include "pixmap.h"
#include "window.h"
#include "gcontext.h"
#include "font.h"
#include "grph.h"
#include "x_gem.h"
#include "gemx.h"

#include <stdio.h>

#include <X11/X.h>


//==============================================================================
void
DrawDelete (p_DRAWABLE draw, p_CLIENT clnt)
{
	if (draw.p->isWind) {
		WINDOW * wind = draw.Window;
		while (RID_Match (clnt->Id, wind->Parent->Id)) {
			wind = wind->Parent;
		}
		WindDelete (wind, clnt);
	
	} else {
		PmapFree (draw.Pixmap, clnt);
	}
}


//------------------------------------------------------------------------------
static CARD16
WindClipLockP (WINDOW * wind, CARD16 border, const GRECT * clip, short n_clip,
               PXY * orig, PXY ** pBuf)
{
	WINDOW * pwnd;
	GRECT    work = wind->Rect, rect;
	BOOL     visb = wind->isMapped;
	CARD16   nClp = 0;
	PXY    * sect, * p_clip;
	int      a, b, n;
	short    l = 0x7FFF, u = 0x7FFF, r = 0x8000, d = 0x8000;
	
	if (border) {
		work.x -= border;
		work.y -= border;
		border *= 2;
		work.w += border;
		work.h += border;
	}
	if (work.x < 0) { work.w += work.x; work.x = 0; }
	if (work.y < 0) { work.h += work.y; work.y = 0; }
	
	*orig = *(PXY*)&wind->Rect;
	
	while ((pwnd = wind->Parent)) {
		if (work.x + work.w > pwnd->Rect.w) work.w = pwnd->Rect.w - work.x;
		if (work.y + work.h > pwnd->Rect.h) work.h = pwnd->Rect.h - work.y;
		if ((work.x += pwnd->Rect.x) < 0) { work.w += work.x; work.x = 0; }
		if ((work.y += pwnd->Rect.y) < 0) { work.h += work.y; work.y = 0; }
		orig->x += pwnd->Rect.x;
		orig->y += pwnd->Rect.y;
		if (pwnd == &WIND_Root  || !(visb &= pwnd->isMapped)) break;
		wind = pwnd;
	}
	if (!visb ||  work.w <= 0  ||  work.h <= 0) return 0;
	
	work.w += work.x -1;
	work.h += work.y -1;
	
	if (clip  &&  n_clip > 0) {
		PXY * c = p_clip = alloca (sizeof(GRECT) * n_clip);
		n       = 0;
		while (n_clip--) {
			c[1].x = (c[0].x = clip->x + orig->x) + clip->w -1;
			c[1].y = (c[0].y = clip->y + orig->y) + clip->h -1;
			clip++;
			if (GrphIntersectP (c, (PXY*)&work)) {
				c += 2;
				n += 1;
			}
		}
		if (!(n_clip = n)) return 0;
	
	} else {
		p_clip = (PXY*)&work;
		if (clip  &&  n_clip < 0) {
			PXY c[2];
			c[1].x = (c[0].x = clip->x) + clip->w -1;
			c[1].y = (c[0].y = clip->y) + clip->h -1;
			if (!GrphIntersectP (p_clip, c)) return 0;
		}
		n_clip = 1;
	}
	
	WindUpdate (xTrue);
	wind_get (0, WF_SCREEN, &a, &b, &n,&n);
	*pBuf = sect = (PXY*)((a << 16) | (b & 0xFFFF));
	
	wind_get_first (wind->Handle, &rect);
	while (rect.w > 0  &&  rect.h > 0) {
		PXY * c = p_clip;
		n       = n_clip;
		rect.w += rect.x -1;
		rect.h += rect.y -1;
		do {
			sect[0] = *(PXY*)&rect.x;
			sect[1] = *(PXY*)&rect.w;
			if (GrphIntersectP (sect, c)) {
				if (l > sect[0].x) l = sect[0].x;
				if (u > sect[0].y) u = sect[0].y;
				if (r < sect[1].x) r = sect[1].x;
				if (d < sect[1].y) d = sect[1].y;
				nClp += 1;
				sect += 2;
			}
			c += 2;
		} while (--n);
		wind_get_next (wind->Handle, &rect);
	}
	if (nClp) {
		sect[0].x = l;
		sect[0].y = u;
		sect[1].x = r;
		sect[1].y = d;
	
	} else {
		WindUpdate (xFalse);
	}
	return nClp;
}

//------------------------------------------------------------------------------
static inline PXY *
SizeToPXY (PXY * dst, const short * src)
{
	__asm__ volatile ("
		moveq.l	#0, d0;
		move.l	(%0), d1;
		sub.l		#0x00010001, d1;
		movem.l	d0/d1, (%1);
		"
		:                   // output
		: "a"(src),"a"(dst) // input
		: "d0","d1"         // clobbered
	);
	return dst;
}

//------------------------------------------------------------------------------
static inline CARD16
SizeToLst (PXY * dst, GRECT * clip, CARD16 n_clip, const short * src)
{
	CARD16 nClp = 0;
	PXY    size[2];
	
	SizeToPXY (size, src);
	
	while (n_clip--) {
		dst[1].x = (dst[0].x = clip->x) + clip->w -1;
		dst[1].y = (dst[0].y = clip->y) + clip->h -1;
		if (GrphIntersectP (dst, size)) {
			dst  += 2;
			nClp += 1;
		}
		clip++;
	}
	return nClp;
}


//------------------------------------------------------------------------------
static inline BOOL
gc_mode (CARD32 * color, BOOL * set, int hdl, const GC * gc)
{
	BOOL  fg;
	short m;
	
	switch (gc->Function) {
		case GXequiv:                                            // !src XOR  dst
		case GXset:                                              //  1
		case GXcopy:         fg = xTrue;  m = MD_REPLACE; break; //  src
		
		case GXcopyInverted:                                     // !src
		case GXclear:        fg = xFalse; m = MD_REPLACE; break; //  0
		
		case GXxor:          fg = xTrue;  m = MD_XOR;     break; //  src XOR  dst
		
		case GXnor:                                              // !src AND !dst
		case GXinvert:       fg = xFalse; m = MD_XOR;     break; //          !dst
		
		case GXnand:                                             // !src OR  !dst
		case GXorReverse:                                        //  src OR  !dst
		case GXor:                                               //  src OR   dst
		case GXandReverse:   fg = xTrue;  m = MD_TRANS;   break; //  src AND !dst
		
		
		case GXorInverted:                                    // !src OR   dst
		case GXandInverted:                                   // !src AND  dst
		case GXand:                                           //  src AND  dst
		case GXnoop:                                          //           dst
		default:             return xFalse;
	}
	if (fg) {
		*color = gc->Foreground;
	} else {
		*color = gc->Background;
		*set   = xTrue;
	}
	vswr_mode (hdl, m);
	
	return xTrue;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_GetGeometry (CLIENT * clnt, xGetGeometryReq * q)
{
	DRAWABLE * draw = NULL;
	
	if ((q->id & ~RID_MASK) && !(draw = DrawFind(q->id).p)) {
		Bad(Drawable, q->id, GetGeometry,);
	
	} else {
		ClntReplyPtr (GetGeometry, r);
		r->root = ROOT_WINDOW;
		
		if (!draw) {
			wind_get_work (q->id & 0x7FFF, (GRECT*)&r->x);
			r->depth       = WIND_Root.Depth;
			r->x          -= WIND_Root.Rect.x +1;
			r->y          -= WIND_Root.Rect.y +1;
			r->borderWidth = 1;
		
		} else {
			r->depth = draw->Depth;
			if (draw->isWind) {
				WINDOW * wind = (WINDOW*)draw;
				*(GRECT*)&r->x = wind->Rect;
				r->x -= wind->BorderWidth;
				r->y -= wind->BorderWidth;
				r->borderWidth = wind->BorderWidth;
			} else {
				PIXMAP * pmap = (PIXMAP*)draw;
				*(long*)&r->x     = 0l;
				*(long*)&r->width = *(long*)&pmap->W;
				r->borderWidth = 0;
			}
			
			DEBUG (GetGeometry," 0x%lX", q->id);
		}
		
		ClntReply (GetGeometry,, "wR.");
	}
}


//------------------------------------------------------------------------------
void
RQ_FillPoly (CLIENT * clnt, xFillPolyReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xFillPolyReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, FillPoly,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, FillPoly,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY   * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				v_hide_c (hdl);
			}
			DEBUG (FillPoly," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsf_color (hdl, color);
			}
			do {
				vs_clip_p    (hdl, sect);
				v_fillarea_p (hdl, len, pxy);
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	
	} else {
		PRINT (FillPoly," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyArc (CLIENT * clnt, xPolyArcReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyArcReq)) / sizeof(xArc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyArc,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyArc,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		xArc  * arc = (xArc*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY   * sect;
		PXY     orig;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				v_hide_c (hdl);
			}
			DEBUG (PolyArc," P:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			*(long*)&orig = 0;
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			clnt->Fnct->shift_arc (&orig, arc, len, ArcPieSlice);
			if (set) {
				vsl_width (hdl, gc->LineWidth);
				vsl_color (hdl, color);
			}
			do {
				int i;
				vs_clip_p (hdl, sect);
				for (i = 0; i < len; ++i) {
					v_ellarc (hdl, arc[i].x, arc[i].y,
					          arc[i].width, arc[i].height,
					          arc[i].angle1, arc[i].angle2);
				}
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyFillArc (CLIENT * clnt, xPolyFillArcReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyFillArcReq)) / sizeof(xArc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyFillArc,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyFillArc,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		xArc  * arc = (xArc*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY   * sect;
		PXY     orig;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				v_hide_c (hdl);
			}
			DEBUG (PolyFillArc," P:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			*(long*)&orig = 0;
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			clnt->Fnct->shift_arc (&orig, arc, len, gc->ArcMode);
			if (set) {
				vsf_color (hdl, color);
			}
			do {
				int i;
				vs_clip_p (hdl, sect);
				for (i = 0; i < len; ++i) {
					v_ellpie (hdl, arc[i].x, arc[i].y,
					          arc[i].width, arc[i].height,
					          arc[i].angle1, arc[i].angle2);
				}
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyLine (CLIENT * clnt, xPolyLineReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyLineReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyLine,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyLine,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY   * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				v_hide_c (hdl);
			}
			DEBUG (PolyLine," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsl_color (hdl, color);
				vsl_width (hdl, gc->LineWidth);
			}
			do {
				vs_clip_p (hdl, sect);
				v_pline_p (hdl, len, pxy);
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	
	} else {
		PRINT (PolyLine," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyPoint (CLIENT * clnt, xPolyPointReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyPointReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyPoint,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyPoint,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY   * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				v_hide_c (hdl);
			}
			DEBUG (PolyPoint," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			if (draw.p->Depth == 1) {
				PmapDrawPoints (draw.Pixmap, gc, pxy, len);
				nClp = 0;
			
			} else {
				if (gc->ClipNum > 0) {
					sect = alloca (sizeof(GRECT) * gc->ClipNum);
					nClp = SizeToLst (sect,
					                  gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
				} else {
					sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
					nClp = 1;
				}
				set = (draw.Pixmap->Vdi > 0);
				hdl = PmapVdi (draw.Pixmap, gc, xFalse);
			}
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsm_color (hdl, color);
			}
			do {
				vs_clip_p   (hdl, sect);
				v_pmarker_p (hdl, len, pxy);
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	
	} else {
		PRINT (PolyPoint," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyFillRectangle (CLIENT * clnt, xPolyFillRectangleReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyFillRectangleReq))
	                / sizeof(GRECT);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyFillRectangle,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyFillRectangle,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		GRECT * rec = (GRECT*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY   * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_r2p (&orig, rec, len);
				v_hide_c (hdl);
			}
			DEBUG (PolyFillRectangle," P:%lX G:%lX (%lu)",
			       q->drawable, q->gc, len);
		
		} else { // Pixmap
			/*if (draw.p->Depth == 1) {   // disabled
				if (clnt->DoSwap) {
					size_t  num = len;
					GRECT * rct = rec;
					while (num--) {
						SwapRCT(rct, rct);
						rct++;
					}
				}
				PmapFillRects (draw.Pixmap, gc, rec, len);
				nClp = 0;
			
			} else*/ {
				if (gc->ClipNum > 0) {
					sect = alloca (sizeof(GRECT) * gc->ClipNum);
					nClp = SizeToLst (sect,
					                  gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
				} else {
					sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
					nClp = 1;
				}
				clnt->Fnct->shift_r2p (NULL, rec, len);
				set = (draw.Pixmap->Vdi > 0);
				hdl = PmapVdi (draw.Pixmap, gc, xFalse);
			}
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsf_color (hdl, color);
			}
			do {
				int i;
				for (i = 0; i < len; i++) {
					PXY clip[2];
					*(GRECT*)clip = rec[i];
					if (GrphIntersectP (clip, sect)) {
						v_bar_p (hdl, clip);
					}
				}
				sect += 2;
			} while (--nClp);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	} else {
		PRINT (PolyFillRectangle," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyRectangle (CLIENT * clnt, xPolyRectangleReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyRectangleReq))
	                / sizeof(GRECT);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyRectangle,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyRectangle,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		GRECT * rec = (GRECT*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY     orig;
		PXY   * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				v_hide_c (hdl);
			}
			DEBUG (PolyRectangle," P:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			*(long*)&orig = 0;
			set  = (draw.Pixmap->Vdi > 0);
			hdl  = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			short d = vsl_width (hdl, gc->LineWidth);
			if (set) {
				vsl_color (hdl, color);
			}
			if (clnt->DoSwap) {
				size_t  n = len;
				GRECT * r = rec;
				while (n--) {
					SwapRCT(r, r);
					r++;
				}
			}	
			do {
				int i;
				vs_clip_p (hdl, sect);
				for (i = 0; i < len; ++i) {
					PXY p[5];
					p[0].x = p[3].x = p[4].x = rec[i].x + orig.x;
					p[1].x = p[2].x = p[0].x + rec[i].w -1;
					p[0].y = p[1].y          = rec[i].y + orig.y;
					p[2].y = p[3].y = p[0].y + rec[i].h -1;
					p[4].y          = p[0].y + d;
					v_pline_p (hdl, 5, p);
				}
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolySegment (CLIENT * clnt, xPolySegmentReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolySegmentReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolySegment,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolySegment,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY   * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, CoordModeOrigin);
				v_hide_c (hdl);
			}
			DEBUG (PolySegment," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			clnt->Fnct->shift_pnt (NULL, pxy, len, CoordModeOrigin);
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsl_color (hdl, color);
				vsl_width (hdl, gc->LineWidth);
			}
			do {
				int   i;
				vs_clip_p (hdl, sect);
				for (i = 0; i < len; i += 2) {
					v_pline_p (hdl, 2, &pxy[i]);
				}
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
static void
_Image_Text (p_DRAWABLE draw, GC * gc,
             BOOL is8N16, void * text, short len, PXY * pos)
{
	short   hdl = GRPH_Vdi;
	short   arr[len];
	PXY   * sect;
	PXY     orig;
	CARD16  nClp;
	
	if (draw.p->isWind) {
		nClp = WindClipLockP (draw.Window, 0,
		                      gc->ClipRect, gc->ClipNum, &orig, &sect);
		if (nClp) {
			int dmy;
			orig.x += pos->x;
			orig.y += pos->y;
			vst_font    (hdl, gc->FontIndex);
			vst_effects (hdl, gc->FontEffects);
			if (gc->FontWidth) {
				vst_height (hdl, gc->FontPoints, &dmy,&dmy,&dmy,&dmy);
				vst_width  (hdl, gc->FontWidth,  &dmy,&dmy,&dmy,&dmy);
			} else {
				vst_point  (hdl, gc->FontPoints, &dmy, &dmy, &dmy, &dmy);
			}
			v_hide_c (hdl);
		}
		
	} else { // Pixmap
		if (gc->ClipNum > 0) {
			sect = alloca (sizeof(GRECT) * gc->ClipNum);
			nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
		} else {
			sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
			nClp = 1;
		}
		orig = *pos;
		hdl  = PmapVdi (draw.Pixmap, gc, xTrue);
	}
	if (nClp) {
		BOOL bg_draw = (gc->Background != WHITE);
		if (is8N16) FontLatin1_C (arr, text, len);
		else        FontLatin1_W (arr, text, len);
		if (!bg_draw) {
			vswr_mode (hdl, MD_REPLACE);
			vst_color (hdl, gc->Foreground);
		}
		do {
			vs_clip_p (hdl, sect);
			if (bg_draw) {
				vswr_mode   (hdl, MD_ERASE);
				vst_color   (hdl, gc->Background);
				v_gtext_arr (hdl, &orig, len, arr);
				vswr_mode   (hdl, MD_TRANS);
				vst_color   (hdl, gc->Foreground);
			}
			v_gtext_arr (hdl, &orig, len, arr);
			sect++;
		} while (--nClp);
		vs_clip_off (hdl);
		
		if (draw.p->isWind) {
			v_show_c (hdl, 1);
			WindClipOff();
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_ImageText8 (CLIENT * clnt, xImageTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, ImageText8,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, ImageText8,);
	
	} else if (q->nChars  &&  gc->ClipNum >= 0) {
		DEBUG (ImageText8," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Image_Text (draw, gc, xTrue, (char*)(q +1), q->nChars, (PXY*)&q->x);
	}
}

//------------------------------------------------------------------------------
void
RQ_ImageText16 (CLIENT * clnt, xImageTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, ImageText16,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, ImageText16,);
	
	} else if (q->nChars  &&  gc->ClipNum >= 0) {
		DEBUG (ImageText16," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Image_Text (draw, gc, xFalse, (char*)(q +1), q->nChars, (PXY*)&q->x);
	}
}

//------------------------------------------------------------------------------
static void
_Poly_Text (p_DRAWABLE draw, GC * gc, BOOL is8N16, xTextElt * t, PXY * pos)
{
	if (t->len) {
		short   hdl = GRPH_Vdi;
		short   arr[t->len];
		PXY   * sect;
		PXY     orig;
		CARD16  nClp;
		
		if (draw.p->isWind) {
			nClp = WindClipLockP (draw.Window, 0,
			                      gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				int dmy;
				orig.x += pos->x;
				orig.y += pos->y;
				vst_font    (hdl, gc->FontIndex);
				vst_color   (hdl, gc->Foreground);
				vst_effects (hdl, gc->FontEffects);
				if (gc->FontWidth) {
					vst_height (hdl, gc->FontPoints, &dmy,&dmy,&dmy,&dmy);
					vst_width  (hdl, gc->FontWidth,  &dmy,&dmy,&dmy,&dmy);
				} else {
					vst_point  (hdl, gc->FontPoints, &dmy, &dmy, &dmy, &dmy);
				}
				v_hide_c (hdl);
			}
		
		} else { // Pixmap
			DEBUG (PolyText8," P:%lX G:%lX (%i,%i)",
			       q->drawable, q->gc, q->x, q->y);
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToLst (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToPXY (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			orig = *pos;
			hdl  = PmapVdi (draw.Pixmap, gc, xTrue);
		}
		if (nClp) {
			if (is8N16) FontLatin1_C (arr,  (char*)(t +1), t->len);
			else        FontLatin1_W (arr, (short*)(t +1), t->len);
			vswr_mode (hdl, MD_TRANS);
			do {
				vs_clip_p (hdl, sect);
				v_gtext_arr (hdl, &orig, t->len, arr);
				sect += 2;
			} while (--nClp);
			vs_clip_off (hdl);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyText8 (CLIENT * clnt, xPolyTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyText8,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyText8,);
	
	} else if (gc->ClipNum >= 0) {
		DEBUG (PolyText8," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Poly_Text (draw, gc, xTrue, (xTextElt*)(q +1), (PXY*)&q->x);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyText16 (CLIENT * clnt, xPolyTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyText16,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyText16,);
	
	} else if (gc->ClipNum >= 0) {
		DEBUG (PolyText16," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Poly_Text (draw, gc, xFalse, (xTextElt*)(q +1), (PXY*)&q->x);
	}
}
