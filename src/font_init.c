//==============================================================================
//
// font_init.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-12-07 - Initial Version.
//==============================================================================
//
#include "font_P.h"
#include "wmgr.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>


static BOOL _N_interim_14_3 = xTrue;


typedef struct {
	char           fh_fmver[8];
	unsigned long  fh_fntsz;
	unsigned long  fh_fbfsz;
	unsigned short fh_cbfsz;
	unsigned short fh_hedsz;
	unsigned short fh_fntid;
	unsigned short fh_sfvnr;
	char           fh_fntnm[70];
	char           fh_mdate[10];
	char           fh_laynm[70];
	char           fh_cpyrt[78];
	unsigned short fh_nchrl;
	unsigned short fh_nchrf;
	unsigned short fh_fchrf;
	unsigned short fh_nktks;
	unsigned short fh_nkprs;
	char           fh_flags;
	char           fh_cflgs;
	char           fh_famcl;
	char           fh_frmcl;
	char           fh_sfntn[32];
	char           fh_sfacn[16];
	char           fh_fntfm[14];
	unsigned short fh_itang;
	unsigned short fh_orupm;
} VQT_FHDR;

typedef struct s_FONT_DB {
	struct s_FONT_DB * next;
	short              id;
	short              cnt;
	FONTFACE         * list;
	char               file[1];
} FONT_DB;


static const short _FONT_Proto[] = {
	' ','!','"','#','$','%','&', 39, '(',')','*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7', '8','9',':',';','<','=','>','?',
	'@','A','B','C','D','E','F','G', 'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W', 'X','Y','Z','[', 92,']','^','_',
	'`','a','b','c','d','e','f','g', 'h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w', 'x','y','z','{','|','}','~'
};


static const short _FONT_TosLatin[256] = {
#	define ___ '\0'
#	define UDF '\0' // UNDEFINED
#	define NBS ' '  // NO-BREAK-SPACE
	0  ,___,___,___,___,___,___,___, ___,___,___,___,___,___,___,___,
	___,___,___,___,___,___,___,___, ___,___,___,___,___,___,___,___,
	' ','!','"','#','$','%','&', 39, '(',')','*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7', '8','9',':',';','<','=','>','?',
	'@','A','B','C','D','E','F','G', 'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W', 'X','Y','Z','[', 92,']','^','_',
	'`','a','b','c','d','e','f','g', 'h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w', 'x','y','z','{','|','}','~','',
	'‡','·','‚','„','‰','Â','Ê','Á', 'Ë','È','Í','Î','Ï','Ì','Ó','Ô',
	'','©','Ú','Û','Ù','ı',UDF,'˜', '¥','µ','˙','˚','¸',UDF,'ü','ø',
	NBS,'≠','õ','ú','ü','ù','|','›', 'π','Ω','¶','Æ','™',___,'æ','ˇ',
	'¯','Ò','˝','˛','∫','Ê','º','˘', ___,___,'ß','Ø','¨','´',___,'®',
	'∂',___,___,'∑','é','è','í','Ä', ___,'ê',___,___,___,___,___,___,
	___,'•',___,___,___,'∏','ô',___, '≤',___,___,___,'ö',___,___,'û',
	'Ö','†','É','∞','Ñ','Ü','ë','á', 'ä','Ç','à','â','ç','°','å','ã',
	___,'§','ï','¢','ì','±','î','ˆ', '≥','ó','£','ñ','Å',___,___,'ò'
};

static const short _FONT_ExtLatin[256] = {
	0  ,___,'',___,___,___,___,'¯', 'Ò',___,___,___,___,___,___,___,
	___,___,___,___,___,___,___,___, ___,'|','Û','Ú','„',___,'ú','˘',
	' ','!','"','#','$','%','&', 39, '(',')','*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7', '8','9',':',';','<','=','>','?',
	'@','A','B','C','D','E','F','G', 'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W', 'X','Y','Z','[', 92,']','^','_',
	'`','a','b','c','d','e','f','g', 'h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w', 'x','y','z','{','|','}','~','',
	'‡','·','‚','„','‰','Â','Ê','Á', 'Ë','È','Í','Î','Ï','Ì','Ó','Ô',
	'','©','Ú','Û','Ù','ı',UDF,'˜', '¥','µ','˙','˚','¸',UDF,'ü','ø',
	NBS,'≠','õ','ú','◊','ù','Ÿ','›', 'π','Ω','¶','Æ','™','-','æ','ˇ',
	'¯','Ò','˝','˛','∫','Ê','º','˘', '⁄','€','ß','Ø','¨','´','‹','®',
	'∂','¬','√','∑','é','è','í','Ä', 'ƒ','ê','≈','∆','«','»','…',' ',
	'À','•','Ã','Õ','Œ','∏','ô','ÿ', '≤','œ','–','—','ö','“','”','û',
	'Ö','†','É','∞','Ñ','Ü','ë','á', 'ä','Ç','à','â','ç','°','å','ã',
	'‘','§','ï','¢','ì','±','î','ˆ', '≥','ó','£','ñ','Å','’','÷','ò'
};


