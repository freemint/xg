//==============================================================================
//
// draw_img.c
//
// Copyright (C) 2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2001-02-03 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "clnt.h"
#include "pixmap.h"
#include "window.h"
#include "event.h"
#include "gcontext.h"
#include "grph.h"
#include "x_gem.h"

#include <stdlib.h>
#include <stdio.h>

#include <X11/X.h>


//==============================================================================
//
// Callback Functions

#include "Request.h"

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
	
	} else if ((short)q->width <= 0  ||  (short)q->height <= 0) {
		Bad(Value, (short)((short)q->width <= 0 ? q->width : q->height),
		           PutImage," width = %i height = %i",
		           (short)q->width, (short)q->height);
	
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
	//
	// fully faked atm
	//
	
	p_DRAWABLE draw = DrawFind(q->drawable);
	
	ClntReplyPtr (GetImage, r);
	
	size_t size = (q->width * draw.p->Depth +7) /8 * q->height;
	
	PRINT (- X_GetImage,
	       " D:%lX [%i,%i/%u,%u] form=%i mask=%lX -> dpth = %i size = %lu",
	       q->drawable, q->x,q->y, q->width, q->height, q->format, q->planeMask,
	       draw.p->Depth, size);
	
	r->depth  = draw.p->Depth;
	r->visual = DFLT_VISUAL;
	
	ClntReply (GetImage, size, NULL);
}


//------------------------------------------------------------------------------
static int
copy_clip (GRECT * r, p_DRAWABLE src, p_DRAWABLE dst, short * coord)
{
	// coord[0,1] src x/y   r[0]   src clip
	// coord[2,3] dst x/y   r[1]   dst clip
	// coord[4,5] w/h       r[2..] dst to be tiled
	int     num  = -1;   // -1: no copy, 0: no tiles, 1..4: num of tiles in r
	GRECT * tile = r +2;
	short   w    = coord[4];
	short   h    = coord[5];
	*(PXY*)&r[0] = *(PXY*)(coord +0); // src
	*(PXY*)&r[1] = *(PXY*)(coord +2); // dst
	
	if (r[1].x     < 0)        { r[0].x -= r[1].x; w += r[1].x;   r[1].x = 0; }
	if (r[1].x + w > dst.p->W)                     w = dst.p->W - r[1].x;
	if (r[1].y     < 0)        { r[0].y -= r[1].y; h += r[1].y;   r[1].y = 0; }
	if (r[1].y + h > dst.p->H)                     h = dst.p->H - r[1].y;
	
	do {
		if (w > 0 && h > 0) {
			num++;
		} else {
			break;
		}
		if (r[0].y < 0) { // above src
			num++;
			tile->x = r[1].x;
			tile->y = r[1].y;
			tile->w = w;
			tile->h = -r[0].y;
			tile++;
			r[1].y -= r[0].y;
			h      += r[0].y;
			r[0].y =  0;
			if (h <= 0) break;
		}
		if (r[0].x < 0) { // left of src
			num++;
			tile->x = r[1].x;
			tile->y = r[1].y;
			tile->w = -r[0].x;
			tile->h = h;
			tile++;
			r[1].x -= r[0].x;
			w      += r[0].x;
			r[0].x =  0;
			if (w <= 0) break;
		}
		if (r[0].x + w > src.p->W) { // right of src
			short d = src.p->W - r[0].x;
			num++;
			tile->x = r[1].x + d;
			tile->y = r[1].y;
			tile->w = w - d;
			tile->h = h;
			tile++;
			w = d;
			if (w <= 0) break;
		}
		if (r[0].y + h > src.p->H) { // below src
			short d = src.p->H - r[0].y;
			num++;
			tile->x = r[1].x;
			tile->y = r[1].y + d;
			tile->w = w;
			tile->h = h - d;
			tile++;
			h = d;
		}
		
	} while (0);
	
	r[0].w = r[1].w = w;
	r[0].h = r[1].h = h;
	
	return num;
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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
	
//	} else if (src_d.p->Depth != dst_d.p->Depth) {
//		Bad(Match,, CopyArea,"(%c:%X,%c:%X):\n           depth %u != %u.",
//		            (src_d.p->isWind ? 'W' : 'P'), src_d.p->Id,
//		            (dst_d.p->isWind ? 'W' : 'P'), dst_d.p->Id,
//		            src_d.p->Depth, dst_d.p->Depth);
	
	} else if ((short)q->width <= 0  ||  (short)q->height <= 0) {
		Bad(Value, (short)((short)q->width <= 0 ? q->width : q->height),
		           CopyArea," width = %i height = %i",
		           (short)q->width, (short)q->height);
	
	} else {
		BOOL debug = xTrue;
		BOOL  no_exp = gc->GraphExpos;
		GRECT r[6], * exps = r +2;
		int   num = copy_clip (r, src_d, dst_d, &q->srcX);
		
		if (num >= 0  &&  r->w > 0  &&  r->h > 0) {
			
			if (dst_d.p->isWind) {
				WINDOW * wind = dst_d.Window;
				if (no_exp) {
					exps--;
					num++;
				}
				if (wind->hasBackGnd) {
					PRECT * sect, area;
					PXY     orig;
					CARD16  nClp = WindClipLockP (wind, 0, NULL, 0, &orig, &sect);
					if (nClp) {
						if (!wind->hasBackPix) {
							vswr_mode (GRPH_Vdi, MD_REPLACE);
							vsf_color (GRPH_Vdi, wind->Back.Pixel);
						}
						area.rd.x = (area.lu.x = orig.x + q->dstX) + q->width  -1;
						area.rd.y = (area.lu.y = orig.y + q->dstY) + q->height -1;
						WindDrawBgnd (wind, orig, &area, sect, nClp, NULL);
						WindClipOff();
					} else {
						num = 0;
					}
				}
			}
			if (no_exp && num) {
				EvntGraphExp (clnt, dst_d, X_CopyArea, num, exps);
				no_exp = xFalse;
			}
			
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
		if (no_exp) {
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
		Bad(Value, q->bitPlane, CopyPlane," bit-plane = %lX", q->bitPlane);
	
	} else if ((short)q->width <= 0  ||  (short)q->height <= 0) {
		Bad(Value, (short)((short)q->width <= 0 ? q->width : q->height),
		           CopyPlane," width = %i height = %i",
		           (short)q->width, (short)q->height);
	
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
		if (debug) {
			PRINT (- X_CopyPlane," #%i(0x%lx)"
			       " G:%lX %c:%lX [%i,%i/%u,%u] to %c:%lX %i,%i",
			       plane, q->bitPlane, q->gc, (src_d.p->isWind ? 'W' : 'P'),
			       q->srcDrawable, q->srcX, q->srcY, q->width, q->height,
			       (dst_d.p->isWind ? 'W' : 'P'), q->dstDrawable, q->dstX, q->dstY);
		}
		/*
		 *  Hacking: GraphicsExposure should be generated if necessary instead!
		 */
		if (gc->GraphExpos) {
			EvntNoExposure (clnt, dst_d.p->Id, X_CopyPlane);
		}
	}
}
