//==============================================================================
//
// wind_draw.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-07 - Module released for beta state.
// 2000-09-19 - Initial Version.
//==============================================================================
//
#include "window_P.h"
#include "event.h"
#include "gcontext.h"
#include "grph.h"
#include "pixmap.h"
#include "wmgr.h"
#include "x_gem.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h> // printf

#include <X11/Xproto.h>


//==============================================================================
void
WindClipOff (void)
{
	WindUpdate (xFalse);
}

//==============================================================================
CARD16
WindClipLock (WINDOW * wind, CARD16 border, const GRECT * clip, short n_clip,
              PXY * orig, GRECT ** pBuf)
{
	PRECT * p;
	CARD16  nClp = WindClipLockP (wind, border, clip, n_clip, orig, &p);
	if (nClp) {
		CARD16 n = nClp;
		*pBuf    = (GRECT*)p;
		do {
			p->rd.x -= p->lu.x -1;
			p->rd.y -= p->lu.y -1;
			p++;
		} while (--n);
	}
	return nClp;	
}

//==============================================================================
CARD16
WindClipLockP (WINDOW * wind, CARD16 border, const GRECT * clip, short n_clip,
               PXY * orig, PRECT ** pBuf)
{
	WINDOW * pwnd;
	GRECT    work = wind->Rect, rect;
	BOOL     visb = wind->isMapped;
	CARD16   nClp = 0;
	PRECT  * sect, * p_clip;
	short    a, b, n;
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
		PRECT * c = p_clip = alloca (sizeof(PRECT) * n_clip);
		n         = 0;
		while (n_clip--) {
			c->rd.x = (c->lu.x = clip->x + orig->x) + clip->w -1;
			c->rd.y = (c->lu.y = clip->y + orig->y) + clip->h -1;
			clip++;
			if (GrphIntersectP (c, (PRECT*)&work)) {
				c++;
				n++;
			}
		}
		if (!(n_clip = n)) return 0;
	
	} else {
		p_clip = (PRECT*)&work;
		if (clip  &&  n_clip < 0) {
			PRECT c;
			c.rd.x = (c.lu.x = clip->x) + clip->w -1;
			c.rd.y = (c.lu.y = clip->y) + clip->h -1;
			if (!GrphIntersectP (p_clip, &c)) return 0;
		}
		n_clip = 1;
	}
	
	WindUpdate (xTrue);
	wind_get (0, WF_SCREEN, &a, &b, &n,&n);
	*pBuf = sect = (PRECT*)(((long)a << 16) | (b & 0xFFFF));
	
	wind_get_first (wind->Handle, &rect);
	while (rect.w > 0  &&  rect.h > 0) {
		PRECT * c = p_clip;
		n         = n_clip;
		rect.w   += rect.x -1;
		rect.h   += rect.y -1;
		do {
			*sect = *(PRECT*)&rect;
			if (GrphIntersectP (sect, c)) {
				if (l > sect->lu.x) l = sect->lu.x;
				if (u > sect->lu.y) u = sect->lu.y;
				if (r < sect->rd.x) r = sect->rd.x;
				if (d < sect->rd.y) d = sect->rd.y;
				sect++;
				nClp++;
			}
			c++;
		} while (--n);
		wind_get_next (wind->Handle, &rect);
	}
	if (nClp) {
		sect->lu.x = l;
		sect->lu.y = u;
		sect->rd.x = r;
		sect->rd.y = d;
	
	} else {
		WindUpdate (xFalse);
	}
	return nClp;
}


