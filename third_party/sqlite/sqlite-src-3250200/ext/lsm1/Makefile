#
# This Makefile is designed for use with main.mk in the root directory of
# this project. After including main.mk, the users makefile should contain:
#
#    LSMDIR=$(TOP)/ext/lsm1/
#    LSMOPTS=-fPIC
#    include $(LSMDIR)/Makefile
#
# The most useful targets are [lsmtest] and [lsm.so].
#

LSMOBJ    = \
  lsm_ckpt.o \
  lsm_file.o \
  lsm_log.o \
  lsm_main.o \
  lsm_mem.o \
  lsm_mutex.o \
  lsm_shared.o \
  lsm_sorted.o \
  lsm_str.o \
  lsm_tree.o \
  lsm_unix.o \
  lsm_win32.o \
  lsm_varint.o \
  lsm_vtab.o

LSMHDR   = \
  $(LSMDIR)/lsm.h \
  $(LSMDIR)/lsmInt.h

LSMTESTSRC = $(LSMDIR)/lsm-test/lsmtest1.c $(LSMDIR)/lsm-test/lsmtest2.c     \
             $(LSMDIR)/lsm-test/lsmtest3.c $(LSMDIR)/lsm-test/lsmtest4.c     \
             $(LSMDIR)/lsm-test/lsmtest5.c $(LSMDIR)/lsm-test/lsmtest6.c     \
             $(LSMDIR)/lsm-test/lsmtest7.c $(LSMDIR)/lsm-test/lsmtest8.c     \
             $(LSMDIR)/lsm-test/lsmtest9.c                                   \
             $(LSMDIR)/lsm-test/lsmtest_datasource.c \
             $(LSMDIR)/lsm-test/lsmtest_func.c $(LSMDIR)/lsm-test/lsmtest_io.c  \
             $(LSMDIR)/lsm-test/lsmtest_main.c $(LSMDIR)/lsm-test/lsmtest_mem.c \
             $(LSMDIR)/lsm-test/lsmtest_tdb.c $(LSMDIR)/lsm-test/lsmtest_tdb3.c \
             $(LSMDIR)/lsm-test/lsmtest_util.c $(LSMDIR)/lsm-test/lsmtest_win32.c


# all: lsm.so

LSMOPTS += -DLSM_MUTEX_PTHREADS=1 -I$(LSMDIR) -DHAVE_ZLIB

lsm.so:	$(LSMOBJ)
	$(TCCX) -shared -o lsm.so $(LSMOBJ)

%.o:	$(LSMDIR)/%.c $(LSMHDR) sqlite3.h
	$(TCCX) $(LSMOPTS) -c $<
	
lsmtest$(EXE): $(LSMOBJ) $(LSMTESTSRC) $(LSMTESTHDR) sqlite3.o
	# $(TCPPX) -c $(TOP)/lsm-test/lsmtest_tdb2.cc
	$(TCCX) $(LSMOPTS) $(LSMTESTSRC) $(LSMOBJ) sqlite3.o -o lsmtest$(EXE) $(THREADLIB) -lz
