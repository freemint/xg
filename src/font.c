//==============================================================================
//
// font.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-11-15 - Initial Version.
//==============================================================================
//
#include "font_P.h"
#include "tools.h"
#include "Atom.h"

#include <stdlib.h>
#include <stdio.h>
#define _GNU_SOURCE
#include <string.h>
#include <fnmatch.h>

#include <X11/X.h>


static const short _FONT_Latin[256] = {
#	define ___ ' '
#	define UDF ' ' // UNDEFINED
#	define NBS ' ' // NO-BREAK-SPACE
	0  ,___,___,___,___,___,___,___, ___,___,___,___,___,___,___,___,
	___,___,___,___,___,___,___,___, ___,___,___,___,___,___,___,___,
	' ','!','"','#','$','%','&', 39, '(',')','*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7', '8','9',':',';','<','=','>','?',
	'@','A','B','C','D','E','F','G', 'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W', 'X','Y','Z','[', 92,']','^','_',
	'`','a','b','c','d','e','f','g', 'h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w', 'x','y','z','{','|','}','~',UDF,
	'à','á','â','ã','ä','å','æ','ç', 'è','é','ê','ë','ì','í','î','ï',
	'ð',UDF,'ò','ó','ô','õ',UDF,'÷', UDF,UDF,'ú','û','ü',UDF,UDF,'ÿ',
	NBS,'­','›','œ','Ÿ','','|','Ý', '¹','½','¦','®','ª',___,'¾',___,
	'ø','ñ','ý','þ','º','æ','¼','ù', ___,___,'§','¯','¬','«',___,'¨',
	'¶',___,___,'·','Ž','','’','€', ___,'',___,___,___,___,___,___,
	___,'¥',___,___,___,'¸','™',___, '²',___,___,___,'š',___,___,'ž',
	'…',' ','ƒ','°','„','†','‘','‡', 'Š','‚','ˆ','‰','','¡','Œ','‹',
	___,'¤','•','¢','“','±','”','ö', '³','—','£','–','',___,___,'˜'
};


//==============================================================================
void
FontDelete (p_FONT font, p_CLIENT clnt)
{
	XrscDelete (clnt->Fontables, font);
}

//------------------------------------------------------------------------------
static inline
p_FONT FontFind (CARD32 id) {
	p_FONT font = FablFind(id).Font;
	if ((DBG_XRSC_TypeError = (font && !font->isFont))) font = NULL;
	return font;
}


//==============================================================================
BOOL
FontValues (p_FONTABLE fabl, CARD32 id)
{
	p_FONTABLE font;
	BOOL       ok = xTrue;
	
	if (!id || !(ok = ((font.Font = FontFind (id))) != NULL)) {
		fabl.p->FontFace    = NULL;
		fabl.p->FontIndex   = 1;
		fabl.p->FontEffects = 0;
		fabl.p->FontPoints  = 10;
	} else {
		FontCopy (fabl, font);
	}
	return ok;
}


//==============================================================================
void
FontLatin1_C (short * arr, const char * str, int len)
{
	while (len--) {
		*(arr++) = _FONT_Latin[(int)*(str++)];
	}
}

//==============================================================================
void
FontLatin1_W (short * arr, const short * str, int len)
{
	while (len--) {
		*(arr++) = _FONT_Latin[*(str++) & 0xFF];
	}
}


//------------------------------------------------------------------------------
static void
_Font_Alias (char * buf, const char * src, size_t len)
{
	FONTALIAS * alias = _FONT_Subst;
	int i;
	
	for (i = 0; i < len; i++) {
		buf[i] = src[i];
	}
	buf[len] = '\0';
	
	while (alias) {
		char * ptr = strstr (buf, alias->Name);
		if (ptr) {
			size_t n_len = strlen (alias->Name);
			size_t p_len = strlen (alias->Pattern);
			int    diff  = p_len - n_len;
			if (diff > 0) {
				memmove (ptr + p_len, ptr + n_len, len - (ptr - buf) - n_len +1);
			} else if (diff < 1) {
				strcpy (ptr + p_len, ptr + n_len);
			}
			memcpy (ptr, alias->Pattern, p_len);
			if ((len += diff) > 255) {
				len      = 255;
				buf[len] = '\0';
			}
		}
		alias = alias->Next;
	}
	
	alias = _FONT_Alias;
	while (alias) {
		if (!strcasecmp (buf, alias->Name)) {
			strcpy (buf, alias->Pattern);
			break;
		}
		alias = alias->Next;
	}
}