static struct {
	FONTFACE Font;
	char     Name[6];
} _FONT_Cursor = {
	{	NULL, NULL, 1, 0x00, 0, xTrue, xTrue, 10,
		16,16,  0,16, 16, 16,0, 0, 8, 0l,  0,16, 16, 16,0, 0,
		0, 153, 16,0,  NULL,  6,{'c'} },
	{	"ursor" }
};
FONTFACE  * _FONT_List  = &_FONT_Cursor.Font;
FONTALIAS * _FONT_Subst = NULL;
FONTALIAS * _FONT_Alias = NULL;


//------------------------------------------------------------------------------
FONTFACE *
_Font_Create (const char * name, size_t len, unsigned type, BOOL sym, BOOL mono)
{
	FONTFACE * face = malloc (sizeof(FONTFACE) + len);
	if (face) {
		face->Next      = NULL;
		face->Type      = type;
		face->isSymbol  = sym;
		face->isMono    = mono;
		face->CharInfos = NULL;
		face->Length    = len;
		memcpy (face->Name, name, len +1);
	}
	return face;
}

//------------------------------------------------------------------------------
void
_Font_Bounds (FONTFACE * face, BOOL mono, short * o_hdl, MFDB * mfdb)
{
	short dist[5], width, minADE, maxADE, dmy[3];
	
	vqt_fontinfo (GRPH_Vdi, &minADE, &maxADE, dist, &width, dmy);
	face->HalfLine =  dist[2];
	face->Ascent   =  dist[4];
	face->MaxAsc   =  dist[3];
	face->MinAsc   =  dist[2];
	face->Descent  = (dist[0] ? dist[0] +1 : 0);
	face->MaxDesc  = (dist[1] ? dist[1] +1 : 0);
	face->MinDesc  =  0;
	face->MinChr   =  minADE;
	face->MaxChr   =  maxADE;
	vqt_width (GRPH_Vdi, '.', &dist[0], &dist[1], &dist[2]);
	face->MinWidth = dist[0] - dist[1] - dist[2];
	face->MinLftBr = dist[1];
	face->MinRgtBr = dist[0] - dist[2];
	if (mono) {
		face->MaxWidth = face->MinWidth;
		face->MaxLftBr = face->MinLftBr;
		face->MaxRgtBr = face->MinRgtBr;
	} else {
		vqt_width (GRPH_Vdi, 'W', &dist[0], &dist[1], &dist[2]);
		face->MaxWidth = dist[0] - dist[1] - dist[2];
	//	if (face->MaxWidth < width) face->MaxWidth = width;
		face->MaxLftBr = dist[1];
		face->MaxRgtBr = dist[0] - dist[2];
	}
	face->MinAttr = face->MaxAttr = 0;
	
	if ((!face->Type || (_N_interim_14_3 && face->Type >= 2)) &&
	    (face->HalfLine >= face->Ascent || face->MaxDesc > face->Ascent /2)) {
		short hgt = face->Ascent + face->Descent;
		short hdl = (o_hdl ? *o_hdl : 0);
		if (hdl <= 0) {
			if (!o_hdl) {
				mfdb          = alloca (sizeof(MFDB));
				mfdb->fd_h    = hgt;
				mfdb->fd_addr = alloca (sizeof(long) * mfdb->fd_h * GRPH_Depth);
			} else if (!mfdb->fd_addr) {
				mfdb->fd_h    = 256;
				mfdb->fd_addr = malloc (sizeof(long) * mfdb->fd_h * GRPH_Depth);
			}
			mfdb->fd_w       = 32;
			mfdb->fd_wdwidth = (mfdb->fd_w +15) /16;
			mfdb->fd_stand   = 0;
			mfdb->fd_nplanes = 1;
			mfdb->fd_r1 = mfdb->fd_r2 = mfdb->fd_r3 = 0;
			if (mfdb->fd_addr) {
				short w_in[20] = { 1, 0,0, 0,0, 1,G_BLACK, 0,0,0, 2,
				                   mfdb->fd_w -1, mfdb->fd_h -1,
				                   GRPH_muWidth, GRPH_muHeight, 0,0,0,0,0 };
				short w_out[57];
				hdl = GRPH_Handle;
				v_opnbm (w_in, mfdb, &hdl, w_out);
				if (hdl > 0) {
					short pxy[4] = { 0, 0, mfdb->fd_w -1, mfdb->fd_h -1 };
					vst_load_fonts(hdl, 0);
					vs_clip       (hdl, 1, pxy);
					vswr_mode     (hdl, MD_TRANS);
					vst_alignment (hdl, 1, 5, dmy, dmy);
					vst_color     (hdl, G_BLACK);
				}
			}
		}
		
		if (hdl > 0) {
			long * buf = mfdb->fd_addr;
			PXY    p   = { 14, 0 };
			short  i;
			
			vst_font (hdl, face->Index);
			if (!face->Type) {
				vst_height (hdl, face->Height, dmy, dmy, dmy, dmy);
				vst_width  (hdl, face->Width,  dmy, dmy, dmy, dmy);
			} else {
				vst_point  (hdl, face->Points, dmy, dmy, dmy, dmy);
			}
			for (i = 0; i < hgt; buf[i++] = 0);
			
			if (face->MaxDesc > face->Ascent /2) {
				short txt[] = {'g','p','q','y'};
				i = sizeof(txt) /2;
				while (i) v_gtext16n (hdl, p, &txt[--i], 1);
				i = hgt;
				while (--i > 0 && !buf[i]);
				face->MaxDesc = i - face->Ascent +1;
			}
			if (face->HalfLine >= face->Ascent) {
				short txt[] = {'a','c','e','m','n','o','r','s','z'};
				i = sizeof(txt) /2;
				while (i) v_gtext16n (hdl, p, &txt[--i], 1);
				i = -1;
				while (++i < hgt && !buf[i]);
				face->HalfLine = face->MinAsc = face->Ascent - i;
			}
			if (face->MaxAsc >= face->Ascent) {
				short txt[] = {'A','O','T'};
				i = sizeof(txt) /2;
				while (i) v_gtext16n (hdl, p, &txt[--i], 1);
				i = -1;
				while (++i < hgt && !buf[i]);
				face->MaxAsc = face->Ascent - i;
			}
			
			if (o_hdl) {
				*o_hdl = hdl;
			} else {
				vst_unload_fonts (hdl, 0);
				v_clsbm          (hdl);
			}
		}
	}
}


