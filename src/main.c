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

#include <X11/Xproto.h>

extern const short  _app;
extern const char * GLBL_Version;

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

static BOOL _MAIN_Mctrl = xFalse;
short       _MAIN_Wupdt = 0;


//==============================================================================
int
main (int argc, char * argv[])
{
	int port = 6000;
	int rtn  = 1;
	
	if (appl_init() < 0) {
		printf ("ERROR: Can't initialize AES.\n");
	
	} else {
		char  r_cmd[] = "U:\\usr\\bin\\xcat", r_tail[] = "";
		BOOL  redir = _app && shel_write (1, 0, 0, r_cmd, r_tail);
		if (redir) sleep (1);
		atexit (shutdown);
		WmgrIntro (xTrue);
		
		if (redir) {
			char   path[] = "/pipe\0ttyp\0\0";
			char * file   = strchr (path, '\0') +1;
			char * ttyp   = strchr (file, '\0');
			size_t len    = ttyp - file;
			DIR  * dir    = opendir (path);
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
		}
		
		printf ("EEKS %s X Server starting ...\n", GLBL_Version);
		
		menu_register (ApplId(0), (char*)"  X");
		
		if (SrvrInit (port) >= 0) {
			CARD32 t_start  = clock() * (1000 / CLOCKS_PER_SEC);
			BOOL   run      = xTrue;
			
			WmgrActivate (xTrue); //(_app == 0);
			
			if (argc > 1  &&  argv[1] && *argv[1]) {
				if (!access (argv[1], X_OK)
				    &&  Pexec (100, argv[1], "\0\0", NULL) > 0) {
					printf ("  Started '%s'.\n",    argv[1]);
				} else {
					printf ("  Can't exec '%s'.\n", argv[1]);
				}
			}
			
			rtn = 0;
			
			while (run) {
				short  msg[8];
				BOOL   reset     = xFalse;
				short  event     = evnt_multi_s (&ev_i, msg, &ev_o);
				CARD16 prev_mask = MAIN_KeyButMask;
				CARD8  meta      = (ev_o.evo_kmeta & 2 ? (ev_o.evo_kmeta &~2)|0x11 :
				                    ev_o.evo_kmeta & 1 ?  ev_o.evo_kmeta     |0x21 :
				                    ev_o.evo_kmeta);
				MAIN_KeyButMask  = meta | PntrMap(ev_o.evo_mbutton);
				MAIN_TimeStamp   = (clock() * (1000 / CLOCKS_PER_SEC) - t_start);
				
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
				
				if (event & MU_BUTTON) {
					if (ev_o.evo_mbutton) {
						if (!_MAIN_Mctrl) {
							wind_update (BEG_MCTRL);
							_MAIN_Mctrl = xTrue;
						}
						ev_i.evi_bclicks = 0x0101;
					} else {
						if (_MAIN_Mctrl) {
							wind_update (END_MCTRL);
							_MAIN_Mctrl = xFalse;
						}
						ev_i.evi_bclicks = 0x0102;
					}
					if (WindButton (prev_mask, ev_o.evo_mclicks) && _MAIN_Mctrl) {
						graf_mkstate_p (&ev_o.evo_mouse,
						                &ev_o.evo_mbutton, &ev_o.evo_kmeta);
						if (!ev_o.evo_mbutton) {
							wind_update (END_MCTRL);
							_MAIN_Mctrl = xFalse;
						}
						MAIN_KeyButMask = ev_o.evo_kmeta | PntrMap(ev_o.evo_mbutton);
						ev_i.evi_bclicks = 0x0102;
					}
					ev_i.evi_bstate = ev_o.evo_mbutton;
				}
				*(PXY*)&ev_i.evi_m2 = ev_o.evo_mouse;
				
				if (event & MU_KEYBD) {
					KybdEvent (ev_o.evo_kreturn, prev_mask);
				} else if (meta != (prev_mask & 0xFF)) {
					KybdEvent (0, prev_mask);
				}
				
				{	long rd_set = MAIN_FDSET_rd, wr_set = MAIN_FDSET_wr;
				if (reset || (Fselect (1, &rd_set, &wr_set, 0)
				              && !SrvrSelect (rd_set, wr_set))) {
					printf ("\nLast client left, server reset ...\n");
					if (_MAIN_Mctrl) {
						puts("*BOING*");
						wind_update (END_MCTRL);
						_MAIN_Mctrl = xFalse;
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
	if (_MAIN_Mctrl) {
		wind_update (END_MCTRL);
		_MAIN_Mctrl = xFalse;
	}
	while (_MAIN_Wupdt > 0) {
		WindUpdate (xFalse);
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