//==============================================================================
void
WindDrawPmap (PIXMAP * pmap, PXY orig, p_PRECT sect)
{
	MFDB  scrn = { NULL, };
	PXY   offs = { sect->lu.x - orig.x, sect->lu.y - orig.y };
	short rd_x = pmap->W -1, rd_y = pmap->H -1;
	short color[2] = { G_BLACK, G_WHITE };
	PXY   pxy[4];
	#define s_lu pxy[0]
	#define s_rd pxy[1]
	#define d_lu pxy[2]
	#define d_rd pxy[3]
	int  d;
	
	if (offs.x >= pmap->W) offs.x %= pmap->W;
	if (offs.y >= pmap->H) offs.y %= pmap->H;
	
	s_lu.y = offs.y;
	s_rd.y = rd_y;
	d_lu.y = sect->lu.y;
	d_rd   = sect->rd;
	while ((d = d_rd.y - d_lu.y) >= 0) {
		if (d < rd_y - s_lu.y) {
			s_rd.y = s_lu.y + d;
		}
		s_lu.x = offs.x;
		s_rd.x = rd_x;
		d_lu.x = sect->lu.x;
		while ((d = d_rd.x - d_lu.x) >= 0) {
			if (d < rd_x - s_lu.x) {
				s_rd.x = s_lu.x + d;
			}
			if (pmap->Depth == 1) {
				vrt_cpyfm_p (GRPH_Vdi, MD_REPLACE,
				             pxy, PmapMFDB(pmap), &scrn, color);
			} else {
				vro_cpyfm_p (GRPH_Vdi, S_ONLY, pxy, PmapMFDB(pmap), &scrn);
			}
			d_lu.x += pmap->W - s_lu.x;
			s_lu.x =  0;
		}
		d_lu.y += pmap->H - s_lu.y;
		s_lu.y =  0;
	}
	#undef s_lu
	#undef s_rd
	#undef d_lu
	#undef d_rd
}

//------------------------------------------------------------------------------
int
WindDrawBgnd (WINDOW * wind, PXY orig, PRECT * area,
              PRECT * sect, int num, GRECT * exps)
{
	int cnt = 0;
	
	v_hide_c (GRPH_Vdi);
	
	if (wind->hasBackPix) {
		do {
			PRECT rect = *area;
			if (GrphIntersectP (&rect, sect)) {
				WindDrawPmap (wind->Back.Pixmap, orig, &rect);
				if (exps) {
					exps->w = rect.rd.x - rect.lu.x +1;
					exps->h = rect.rd.y - rect.lu.y +1;
					exps->x = rect.lu.x - orig.x;
					exps->y = rect.lu.y - orig.y;
					exps++;
				}
				cnt++;
			}
			sect++;
		} while (--num);
	
	} else {
		do {
			PRECT rect = *area;
			if (GrphIntersectP (&rect, sect)) {
				v_bar_p (GRPH_Vdi, &rect.lu);
				if (exps) {
					exps->w = rect.rd.x - rect.lu.x +1;
					exps->h = rect.rd.y - rect.lu.y +1;
					exps->x = rect.lu.x - orig.x;
					exps->y = rect.lu.y - orig.y;
					exps++;
				}
				cnt++;
			}
			sect++;
		} while (--num);
	}
	v_show_c (GRPH_Vdi, 1);
	
	return cnt;
}

//------------------------------------------------------------------------------
static void
draw_brdr (WINDOW * wind, PRECT * work, PRECT * area, PRECT * sect, int num)
{
	int   i = wind->BorderWidth;
	short l = work->lu.x - i;
	short r = work->rd.x + i;
	int   n = 0;
	PRECT brdr[4];
	
	brdr[0].lu.x = l;
	brdr[0].lu.y = work->lu.y - i;
	brdr[0].rd.x = r;
	brdr[0].rd.y = work->lu.y - 1;
	if (GrphIntersectP (brdr, area)) n++;
	brdr[n].lu.x = l;
	brdr[n].lu.y = work->lu.y;
	brdr[n].rd.x = work->lu.x - 1;
	brdr[n].rd.y = work->rd.y;
	if (GrphIntersectP (brdr + n, area)) n++;
	brdr[n].lu.x = work->rd.x + 1;
	brdr[n].lu.y = work->lu.y;
	brdr[n].rd.x = work->rd.x + i;
	brdr[n].rd.y = work->rd.y;
	if (GrphIntersectP (brdr + n, area)) n++;
	brdr[n].lu.x = l;
	brdr[n].lu.y = work->rd.y + 1;
	brdr[n].rd.x = r;
	brdr[n].rd.y = work->rd.y + i;
	if (GrphIntersectP (brdr + n, area)) n++;
	else if (!n)                          return;
	
	vsf_color (GRPH_Vdi, wind->BorderPixel);
	v_hide_c  (GRPH_Vdi);
	do {
		vs_clip_p (GRPH_Vdi, (PXY*)sect++);
		for (i = 0; i < n; v_bar_p (GRPH_Vdi, &brdr[i++].lu));
	} while (--num);
	v_show_c    (GRPH_Vdi, 1);
	vs_clip_off (GRPH_Vdi);
}

