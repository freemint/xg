#include "main.h"
#include "tools.h"
#include "clnt.h"
#include "x_gem.h" // for GRECT,PXY

#include <stdio.h>
#include <ctype.h>

#include <X11/Xproto.h>


//==============================================================================
void *
ClntSwap (void * buf, const char * form)
{
	char * r = (char*)buf;
	char   f;
	while ((f = *(form++))) if (isdigit(f) && !(f & 1)) {
		r += f - '0';
		
	} else switch (f) {
		case 'R': SwapRCT ((GRECT*)r, (GRECT*)r);    r += 8; break;
		case 'P': SwapPXY ((PXY*)r, (PXY*)r);        r += 4; break;
		
		case ':': *(CARD16*)r = Swap16(*(CARD16*)r); r += 2;
		case '.': *(CARD16*)r = Swap16(*(CARD16*)r); r += 2; break;
		
		case 'l': // CARD32
		case 'a': // ATOM
		case 'w': // WINDOW
		case 'd': // DRAWABLE
		case 's': // SETof...
		case 'v': // VISUALID
		case 'm': // COLORMAP
		case 'g': // GCONTEXT
		case 'p': // PIXMAP
		case 't': // TIMESTAMP
		case 'f': // FONT/FONTABLE
		case '*': // BITMASK
			*(CARD32*)r = Swap32(*(CARD32*)r); r += 4;
			break;
		
		default:
			printf ("ERROR: Invalid '%c' in format '%s'.\n", f, form);
			exit(1);
	}
	return buf;
}