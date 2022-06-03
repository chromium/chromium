// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include <sstream>

#include "third_party/libjingle_xmpp/xmllite/qname.h"
#include "third_party/libjingle_xmpp/xmllite/xmlbuilder.h"
#include "third_party/libjingle_xmpp/xmllite/xmlparser.h"
#include "third_party/libjingle_xmpp/xmllite/xmlprinter.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlBuilder;
using jingle_xmpp::XmlParser;
using jingle_xmpp::XmlPrinter;

XmlBuilder builder;
XmlParser parser(&builder);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1)
    return 0;

  uint8_t* data_copy = new uint8_t[size + 1];
  memcpy(data_copy, data, size);
  data_copy[size] = 0;

  if (parser.Parse(reinterpret_cast<char*>(data_copy), size, true)) {
    std::stringstream ss;
    XmlPrinter::PrintXml(&ss, builder.BuiltElement());
  }
  parser.Reset();
  delete[] data_copy;
  return 0;
}