//------------------------------------------------------------------------------
static void
draw_deco (PRECT * work, PRECT * area, PRECT * sect, int num)
{
	BOOL  lft = xFalse, rgt = xFalse;
	int   n = 0, i;
	PRECT brdr[3];
	PXY   pxy[4], l_a[3], l_b[2], l_c[2], l_d[3], r_a[5], r_b[3];
	short deco_size = (WMGR_Decor *3) /2;
	
	brdr[0].lu.x = 0;
	brdr[0].lu.y = 0;
	brdr[0].rd.x = work->lu.x -1;
	brdr[0].rd.y = work->rd.y;
	if (GrphIntersectP (brdr, area)) {
		if (brdr[0].rd.y >= work->rd.y - deco_size) lft = xTrue;
		n++;
	}
	brdr[n].lu.x = work->rd.x + 1;
	brdr[n].lu.y = 0;
	brdr[n].rd.x = 0x7FFF;
	brdr[n].rd.y = work->rd.y;
	if (GrphIntersectP (brdr + n, area)) {
		if (brdr[n].rd.y >= work->rd.y - deco_size) rgt = xTrue;
		n++;
	}
	brdr[n].lu.x = 0;
	brdr[n].lu.y = work->rd.y +1;
	brdr[n].rd.x = 0x7FFF;
	brdr[n].rd.y = 0x7FFF;
	if (GrphIntersectP (brdr + n, area)) {
		if (!lft && brdr[n].lu.x <= work->lu.x + deco_size) lft = xTrue;
		if (!rgt && brdr[n].rd.x >= work->rd.x - deco_size) rgt = xTrue;
		n++;
	} else if (!n) {
		return;
	}
	
	if (lft) {
		l_a[0].x = l_a[1].x = work->lu.x - WMGR_Decor;
		l_a[2].x =            work->lu.x              -3;
		l_a[0].y =            work->rd.y + WMGR_Decor -1;
		l_a[1].y = l_a[2].y = work->rd.y - deco_size;
		l_b[0].x =            work->lu.x              -1;
		l_b[1].x =            work->lu.x + deco_size  -1;
		l_b[0].y = l_b[1].y = work->rd.y              +2;
		l_c[0].x = l_c[1].x = work->lu.x              -2;
		l_c[0].y =            work->rd.y - deco_size  +1;
		l_c[1].y =            work->rd.y              +1;
		l_d[0].x =            work->lu.x - WMGR_Decor +1;
		l_d[1].x = l_d[2].x = work->lu.x + deco_size;
		l_d[0].y = l_d[1].y = work->rd.y + WMGR_Decor;
		l_d[2].y =            work->rd.y              +3;
	}
	if (rgt) {
		r_a[0].x = r_a[1].x = work->rd.x - deco_size;
		r_a[2].x = r_a[3].x = work->rd.x              +2;
		r_a[4].x =            work->rd.x + WMGR_Decor -1;
		r_a[0].y =            work->rd.y + WMGR_Decor -1;
		r_a[1].y = r_a[2].y = work->rd.y              +2;
		r_a[3].y = r_a[4].y = work->rd.y - deco_size;
		r_b[0].x =            work->rd.x - deco_size  +1;
		r_b[1].x = r_b[2].x = work->rd.x + WMGR_Decor;
		r_b[0].y = r_b[1].y = work->rd.y + WMGR_Decor;
		r_b[2].y =            work->rd.y - deco_size  +1;
	}
	pxy[0].x = pxy[1].x = work->lu.x -1;
	pxy[2].x = pxy[3].x = work->rd.x +1;
	pxy[0].y = pxy[3].y = work->lu.y;
	pxy[1].y = pxy[2].y = work->rd.y +1;
	
	vsf_color (GRPH_Vdi, G_LWHITE);
	vsl_color (GRPH_Vdi, G_LBLACK);
	v_hide_c  (GRPH_Vdi);
	do {
		vs_clip_p (GRPH_Vdi, (PXY*)sect++);
		for (i = 0; i < n; v_bar_p (GRPH_Vdi, &brdr[i++].lu));
		v_pline_p (GRPH_Vdi, 4, pxy);
		if (lft || rgt) {
			vsl_color (GRPH_Vdi, G_WHITE);
			if (lft) {
				v_pline_p (GRPH_Vdi, sizeof(l_a) / sizeof(PXY), l_a);
				v_pline_p (GRPH_Vdi, sizeof(l_b) / sizeof(PXY), l_b);
			}
			if (rgt) {
				v_pline_p (GRPH_Vdi, sizeof(r_a) / sizeof(PXY), r_a);
			}
			vsl_color (GRPH_Vdi, G_LBLACK);
			if (lft) {
				v_pline_p (GRPH_Vdi, sizeof(l_c) / sizeof(PXY), l_c);
				v_pline_p (GRPH_Vdi, sizeof(l_d) / sizeof(PXY), l_d);
			}
			if (rgt) {
				v_pline_p (GRPH_Vdi, sizeof(r_b) / sizeof(PXY), r_b);
			}
		}
	} while (--num);
	v_show_c    (GRPH_Vdi, 1);
	vs_clip_off (GRPH_Vdi);
}

