//==============================================================================
//
// srvr.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-03 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "tools.h"
#include "grph.h"
#include "server.h"
#include "clnt.h"
#include "wmgr.h"
#include "Atom.h"
#include "pixmap.h"
#include "Cursor.h"
#include "Pointer.h"
#include "keyboard.h"
#include "window.h"
#include "x_mint.h"

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#include <X11/Xproto.h>


#define RELEASE   1001
#define MOTIONB   0uL


static long SRVR_RdSet  = 0L;
static int  SRVR_Socket = -1;

//==============================================================================
static const char _SRVR_Vendor[] = "FreeMiNT/ATARI";
static struct s_setup {
	xConnSetupPrefix pref;
	xConnSetup       conn;
	char             vndr[Align(sizeof(_SRVR_Vendor)-1)];
	xPixmapFormat    pfrm[5];
	xWindowRoot      root;
}
_SRVR_ConnMSB = {
	{ X_Reply, 0, X_PROTOCOL, X_PROTOCOL_REVISION, 0000 },
	{ RELEASE, 0uL,RID_MASK, MOTIONB,
	  sizeof(_SRVR_Vendor) -1,
	  0uL, 1, 0u, MSBFirst, MSBFirst, 16, 16, 00,00 },
	{ "" },
},
_SRVR_ConnLSB;

static size_t _SRVR_SetupBytes  = sizeof(xConnSetupPrefix) + sizeof(xConnSetup)
                                + Align(sizeof(_SRVR_Vendor)-1)
                                + sizeof(xWindowRoot);
