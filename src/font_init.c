//==============================================================================
//
// font.c
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-12-07 - Initial Version.
//==============================================================================
//
#include "font_P.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>


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


static const short _FONT_Poto[] = {
	' ','!','"','#','$','%','&', 39, '(',')','*','+',',','-','.','/',
	'0','1','2','3','4','5','6','7', '8','9',':',';','<','=','>','?',
	'@','A','B','C','D','E','F','G', 'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W', 'X','Y','Z','[', 92,']','^','_',
	'`','a','b','c','d','e','f','g', 'h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w', 'x','y','z','{','|','}','~'
};

FONTFACE * _FONT_List = NULL;


//------------------------------------------------------------------------------
static void
_save_face (FILE * file, FONTFACE * face)
{
	fputs (face->Name, file);
	fputs ("\n",       file);
	fprintf (file, "   %i, %i,%i   %i,%i   %i,%i"
	         "   %i, %i,%i, %i,%i   %i, %i,%i, %i,%i\n",
	         face->Points, face->Width, face->Height,
	         face->MinChr, face->MaxChr, face->Ascent, face->Descent,
	         face->MinWidth, face->MinAsc, face->MinDesc,
	         face->MinLftBr, face->MinRgtBr,
	         face->MaxWidth, face->MaxAsc, face->MaxDesc,
	         face->MaxLftBr, face->MaxRgtBr);
}

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
_Font_Bounds (FONTFACE * face, BOOL mono)
{
	int dist[5], width, minADE, maxADE, dmy[3];
	
	vqt_fontinfo (GRPH_Vdi, &minADE, &maxADE, dist, &width, dmy);
	face->Ascent  = dist[4];
	face->MaxAsc  = dist[3];
	face->MinAsc  = dist[2];
	face->Descent = dist[0];
	face->MaxDesc = dist[1];
	face->MinDesc = 0;
	face->MinChr  = minADE;
	face->MaxChr  = maxADE;
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
}

