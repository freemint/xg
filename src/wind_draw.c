//==============================================================================
//
// wind_draw.c
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-07 - Module released for beta state.
// 2000-09-19 - Initial Version.
//==============================================================================
//
#include "window_P.h"
#include "event.h"
#include "gcontext.h"
#include "grph.h"
#include "wmgr.h"
#include "x_gem.h"

#include <stdio.h> // printf

#include <X11/Xproto.h>


//==============================================================================
CARD16
WindClipLock (WINDOW * wind, CARD16 border, const GRECT * clip, short n_clip,
              PXY * orig, GRECT ** pBuf)
{
	WINDOW * pwnd;
	GRECT    work = wind->Rect, rect, * sect;
	BOOL     visb = wind->isMapped;
	CARD16   nClp = 0;
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
	
	if (!n_clip || !clip) {
		clip   = &work;
		n_clip = 1;
	
	} else if (n_clip < 0) {
		if (!GrphIntersect (&work, clip)) return 0;
		clip   = &work;
		n_clip = 1;
		
	} else { // (n_clip > 0)
		GRECT * c = alloca (sizeof(GRECT) * n_clip);
		n         = 0;
		while (n_clip--) {
			c[n]   =  *(clip++);
			c[n].x += orig->x;
			c[n].y += orig->y;
			if (GrphIntersect (c + n, &work)) n++;
		}
		clip = c;
		if (!(n_clip = n)) return 0;
	}
	
	WindUpdate (xTrue);
	wind_get (0, WF_SCREEN, &a, &b, &n,&n);
	*pBuf = sect = (GRECT*)((a << 16) | (b & 0xFFFF));
	
	wind_get_first (wind->Handle, &rect);
	while (rect.w > 0  &&  rect.h > 0) {
		const GRECT * c = clip;
		n               = n_clip;
		do {
			*sect = rect;
			if (GrphIntersect (sect, c++)) {
				a = sect->x + sect->w -1;
				b = sect->y + sect->h -1;
				if (l > sect->x) l = sect->x;
				if (u > sect->y) u = sect->y;
				if (r < a)       r = a;
				if (d < b)       d = b;
				nClp++;
				sect++;
			}
		} while (--n);
		wind_get_next (wind->Handle, &rect);
	}
	if (nClp) {
		sect->x = l;
		sect->y = u;
		sect->w = r - l +1;
		sect->h = d - u +1;
	
	} else {
		WindUpdate (xFalse);
	}
/*	
	if (work.w > 0 && work.h > 0  && (!a_clip || GrphIntersect (&work, &clip))) {
		GRECT * sec;
		int     a, b, c;
		short   l = 0x7FFF, u = 0x7FFF, r = 0x8000, d = 0x8000;
		WindUpdate (xTrue);
		c = wind_get (0, WF_SCREEN, &a, &b, &c,&c);
		*pBuf = sec = (GRECT*)((a << 16) | (b & 0xFFFF));
		
		wind_get_first (wind->Handle, sec);
		while (sec->w > 0  &&  sec->h > 0) {
			if (GrphIntersect (sec, &work)) {
				a = sec->x + sec->w -1;
				b = sec->y + sec->h -1;
				if (l > sec->x) l = sec->x;
				if (u > sec->y) u = sec->y;
				if (r < a)      r = a;
				if (d < b)      d = b;
				sec++;
				nClp++;
			}
			wind_get_next (wind->Handle, sec);
		}
		if (nClp) {
			sec->x = l;
			sec->y = u;
			sec->w = r - l +1;
			sec->h = d - u +1;
		} else {
			*pBuf = NULL;
			WindUpdate (xFalse);
		}
	}*/
	return nClp;
}

//==============================================================================
void
WindClipOff (void)
{
	WindUpdate (xFalse);
}