extern int      GRPH_DepthNum;
extern xDepth * GRPH_DepthMSB[], * GRPH_DepthLSB[];

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int
SrvrInit (int port)
{
	struct sockaddr_in server;
	int                i;
	int max_conn = 20;
	
	SRVR_Socket = socket (AF_INET, SOCK_STREAM, 0);   // get socket descriptor
	if (SRVR_Socket < 0) {
		printf ("Can't open stream socket.\n");
		return -1;
	}
	i = 1;
	setsockopt(SRVR_Socket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
	i = 1;
	setsockopt(SRVR_Socket, SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));
	
	// fill in server socket structure
	memset (&server, 0, sizeof(server));
	server.sin_family      = AF_INET;
	server.sin_port        = htons (port);
	server.sin_addr.s_addr = htons (INADDR_ANY);
	
	if (bind (SRVR_Socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
		printf ("Can't bind stream socket.\n");
		return -1;
	}
	if (listen (SRVR_Socket, max_conn) < 0) {
		printf ("Can't listen to stream socket.\n");
		return -1;
	}
	
	/***** Now connection is up *****/
	
	printf ("  using port %i (fd %i) for maximum of %i connections,\n",
	        port, SRVR_Socket, max_conn);
	
	MAIN_FDSET_rd = SRVR_RdSet = 1uL << SRVR_Socket;
	
	AtomInit (xTrue);
	ClntInit (xTrue);
	PmapInit (xTrue);
	WindInit (xTrue);
	
	_SRVR_ConnMSB.conn.minKeyCode = KYBD_MapMin;
	_SRVR_ConnMSB.conn.maxKeyCode = KYBD_MapMax;
	_SRVR_ConnMSB.conn.numFormats = i = GrphSetup (&_SRVR_ConnMSB.pfrm);
	_SRVR_SetupBytes             += i * sizeof(xPixmapFormat);
	_SRVR_ConnMSB.pref.length = Units (_SRVR_SetupBytes - sz_xConnSetupPrefix
	                             + GRPH_DepthNum * (sz_xDepth + sz_xVisualType));
	memcpy (_SRVR_ConnMSB.vndr, _SRVR_Vendor, sizeof(_SRVR_Vendor) -1);
	memcpy (&_SRVR_ConnLSB, &_SRVR_ConnMSB, _SRVR_SetupBytes);
	ClntSwap (&_SRVR_ConnLSB, "2.:l4ll.");
	ClntSwap (&_SRVR_ConnLSB.pfrm[i], "wlllsPP:v");
	
	if (i  &&  WmgrInit (xTrue)) {
		CrsrInit (xTrue);
		printf("... ready\n\n");
	
	} else {
		close (SRVR_Socket);
		SRVR_Socket = -1;
	}
	
	return SRVR_Socket;
}

//==============================================================================
void
SrvrReset()
{
	ClntInit (xFalse);
	WindInit (xFalse);
	CrsrInit (xFalse);
	PmapInit (xFalse);
	AtomInit (xFalse);
	PntrInit (xFalse);
	WmgrInit (xFalse);
	printf("... ready\n\n");
}

//==============================================================================
int
SrvrSelect (long rd_set, long wr_set)
{
	if (rd_set & SRVR_RdSet) {
		struct hostent   * peer;
		const char       * addr;
		struct sockaddr_in sock_in;
		size_t s_len    = sizeof(sock_in);
		
		int fd  = accept (SRVR_Socket, (struct sockaddr*)&sock_in, &s_len);
		if (fd < 0) {
			printf ("accept failed.\n");
		
		} else {
			fcntl (fd, F_SETFL, O_NONBLOCK);
			
			addr = inet_ntoa(sock_in.sin_addr);
			//peer = gethostbyaddr (addr, strlen(addr), AF_INET);
			peer = gethostbyname (addr);
			ClntCreate (fd, (peer ? peer->h_name : "<unknown>"),
			            addr, sock_in.sin_port);
		}
		if (!(rd_set &= ~SRVR_RdSet) && !wr_set) return 1;
	}
	return (ClntSelect (rd_set, wr_set));
}

//==============================================================================
size_t
SrvrSetup (void* buf, CARD16 maxreqlen, int DoSwap, long rid)
{
	struct s_setup * s   = (struct s_setup*)buf;
	char           * p   = ((char*)buf) + _SRVR_SetupBytes;
	size_t           len = _SRVR_SetupBytes;
	int              i;
	xDepth        ** d;
	
	if (DoSwap) {
		memcpy (s, &_SRVR_ConnLSB, _SRVR_SetupBytes);
		s->conn.ridBase        = Swap32(rid);
		s->conn.maxRequestSize = Swap16(maxreqlen);
		d = GRPH_DepthLSB;
	
	} else {
		memcpy (s, &_SRVR_ConnMSB, _SRVR_SetupBytes);
		s->conn.ridBase        = rid;
		s->conn.maxRequestSize = maxreqlen;
		d = GRPH_DepthMSB;
	}
	for (i = 0; i < GRPH_DepthNum; ++i) {
		memcpy (p, d[i], sizeof(xDepth) + sizeof(xVisualType));
		p   += sizeof(xDepth) + sizeof(xVisualType);
		len += sizeof(xDepth) + sizeof(xVisualType);
	}
	
/***{
	FILE * f = fopen ("_setup.txt", "w");
	int ii, j;
	for (ii = 0; ii < _SRVR_SetupBytes; ii += 8) {
		for (j = ii; j < ii+8 && j < _SRVR_SetupBytes; ++j) {
			fprintf (f, "%02x ", ((char*)buf)[j]);
		}
		fprintf (f, "\n");
	}
	fclose (f);
}***/
	if (_SRVR_SetupBytes > maxreqlen) {
		printf ("FATAL: setup length > maxreqlen (%li/%u)!\n",
		        _SRVR_SetupBytes, maxreqlen);
		exit (1);
	}
	return len;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_Bell (CLIENT * clnt, xBellReq * q)
{
	static CARD8 beepsnd[] = {
		   7,0xFE,  /* Soundkanal A einschalten */
		   9,0,     /* Lautst„rke Kanal B auf 0 */
		  10,0,     /* dito fr C */
		   0,50,    /*(28) Periodendauer Kanal A - Lowbyte */
		   1,0,     /*(1) dito, diesmal Highbyte */
		   8,0,     /* v -> Lautst„rke Kanal A */
		0xFE,0,     /* t -> L„nge */
		   7,0xFF,  /* Alle Kan„le wieder aus */
		   8,0,     /* Kanal A Lautst„rke 0 */
		0xFF,0      /* Und Ende */
	};
	CARD8 t = 5;  /* L„nge des Ger„usches (in 1/50tel Sekundeneinheiten) */
	CARD8 v = 15; /* Lautst„rke (von 0 bis 15) */
	
	beepsnd[11] = v;
	beepsnd[13] = t;
	Dosound (beepsnd);
	
	DEBUG (Bell," ");
}

//------------------------------------------------------------------------------
void
RQ_QueryExtension (CLIENT * clnt, xQueryExtensionReq * q)
{
	ClntReplyPtr (QueryExtension, r);
	
	PRINT (- X_QueryExtension," '%.*s'", q->nbytes, (char*)(q +1));
	
	r->present      = xFalse;
	r->major_opcode = 0;
	r->first_event  = 0;
	r->first_error  = 0;
	
	ClntReply (QueryExtension,, NULL);
}

//------------------------------------------------------------------------------
void
RQ_ListExtensions (CLIENT * clnt, xListExtensionsReq * _unused_)
{
	char ext[] = "GEM-WM_PROPERTIES";
	ClntReplyPtr (ListExtensions, r);
	
	PRINT (- X_ListExtensions," ");
	
	r->nExtensions = 1;
	{
		CARD8 * q = (CARD8*)(r +1);
		q[0] =     (CARD8)(sizeof(ext) -1);
		memcpy (q +1, ext, sizeof(ext) -1);
	}
	ClntReply (ListExtensions, (sizeof(ext)), "l");
}


//------------------------------------------------------------------------------
void
RQ_SetScreenSaver (CLIENT * clnt, xSetScreenSaverReq * q)
{
	PRINT (- X_SetScreenSaver," ");
}

//------------------------------------------------------------------------------
void
RQ_ForceScreenSaver (CLIENT * clnt, xForceScreenSaverReq * q)
{
	PRINT (- X_ForceScreenSaver," ");
}

//------------------------------------------------------------------------------
void
RQ_GetScreenSaver (CLIENT * clnt, xGetScreenSaverReq * q)
{
	PRINT (- X_GetScreenSaver," ");
}


//------------------------------------------------------------------------------
void
RQ_ChangeKeyboardControl (CLIENT * clnt, xChangeKeyboardControlReq * q)
{
	PRINT (- X_ChangeKeyboardControl," ");
}

//------------------------------------------------------------------------------
void
RQ_GetKeyboardControl (CLIENT * clnt, xGetKeyboardControlReq * q)
{
	// Returns the current control values for the keyboard
	//
	// (no request parameters)
	//
	// BOOL   globalAutoRepeat:
	// CARD16 sequenceNumber:
	// CARD32 ledMask:
	// CARD8  keyClickPercent:
	// CARD8  bellPercent:
	// CARD16 bellPitch:
	// CARD16 bellDuration:
	// BYTE   map[32]:         bit masks start here
	//...........................................................................
	
	ClntReplyPtr (GetKeyboardControl, r);
	
	memset (r, sizeof(xGetKeyboardControlReply), 0);
	
	ClntReply (GetKeyboardControl,, "l2:");
	
	PRINT (- X_GetKeyboardControl," ");
}


//------------------------------------------------------------------------------
void
RQ_ChangeHosts (CLIENT * clnt, xChangeHostsReq * q)
{
	PRINT (- X_ChangeHosts," ");
}

//------------------------------------------------------------------------------
void
RQ_ListHosts (CLIENT * clnt, xListHostsReq * q)
{
	PRINT (- X_ListHosts," ");
}

//------------------------------------------------------------------------------
void
RQ_SetAccessControl (CLIENT * clnt, xSetAccessControlReq * q)
{
	PRINT (- X_SetAccessControl," ");
}


//------------------------------------------------------------------------------
void
RQ_NoOperation (CLIENT * clnt, xNoOperationReq * _unused_)
{
	DEBUG (NoOperation," ");
}
