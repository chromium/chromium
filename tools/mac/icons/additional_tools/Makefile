CXXFLAGS ?= -g -Os
DSYMUTIL ?= dsymutil
STRIP ?= strip
PNG_INCLUDES ?= -I/opt/local/include
PNG_LIBS ?= -L/opt/local/lib -lpng

all: makeicns unmakeicns unpackicon makepng

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

makepng.o: makepng.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(PNG_INCLUDES)

makeicns: makeicns.o
	$(CXX) -Wl,-dead_strip $^ -o $@
	$(DSYMUTIL) $@
	$(STRIP) $@

unmakeicns: unmakeicns.o unpackicon.o makepng.o
	$(CXX) -Wl,-dead_strip $^ -o $@ $(PNG_LIBS)
	$(DSYMUTIL) $@
	$(STRIP) $@

unpackicon: unpackicon_main.o unpackicon.o
	$(CXX) -Wl,-dead_strip $^ -o $@
	$(DSYMUTIL) $@
	$(STRIP) $@

makepng: makepng_main.o makepng.o
	$(CXX) -Wl,-dead_strip $^ -o $@ $(PNG_LIBS)
	$(DSYMUTIL) $@
	$(STRIP) $@

.PHONY: clean
clean:
	$(RM) -f *.o makeicns unmakeicns unpackicon
	$(RM) -rf makeicns.dSYM unmakeicns.dSYM unpackicon.dSYM