//------------------------------------------------------------------------------
static BOOL
draw_wind (WINDOW * wind, PRECT * work,
           PRECT * area, PRECT * sect, int nClp)
{
	GRECT * exps = (GRECT*)(wind->u.List.AllMasks & ExposureMask
	                        ? area +2 : NULL);
	PXY orig = work->lu;
	int nEvn = 0;
	
	if (GrphIntersectP (work, area)) {
		if (wind->hasBackGnd) {
			if (!wind->hasBackPix) {
				vsf_color (GRPH_Vdi, wind->Back.Pixel);
			}
			nEvn = WindDrawBgnd (wind, orig, work, sect, nClp, exps);
		
		} else if (exps) {
			do {
				*exps = *(GRECT*)area;
				if (GrphIntersectP ((PRECT*)exps, sect)) {
					exps->w -= exps->x -1;
					exps->h -= exps->y -1;
					exps->x -= orig.x;
					exps->y -= orig.y;
					exps++;
					nEvn++;
				}
				sect++;
			} while (--nClp);
			exps -= nEvn;
		}
		if (nEvn && exps) EvntExpose (wind, nEvn, exps);
	}
	return (nEvn > 0);
}

//==============================================================================
void
WindDrawSection (WINDOW * wind, const GRECT * clip)
{
	BOOL    enter = xTrue;
	int     level = 0;
	PXY     base;
	PRECT * sect, * area;
	short   nClp;
	
	vswr_mode (GRPH_Vdi, MD_REPLACE);
	
	if (wind->GwmDecor) {
		if ((nClp = WindClipLockP (wind, WMGR_Decor, clip, -1, &base, &sect))) {
			PRECT work = { base, {
			               base.x + wind->Rect.w -1, base.y + wind->Rect.h -1 } };
			area = sect + nClp;
			draw_deco (&work, area, sect, nClp);
			if (draw_wind (wind, &work, area, sect, nClp)
			     && (wind = wind->StackBot)) {
				*area = work;
				level = 1;
			} else {
				WindClipOff();
				nClp = 0;
			}
		}
	} else {
		nClp  = WindClipLockP (wind, wind->BorderWidth, clip, -1, &base, &sect);
		level = 0;
		area  = sect + nClp;
		base.x -= wind->Rect.x;
		base.y -= wind->Rect.y;
	}
	if (!nClp) return;
	
	do {
		if (enter && wind->isMapped) {
			PXY     orig = { base.x + wind->Rect.x, base.y + wind->Rect.y };
			PRECT * work = area +1;
			work->lu   = orig;
			work->rd.x = orig.x + wind->Rect.w -1;
			work->rd.y = orig.y + wind->Rect.h -1;
			if (wind->ClassInOut) {
				if (wind->hasBorder && wind->BorderWidth) {
					draw_brdr (wind, work, area, sect, nClp);
				}
				enter = draw_wind (wind, work, area, sect, nClp);
			
			} else { // InputOnly
				enter = GrphIntersectP (work, area);
			}
			if (enter) {
				if (wind->StackBot) {
					level++;
					area = work;
					base = orig;
					wind = wind->StackBot;
					continue;
				}
			}
		}
		if (level) {
			if (wind->NextSibl) {
				enter = xTrue;
				wind  = wind->NextSibl;
			} else if (--level) {
				area--;
				enter  =  xFalse;
				wind   =  wind->Parent;
				base.x -= wind->Rect.x;
				base.y -= wind->Rect.y;
			}
		}
	} while (level);
	
	WindClipOff();
}