//------------------------------------------------------------------------------
static void
_read_alias (void)
{
	FONTALIAS ** subst = &_FONT_Subst;
	FONTALIAS ** alias = &_FONT_Alias;
	FILE       * file  = fopen (PATH_FontsAlias, "r");
	char         buf[258];
	
	if (!file) return;
	
	while (fgets (buf, sizeof(buf), file)) {
		FONTALIAS * elem;
		size_t len_a, len_b;
		char * a = buf, * b;
		BOOL   is_subst;
		
		while (isspace(*a)) a++;
		if (!*a  ||  *a == '!') continue;
		
		if (!(len_a = strcspn (a, " \t:\r\n"))) break;
		b = a + len_a;
		while (isspace(*(b))) b++;
		if ((is_subst = (*b == ':'))) {
			while (isspace(*++b));
		}
		if (!(len_b = strcspn (b, " \t\r\n"))) break;
		
		if ((elem = malloc (sizeof(FONTALIAS) + len_a + len_b))) {
			char * p = elem->Pattern = elem->Name + len_a +1;
			while (len_b--) *(p++) = tolower (*(b++)); *p = '\0';
			p = elem->Name;
			while (len_a--) *(p++) = tolower (*(a++)); *p = '\0';
			if (is_subst) {
				*subst = elem;
				subst  = &elem->Next;
			} else {
				*alias = elem;
				alias  = &elem->Next;
			}
			elem->Next = NULL;
		} else {
			break;
		}
	}
	fclose (file);
}

