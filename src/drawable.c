#include "main.h"
#include "tools.h"
#include "clnt.h"
#include "pixmap.h"
#include "window.h"
#include "gcontext.h"
#include "event.h"
#include "font.h"
#include "grph.h"
#include "x_gem.h"
#include "gemx.h"

#include <stdlib.h>
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
static inline GRECT * 
SizeToRCT (GRECT * dst, const short * src)
{
	__asm__ volatile ("
		clr.l		d0;
		move.l	(%0), d1;
		movem.l	d0/d1, (%1);
		"
		:                   // output
		: "a"(src),"a"(dst) // input
		: "d0","d1"         // clobbered
	);
	return dst;
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
				SizeToRCT ((GRECT*)&r->x, &pmap->W);
				r->borderWidth   = 0;
			}
			
			DEBUG (GetGeometry," 0x%lX", q->id);
		}
		
		ClntReply (GetGeometry,, "wR.");
	}
}

//------------------------------------------------------------------------------
void
RQ_PutImage (CLIENT * clnt, xPutImageReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	GC       * gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PutImage,);
		
	} else if (!gc) {
		Bad(GC, q->gc, PutImage,);
	
	} else if ((q->format == XYBitmap  &&  q->depth != 1) ||
	           (q->format != XYBitmap  &&  q->depth != draw.p->Depth)) {
		Bad(Match,, PutImage, /* q->depth */);
	
	} else if ((q->format == ZPixmap  &&  q->leftPad) ||
	           q->leftPad >= PADD_BITS) {
		Bad(Match,, PutImage, /* q->leftPad */);
	
	} else {
		GRECT r[2] = { {0, 0, q->width, q->height} };
		
		if (q->dstX < 0) { r[1].x = 0; r[0].w -= r[0].x = -q->dstX; }
		else             { r[1].x = q->dstX; }
		if (q->dstY < 0) { r[1].y = 0; r[0].h -= r[0].y = -q->dstY; }
		else             { r[1].y = q->dstY; }
		if (r[0].x + r[0].w > draw.p->W) r[0].w = draw.p->W - r[0].x;
		if (r[0].y + r[0].h > draw.p->H) r[0].h = draw.p->H - r[0].y;
		
		if (r[0].w > 0  &&  r[0].h > 0) {
			MFDB mfdb = { (q +1), q->width, q->height,
			              (q->width + q->leftPad + PADD_BITS -1) /16,
			              0, q->depth, 0,0,0 };
			
			r[0].x += q->leftPad;
			r[1].w =  r[0].w;
			r[1].h =  r[0].h;
			
			if (q->depth == 1) {   // all possible formats are matching
				
				DEBUG (PutImage, " '%s' %c:%lX G:%lX [%i+%i,%i/%u,%u*%u] = %lu\n"
				       "          [%i,%i/%i,%i] -> [%i,%i/%i,%i]",
				       (q->format == XYBitmap ? "Bitmap" :
				        q->format == XYPixmap ? "XYPixmap" :
				        q->format == ZPixmap  ? "ZPixmap"  : "???"),
				       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc,
				       q->dstX, q->leftPad, q->dstY, q->width, q->height, q->depth,
				       (q->length *4) - sizeof (xPutImageReq),
				       r[0].x,r[0].y,r[0].w,r[0].h, r[1].x,r[1].y,r[1].w,r[1].h);
				
				if (draw.p->isWind) WindPutMono (draw.Window, gc, r, &mfdb);
				else                PmapPutMono (draw.Pixmap, gc, r, &mfdb);
			
			} else if (q->format == XYPixmap) {
				
				PRINT (- X_PutImage, " '%s' %c:%lX G:%lX [%i+%i,%i/%u,%u*%u] = %lu\n"
				       "          [%i,%i/%i,%i] -> [%i,%i/%i,%i]",
				       "XYPixmap", (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc,
				       q->dstX, q->leftPad, q->dstY, q->width, q->height, q->depth,
				       (q->length *4) - sizeof (xPutImageReq),
				       r[0].x,r[0].y,r[0].w,r[0].h, r[1].x,r[1].y,r[1].w,r[1].h);
				
			} else if (q->depth == 16) {
				if (draw.p->isWind) WindPutColor (draw.Window, gc, r, &mfdb);
				else                PmapPutColor (draw.Pixmap, gc, r, &mfdb);
				
			} else if (!(mfdb.fd_addr
			               = malloc (mfdb.fd_wdwidth *2 * q->height * q->depth))) {
				printf ("Can't allocate buffer.\n");
				
			} else { // q->format == ZPixmap
				
				DEBUG (PutImage, " '%s' %c:%lX G:%lX [%i+%i,%i/%u,%u*%u] = %lu\n"
				       "          [%i,%i/%i,%i] -> [%i,%i/%i,%i]",
				       "ZPixmap", (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc,
				       q->dstX, q->leftPad, q->dstY, q->width, q->height, q->depth,
				       (q->length *4) - sizeof (xPutImageReq),
				       r[0].x,r[0].y,r[0].w,r[0].h, r[1].x,r[1].y,r[1].w,r[1].h);
				
				if (GRPH_Depth == 4) {
					GrphRasterI4 (mfdb.fd_addr, (char*)(q +1), q->width, q->height);
				} else if (GRPH_Format == SCRN_INTERLEAVED) {
					GrphRasterI8 (mfdb.fd_addr, (char*)(q +1), q->width, q->height);
				} else { // (GRPH_Format == SCRN_PACKEDPIXEL)
					GrphRasterP8 (mfdb.fd_addr, (char*)(q +1), q->width, q->height);
				}
				if (draw.p->isWind) WindPutColor (draw.Window, gc, r, &mfdb);
				else                PmapPutColor (draw.Pixmap, gc, r, &mfdb);
				
				free (mfdb.fd_addr);
			}
		
		} else {
			PRINT (- X_PutImage, " '%s' %c:%lX G:%lX [%i+%i,%i/%u,%u*%u] = %lu\n"
			       "          [%i,%i/%i,%i] -> [%i,%i/%i,%i]",
			       (q->format == XYBitmap ? "Bitmap" :
			        q->format == XYPixmap ? "XYPixmap" :
			        q->format == ZPixmap  ? "ZPixmap"  : "???"),
			       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc,
			       q->dstX, q->leftPad, q->dstY, q->width, q->height, q->depth,
			       (q->length *4) - sizeof (xPutImageReq),
			       r[0].x,r[0].y,r[0].w,r[0].h, r[1].x,r[1].y,r[1].w,r[1].h);
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_GetImage (CLIENT * clnt, xGetImageReq * q)
{
	PRINT (- X_GetImage," D:%lX [%i,%i/%u,%u] form=%i mask=%lu",
	       q->drawable, q->x,q->y, q->width, q->height, q->format, q->planeMask);
}


//------------------------------------------------------------------------------
void
RQ_CopyArea (CLIENT * clnt, xCopyAreaReq * q)
{
	p_DRAWABLE src_d = DrawFind(q->srcDrawable);
	p_DRAWABLE dst_d = DrawFind(q->dstDrawable);
	GC       * gc    = GcntFind(q->gc);
	
	if (!src_d.p) {
		Bad(Drawable, q->srcDrawable, CopyArea,);
		
	} else if (!dst_d.p) {
		Bad(Drawable, q->dstDrawable, CopyArea,);
		
	} else if (!gc) {
		Bad(GC, q->gc, CopyArea,);
	
	} else if (src_d.p->Depth != dst_d.p->Depth) {
		Bad(Match,, CopyArea,"(%c:%X,%c:%x):\n           depth %u != %u.",
		            (src_d.p->isWind ? 'W' : 'P'), src_d.p->Id,
		            (dst_d.p->isWind ? 'W' : 'P'), dst_d.p->Id,
		            src_d.p->Depth, dst_d.p->Depth);
	
	} else {
		BOOL debug = xTrue;
		GRECT r[4] = { {0,       0,       src_d.p->W,  src_d.p->H},
		               {0,       0,       dst_d.p->W,  dst_d.p->H},
		               {q->srcX, q->srcY, q->width +1, q->height +1},
		               {q->dstX, q->dstY, q->width +1, q->height +1} };
		
		if (GrphIntersect (&r[0], &r[2]) && GrphIntersect (&r[1], &r[3])) {
			if (r[0].w > r[1].w) r[0].w = r[1].w;
			else                 r[1].w = r[0].w;
			if (r[0].h > r[1].h) r[0].h = r[1].h;
			else                 r[1].h = r[0].h;
			
			if (src_d.p->isWind) {
			
			} else { // src_d is Pixmap
				MFDB * mfdb = PmapMFDB(src_d.Pixmap);
				
				DEBUG (CopyArea," G:%lX P:%lX [%i,%i/%u,%u] to %c:%lX (%i,%i)\n"
					    "          [%i,%i/%i,%i] -> [%i,%i/%i,%i]",
				       q->gc, q->srcDrawable,
				       q->srcX, q->srcY, q->width, q->height,
				       (dst_d.p->isWind ? 'W' : 'P'), q->dstDrawable,
				       q->dstX, q->dstY,
					    r[0].x,r[0].y,r[0].w,r[0].h, r[1].x,r[1].y,r[1].w,r[1].h);
				debug = xFalse;
				
				if (src_d.p->Depth == 1) {
					if (dst_d.p->isWind) WindPutMono (dst_d.Window, gc, r, mfdb);
					else                 PmapPutMono (dst_d.Pixmap, gc, r, mfdb);
				
				} else {
					if (dst_d.p->isWind) WindPutColor (dst_d.Window, gc, r, mfdb);
					else                 PmapPutColor (dst_d.Pixmap, gc, r, mfdb);
				}
			}
		}
		// Hacking: GraphicsExposure should be generated if necessary instead!
		//
		if (gc->GraphExpos) {
			EvntNoExposure (clnt, dst_d.p->Id, X_CopyArea);
		}
		if (debug) {
			PRINT (- X_CopyArea," G:%lX %c:%lX [%i,%i/%u,%u] to %c:%lX (%i,%i)\n"
				    "          [%i,%i/%i,%i] -> [%i,%i/%i,%i]  (%s)",
			       q->gc, (src_d.p->isWind ? 'W' : 'P'), q->srcDrawable,
			       q->srcX, q->srcY, q->width, q->height,
			       (dst_d.p->isWind ? 'W' : 'P'), q->dstDrawable,
			       q->dstX, q->dstY,
				    r[0].x,r[0].y,r[0].w,r[0].h, r[1].x,r[1].y,r[1].w,r[1].h,
				    (gc->GraphExpos ? "exp" : "-"));
		}
	}
}

//------------------------------------------------------------------------------
static int
_mask2plane (CARD32 mask, CARD16 depth)
{
	int plane = -1;
	
	if (mask) while (++plane < depth) {
		if (mask & 1) {
			mask &= ~1uL;
			break;
		}
		mask >>= 1;
	}
	return (mask ? -1 : plane);
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void
RQ_CopyPlane (CLIENT * clnt, xCopyPlaneReq * q)
{
	p_DRAWABLE src_d = DrawFind(q->srcDrawable);
	p_DRAWABLE dst_d = DrawFind(q->dstDrawable);
	GC       * gc    = GcntFind (q->gc);
	int        plane;
	
	if (!src_d.p) {
		Bad(Drawable, q->srcDrawable, CopyPlane,);
		
	} else if (!dst_d.p) {
		Bad(Drawable, q->dstDrawable, CopyPlane,);
		
	} else if (!gc) {
		Bad(GC, q->gc, CopyPlane,);
	
	} else if ((plane = _mask2plane(q->bitPlane, src_d.p->Depth)) < 0) {
		Bad(Value, q->bitPlane, CopyPlane,);
	
	} else {
		BOOL debug = xTrue;
		GRECT r[4] = { {0,       0,       src_d.p->W, src_d.p->H},
		               {0,       0,       dst_d.p->W, dst_d.p->H},
		               {q->srcX, q->srcY, q->width,   q->height},
		               {q->dstX, q->dstY, q->width,   q->height} };
		
		if (GrphIntersect (&r[0], &r[2]) && GrphIntersect (&r[1], &r[3])) {
			if (r[0].w > r[1].w) r[0].w = r[1].w;
			else                 r[1].w = r[0].w;
			if (r[0].h > r[1].h) r[0].h = r[1].h;
			else                 r[1].h = r[0].h;
			
			if (src_d.p->isWind) {
				//
				
			} else { // src_d is Pixmap
				MFDB mfdb = *PmapMFDB(src_d.Pixmap);
				
				DEBUG (CopyPlane," #%i(0x%lx)"
				       " G:%lX %c:%lX [%i,%i/%u,%u] to %c:%lX %i,%i",
				       plane, q->bitPlane, q->gc, (src_d.p->isWind ? 'W' : 'P'),
				       q->srcDrawable, q->srcX, q->srcY, q->width, q->height,
			   	    (dst_d.p->isWind ? 'W' : 'P'),
			   	    q->dstDrawable, q->dstX, q->dstY);
				
				if (src_d.p->Depth > 1) {
					if (plane) {
						mfdb.fd_addr = src_d.Pixmap->Mem
						           + src_d.Pixmap->nPads *2 * src_d.Pixmap->H * plane;
					}
					mfdb.fd_nplanes = 1;
				}
				if (dst_d.p->isWind) WindPutMono (dst_d.Window, gc, r, &mfdb);
				else                 PmapPutMono (dst_d.Pixmap, gc, r, &mfdb);
				debug = xFalse;
			}
			
		}
		// Hacking: GraphicsExposure should be generated if necessary instead!
		//
		if (gc->GraphExpos) {
			EvntNoExposure (clnt, dst_d.p->Id, X_CopyPlane);
		}
		if (debug) {
			PRINT (- X_CopyPlane," #%i(0x%lx)"
			       " G:%lX %c:%lX [%i,%i/%u,%u] to %c:%lX %i,%i",
			       plane, q->bitPlane, q->gc, (src_d.p->isWind ? 'W' : 'P'),
			       q->srcDrawable, q->srcX, q->srcY, q->width, q->height,
			       (dst_d.p->isWind ? 'W' : 'P'), q->dstDrawable, q->dstX, q->dstY);
		}
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
	
	} else if (len) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = 0;
		GRECT * sect, _clip;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				hdl = GRPH_Vdi;
				v_hide_c (hdl);
			}
			DEBUG (FillPoly," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			sect = SizeToRCT (&_clip, &draw.Pixmap->W);
			nClp = 1;
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsf_color (hdl, color);
			}
			do {
				vs_clip_r    (hdl, sect++);
				v_fillarea_p (hdl, len, pxy);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
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
RQ_PolyLine (CLIENT * clnt, xPolyLineReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyLineReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyLine,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyLine,);
	
	} else if (len) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = 0;
		GRECT * sect, _clip;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				hdl = GRPH_Vdi;
				v_hide_c (hdl);
			}
			DEBUG (PolyLine," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			sect = SizeToRCT (&_clip, &draw.Pixmap->W);
			nClp = 1;
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
				vs_clip_r (hdl, sect++);
				v_pline_p (hdl, len, pxy);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
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
	
	} else if (len) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = 0;
		GRECT * sect, _clip;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				hdl = GRPH_Vdi;
				v_hide_c (hdl);
			}
			DEBUG (PolyPoint," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			if (draw.p->Depth == 1) {
				PmapDrawPoints (draw.Pixmap, gc, pxy, len);
				nClp = 0;
			
			} else {
				sect = SizeToRCT (&_clip, &draw.Pixmap->W);
				nClp = 1;
				set = (draw.Pixmap->Vdi > 0);
				hdl = PmapVdi (draw.Pixmap, gc, xFalse);
			}
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsm_color (hdl, color);
			}
			do {
				vs_clip_r   (hdl, sect++);
				v_pmarker_p (hdl, len, pxy);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
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
	
	} else if (len) {
		BOOL    set = xTrue;
		GRECT * rec = (GRECT*)(q +1);
		short   hdl = 0;
		GRECT * sect, _clip;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
				clnt->Fnct->shift_r2p (&orig, rec, len);
				hdl = GRPH_Vdi;
				v_hide_c (hdl);
			}
			DEBUG (PolyFillRectangle," P:%lX G:%lX (%lu)",
			       q->drawable, q->gc, len);
		
		} else { // Pixmap
			size_t  num = len;
			GRECT * rct = rec;
			if (draw.p->Depth == 1) {
				if (clnt->DoSwap) {
					while (num--) {
						SwapRCT(rct, rct);
						rct++;
					}
				}
				PmapFillRects (draw.Pixmap, gc, rec, len);
				nClp = 0;
			
			} else {
				sect = SizeToRCT (&_clip, &draw.Pixmap->W);
				nClp = 1;
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
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; v_bar_p (hdl, (PXY*)&rec[i++]));
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
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
	
	} else if (len) {
		BOOL    set = xTrue;
		GRECT * rec = (GRECT*)(q +1);
		short   hdl = 0;
		PXY     orig;
		GRECT * sect, _clip;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
				hdl = GRPH_Vdi;
				v_hide_c (hdl);
			}
			DEBUG (PolyRectangle," P:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			orig.x = 0;
			orig.y = 0;
			sect = SizeToRCT (&_clip, &draw.Pixmap->W);
			nClp = 1;
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
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; ++i) {
					PXY p[5];
					p[0].x = p[3].x = p[4].x = rec[i].x + orig.x;
					p[1].x = p[2].x = p[0].x + rec[i].w;
					p[0].y = p[1].y          = rec[i].y + orig.y;
					p[2].y = p[3].y = p[0].y + rec[i].h;
					p[4].y          = p[0].y + d;
					v_pline_p (hdl, 5, p);
				}
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
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
	
	} else if (len) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = 0;
		GRECT * sect, _clip;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, CoordModeOrigin);
				hdl = GRPH_Vdi;
				v_hide_c (hdl);
			}
			DEBUG (PolySegment," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			sect = SizeToRCT (&_clip, &draw.Pixmap->W);
			nClp = 1;
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
				int i;
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; i += 2) {
					v_pline_p (hdl, 2, &pxy[i]);
				}
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
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
	short   arr[len];
	short   hdl = 0;
	GRECT * sect, _clip;
	PXY     orig;
	CARD16  nClp;
	
	if (draw.p->isWind) {
		if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
			int dmy;
			orig.x += pos->x;
			orig.y += pos->y;
			hdl = GRPH_Vdi;
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
		orig.x = pos->x;
		orig.y = pos->y;
		sect = SizeToRCT (&_clip, &draw.Pixmap->W);
		nClp = 1;
		hdl  = PmapVdi (draw.Pixmap, gc, xTrue);
	}
	if (nClp) {
		if (is8N16) FontLatin1_C (arr, text, len);
		else        FontLatin1_W (arr, text, len);
		do {
			vs_clip_r (hdl, sect++);
			if (gc->Background == WHITE) {
				vswr_mode   (hdl, MD_REPLACE);
			} else {
				vswr_mode   (hdl, MD_ERASE);
				vst_color   (hdl, gc->Background);
				v_gtext_arr (hdl, &orig, len, arr);
				vswr_mode   (hdl, MD_TRANS);
			}
			vst_color   (hdl, gc->Foreground);
			v_gtext_arr (hdl, &orig, len, arr);
		} while (--nClp);
		vs_clip_r (hdl, NULL);
		
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
	
	} else if (q->nChars) {
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
	
	} else if (q->nChars) {
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
		short   arr[t->len];
		short   hdl = 0;
		GRECT * sect, _clip;
		PXY     orig;
		CARD16  nClp;
		
		if (draw.p->isWind) {
			if ((nClp = WindClipLock (draw.Window, 0, NULL,0, &orig, &sect))) {
				int dmy;
				orig.x += pos->x;
				orig.y += pos->y;
				hdl = GRPH_Vdi;
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
			orig.x = pos->x;
			orig.y = pos->y;
			sect = SizeToRCT (&_clip, &draw.Pixmap->W);
			nClp = 1;
			hdl  = PmapVdi (draw.Pixmap, gc, xTrue);
		}
		if (nClp) {
			if (is8N16) FontLatin1_C (arr,  (char*)(t +1), t->len);
			else        FontLatin1_W (arr, (short*)(t +1), t->len);
			vswr_mode (hdl, MD_TRANS);
			do {
				vs_clip_r (hdl, sect++);
				v_gtext_arr (hdl, &orig, t->len, arr);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
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
	
	} else {
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
	
	} else {
		DEBUG (PolyText16," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Poly_Text (draw, gc, xFalse, (xTextElt*)(q +1), (PXY*)&q->x);
	}
}
