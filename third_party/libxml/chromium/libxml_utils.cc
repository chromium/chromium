// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libxml/chromium/libxml_utils.h"

namespace internal {

std::string XmlStringToStdString(const xmlChar* xmlstring) {
  if (!xmlstring)
    return std::string();

  // xmlChar*s are UTF-8, so this cast is safe.
  return std::string(reinterpret_cast<const char*>(xmlstring));
}

}  // namespace internal
