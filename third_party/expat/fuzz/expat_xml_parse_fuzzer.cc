// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "third_party/expat/src/expat/lib/expat.h"

#include <vector>

std::vector<const char*> kEncodings = {{"UTF-16", "UTF-8", "ISO-8859-1",
                                        "US-ASCII", "UTF-16BE", "UTF-16LE",
                                        "INVALIDENCODING"}};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const char* dataPtr = reinterpret_cast<const char*>(data);

  for (int use_ns = 0; use_ns <= 1; ++use_ns) {
    for (auto enc : kEncodings) {
      XML_Parser parser =
          use_ns ? XML_ParserCreateNS(enc, '\n') : XML_ParserCreate(enc);
      XML_Parse(parser, dataPtr, size, true);
      XML_ParserFree(parser);
    }
  }

  return 0;
}
