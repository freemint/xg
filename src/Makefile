WARN    = -Wall -Wpointer-arith -Wwrite-strings -Wmissing-prototypes -Wshadow \
          -Werror #-Wcast-qual
CCOPTS  = -D_TRACE #-g -O2 -fstrength-reduce #-DNODEBUG
CCFLAGS = -funsigned-char -fomit-frame-pointer \
          -I. -I../include -I/usr/GEM/include \
          $(CCOPTS) $(WARN)

SHELL = /bin/ash

#TMPDIR = /ram/tmp

CC  = gcc
CXX = g++ #-pipe
LD  = gcc -g
AR  = ar rs

PRG    = eeks.app
OBJDIR = ../obj
LIBS   = gem socket
DEPEND = $(OBJDIR)/depends

LDFLAGS = -v $(CCOPTS)


$(OBJDIR)/%.o: %.c ; rm -f $(PRG) ; $(CC) $(CCFLAGS) -D_$(<:.c=)_ -c $< -o $@


ALL: $(PRG)

SRC = main.c srvr.c clnt.c clnt_swap.c Request.c wmgr.c \
      Atom.c selection.c Property.c colormap.c Cursor.c Pointer.c keyboard.c \
      fontable.c gcontext.c font_init.c font.c \
      drawable.c pixmap.c pixm_mono.c grph.c \
      window.c wind_draw.c wind_grab.c wind_pntr.c event.c \
      xrsc.c x_gem.c

OBJ = $(SRC:%.c=$(OBJDIR)/%.o)


$(PRG): $(OBJDIR) $(OBJ) Makefile version.c
	@expr "0`cat -s $(OBJDIR)/build`" + 1 > $(OBJDIR)/build
	@$(CC) $(CCFLAGS) -c version.c \
	       -DDATE=\"`date +%m%d`\" -DBUILD=\"`cat -s $(OBJDIR)/build`\" \
	       -o $(OBJDIR)/version.o
	$(LD) $(LDFLAGS) -o $@ $(OBJ) $(OBJDIR)/version.o $(LIBS:%=-l%)
	stack -S 128k $@


$(OBJDIR):
	@echo "*** Prepare obj-Directory ***"
	@mkdir $(OBJDIR)
	@echo "0" > $(OBJDIR)/build
	@echo "Create dependencies ..."
	@$(MAKE) $(DPND)
	@echo "*** Make $(PRG) ***"


package: ALL
	cp eeks.app eeks-g.app
	tar c eeks-g.app eeks.rsc | gzip -9 > ../`date +eeks%m%d-g.tgz` ;\
	    \rm -f eeks-g.app &
	strip eeks.app
	tar c eeks.app eeks.rsc | gzip -9 > ../`date +eeks%m%d.tgz` &
	@cd ../..;\
	tar cv EEKS/include/*.h EEKS/obj/build\
	    EEKS/src/Makefile EEKS/src/*.c EEKS/src/eeks.h EEKS/src/eeks.rsc\
	| gzip -9 > EEKS/`date +eeks%m%d-src.tgz`

clean: ; rm -f *.% *.[bB][aA][kK] ../include/*.%


#_______________________________________________________________________________
DPND = $(SRC:.c=.d)
depend: __udpnd Makefile $(DPND)

__udpnd: ; rm -f $(DEPEND)

$(DPND):
	$(CC) -pipe $(CCFLAGS: $(WARN)=) -M $(@:%.d=%.c) -D_$(@:.d=)_ \
	| sed "s,^\(.*\)\.o:,$(OBJDIR)/\1.o:," >> $(DEPEND)

-include $(OBJDIR)/depends