//==============================================================================
void
WindPutMono (p_WINDOW wind, p_GC gc, p_GRECT r, p_MFDB src)
{
	if (WindVisible (wind)) {
		PXY pos;
		short hdl = WindOrigin (wind, &pos);
		MFDB  dst = { NULL };
		GRECT sec;
		short colors[2] = { gc->Foreground, gc->Background };
		
		r[1].x += pos.x;
		r[1].y += pos.y;
		
		WindUpdate (xTrue);
		wind_get_first (hdl, &sec);
		while (sec.w && sec.h) {
			if (GrphIntersect (&sec, &r[1])) {
				short mode = (gc->Function == GXor ? MD_TRANS : MD_REPLACE);
				PXY p[4] = { {r[0].x + (sec.x - r[1].x), r[0].y + (sec.y - r[1].y)},
				             *(PXY*)&sec.w, *(PXY*)&sec.x,
				             {sec.x + sec.w -1,          sec.y + sec.h -1} };
				p[1].x += p[0].x -1;
				p[1].y += p[0].y -1;
				v_hide_c  (GRPH_Vdi);
				vrt_cpyfm_p (GRPH_Vdi, mode, p, src, &dst, colors);
				v_show_c  (GRPH_Vdi, 1);
			}
			wind_get_next (hdl, &sec);
		}
		WindUpdate (xFalse);
	}
}

//==============================================================================
void
WindPutColor (p_WINDOW wind, p_GC gc, p_GRECT r, p_MFDB src)
{
	if (WindVisible (wind)) {
		PXY pos;
		short hdl = WindOrigin (wind, &pos);
		MFDB  dst = { NULL };
		GRECT sec;
		
		r[1].x += pos.x;
		r[1].y += pos.y;
		
		WindUpdate (xTrue);
		wind_get_first (hdl, &sec);
		while (sec.w && sec.h) {
			if (GrphIntersect (&sec, &r[1])) {
				PXY p[4] = { {r[0].x + (sec.x - r[1].x), r[0].y + (sec.y - r[1].y)},
				             *(PXY*)&sec.w, *(PXY*)&sec.x,
				             {sec.x + sec.w -1,          sec.y + sec.h -1} };
				p[1].x += p[0].x -1;
				p[1].y += p[0].y -1;
				v_hide_c  (GRPH_Vdi);
				vro_cpyfm_p (GRPH_Vdi, gc->Function, p, src, &dst);
				v_show_c  (GRPH_Vdi, 1);
			}
			wind_get_next (hdl, &sec);
		}
		WindUpdate (xFalse);
	}
}