//==============================================================================
void
FontInit (short count)
{
	struct FONT_DB {
		struct FONT_DB * next;
		short            id;
		short            cnt;
		FONTFACE       * list;
		char             file[1];
	} * font_db = NULL;
	
	FONTFACE ** list = &_FONT_List;
	FILE      * f_db = NULL;
	int i, j, k;
	
	if (   (access ("/var/lib/Xapp", R_OK|W_OK|X_OK) &&
	        mkdir ("/var/lib/Xapp", S_IRWXU|S_IRWXG|S_IRWXO))
	    || (!access ("/var/lib/Xapp/fonts.db", F_OK) &&
	        (   access ("/var/lib/Xapp/fonts.db", W_OK)
	         || !(f_db = fopen ("/var/lib/Xapp/fonts.db", "r"))))) {
		printf ("  \33pERROR\33q: Can't acess /var/lib/.\n");
		return;
	}
	if (f_db) {
		FONTFACE * face = NULL, ** fptr = NULL;
		unsigned   type, isMono, isSymbol;
		char  buf[258], c;
		while (fgets (buf, sizeof(buf), f_db)) {
			int len = strlen (buf);
			if (buf[len-1] == '\n') buf[--len] = '\0';
			
			if (sscanf (buf, "%i: %u,%u,%u%c%n",
			            &i, &type, &isSymbol, &isMono, &c,&j) == 5  &&  c == ' ') {
				struct FONT_DB * db;
				if (face) {
					puts("A");
					break;
				} else if (!(db = malloc (sizeof(struct FONT_DB) + len - j))) {
					puts("a");
					break;
				}
				db->next = font_db;
				db->id   = i;
				db->cnt  = 0;
				db->list = NULL;
				memcpy (db->file, buf + j, len - j +1);
				fptr    = &db->list;
				font_db = db;
			
			} else if (!font_db) {
				puts("B");
				break;
			
			} else if (buf[0] == '-') {
				if (face) {
					puts("C");
					break;
				} else if (!(face = _Font_Create (buf, len,
				                                  type, isSymbol, isMono))) {
					puts("c");
					break;
				}
			
			} else if (!face) {
				puts("D");
				break;
			
			} else if (sscanf (buf, "%i, %hi,%hi %hi,%hi %hi,%hi"
			                   "%hi, %hi,%hi, %hi,%hi %hi, %hi,%hi, %hi,%hi",
					             &i, &face->Width, &face->Height,
					             &face->MinChr, &face->MaxChr,
					             &face->Ascent, &face->Descent,
					             &face->MinWidth, &face->MinAsc, &face->MinDesc,
					             &face->MinLftBr, &face->MinRgtBr,
					             &face->MaxWidth, &face->MaxAsc, &face->MaxDesc,
					             &face->MaxLftBr, &face->MaxRgtBr) == 17) {
				face->Index   = font_db->id;
				face->Effects = 0;
				face->Points  = i;
				*fptr = face;
				fptr  = &face->Next;
				face  = NULL;
				font_db->cnt++;
			
			} else {
				if (face) free (face);
				face = NULL;
				puts("d");
				break;
			}
		}
		
		fclose (f_db);
	}
	
	f_db = fopen ("/var/lib/Xapp/fonts.db", "w");
	
	printf ("  loaded %i font%s\n", count, (count == 1 ? "" : "s"));
	for (i = 1; i <= count; i++) {
		struct FONT_DB * db = font_db;
		char _tmp[1000];
		VQT_FHDR * fhdr = (VQT_FHDR*)_tmp;
		XFNT_INFO  info;
		const char * fmly = info.family_name,
		           * wght = info.style_name,
		           * creg = "ISO8859",
		           * cenc = "1",
		           * fndr = NULL, * setw = NULL, * astl = NULL;
		char         slnt[3] = "\0\0", resx[6] = "72", resy[6] = "72",
		             spcg[2] = "\0";
		BOOL       isMono, isSymbol;
		char       * p;
		
		vqt_ext_name (GRPH_Vdi, i, info.font_name, &j, &k);
		isMono   = (k & 0x01 ? 1 : 0);
		isSymbol = (k & 0x10 ? 1 : 0);
		
		info.size           = sizeof(info);
		info.font_name[0]   = '\0';
		info.family_name[0] = '\0';
		info.style_name[0]  = '\0';
		info.pt_cnt         = 0;
		vqt_xfntinfo (GRPH_Vdi, 0x010F, 0, i, (XFNT_INFO*)&info);
		if (info.pt_cnt > 1  &&
		    info.pt_sizes[info.pt_cnt -1] == info.pt_sizes[info.pt_cnt -2]) {
			info.pt_cnt--;
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
		
		if (f_db) fprintf (f_db, "%i: %u,%u,%u %s\n", info.id,
		                   info.format, isSymbol, isMono, info.file_name1);
		
		if (db && db->list) {
			*list = db->list;
			do {
				if (f_db) _save_face (f_db, *list);
			} while (*(list = &(*list)->Next));
			db->list = NULL;
			
			continue;
		}
		
		vst_font (GRPH_Vdi, info.id);
		
		if (info.format > 1) {
			vqt_fontheader (GRPH_Vdi, _tmp, info.file_name1);
			spcg[0] = (fhdr->fh_cflgs & 2 ? fhdr->fh_famcl == 3 ? 'C' : 'M' : 'P');
			slnt[0] = (fhdr->fh_cflgs & 1 ? 'I' : 'R');
			strcpy (resx, "0");
			strcpy (resy, "0");
		}
		if (info.format == 2) { //___Speedo___
			if (!strncasecmp (fmly, "Bits ", 5)) {
				fndr =  "Bitstream";
				fmly += 5;
			} else if ((p = strstr (fhdr->fh_cpyrt, "opyright"))) {
				if ((p = strstr (p, "by "))) {
					fndr = p +3;
					if ((p = strchr (fndr, ' '))) *p = '\0';
				}
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
			
		} else {
			if (!*spcg) {
				spcg[0] = (isMono ? 'C' : 'P');
			}
			if (isSymbol) {
				creg = "Gdos";
				cenc = "FONTSPECIFIC";
			}
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
			if (info.format == 1) {
				if (!*slnt)          slnt[0] = '*';
				if (!wght || !*wght) wght    = "*";
			} else {
				if (!*slnt)          slnt[0] = 'R';
				if (!wght || !*wght) wght    = "Medium";
			}
			if (!setw) setw = "Normal";
		}
		
		if (fmly) {
			while ((p = strchr (fmly, ' '))) strcpy (p, p +1);
		}
		
		sprintf (info.file_name1,
		         "-%s-%s-%s-%s-%s-%s-%%u-%%u0-%s-%s-%s-%%u-%s-%s",
		         (fndr ? fndr : ""), (fmly ? fmly : ""), (wght ? wght : ""),
		         slnt, (setw ? setw : ""), (astl ? astl : ""),
		          resx, resy, spcg, creg, cenc);
		for (j = 0; j < info.pt_cnt; ++j) {
			int len, wdth, pxsz, avrg, dmy;
			vst_point (GRPH_Vdi, info.pt_sizes[j], &dmy,&dmy, &wdth, &pxsz);
			
			if (spcg[0] == 'P') {
				short ext[8];
				vqt_extent_n (GRPH_Vdi, _FONT_Poto, sizeof(_FONT_Poto) /2, ext);
				avrg = ((ext[4] - ext[0]) *10) / (sizeof(_FONT_Poto) /2);
			} else {
				avrg = wdth * 10;
			}
			len = sprintf (fhdr->fh_fmver, info.file_name1,
			               pxsz, info.pt_sizes[j], avrg);

			if ((*list = _Font_Create (fhdr->fh_fmver, len,
			                           info.format, isSymbol, isMono))) {
				FONTFACE * face = *list;
				face->Index   = info.id;
				face->Effects = 0;
				face->Points  = info.pt_sizes[j];
				face->Height  = pxsz;
				face->Width   = wdth;
				_Font_Bounds (face, (spcg[0] != 'P'));
				if (f_db) _save_face (f_db, face);
				list = &face->Next;
			}
		}
	}
	if (f_db) fclose (f_db);
	
	while (font_db) {
		struct FONT_DB * db = font_db;
		while (db->list) {
			FONTFACE * face = db->list;
			db->list = face->Next;
			free (face);
		}
		font_db = db->next;
		free (db);
	}
}