//==============================================================================
void
WindDrawSection (WINDOW * wind, const GRECT * clip)
{
	PXY     pos;
	GRECT * sect;
	CARD16  deco = (wind->GwmDecor ? WMGR_Decor : 0);
	short   nClp = WindClipLock (wind, (deco ? deco : wind->BorderWidth),
	                             clip, -1, &pos, &sect);
	if (nClp) {
		// sect points now to nClp clipping section(s)
		// set c to window clipping stack
		GRECT  * c = (sect + nClp);
		WINDOW * w = wind;
		BOOL     b = xTrue;
		
		// set to parents origin
		pos.x -= wind->Rect.x;
		pos.y -= wind->Rect.y;
		
		if (clip) {
			*c = *clip;
		} else {
			*(PXY*)c = pos;
			c->w = wind->Parent->Rect.w;
			c->h = wind->Parent->Rect.h;
		}
		c++; // (c + 1) buffer for exposure rectangles
		
		vswr_mode (GRPH_Vdi, MD_REPLACE);
		
		while(1) {
			if (b && w->isMapped && w->ClassInOut) {
				BOOL  draw = xTrue;
				BOOL  evn  = ((w->u.List.AllMasks & ExposureMask) != 0);
				int   exp  = 0 ,i;
				GRECT o    = w->Rect;
				o.x += pos.x;
				o.y += pos.y;
				*c = o;
				
				if (deco) {
					c->x -= deco;
					c->y -= deco;
					c->w += deco *2;
					c->h += deco *2;
					if ((draw = GrphIntersect (c, c -1))) {
						PXY * p = (PXY*)(c +1);
						p[0].x = p[4].x = 0;
						p[1].x          = o.x             -2;
						p[2].x          = o.x + w->Rect.w -1;
						p[3].x = p[5].x = 0x7FFF;
						p[0].y = p[2].y = o.y;
						p[4].y =(p[1].y = p[3].y = o.y + w->Rect.h) +1;
						p[5].y          = 0x7FFF;
						p[6].x = p[7].x = o.x -1;
						p[8].x = p[9].x = o.x + w->Rect.w;
						p[6].y = p[9].y = o.y;
						p[7].y = p[8].y = o.y + w->Rect.h;
						vsl_type  (GRPH_Vdi, SOLID);
						vsl_width (GRPH_Vdi, 1);
						vsl_color (GRPH_Vdi, LBLACK);
						vsf_color (GRPH_Vdi, LWHITE);
						v_hide_c  (GRPH_Vdi);
						for (i = 0; i < nClp; ++i) {
							GRECT r = *c;
							if (GrphIntersect (&r, sect +i)) {
								vs_clip_r (GRPH_Vdi, &r);
								v_bar_p   (GRPH_Vdi,    p +0);
								v_bar_p   (GRPH_Vdi,    p +2);
								v_bar_p   (GRPH_Vdi,    p +4);
								v_pline_p (GRPH_Vdi, 4, p +6);
							}
						}
						v_show_c  (GRPH_Vdi, 1);
						vs_clip_r (GRPH_Vdi, NULL);
						*c = o;
					}
					deco = 0;
				
				} else if (w->hasBorder && w->BorderWidth) {
					c->x -= w->BorderWidth;
					c->y -= w->BorderWidth;
					c->w += w->BorderWidth *2;
					c->h += w->BorderWidth *2;
					if ((draw = GrphIntersect (c, c -1))) {
						PXY * p = (PXY*)(c +1);
						p[0].x = p[2].x = p[6].x = o.x - w->BorderWidth;
						p[3].x =                   o.x - 1;
						p[1].x = p[5].x = p[7].x = (p[4].x = o.x + w->Rect.w) -1
						       + w->BorderWidth;
						p[0].y = o.y - w->BorderWidth;
						p[1].y = (p[2].y = p[4].y = o.y) -1;
						p[7].y = (p[3].y = p[5].y = (p[6].y = o.y + w->Rect.h) -1)
						       + w->BorderWidth;
						vsf_color (GRPH_Vdi, w->BorderPixel);
						v_hide_c  (GRPH_Vdi);
						for (i = 0; i < nClp; ++i) {
							GRECT r = *c;
							if (GrphIntersect (&r, sect +i)) {
								vs_clip_r (GRPH_Vdi, &r);
								v_bar_p (GRPH_Vdi, p +0);
								v_bar_p (GRPH_Vdi, p +2);
								v_bar_p (GRPH_Vdi, p +4);
								v_bar_p (GRPH_Vdi, p +6);
							}
						}
						v_show_c  (GRPH_Vdi, 1);
						vs_clip_r (GRPH_Vdi, NULL);
						*c = o;
					}
				}
				if (draw && GrphIntersect (c, c -1)) {
					if (w->hasBackGnd) {
						GRECT * r = (c +1);
						vsf_color (GRPH_Vdi, w->Back.Pixel);
						v_hide_c  (GRPH_Vdi);
						for (i = 0; i < nClp; ++i) {
							*r = o;
							if (GrphIntersect (r, c) && GrphIntersect (r, sect +i)) {
								PXY p[2] = { *(PXY*)r, *(PXY*)r };
								p[1].x += r->w -1;
								p[1].y += r->h -1;
								v_bar_p (GRPH_Vdi, p);
								if (evn) {
									r->x -= o.x;
									r->y -= o.y;
									r++;
									exp++;
								}
							}
						}
						v_show_c (GRPH_Vdi, 1);
						
					} else if (evn) {
						GRECT * r = (c +1);
						for (i = 0; i < nClp; ++i) {
							*r = o;
							if (GrphIntersect (r, sect +i)) {
								r->x -= o.x;
								r->y -= o.y;
								r++;
								exp++;
							}
						}
					}
					if (exp) {
						EvntExpose (w, exp, (c +1));
					}
					if (w->StackBot) {
						pos = *(PXY*)&o;
						c++;
						w = w->StackBot;
						continue;
					}
				}
			}
			if (w == wind) {
				break;
			} else if (w->NextSibl) {
				w = w->NextSibl;
				b = xTrue;
			
			} else if ((w = w->Parent) == wind) {
				break;
			} else {
				pos.x -= w->Rect.x;
				pos.y -= w->Rect.y;
				c--;
				b = xFalse;
			}
		}
			
		WindClipOff();
	}
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
		int colors[2] = { gc->Foreground, gc->Background };
		
		r[1].x += pos.x;
		r[1].y += pos.y;
		
		WindUpdate (xTrue);
		wind_get_first (hdl, &sec);
		while (sec.w && sec.h) {
			if (GrphIntersect (&sec, &r[1])) {
				int p[8] = { r[0].x + (sec.x - r[1].x), r[0].y + (sec.y - r[1].y),
				             sec.w,                     sec.h,
				             sec.x,                     sec.y,
				             sec.x + sec.w -1,          sec.y + sec.h -1 };
				p[2] += p[0] -1;
				p[3] += p[1] -1;
				v_hide_c  (GRPH_Vdi);
				vrt_cpyfm (GRPH_Vdi, MD_REPLACE, p, src, &dst, colors);
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
				int p[8] = { r[0].x + (sec.x - r[1].x), r[0].y + (sec.y - r[1].y),
				             sec.w,                     sec.h,
				             sec.x,                     sec.y,
				             sec.x + sec.w -1,          sec.y + sec.h -1 };
				p[2] += p[0] -1;
				p[3] += p[1] -1;
				v_hide_c  (GRPH_Vdi);
				vro_cpyfm (GRPH_Vdi, gc->Function, p, src, &dst);
				v_show_c  (GRPH_Vdi, 1);
			}
			wind_get_next (hdl, &sec);
		}
		WindUpdate (xFalse);
	}
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_ClearArea (CLIENT * clnt, xClearAreaReq * q)//, WINDOW * wind)
{
	WINDOW * wind = WindFind (q->window);
	
	if (!wind) {
		Bad(Drawable, q->window, ClearArea,);
		
	} else {
		GRECT * clip = (GRECT*)&q->x, * sect;
		PXY     orig;
		CARD16  nClp;
		
		DEBUG (ClearArea," W:%X [%i,%i/%i,%i]",
		       wind->Id, q->x, q->y, q->width, q->height);
		
		if (!clip->w || clip->w + clip->x > wind->Rect.w) {
			clip->w = wind->Rect.w - clip->x;
		}
		if (!clip->h || clip->h + clip->y > wind->Rect.h) {
			clip->h = wind->Rect.h - clip->y;
		}
		if (clip->w > 0  &&  clip->h > 0  &&
		           (nClp = WindClipLock (wind, 0, clip,1, &orig, &sect))) {
			GRECT * r = (sect + nClp);
			short clip_x = (clip->x = r->x - orig.x) + (clip->w = r->w) -1,
			      clip_y = (clip->y = r->y - orig.y) + (clip->h = r->h) -1;
			BOOL  e = (q->exposures && (wind->u.List.AllMasks & ExposureMask));
			
			// assemble all mapped and intersecting child geometries
			short num = 0; // number of mapped childs
			
			if (wind->StackBot) {
				WINDOW * w = wind->StackBot;
				do if (w->isMapped) {
					register int b = w->BorderWidth;
					*r = w->Rect;
					r->w += r->x;
					r->h += r->y;
					if ((b  && clip_x  >= (r->x -= b) &&  clip_y  >= (r->y -= b)
						     && clip->x <  (r->w += b) &&  clip->y <  (r->h += b)) ||
					    (!b && clip_x  >=  r->x       &&  clip_y  >=  r->y
					        && clip->x <   r->w       &&  clip->y <   r->h )) {
						r->x += orig.x;
						r->y += orig.y;
						r->w += orig.x -1;
						r->h += orig.y -1;
						r++;
						num++;
					}
				} while ((w = w->NextSibl));
			}
			
			vswr_mode (GRPH_Vdi, MD_REPLACE);
			vsf_color (GRPH_Vdi, wind->Back.Pixel);
			
			if (num) {
				struct { short l, u, r, d; }
				    * g = (void*)(sect + nClp), // list of childs geometries
				   ** s = (void*)(g + num),     // sorted pointers into geom. list
				    * a = (void*)(s + num);     // list of areas to be drawn
				short cnt = 0;   // counter for free areas
				short i   = 0;
				
				// sort child geometries into list, first by y, then by x
				
				s[0] = g++;
				for (i = 1; i < num; ++i) {
					register short j = i -1;
					while (j >= 0  && ( s[j]->u >  g->u  ||
					                   (s[j]->u == g->u  &&  s[j]->l > g->l) )) {
						s[j+1] = s[j];
						j--;
					}
					s[j+1] = g++;
				}
				
				{	// find free rectangles between child geometries
					
					short beg = 0;
					short d   = clip->y + orig.y;
					do {
						short x = clip->x + orig.x;
						short u = d;
						short end;
						if (u < s[beg]->u) {
							// found area above first child geometry
							a[cnt].l = x;
							a[cnt].u = u;
							a[cnt].r = clip_x + orig.x;
							a[cnt].d = s[beg]->u -1;
							cnt++;
							u = s[beg]->u;
						}
						d = s[beg]->d;
						
						for (end = beg +1; end < num; ++end) {
							if (s[end]->u > u) {
								if (s[end]->u <= d) d = s[end]->u -1;
								break;
							} else if (s[end]->d < d) {
								d = s[end]->d;
							}
						}
						i = beg;
						while (i < end) {
							if (x < s[i]->l) {
								// free area on left side of child
								a[cnt].l = x;
								a[cnt].u = u;
								a[cnt].r = s[i]->l -1;
								a[cnt].d = d;
								cnt++;
							}
							if (x <= s[i]->r) {
								x = s[i]->r +1;
							}
							if (s[i]->d > d)     i++;
							else if (i == beg)   beg = ++i;
							else {
								short j = i;
								while (++j < num) s[j-1] = s[j];
								if (--num < end)  end    = num;
							}
						}
						if (x <= clip_x + orig.x) {
							// free area on right side of last child
							a[cnt].l = x;
							a[cnt].u = u;
							a[cnt].r = clip_x + orig.x;
							a[cnt].d = d;
							cnt++;
						}
						
						d++;
						if (i > beg) {
							while (i < num  &&  s[i]->u == d  && s[i]->l < s[i-1]->l) {
								short      j    = i;
								typeof(*s) save = s[i];
								do {
									s[j] = s[j-1];
								} while (--j > beg  &&  save->l < s[j]->l);
								s[j] = save;
							}
							i++;
						}
					} while (beg < num);
					
					if (d <= clip_y + orig.y) {
						// free area on right side of last child
						a[cnt].l = clip->x + orig.x;
						a[cnt].u = d;
						a[cnt].r = clip_x  + orig.x;
						a[cnt].d = clip_y  + orig.y;
						cnt++;
					}
				}
				#define X_ 0
				DEBUG (, "          %i child%s, %i area%s",
				       num, (num == 1 ? "" : "s"), cnt, (cnt == 1 ? "" : "s"));
				
				// draw free rectangles
				
				do {
					int exp =  0;
					r       =  (GRECT*)(a + cnt);
					sect->w += sect->x -1;
					sect->h += sect->y -1;
					for (i = 0; i < cnt; ++i) {
						r->x = (sect->x >= a[i].l ? sect->x : a[i].l);
						r->y = (sect->y >= a[i].u ? sect->y : a[i].u);
						r->w = (sect->w <= a[i].r ? sect->w : a[i].r);
						r->h = (sect->h <= a[i].d ? sect->h : a[i].d);
						if (r->x <= r->w  &&  r->y <= r->h) {
							v_hide_c (GRPH_Vdi);
							v_bar_p  (GRPH_Vdi, (PXY*)r);
							v_show_c (GRPH_Vdi, 1);
							r++;
							exp++;
						}
					}
					if (exp && e) {
						r = (GRECT*)(a + cnt);
						for (i = 0; i < exp; ++i) {
							r[i].w -= r[i].x;
							r[i].h -= r[i].y;
							r[i].x -= orig.x;
							r[i].y -= orig.y;
						}
						EvntExpose (wind, exp, r);
					}
					sect++;
				} while (--nClp);
			
			} else { // the clear area intersects no child areas
				int i;
				for (i = 0; i < nClp; ++i) {
					sect[i].w += sect[i].x -1;
					sect[i].h += sect[i].y -1;
					v_hide_c (GRPH_Vdi);
					v_bar_p  (GRPH_Vdi, (PXY*)sect);
					v_show_c (GRPH_Vdi, 1);
				}
				if (e) {
					for (i = 0; i < nClp; ++i) {
						sect[i].w -= sect[i].x;
						sect[i].h -= sect[i].y;
						sect[i].x -= orig.x;
						sect[i].y -= orig.y;
					}
					EvntExpose (wind, nClp, sect);
				}
			}
			
			WindClipOff();
		}
	}
}
