# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PUFFIN_SOURCES = \
	bit_reader.cc \
	bit_writer.cc \
	extent_stream.cc \
	file_stream.cc \
	huffer.cc \
	huffman_table.cc \
	memory_stream.cc \
	puffer.cc \
	puff_reader.cc \
	puff_writer.cc \
	puffin_stream.cc \
	utils.cc

UNITTEST_SOURCES = \
	bit_io_unittest.cc \
	puff_io_unittest.cc \
	puffin_unittest.cc \
	stream_unittest.cc \
	testrunner.cc \
	utils_unittest.cc

OBJDIR = obj
SRCDIR = src
PUFFIN_OBJECTS = $(addprefix $(OBJDIR)/, $(PUFFIN_SOURCES:.cc=.o))
UNITTEST_OBJECTS = $(addprefix $(OBJDIR)/, $(UNITTEST_SOURCES:.cc=.o))

LIBPUFFIN = libpuffin.so
UNITTESTS = puffin_unittests

CXXFLAGS ?= -O3 -ggdb
CXXFLAGS += -Wall -fPIC -std=c++14
CPPFLAGS += -I../ -Isrc/include
LDLIBS = -lgflags -lglog -lprotobuf-lite -lgtest -lpthread

VPATH = $(SRCDIR)

all: $(LIBPUFFIN)

$(OBJDIR):
	mkdir -p $@

$(PUFFIN_OBJECTS): | $(OBJDIR)

$(LIBPUFFIN): $(PUFFIN_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $^ -o $@ $(LDLIBS)

$(UNITTESTS): $(UNITTEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LIBPUFFIN) $(LDLIBS)

test: $(LIBPUFFIN) $(UNITTESTS)

clean:
	rm -rf $(OBJDIR) $(LIBPUFFIN) $(UNITTESTS)

$(OBJDIR)/%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean test