//------------------------------------------------------------------------------
static FONT_DB * 
_read_fontdb (FILE * f_db)
{
	FONT_DB  * font_db = NULL;
	FONTFACE * face = NULL, ** fptr = NULL;
	char       buf[258];
	int  major = -1, minor = -1, tiny = 0;
	
	if (!fgets (buf, sizeof(buf), f_db)
	    || sscanf (buf, "# fonts.db; %d.%d.%d ", &major, &minor, &tiny) < 2
	    || minor < 14 || (minor == 14  &&  tiny < 2)) {
		fclose (f_db);
		return NULL;
	}
	
	while (fgets (buf, sizeof(buf), f_db)) {
		unsigned type, isMono, isSymbol;
		int      id, pre;
		char     c;
		int      len = strlen (buf);
		if (!len || buf[0] == '#') continue;
		if (buf[len-1] == '\n') buf[--len] = '\0';
		
		if (sscanf (buf, "%i: %u,%u,%u%c%n",
		            &id, &type, &isSymbol, &isMono, &c,&pre) == 5  &&  c == ' ') {
			FONT_DB * db;
			len -= pre;
			if (face) {
				printf ("A\n");
				break;
			} else if (!(db = malloc (sizeof(FONT_DB) + len))) {
				printf ("a\n");
				break;
			}
			db->next = font_db;
			db->id   = id;
			db->cnt  = 0;
			db->list = NULL;
			if (len > 0) memcpy (db->file, buf + pre, len +1);
			else         db->file[0] = '\0';
			fptr    = &db->list;
			font_db = db;
		
		} else if (!font_db) {
			printf ("B\n");
			break;
		
		} else if (buf[0] == '-') {
			if (face) {
				printf ("C\n");
				break;
			} else if (!(face = _Font_Create (buf, len, type, isSymbol, isMono))) {
				printf ("c\n");
				break;
			}
		
		} else if (!face) {
			printf ("D\n");
			break;
		
		} else if (sscanf (buf, "%i, %hi,%hi, %hi  %hi,%hi %hi,%hi"
		                   "%hi, %hi,%hi, %hi,%hi %hi, %hi,%hi, %hi,%hi",
				             &pre, &face->Width, &face->Height, &face->HalfLine,
				             &face->MinChr, &face->MaxChr,
				             &face->Ascent, &face->Descent,
				             &face->MinWidth, &face->MinAsc, &face->MinDesc,
				             &face->MinLftBr, &face->MinRgtBr,
				             &face->MaxWidth, &face->MaxAsc, &face->MaxDesc,
				             &face->MaxLftBr, &face->MaxRgtBr) == 18) {
			face->CharSet = NULL;
			face->Index   = font_db->id;
			face->Effects = 0;
			face->Points  = pre;
			*fptr = face;
			fptr  = &face->Next;
			face  = NULL;
			font_db->cnt++;
		
		} else {
			if (face) free (face);
			face = NULL;
			printf ("d\n");
			break;
		}
	}	
	fclose (f_db);
	
	return font_db;
}

//------------------------------------------------------------------------------
static void
_save_face (FILE * file, FONTFACE * face)
{
	fputs (face->Name, file);
	fputs ("\n",       file);
	fprintf (file, "   %i, %i,%i, %i   %i,%i   %i,%i"
	         "   %i, %i,%i, %i,%i   %i, %i,%i, %i,%i\n",
	         face->Points, face->Width, face->Height, face->HalfLine,
	         face->MinChr, face->MaxChr, face->Ascent, face->Descent,
	         face->MinWidth, face->MinAsc, face->MinDesc,
	         face->MinLftBr, face->MinRgtBr,
	         face->MaxWidth, face->MaxAsc, face->MaxDesc,
	         face->MaxLftBr, face->MaxRgtBr);
	fflush  (file);
}