//------------------------------------------------------------------------------
static FONTFACE **
_Font_Match (const char * pattern, FONTFACE * start)
{
	FONTFACE ** face = (start ? &start->Next : &_FONT_List);
	
	while (*face) {
		if (!fnmatch (pattern, (*face)->Name, FNM_NOESCAPE|FNM_CASEFOLD)) {
			break;
		}
		face = &(*face)->Next;
	}
	return face;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_ListFonts (CLIENT * clnt, xListFontsReq * q)
{
	// Returns a list of available font names that matches the pattern.
	//
	// CARD16 maxNames: number of maximum names to return
	// CARD16 nbytes:   length of pattern
	char * patt = (char*)(q+1); // search pattern
	//
	// Reply:
	// CARD16 nFonts: number of names found
	//...........................................................................
   
	ClntReplyPtr (ListFonts, r);
	FONTFACE  * face  = _FONT_List;
	char      * list  = (char*)(r +1);
	size_t      size  = 0;
	char        buf[500] = "";
	
	PRINT (ListFonts," '%.*s' max=%u",
	       q->nbytes, (char*)(q +1), q->maxNames);
	
	_Font_Alias (buf, patt, q->nbytes);
	r->nFonts = 0;
	while (face  &&  r->nFonts < q->maxNames) {
		if (!fnmatch (buf, face->Name, FNM_NOESCAPE|FNM_CASEFOLD)) {
			memcpy (list + size, &face->Length, face->Length +1);
			size += face->Length +1;
			r->nFonts++;
		}
		face = face->Next;
	}
	ClntReply (ListFonts, size, ".");
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_OpenFont (CLIENT * clnt, xOpenFontReq * q)
{
	// Associates identifier with a font.
	//
	// Font   fid:
	// CARD16 nbytes: length of pattern
	char * patt = (char*)(q+1); // search pattern
	//...........................................................................
   
   if (FablFind (q->fid).p) {
		Bad(IDChoice, q->fid, OpenFont,);
	
	} else { //..................................................................
		
		FONTFACE  * face;
		FONT      * font;
		unsigned    w, h;
		char        buf[500] = "";

		PRINT (OpenFont," F:%lX '%.*s'", q->fid, q->nbytes, patt);
		
		_Font_Alias (buf, patt, q->nbytes);
		face = *_Font_Match (buf, NULL);
		
		if (!face  &&  sscanf (patt, "%ux%u", &w, &h) == 2) {
			sprintf (buf, "*-%u-*-*-*-C-%u0-ISO8859-1", h, w);
			face = *_Font_Match (buf, NULL);
			
			if (!face) {
				FONTFACE ** prot = &_FONT_List;
				sprintf (buf, "*-Medium-*-*-*-%u-*-*-*-C-*-ISO8859-1", h);
				while (*(prot = _Font_Match (buf, *prot)) &&
				       ((*prot)->Type < 2 || (*prot)->isSymbol));
				if (!*prot) {
					prot = &_FONT_List;
					while (*prot && ((*prot)->Type < 2
					       || !(*prot)->isMono || (*prot)->isSymbol)) {
						prot = &(*prot)->Next;
					}
				}
				if (*prot) {
					int i, dmy[3];
					face = _Font_Create (patt, strlen(patt), 0, xFalse, xTrue);
					if (!face) {
						Bad(Alloc,, OpenFont," (generic)");
						return;
					}
					face->Index   = (*prot)->Index;
					face->Effects = 0;
					face->Points  = 0;
					vst_font (GRPH_Vdi, face->Index);
					i = 0;
					while (i < h) {
						int c_h;
						vst_height (GRPH_Vdi, h - i, dmy, dmy, dmy, &c_h);
						if (c_h <= h) {
							face->Height = h - i;
							break;
						}
						i++;
					}
					i = 0;
					while (1) {
						int c_w;
						vst_width  (GRPH_Vdi, w + i, dmy,dmy, &c_w, dmy);
						if (c_w >= w) {
							face->Width = w + i;
							break;
						}
						i++;
					}
					vst_width  (GRPH_Vdi, face->Width, dmy,dmy, &w,&h);
					_Font_Bounds (face, xTrue);
					face->Next = *prot;
					*prot      = face;
				}
			}
		}
		if (!face && (strncasecmp (patt, "cursor", 6) || q->nbytes > 6)) {
			Bad(Name,, OpenFont,"('%.*s')", (int)q->nbytes, patt);
		
		} else if (!(font = XrscCreate (FONT, q->fid, clnt->Fontables))) {
			Bad(Alloc,, OpenFont,);
		
		} else {
			font->isFont = xTrue;
			
			if (!face) {
				FontValues ((p_FONTABLE)font, None);
			
			} else {
				font->FontFace    = face;
				font->FontIndex   = face->Index;
				font->FontEffects = face->Effects;
				if (face->Type) {
					font->FontPoints  = face->Points;
					font->FontWidth   = 0;
				} else {
					font->FontPoints  = face->Height;
					font->FontWidth   = face->Width;
				}
			}
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_CloseFont (CLIENT * clnt, xCloseFontReq * q)
{
	// Deletes association between resource ID ans font.
	//
	// CARD32 id: font
	//
	//...........................................................................
	
	FONT * font = FontFind (q->id);
	
	if (!font) {
		Bad(Font, q->id, CloseFont,);
	
	} else { //..................................................................
	
		DEBUG (CloseFont," F:%lX", q->id);
		
		FontDelete (font, ClntFind (q->id));
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_QueryFont (CLIENT * clnt, xQueryFontReq * q)
{
	// Returns logical informations about a font
	//
	// CARD32 id: font
	//
	// Reply:
	// xCharInfo minBounds, maxBounds
	// CARD16    minCharOrByte2, maxCharOrByte2
	// CARD8     minByte1, maxByte1
	// INT16     fontAscent, fontDescent
	// CARD16    defaultChar
	// CARD8     drawDirection
	// BOOL      allCharsExist
	// CARD16    nFontProps:     followed by this many xFontProp structures
	// CARD32    nCharInfos:     followed by this many xCharInfo structures
	//...........................................................................
	
	FONT * font = FontFind (q->id);
	
	if (!font) {
		Bad(Font, q->id, QueryFont,);
	
	} else { //..................................................................
	
		FONTFACE * face = font->FontFace;
		size_t     size = 0;
		ClntReplyPtr (QueryFont, r);
		
		PRINT (QueryFont," F:%lX", q->id);
		
		memcpy (&r->minBounds, &face->MinLftBr, (sizeof(xCharInfo) *2) +4);
		r->minCharOrByte2 = face->MinChr;
		r->maxCharOrByte2 = face->MaxChr;
		r->defaultChar    = ' ';
		r->drawDirection  = FontLeftToRight;
		r->minByte1       = 0;
		r->maxByte1       = 0;
		r->allCharsExist  = xTrue;
		r->fontAscent     = face->Ascent;
		r->fontDescent    = face->Descent;
		r->nFontProps     = 0;
		while (1) {
			Atom * atom = (Atom*)(r +1);
			if (!(*(atom++) = AtomGet ("FONT", 4, xFalse)) ||
			    !(*(atom++) = AtomGet (face->Name, face->Length, xFalse))) break;
			r->nFontProps++;
			size += sizeof(Atom) *2;
			break;
		}
		if (r->nFontProps && clnt->DoSwap) {
			Atom * a = (Atom*)(r +1);
			int    n = r->nFontProps;
			while (n--) {
				*a = Swap16(*a);
				a++;
			}
		}
		if (face->isMono) {
			r->nCharInfos  = 0;
		} else {
			xCharInfo * info = (xCharInfo*)((char*)(r +1) + size);
			r->nCharInfos    = face->MaxChr - face->MinChr +1;
			size += r->nCharInfos * sizeof(xCharInfo);
			if (face->CharInfos) {
				memcpy (info, face->CharInfos, size);
			} else {
				xCharInfo * p = info;
				int c, ld, rd, w;
				vst_font    (GRPH_Vdi, face->Index);
				vst_effects (GRPH_Vdi, face->Effects);
				vst_point   (GRPH_Vdi, face->Points, &c, &c, &c, &c);
				for (c = face->MinChr; c <= face->MaxChr; ++c) {
					vqt_width (GRPH_Vdi, c, &w, &ld, &rd);
					p->leftSideBearing  = ld;
					p->rightSideBearing = w - rd;
					p->characterWidth   = w;
					p->ascent           = face->MaxAsc;
					p->descent          = face->MaxDesc;
					p->attributes       = 0;
					p++;
				}
				if ((face->CharInfos = malloc (size))) {
					memcpy (face->CharInfos, info, size);
				}
			}
			if (clnt->DoSwap) {
				short * p = (short*)info;
				int     n = size /2;
				while (n--) {
					*p = Swap16(*p);
					p++;
				}
			}
		}
		
		ClntReply (QueryFont, size, ":::4:::4::4:l");
	}
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void
RQ_QueryTextExtents (CLIENT * clnt, xQueryTextExtentsReq * q)
{
	// BOOL oddLength: True if last char2b is unused
	// Font fid:
	CARD16 * text = (CARD16*)(q +1);
	//
	// Reply:
	// CARD8  drawDirection:
	// INT16  fontAscent, fontDescent:
	// INT16  overallAscent, overallDescent:
	// INT32  overallWidth, overallLeft, overallRight:
	//...........................................................................
	
	p_FONTABLE fabl = FablFind (q->fid);
	
	if (!fabl.p) {
		Bad(Font, q->fid, QueryTextExtents,);
	
	} else { //..................................................................
	
		FONTFACE * face = fabl.p->FontFace;
		size_t     size = ((q->length *4) - sizeof(xQueryTextExtentsReq)) /2
		                - (q->oddLength ? 1 : 0);
		char   str[size +1];
		int    ext[8], i;
		ClntReplyPtr (QueryTextExtents, r);
		
		PRINT (QueryTextExtents,"('%s') F:%lX", str, q->fid);
		
		for (i = 0; i < size; ++i) str[i] = text[i];
		str[size] = '\0';
		vst_font    (GRPH_Vdi, fabl.p->FontIndex);
		vst_effects (GRPH_Vdi, fabl.p->FontEffects);
		if (fabl.p->FontWidth) {
			vst_height (GRPH_Vdi, fabl.p->FontPoints, ext, ext, ext, ext);
			vst_width  (GRPH_Vdi, fabl.p->FontWidth,  ext, ext, ext, ext);
		} else {
			vst_point  (GRPH_Vdi, fabl.p->FontPoints, ext, ext, ext, ext);
		}
		vqt_extent (GRPH_Vdi, str, ext);
		r->drawDirection  = FontLeftToRight;
		r->fontAscent     = face->Ascent;
		r->fontDescent    = face->Descent;
		r->overallAscent  = face->MaxAsc;
		r->overallDescent = face->MaxDesc;
		r->overallWidth   = ext[4] - ext[0];
		r->overallLeft    = face->MinLftBr; 
		r->overallRight   = face->MaxRgtBr;
		
		ClntReply (QueryTextExtents,, "PPlll");
	}
}


//------------------------------------------------------------------------------
void
RQ_ListFontsWithInfo (CLIENT * clnt, xListFontsWithInfoReq * q)
{
	PRINT (- X_ListFontsWithInfo," '%.*s' max=%u",
	       q->nbytes, (char*)(q +1), q->maxNames);
}


//------------------------------------------------------------------------------
void
RQ_SetFontPath (CLIENT * clnt, xSetFontPathReq * q)
{
	PRINT (- X_SetFontPath," (%u)", q->nFonts);
}

//------------------------------------------------------------------------------
void
RQ_GetFontPath (CLIENT * clnt, xGetFontPathReq * q)
{
	PRINT (- X_GetFontPath," ");
}