//------------------------------------------------------------------------------
static CARD16
clip_children (WINDOW * wind, PXY orig, PRECT * dst, PRECT * clip)
{
	CARD16 num = 0, cnt = 0, i;
	
	wind = wind->StackBot;
	while (wind) {
		if (wind->isMapped) {
			register int b = wind->BorderWidth;
			dst->rd.x = (dst->lu.x = orig.x + wind->Rect.x) + wind->Rect.w -1;
			dst->rd.y = (dst->lu.y = orig.y + wind->Rect.y) + wind->Rect.h -1;
			if (b) {
				dst->lu.x -= b;
				dst->lu.y -= b;
				dst->rd.x += b;
				dst->rd.y += b;
			}
			if (GrphIntersectP (dst, clip)) {
				dst++;
				num++;
			}
		}
		wind = wind->NextSibl;
	}
	if (num) {
		short beg, end, d;
		PRECT ** list = (PRECT**)dst;
		PRECT  * area = (PRECT*)(list + num);

		// sort child geometries into list, first by y, then by x
		
		list[0] = dst -= num;
		for (i = 1; i < num; i++) {
			int j = i;
			dst++;
			while (j > 0  &&  list[j-1]->lu.y > dst->lu.y) {
				list[j] = list[j-1];
				j--;
			}
			while (--j >= 0
			       &&  list[j]->lu.y == dst->lu.y  &&  list[j]->lu.x > dst->lu.x) {
				list[j+1] = list[j];
			}
			list[j+1] = dst;
		}
		
		// find free rectangles between child geometries
					
		beg = 0;
		d   = clip->lu.y;
		do {
			short x = clip->lu.x;
			short u = d;
			if (u < list[beg]->lu.y) { // found area above first child geometry
				area->lu.x = x;
				area->lu.y = u;
				area->rd.x = clip->rd.x;
				area->rd.y = (u = list[beg]->lu.y) -1;
				area++;
				cnt++;
			}
			d = list[beg]->rd.y;
			
			for (end = beg +1; end < num; ++end) {
				if (list[end]->lu.y > u) {
					if (list[end]->lu.y <= d) d = list[end]->lu.y -1;
					break;
				} else if (list[end]->rd.y < d) {
					d = list[end]->rd.y;
				}
			}
			i = beg;
			while (i < end) {
				if (x < list[i]->lu.x) { // free area on left side of child
					area->lu.x = x;
					area->lu.y = u;
					area->rd.x = list[i]->lu.x -1;
					area->rd.y = d;
					area++;
					cnt++;
				}
				if (x <= list[i]->rd.x) {
					x = list[i]->rd.x +1;
				}
				if (list[i]->rd.y > d) i++;
				else if (i == beg)     beg = ++i;
				else {
					short j = i;
					while (++j < num) list[j-1] = list[j];
					if (--num < end)  end = num;
				}
			}
			if (x <= clip->rd.x) { // free area on right side of last child
				area->lu.x = x;
				area->lu.y = u;
				area->rd.x = clip->rd.x;
				area->rd.y = d;
				area++;
				cnt++;
			}
			d++;
			
			if (i > beg) {
				while (i < num  &&  list[i]->lu.y == d
				       &&  list[i]->lu.x < list[i-1]->lu.x) {
					short   j    = i;
					PRECT * save = list[i];
					do {
						list[j] = list[j-1];
					} while (--j > beg  &&  save->lu.x < list[j]->lu.x);
					list[j] = save;
				}
				i++;
			}
		} while (beg < num);
		
		if (d <= clip->rd.y) { // free area at the bottom
			area->lu.x = clip->lu.x;
			area->lu.y = d;
			area->rd   = clip->rd;
			area++;
			cnt++;
		}
		
		if (cnt) {
			memmove (dst - num, area - cnt, sizeof(PRECT) * cnt);
		}
	
	} else { // no children
		*dst = *clip;
		cnt  = 1;
	}
	
	return cnt;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_ClearArea (CLIENT * clnt, xClearAreaReq * q)
{
	// Fill the window's area with it's background.
	//
	// BOOL   exposures:
	// Window window:
	// INT16  x, y:
	// CARD16 width, height:
	//...........................................................................
	
	WINDOW * wind = WindFind (q->window);
	BOOL     evn;
	
	if (!wind) {
		Bad(Window, q->window, ClearArea,);
	
	} else if (!wind->ClassInOut) {
		Bad(Match,, ClearArea,"(W:%X) class InputOnly", wind->Id);
	
	} else if ((evn = (q->exposures && (wind->u.List.AllMasks & ExposureMask)))
	           || wind->hasBackGnd) {
		PRECT * sect;
		PXY     orig;
		CARD16  nClp;
		GRECT * exps = NULL;
		CARD16  nEvn = 0;
		
		DEBUG (ClearArea," W:%X [%i,%i/%i,%i]",
		       wind->Id, q->x, q->y, q->width, q->height);
		
		if ((short)q->width <= 0 || q->x + q->width > wind->Rect.w) {
			q->width = wind->Rect.w - q->x;
		}
		if((short)q->height <= 0 || q->y + q->height > wind->Rect.h) {
			q->height = wind->Rect.h - q->y;
		}
		if (q->width > 0  &&  q->height > 0
		    && (nClp = WindClipLockP (wind, 0, (GRECT*)&q->x, 1, &orig, &sect))) {
			
			PRECT clip = sect[nClp];
			short num;
			
			if (wind->Id == ROOT_WINDOW) {
				if (evn) {
					exps = (GRECT*)sect;
					nEvn = nClp;
				}
				if (wind->hasBackGnd) {
					extern OBJECT WMGR_Desktop;
					do {
						sect->rd.x -= sect->lu.x -1;
						sect->rd.y -= sect->lu.y -1;
						objc_draw (&WMGR_Desktop, 0, 1,
						           sect->lu.x, sect->lu.y, sect->rd.x, sect->rd.y);
						sect->lu.x -= orig.x;
						sect->lu.y -= orig.y;
						sect++;
					} while (--nClp);
				}
				WindClipOff();
			
			} else if ((num = clip_children (wind, orig, sect + nClp, &clip))) {
				PRECT * area = sect + nClp;
				
				if (evn) {
					exps = (GRECT*)(area + num);
				}
				if (wind->hasBackGnd) {
					if (!wind->hasBackPix) {
						vswr_mode (GRPH_Vdi, MD_REPLACE);
						vsf_color (GRPH_Vdi, wind->Back.Pixel);
					}
					do {
						CARD16 n = WindDrawBgnd (wind, orig, sect++, area, num, exps);
						if (evn) {
							nEvn += n;
							exps += n;
						}
					} while (--nClp);
				
				} else if (evn) do {
					int i;
					for (i = 0; i < num; i++) {
						*exps = *(GRECT*)sect;
						if (GrphIntersectP ((PRECT*)exps, area + i)) {
							exps->w -= exps->x -1;
							exps->h -= exps->y -1;
							exps->x -= orig.x;
							exps->y -= orig.y;
							exps++;
							nEvn++;
						}
					}
					sect++;
				} while (--nClp);
				
				if (evn) {
					exps -= nEvn;
				}
			}
			
			if (nEvn) {
				EvntExpose (wind, nEvn, exps);
			}
			WindClipOff();
		}
	}
}
