//==============================================================================
//
// srvr.h
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-03 - Initial Version.
//==============================================================================
//
#ifndef __SERVER_H__
#	define __SERVER_H__


int    SrvrInit   (int port);
void   SrvrReset  (void);
BOOL   SrvrSelect (void);
size_t SrvrSetup  (void * buf, CARD16 maxreqlen, int DoSwap, long rid);


#endif __SERVER_H__
