# See LICENSE file for copyright and license details.

include config.mk

GEN = \
	runetype/isalpharune.c \
	runetype/iscntrlrune.c \
	runetype/isdigitrune.c \
	runetype/islowerrune.c \
	runetype/isspacerune.c \
	runetype/istitlerune.c \
	runetype/isupperrune.c \

GENOBJ = $(GEN:.c=.o)

SRC = \
	utf/chartorune.c \
	utf/fullrune.c \
	utf/runelen.c \
	utf/runetochar.c \
	utf/utfecpy.c \
	utf/utflen.c \
	utf/utfrrune.c \
	utf/utfrune.c \
	utf/utfutf.c \
	runestr/runestrcat.c \
	runestr/runestrchr.c \
	runestr/runestrcmp.c \
	runestr/runestrcpy.c \
	runestr/runestrdup.c \
	runestr/runestrecpy.c \
	runestr/runestrlen.c \
	runestr/runestrrchr.c \
	runestr/runestrstr.c \
	runetype/isalnumrune.c \
	runetype/isblankrune.c \
	runetype/isgraphrune.c \
	runetype/isprintrune.c \
	runetype/ispunctrune.c \
	runetype/isvalidrune.c \
	runetype/isxdigitrune.c \
	runetype/runetype.c \
	$(GEN)

OBJ = $(SRC:.c=.o)

TESTSRC = \
	test/boundary.c \
	test/kosme.c \
	test/malformed.c \
	test/overlong.c

TEST = $(TESTSRC:.c=)

LIB = lib/libutf.a
HDR = include/utf.h
MAN = share/man/rune.3 share/man/isalpharune.3

all: $(LIB)

$(LIB): $(OBJ)
	@echo AR -r $@
	@mkdir -p lib
	@rm -f $@
	@$(AR) -rcs $@ $(OBJ)

.c.o:
	@echo CC -c $<
	@$(CC) $(CFLAGS) -c -o $@ $<

.c:
	@echo CC -o $@
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIB)

$(OBJ): $(HDR)

$(GEN): bin/mkrunetype.awk share/UnicodeData-$(UNICODE).txt runetype/runetype.h runetype/runetypebody.h
	@echo AWK -f bin/mkrunetype.awk
	@$(AWK) -f bin/mkrunetype.awk share/UnicodeData-$(UNICODE).txt

$(GENOBJ) runetype/runetype.o: runetype/runetype.h

$(TEST): $(LIB) test/tap.h

tests: $(TEST)
	@echo testing
	@prove $(TEST)

install: $(LIB) $(HDR) $(MAN)
	@echo installing libutf to $(DESTDIR)$(PREFIX)
	@mkdir -p $(DESTDIR)$(PREFIX)/lib
	@cp $(LIB) $(DESTDIR)$(PREFIX)/lib/
	@mkdir -p $(DESTDIR)$(PREFIX)/include
	@cp $(HDR) $(DESTDIR)$(PREFIX)/include/
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/man3
	@cp share/man/rune.3 $(DESTDIR)$(PREFIX)/share/man/man3/
	@sed 's/$$UNICODE/$(UNICODE)/g' share/man/isalpharune.3 > $(DESTDIR)$(PREFIX)/share/man/man3/isalpharune.3

uninstall:
	@echo uninstalling libutf from $(DESTDIR)$(PREFIX)
	@rm -f $(DESTDIR)$(PREFIX)/lib/$(LIB)
	@rm -f $(DESTDIR)$(PREFIX)/include/utf.h
	@rm -f $(DESTDIR)$(PREFIX)/share/man/man3/rune.3
	@rm -f $(DESTDIR)$(PREFIX)/share/man/man3/isalpharune.3

clean:
	@echo cleaning
	@rm -f $(GEN) $(OBJ) $(LIB) $(TEST)
