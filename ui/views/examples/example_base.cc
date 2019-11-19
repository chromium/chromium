// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/example_base.h"

#include <stdarg.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

// Logs the specified string to the status area of the examples window.
// This function can only be called if there is a visible examples window.
void LogStatus(const std::string& status);

ExampleBase::~ExampleBase() = default;

ExampleBase::ExampleBase(const char* title) : example_title_(title) {
  container_ = new View();
}

// Prints a message in the status area, at the bottom of the window.
void ExampleBase::PrintStatus(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string msg;
  base::StringAppendV(&msg, format, ap);
  LogStatus(msg);
}

}  // namespace examples
}  // namespace views
