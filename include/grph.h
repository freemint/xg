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

BOOL GrphIntersect  (p_GRECT a, const struct s_GRECT * b);


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
void FT_Grph_ShiftArc_MSB (const p_PXY , p_xArc  arc, size_t num);
void FT_Grph_ShiftArc_LSB (const p_PXY , p_xArc  arc, size_t num);
void FT_Grph_ShiftPnt_MSB (const p_PXY , p_PXY pxy,   size_t num, short mode);
void FT_Grph_ShiftPnt_LSB (const p_PXY , p_PXY pxy,   size_t num, short mode);
void FT_Grph_ShiftR2P_MSB (const p_PXY , p_GRECT rct, size_t num);
void FT_Grph_ShiftR2P_LSB (const p_PXY , p_GRECT rct, size_t num);


#endif __GRPH_H__
