#ifndef __SERVER_H__
#	define __SERVER_H__


int    SrvrInit   (int port);
void   SrvrReset  (void);
int    SrvrSelect (long rd_set, long wr_set);
size_t SrvrSetup  (void * buf, CARD16 maxreqlen, int DoSwap, long rid);


#endif __SERVER_H__
