//==============================================================================
//
// main.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-05-30 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "server.h"
#include "Pointer.h"
#include "keyboard.h"
#include "wmgr.h"
#include "x_gem.h"
#include "x_mint.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <mint/ssystem.h>

#include <X11/X.h>
#include <X11/Xproto.h>

extern const short  _app;
extern const char * GLBL_Version;
extern const char * GLBL_Build;

static void shutdown (void);


static EVMULTI_IN ev_i = {
	MU_MESAG|MU_TIMER|MU_BUTTON|MU_KEYBD,
	0x0102,0x03,0x00,
	MO_ENTER, {},
	MO_LEAVE, { 0,0, 1,1 },
};
static EVMULTI_OUT ev_o;


long MAIN_FDSET_wr = 0L;
long MAIN_FDSET_rd = 0L;

CARD32 MAIN_TimeStamp  = 0;
CARD16 MAIN_KeyButMask = 0;
PXY  * MAIN_PointerPos = &ev_o.evo_mouse;

short _MAIN_Mctrl = 0;
short _MAIN_Wupdt = 0;


//==============================================================================
int
main (int argc, char * argv[])
{
	int port = 6000;
	int rtn  = 1;
	
	if (appl_init() < 0) {
		printf ("ERROR: Can't initialize AES.\n");
	
	} else {
		short xcon  = Fopen ("/dev/xconout2", 0x80);
		short redir = _app;
		
		short c_id = -1;
		if (xcon >= 0) {
			const char * args[] = {
				"-g","-0-0", "-notify", "-file", "/dev/xconout2" };
			c_id = WmgrLaunch ("/usr/X11/bin/xconsole",
			                   sizeof(args) / sizeof(*args), args);
			if (c_id >= 0) {
				(void)Pkill (c_id, 17);
				redir = 0;
			} else {
				Fclose (xcon);
			}
		}
		if (redir) {
			char  r_cmd[] = "U:\\usr\\bin\\xcat", r_tail[] = "";
			if ((redir = shel_write (1, 0, 0, r_cmd, r_tail))) {
				sleep (1);
			}
		}
		atexit (shutdown);
		WmgrIntro (xTrue);
		
		if (redir) {
			short  typ, pid;
			char   path[33] = "/pipe\0ttyp\0\0";
			char * file     = strchr (path, '\0') +1;
			char * ttyp     = strchr (file, '\0');
			size_t len      = ttyp - file;
			DIR  * dir      = opendir (path);
			if (dir) {
				struct dirent * ent;
				int fd;
				file[-1] = '/';
				while ((ent = readdir (dir))) {
					if (!strncmp (ent->d_name, file, len)
					    &&  *ttyp < ent->d_name[len]) {
						*ttyp = ent->d_name[len];
					}
				}
				closedir (dir);
				if (*ttyp && (fd = open (path, O_WRONLY))) {
					dup2 (fd, STDOUT_FILENO);
				}
			}
			if (appl_search (-redir, path, &typ, &pid)) {
				(void)Pkill (pid,    1);
				(void)Pkill (pid +1, 1);
			}
		}
		
		printf ("X Server %s [%s] starting ...\n", GLBL_Version, GLBL_Build);
		
		menu_register (ApplId(0), (char*)"  X");
		
		if (SrvrInit (port) >= 0) {
			CARD32  t_start  = clock() * (1000 / CLOCKS_PER_SEC);
			BOOL    run      = xTrue;
			CARD8 * kb_shift = (CARD8*)Ssystem (S_OSHEADER, 0x0024, 0);
			clock_t kb_tmout = 0x7FFFFFFF;
			
			WmgrActivate (xTrue); //(_app == 0);
			
			if (c_id >= 0) {
				Fclose (xcon);
				(void)Pkill (c_id, 19);
			}
			
			if (argc > 1  &&  argv[1] && *argv[1]) {
				int i;
				if (!access (argv[1], X_OK)) {
					WmgrLaunch (argv[1], argc -2, (const char **)(argv +2));
				}
				for (i = 2; i < argc; i++) printf("  |%s|\n", argv[i]);
			}
			
			rtn = 0;
			
			while (run) {
				short  msg[8];
				BOOL   reset = xFalse;
				short  event = evnt_multi_s (&ev_i, msg, &ev_o);
				CARD8  meta  = (*kb_shift & (K_RSHIFT|K_LSHIFT|K_LOCK|K_CTRL|K_ALT))
				             | (*kb_shift & K_ALTGR ? 0x20 : 0);
				short  chng;
				MAIN_TimeStamp = (clock() * (1000 / CLOCKS_PER_SEC) - t_start);
				
				if (event & MU_KEYBD) {
					if (meta == (K_CTRL|K_ALT)  &&  ev_o.evo_kreturn == 0x0E08) {
						if (_app) {
							fputs ("\33p   X Server Shutdown forced   \33q\n", stderr);
							exit (1);
						} else {
							printf ("\33p   X Server Reset forced   \33q\n");
							reset = xTrue;
						}
					}
					chng = KybdEvent (ev_o.evo_kreturn, meta);
					if (KYBD_Pending)  kb_tmout = MAIN_TimeStamp + KYBD_Repeat;
				} else if (meta != KYBD_PrvMeta || MAIN_TimeStamp > kb_tmout) {
					chng = KybdEvent (0, meta);
					 kb_tmout = 0x7FFFFFFF;
				} else {
					chng = 0;
				}
				if (WMGR_Cursor && (chng &= K_ALT|K_CTRL)) {
					WmgrKeybd (chng);
				}
				
				if (event & MU_BUTTON) {
					CARD16 prev_mask          = MAIN_KeyButMask & 0xFF00;;
					*(CARD8*)&MAIN_KeyButMask = PntrMap(ev_o.evo_mbutton) >>8;
					if (ev_o.evo_mbutton) {
						if (!prev_mask) WindMctrl (xTrue);
						ev_i.evi_bclicks = 0x0101;
					} else {
						WindMctrl (xFalse);
						ev_i.evi_bclicks = 0x0102;
					}
					if (WindButton (prev_mask, ev_o.evo_mclicks) && _MAIN_Mctrl) {
						graf_mkstate_p (&ev_o.evo_mouse,
						                &ev_o.evo_mbutton, &ev_o.evo_kmeta);
						if (!ev_o.evo_mbutton) {
							WindMctrl (xFalse);
						}
						*(CARD8*)&MAIN_KeyButMask = PntrMap(ev_o.evo_mbutton) >>8;
						meta = (*kb_shift & (K_RSHIFT|K_LSHIFT|K_LOCK|K_CTRL|K_ALT))
				           | (*kb_shift & K_ALTGR ? 0x20 : 0);
						if (meta != KYBD_PrvMeta
						    && (chng = KybdEvent (0, meta) & (K_ALT|K_CTRL))
						    && WMGR_Cursor) {
								WmgrKeybd (chng);
						}
						ev_i.evi_bclicks = 0x0102;
					}
					ev_i.evi_bstate = ev_o.evo_mbutton;
				}
				*(PXY*)&ev_i.evi_m2 = ev_o.evo_mouse;
				
				if (event & MU_MESAG) {
					if (msg[0] == MN_SELECTED) {
						run = WmgrMenu (msg[3], msg[4], ev_o.evo_kmeta);
					} else if (msg[0] == AP_TERM) {
						run = xFalse;
						break;
					} else {
						reset = WmgrMessage (msg);
					}
				}
				
				if      (event & MU_M1) WindPointerWatch (xTrue);
				else if (event & MU_M2) WindPointerMove  (NULL);
				
				{	long rd_set = MAIN_FDSET_rd, wr_set = MAIN_FDSET_wr;
				if (reset || (Fselect (1, &rd_set, &wr_set, 0)
				              && !SrvrSelect (rd_set, wr_set))) {
					printf ("\nLast client left, server reset ...\n");
					if (_MAIN_Mctrl) {
						puts("*BOING*");
						while (_MAIN_Mctrl > 0) {
							WindMctrl (xFalse);
						}
					}
					while (_MAIN_Wupdt > 0) {
						WindUpdate (xFalse);
					}
					SrvrReset();
				}}
			} 
		}
	}
	return rtn;
}

//------------------------------------------------------------------------------
static void
shutdown (void)
{
	while (_MAIN_Wupdt > 0) {
		WindUpdate (xFalse);
	}
	while (_MAIN_Mctrl > 0) {
		WindMctrl (xFalse);
	}
	WmgrExit();
	appl_exit();
	printf ("\nbye ...\n");
}


//==============================================================================
void
MainSetMove (BOOL onNoff)
{
	if (onNoff) ev_i.evi_flags |=  MU_M2;
	else        ev_i.evi_flags &= ~MU_M2;
}


//==============================================================================
void
MainSetWatch (const p_GRECT area, BOOL leaveNenter)
{
	if (area) {
		ev_i.evi_flags  |= MU_M1;
		ev_i.evi_m1leave = leaveNenter;
		ev_i.evi_m1.x    = area->x;
		ev_i.evi_m1.y    = area->y;
		ev_i.evi_m1.w    = area->w;
		ev_i.evi_m1.h    = area->h;
	
	} else {
		ev_i.evi_flags &= ~MU_M1;
	}
}
