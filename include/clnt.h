#ifndef __CLNT_H__
# define __CLNT_H__

#include "xrsc.h"
#include <stdarg.h>


struct _xReq;   typedef struct _xReq *   p_xReq;


#define CLNTENTLEN        32


typedef struct {
	size_t Left;
	size_t Done;
	char * Mem;
} NETBUF;


//--- Request Callback Functions ---
typedef void (*RQSTCB)(p_CLIENT , p_xReq);

#ifndef __p_xArc
# define __p_xArc
	struct _xArc; typedef struct _xArc * p_xArc;
#endif
typedef struct {
	void (*reply)(p_CLIENT , CARD32 size, const char * form);
	void (*error)(p_CLIENT , CARD8 code, CARD8 majOp, CARD16 minOp, CARD32 val);
	void (*event)(p_CLIENT , p_WINDOW , CARD16 evnt, va_list);
	void (*shift_arc)(const p_PXY origin, p_xArc  arc, size_t num);
	void (*shift_pnt)(const p_PXY origin, p_PXY   pxy, size_t num, short mode);
	void (*shift_r2p)(const p_PXY origin, p_GRECT rct, size_t num);
} FUNCTABL;


typedef struct s_CLIENT {
	XRSC(CLIENT,       _unused);
	CARD16             SeqNum;
	char             * Name;
	char             * Addr;
	int                Port;
	int                Fd;
	NETBUF             oBuf;
	NETBUF             iBuf;
	RQSTCB             Eval;
	const FUNCTABL   * Fnct;
	BOOL               DoSwap;
	BYTE               CloseDown;
	CARD32             EventReffs;
	XRSCPOOL(DRAWABLE, Drawables, 6);
	XRSCPOOL(FONTABLE, Fontables, 5);
	XRSCPOOL(CURSOR,   Cursors,   4);
	char               Entry[CLNTENTLEN+2];
} CLIENT;


typedef struct {
	RQSTCB       func;
	const char * Name;
	const char * Form;
} REQUEST;
extern const REQUEST RequestTable[/*FirstExtensionError*/];


static inline CLIENT * ClntFind (CARD32 id) {
	extern XRSCPOOL(CLIENT, CLNT_Pool,0);
	return Xrsc(CLIENT, RID_Base(id), CLNT_Pool);
}


void   ClntInit   (BOOL initNreset);
int    ClntSelect (long rd_set, long wr_set);
int    ClntCreate (int fd, const char * name, const char * addr, int port);
int    ClntDelete (CLIENT *);
void * ClntSwap   (void * buf, const char * form);
void   ClntPrint  (CLIENT *, int req, const char * form,
                   ...) __attribute__ ((format (printf, 3, 4)));
void   ClntError  (CLIENT *, int err, CARD32 val, int req, const char * form,
                   ...) __attribute__ ((format (printf, 5, 6)));

#define ClntReplyPtr(T,r) x##T##Reply * r = (x##T##Reply*)(clnt->oBuf.Mem \
                                          + clnt->oBuf.Left + clnt->oBuf.Done)
#define ClntReply(T,s,f)   clnt->Fnct->reply (clnt, sz_x##T##Reply + (s +0), f)


#define X_   0
#define PRINT( req, frm, args...)   ClntPrint (clnt, X_##req, frm, ## args)

#ifndef NODEBUG
# define Bad( err, val, req, frm, args...) \
	ClntError (clnt, Bad##err, val +0, X_##req, "_" frm, ## args)
#else
# define Bad( err, val, req, frm, args...) \
	clnt->Fnct->error (clnt, Bad##err, X_##req,0, val +0)
#endif

#ifdef TRACE
# define DEBUG( req, frm, args...)   PRINT (req, frm, ## args)
#else
# define DEBUG( req, frm, args...)
#endif


extern CARD32 CNFG_MaxReqLength;                      // length in units (longs)
#define       CNFG_MaxReqBytes (CNFG_MaxReqLength *4) // same in bytes


//void _Xrsc_Free(void*);
//#define free(m)  _Xrsc_Free(m)


#endif __CLNT_H__