//==============================================================================
void
FontInit (short count)
{
	FONT_DB * font_db = NULL;
	
	FONTFACE ** list = &_FONT_List;
	FILE      * f_db = NULL;
	int         i, j;
	
	short o_hdl = 0;
	MFDB  mfdb  = { NULL };
	
	
	_N_interim_14_3 = (count <= 50);
	
	
	_read_alias();
	
	if (   (access (PATH_LibDir, R_OK|W_OK|X_OK) &&
	        mkdir  (PATH_LibDir, S_IRWXU|S_IRWXG|S_IRWXO))
	    || (!access (PATH_FontsDb, F_OK) &&
	        (   access (PATH_FontsDb, W_OK)
	         || !(f_db = fopen (PATH_FontsDb, "r"))))) {
		printf ("  \33pERROR\33q: Can't acess %s\n", PATH_LibDir);
		return;
	}
	font_db = _read_fontdb (f_db);
	
	/*--- scan VDI fonts ---*/
	
	while (*list) list = &(*list)->Next;
	
	if ((f_db = fopen (PATH_FontsDb, "w"))) {
		fprintf (f_db, "# fonts.db; %s\n", GLBL_Version);
	}
	printf ("  loaded %i font%s\n", count, (count == 1 ? "" : "s"));
	for (i = 1; i <= count; i++) {
		FONT_DB     * db = font_db;
		char          _tmp[1000];
		VQT_FHDR    * fhdr = (VQT_FHDR*)_tmp;
		XFNT_INFO     info;
		const char  * fmly = info.family_name,
		            * wght = info.style_name,
		            * creg = "ISO8859",
		            * cenc = "1",
		            * fndr = NULL, * setw = NULL, * astl = NULL;
		char          slnt[3] = "\0\0", spcg[2] = "\0";
		unsigned      resx = 72, resy = 72;
		BOOL          isMono, isSymbol;
		const short * cset = NULL;
		BOOL          latn = xFalse;
		char        * p;
		short         type, dmy;
		
		vqt_ext_name (GRPH_Vdi, i, info.font_name, &dmy, &type);
		isMono   = (type & 0x01 ? 1 : 0);
		isSymbol = (type & 0x10 ? 1 : 0);
		
		info.size           = sizeof(info);
		info.font_name[0]   = '\0';
		info.family_name[0] = '\0';
		info.style_name[0]  = '\0';
		info.pt_cnt         = 0;
		vqt_xfntinfo (GRPH_Vdi, 0x010F, 0, i, &info);
		if (info.pt_cnt > 1  &&
		    info.pt_sizes[info.pt_cnt -1] == info.pt_sizes[info.pt_cnt -2]) {
			info.pt_cnt--;
		}
		
		if (!isSymbol) {
			if (info.format == 1
			    &&  info.font_name[0] =='›'&&  info.font_name[1] =='-') {
				latn = xTrue;
				cset = _FONT_ExtLatin;
			} else {
				cset = _FONT_TosLatin;
			}
		}
		
		while (db) {
			if (db->id == info.id  &&  !strcmp (db->file, info.file_name1)) {
				if (db->cnt != info.pt_cnt) {
					db = NULL;
				} else for (j = 0; j < info.pt_cnt; ++j) {
					FONTFACE * face = db->list;
					while (face  &&  face->Points != info.pt_sizes[j]) {
						face = face->Next;
					}
					if (!face) {
						db = NULL;
						break;
					}
				}
				break;
			}
			db = db->next;
		}
		
		if (f_db) {
			fprintf (f_db, "%i: %u,%u,%u %s\n", info.id,
			               info.format, isSymbol, isMono, info.file_name1);
		#if 0
			if (info.format > 2) {
				fprintf (f_db, "# font   |%s|\n", info.font_name);
				fprintf (f_db, "# family |%s|\n", info.family_name);
				fprintf (f_db, "# style  |%s|\n", info.style_name);
				fflush  (f_db);
			}
		#endif
		}
		
		if (db && db->list) {
			*list = db->list;
			do {
				if (f_db) _save_face (f_db, *list);
				(*list)->CharSet = cset;
			} while (*(list = &(*list)->Next));
			db->list = NULL;
			
			continue;
		}
		
		vst_font (GRPH_Vdi, info.id);
		
		if (info.format > 1) {
			vqt_fontheader (GRPH_Vdi, (char*)fhdr, info.file_name1);
			
			if (fhdr->fh_cflgs && fhdr->fh_famcl) {
				spcg[0] = (fhdr->fh_cflgs & 2 ? fhdr->fh_famcl == 3 ? 'C' : 'M'
				                          : 'P');
				slnt[0] = (fhdr->fh_cflgs & 1 ? 'I' : 'R');
			}
			
			if (info.format == 2) {  //___Speedo___
			
				if (!strncasecmp (fmly, "Bits ", 5)) {
					fndr =  "Bitstream";
					fmly += 5;
				} else if ((p = strstr (fhdr->fh_cpyrt, "opyright"))) {
					if ((p = strstr (p, "by "))) {
						fndr = p +3;
						if ((p = strchr (fndr, ' '))) *p = '\0';
					}
				}
				if ((p = strpbrk (fmly, " -*?"))) {
					char * q = p;
					char   c;
					do {
						if ((c = *(p++)) != ' ') {
							*(q++) = (c == '-' || c == '?' || c == '*' ? '_' : c);
						}
					} while (c);
				}
				if (fhdr->fh_cflgs & 0x04  ||  fhdr->fh_famcl == 1) {
					astl = "Serif";
				} else if (fhdr->fh_famcl == 2) {
					astl = "Sans";
				} else if (fhdr->fh_famcl == 4) {
					astl = "Script";
				} else if (fhdr->fh_famcl == 5) {
					astl = "Decorated";
				}
				switch (fhdr->fh_frmcl >> 4) {
					case  1: wght = "Thin";       break;
					case  2: wght = "UltraLight"; break;
					case  3: wght = "ExtraLight"; break;
					case  4: wght = "Light";      break;
					case  5: wght = "Book";       break;
					case  6: //wght = "Normal";     break;
					case  7: wght = "Medium";     break;
					case  8: wght = "Semibold";   break;
					case  9: wght = "Demibold";   break;
					case 10: wght = "Bold";       break;
					case 11: wght = "ExtraBold";  break;
					case 12: wght = "UltraBold";  break;
					case 13: wght = "Heavy";      break;
					case 14: wght = "Black";      break;
				}
				switch (fhdr->fh_frmcl & 0x0F) {
					case  4: setw = "Condensed";     break;
					case  6: setw = "SemiCondensed"; break;
					case  8: setw = "Normal";        break;
					case 10: setw = "SemiExpanded";  break;
					case 12: setw = "Expanded";      break;
				}
			
			} else { //___TrueType_Type1___
				
				BOOL chk = xTrue;
				char * m = NULL;
			#if 0
				if (f_db) {
					fprintf (f_db, "# cflgs %02X   famcl %02X   frmcl %02X \n",
					         fhdr->fh_cflgs, fhdr->fh_famcl, fhdr->fh_frmcl);
					fprintf (f_db, "# fmver |%.9s|\n", fhdr->fh_fmver);
					fflush  (f_db);
					fprintf (f_db, "# fntnm |%.71s|\n", fhdr->fh_fntnm);
					fflush  (f_db);
					fprintf (f_db, "# mdate |%.11s|\n", fhdr->fh_mdate);
					fflush  (f_db);
					fprintf (f_db, "# laynm |%.71s|\n", fhdr->fh_laynm);
					fflush  (f_db);
					fprintf (f_db, "# cpyrt |%.79s|\n", fhdr->fh_cpyrt);
					fflush  (f_db);
					fprintf (f_db, "# sfntn |%.33s|\n", fhdr->fh_sfntn);
					fflush  (f_db);
					fprintf (f_db, "# sfacn |%.17s|\n", fhdr->fh_sfacn);
					fflush  (f_db);
					fprintf (f_db, "# fntfm |%.15s|\n", fhdr->fh_fntfm);
					fflush  (f_db);
				}
			#endif
				
				if (!strcasecmp (wght, "Regular")) {
					if ((m = strrchr (fmly, '-'))) wght = m +1;
					else                           wght = "";
					slnt[0] = 'R';
				}
				if (!wght[0]) {
					wght = NULL;
				} else if (!strcasecmp (wght, "Normal") ||
				           !strcasecmp (wght, "Medium") ||
				           !strcasecmp (wght, "Plain")) {
					setw = (toupper(wght[0]) == 'N' ? "Normal" : NULL);
					wght = (toupper(wght[0]) == 'M' ? "Medium" : NULL);
					chk  = xFalse;
					if (m) {
						*m = '\0';
						m  = NULL;
					}
				} else if (!strcasecmp (wght, "Outline")) {
					astl = "Outline";
					wght = NULL;
					if (m) {
						*m = '\0';
						m  = NULL;
					}
				} else {
					const char * q;
					int          n;
					while (wght) {
						if ((p = strstr (wght, q = "Normal")) ||
						           (p = strstr (wght, q = "normal"))) {
							setw    = "Normal";
						} else if ((p = strstr (wght, q = "Italic")) ||
						           (p = strstr (wght, q = "italic"))) {
							slnt[0] = 'I';
						} else if ((p = strstr (wght, q = "Oblique")) ||
						           (p = strstr (wght, q = "oblique")) ||
						           (p = strstr (wght, q = "Obl"))) {
							slnt[0] = 'O';
						} else if ((p = strstr (wght, q = "Regular")) ||
						           (p = strstr (wght, q = "regular")) ||
						           (p = strstr (wght, q = "Reg"))) {
							slnt[0] = 'R';
						} else {
							break;
							if (m) wght = NULL;
							q = NULL;
						}
						n = strlen (q);
						while (p > wght && (p[-1] == ' ' || p[-1] == '-')) {
							p--;
							n++;
						}
						if      (!wght[n]) wght = NULL;
						else if (!p[n])    *p   = '\0';
						else               strcpy (p, p + n);
					}
				}
				if (wght) {
					if (!*wght) {
						wght = NULL;
					} else if (m && *m) {
						if (strcasecmp (wght, "Thin") &&
						    strcasecmp (wght, "UltraLight") &&
						    strcasecmp (wght, "ExtraLight") &&
						    strcasecmp (wght, "Light") &&
						    strcasecmp (wght, "Book") &&
						    strcasecmp (wght, "Medium") &&
						    strcasecmp (wght, "Semibold") &&
						    strcasecmp (wght, "Demibold") &&
						    strcasecmp (wght, "Bold") &&
						    strcasecmp (wght, "ExtraBold") &&
						    strcasecmp (wght, "UltraBold") &&
						    strcasecmp (wght, "Heavy") &&
						    strcasecmp (wght, "Black")) {
							wght = NULL;
						} else {
							chk = xFalse;
							*m  = '\0';
						}
					}
				}
				if (!setw) {
					const char * q;
					if ((p = strstr (fmly, q = "Condensed")) ||
					    (p = strstr (fmly, q = "condensed")) ||
					    ((p = strstr (fmly, q = "Cond")) && !islower(p[4]))) {
						setw = "Condensed";
					} else {
						q = NULL;
					}
					if (q) {
						int n = strlen (q);
						while (p > fmly && (p[-1] == ' ' || p[-1] == '-')) {
							p--;
							n++;
						}
						if (!p[n]) *p = '\0';
						else       strcpy (p, p + n);
					}
				}
				if (!astl) {
					const char * q;
					if ((p = strstr (fmly, q = "Sans")) ||
					    (p = strstr (fmly, q = "sans"))) {
						astl = "Sans";
					} else if ((p = strstr (fmly, q = "Outline")) ||
					           (p = strstr (fmly, q = "outline"))) {
						astl = "Outline";
					} else {
						q = NULL;
					}
					if (q) {
						int n = strlen (q);
						while (p > fmly && (p[-1] == ' ' || p[-1] == '-')) {
							p--;
							n++;
						}
						if (!p[n]) *p = '\0';
						else       strcpy (p, p + n);
					}
				}
				if ((p = strpbrk (fmly, " -*?"))) {
					char * q = p;
					char   c;
					do {
						if ((c = *(p++)) != ' ') {
							*(q++) = (c == '-' || c == '?' || c == '*' ? '_' : c);
						}
					} while (c);
				}
				if ((p = strchr (fmly, '('))) {
					char * q = strchr (p +1, ')');
					if (q) {
						int n = (q - p) -1;
						if (n) memcpy (fhdr->fh_cpyrt, p +1, n);
						fhdr->fh_cpyrt[n] = '\0';
						if (*(fndr = fhdr->fh_cpyrt) == '©') fndr++;
						if (!*(++q)) *p = '\0';
						else         strcpy (p, q);
					}
				}
				if (wght && chk) {
					if ((p = strpbrk (wght, " *?"))) {
						char * q = p;
						char   c;
						do {
							if ((c = *(p++)) != ' ') {
								*(q++) = (c == '?' || c == '*' ? '_' : c);
							}
						} while (c);
					}
					while ((p = strchr (wght, '-'))) {
						if (p[1]) {
							wght = p +1;
						} else {
							*p = '\0';
						}
					}
					if (!*wght) wght = NULL;
				}
				if (wght && (p = strstr (fmly, wght))) {
					int n = strlen (wght);
					while (p > fmly && (p[-1] == ' ' || p[-1] == '-')) {
						p--;
						n++;
					}
					if (p > fmly) {
						if (!p[n]) *p = '\0';
						else       strcpy (p, p + n);
					}
				}
			}
			resx = 0;
			resy = 0;
			
		} else if (latn) {
			
			do {
				fndr = info.font_name +2;
				if (!(p = strchr (fndr, '-'))) break;
				*(p++) = '\0';
				fmly   = p;
				if (!(p = strchr (fmly, '-'))) break;
				*p = '\0';
				if (!*(++p)) break;
				if (*p && *p != '-') switch (*(p++)) {
					case 'M': wght = "Medium"; break;
					case 'B': wght = "Bold";   break;
				}
				if (*(p++) != '-') break;
				switch (*p) {
					case 'R':
					case 'I': slnt[0] = *(p++); break;
				}
				if (*(p++) != '-') break;
				if (*p && *p != '-') switch (*(p++)) {
					case 'C': setw = "Condensed";     break;
					case 'c': setw = "SemiCondensed"; break;
					case 'N': setw = "Normal";        break;
					case 'e': setw = "SemiExpanded";  break;
					case 'E': setw = "Expanded";      break;
				}
				if (*(p++) != '-') break;
				if (*p != '-') {
					p++;   // AddStyle
				}
				if (*(p++) != '-') break;
				switch (*p) {
					case 'C':
					case 'P':
					case 'M': spcg[0] = *(p++); break;
				}
				if (*(p++) != '-') break;
				if (*p && *p != '-') switch (*(p++)) {
					case 'I': creg = "ISO8859";                   break;
					case '!': creg = fndr; cenc = "FONTSPECIFIC"; break;
				}
				if (*(p++) != '-') break;
				switch (*p) {
					case '1'...'9': cenc = p++; *p = '\0'; break;
					case 'F':       cenc = "FONTSPECIFIC"; break;
				}
			} while(0);
			resx = 75;
			resy = 75;
			
		} else {
			
			if (!*fmly) {
				fmly = info.font_name;
			}
			if ((p = strstr (fmly, " sans")) || (p = strstr (fmly, " Sans"))) {
				strcpy (p, p +5);
				astl = "Sans";
			}
			if (!strcasecmp (wght, "Regular")) {
				wght = "Medium";
				if (!*slnt) slnt[0] = 'R';
			} else {
				if ((p = strstr(wght, "italic")) || (p = strstr(wght, "Italic"))) {
					strcpy ((p[-1] == ' ' ? p -1 : p), p +6);
					if (!*slnt) slnt[0] = 'I';
				}
			}
			if (fmly) {
				while ((p = strchr (fmly, ' '))) strcpy (p, p +1);
			}
			if (wght && !*wght) wght = NULL;
		}
		
		if (isSymbol) {
			creg = "Gdos";
			cenc = "FONTSPECIFIC";
		}
		if (!spcg[0]) spcg[0] = (isMono ? 'C' : 'P');
		if (!slnt[0]) slnt[0] = 'R';
		if (!wght)    wght    = "Medium";
		if (!setw)    setw    = "Normal";
		
		sprintf (info.file_name1,
		         "-%s-%s-%s-%s-%s-%s-%%u-%%u0-%u-%u-%s-%%u-%s-%s",
		         (fndr ? fndr : ""), (fmly ? fmly : ""), (wght ? wght : ""),
		         slnt, (setw ? setw : ""), (astl ? astl : ""),
		         resx, resy, spcg, creg, cenc);
		for (j = 0; j < info.pt_cnt; ++j) {
			int   len, avrg;
			short wdth, pxsz;
			vst_point (GRPH_Vdi, info.pt_sizes[j], &dmy,&dmy, &wdth, &pxsz);
			
			if (spcg[0] == 'P') {
				short ext[8];
				vqt_extent16n (GRPH_Vdi, _FONT_Proto, sizeof(_FONT_Proto) /2, ext);
				avrg = ((ext[4] - ext[0]) *10) / (sizeof(_FONT_Proto) /2);
			} else {
				avrg = wdth * 10;
			}
			len = sprintf (fhdr->fh_fmver, info.file_name1,
			               pxsz, info.pt_sizes[j], avrg);

			if ((*list = _Font_Create (fhdr->fh_fmver, len,
			                           info.format, isSymbol, isMono))) {
				FONTFACE * face = *list;
				face->CharSet = cset;
				face->Index   = info.id;
				face->Effects = 0;
				face->Points  = info.pt_sizes[j];
				face->Height  = pxsz;
				face->Width   = wdth;
				if (face->Type == 4) {
					// For determineing TrueType, finish the startup intro and
					// unlock the screen to avoid NVDI producing a deadlock while
					// possible reporting damaged fonts in console window.
					//
					WmgrIntro (xFalse);
					graf_mouse (BUSYBEE, NULL);
				}
				_Font_Bounds (face, (spcg[0] != 'P'), &o_hdl, &mfdb);
				if (f_db) _save_face (f_db, face);
				list = &face->Next;
			}
		}
	}
	if (mfdb.fd_addr) {
		if (o_hdl) {
			vst_unload_fonts (o_hdl, 0);
			v_clsbm          (o_hdl);
		}
		free (mfdb.fd_addr);
	}
	if (f_db) {
		fputs ("# end of db\n", f_db);
		fclose (f_db);
	}
	
	while (font_db) {
		FONT_DB * db = font_db;
		while (db->list) {
			FONTFACE * face = db->list;
			db->list = face->Next;
			free (face);
		}
		font_db = db->next;
		free (db);
	}
}
