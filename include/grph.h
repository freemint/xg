//==============================================================================
//
// grph.h
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-05 - Initial Version.
//==============================================================================
//
#ifndef __GRPH_H__
# define __GRPH_H__
# ifdef _grph_
#  define CONST
# else
#  define CONST   const
# endif


BOOL GrphInit  (void);
void GrphExit  (void);
int  GrphSetup (void * format_arr);

#if 1
# define INL_ISCT
# define S_BOOL   static inline BOOL
#else
# define S_BOOL BOOL
#endif
BOOL   GrphIntersect  (p_GRECT a, const struct s_GRECT * b);
S_BOOL GrphIntersectP (p_PXY   a, const struct s_PXY   * b);


extern CONST short GRPH_Handle;
extern CONST short GRPH_Vdi;
extern CONST short GRPH_ScreenW, GRPH_ScreenH, GRPH_Depth,
                   GRPH_muWidth, GRPH_muHeight;
enum {
	SCRN_INTERLEAVED = 0,
	SCRN_STANDARD    = 1,
	SCRN_PACKEDPIXEL = 2
};
extern CONST short GRPH_Format;

void GrphRasterI4 (short * dst, char * src, CARD16 width, CARD16 height);
void GrphRasterI8 (short * dst, char * src, CARD16 width, CARD16 height);
void GrphRasterP8 (char  * dst, char * src, CARD16 width, CARD16 height);


#ifndef __p_xArc
# define __p_xArc
	struct _xArc; typedef struct _xArc * p_xArc;
#endif
void FT_Grph_ShiftArc_MSB (const p_PXY , p_xArc  arc, size_t num, short mode);
void FT_Grph_ShiftArc_LSB (const p_PXY , p_xArc  arc, size_t num, short mode);
void FT_Grph_ShiftPnt_MSB (const p_PXY , p_PXY   pxy, size_t num, short mode);
void FT_Grph_ShiftPnt_LSB (const p_PXY , p_PXY   pxy, size_t num, short mode);
void FT_Grph_ShiftR2P_MSB (const p_PXY , p_GRECT rct, size_t num);
void FT_Grph_ShiftR2P_LSB (const p_PXY , p_GRECT rct, size_t num);


#ifdef INL_ISCT

S_BOOL GrphIntersectP (p_PXY dst, const struct s_PXY * src)
{
	register BOOL res = 1;
	__asm__ volatile ("
		movem.l	(%1), d0/d1; | a.x0:a.y0/a.x1:a.y1
		movem.l	(%2), d2/d3; | b.x0:b.y0/b.x1:b.y1
			cmp.w		d0, d2;
			ble.b		1f;   | ? b.y0 <= a.y0
			move.w	d2, d0; | a.y0 = b.y0
		1:	cmp.w		d1, d3;
			bge.b		2f;   | ? b.y1 >= a.y1
			move.w	d3, d1; | a.y1 = b.y1
		2:	cmp.w		d0, d1;
			bge.b		3f;   | ? a.y1 >= a.y0
			clr.b		%0;
		3:
		swap		d0; | a.y0:a.x0
		swap		d1; | a.y1:a.x1
		swap		d2; | b.y0:b.x0
		swap		d3; | b.y1:b.x1
			cmp.w		d0, d2;
			ble.b		5f;   | ? b.x0 <= a.x0
			move.w	d2, d0; | a.x0 = b.x0
		5:	cmp.w		d1, d3;
			bge.b		6f;   | ? b.x1 >= a.x1
			move.w	d3, d1; | a.x1 = b.x1
		6:	cmp.w		d0, d1;
			bge.b		7f;   | ? a.x1 >= a.x0
			clr.b		%0;
		7:
		swap		d0;
		swap		d1;
		movem.l	d0/d1, (%1); | a.x:a.y/a.w:a.h
		"
		: "=d"(res)           // output
		: "a"(dst),"a"(src)   // input
		: "d0","d1","d2","d3" // clobbered
	);
	return res;
}

#endif INL_ISCT
#undef S_BOOL

#endif __GRPH_H__
