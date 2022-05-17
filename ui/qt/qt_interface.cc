// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/qt_interface.h"

#include <cstdlib>
#include <cstring>

namespace qt {

String::String() = default;

String::String(const char* str) {
  if (str)
    str_ = strdup(str);
}

String::String(String&& other) {
  str_ = other.str_;
  other.str_ = nullptr;
}

String& String::operator=(String&& other) {
  free(str_);
  str_ = other.str_;
  other.str_ = nullptr;
  return *this;
}

String::~String() {
  free(str_);
}

}  // namespace qt
