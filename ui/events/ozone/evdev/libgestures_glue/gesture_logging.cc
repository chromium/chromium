// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_logging.h"

#include <gestures/gestures.h>
#include <stdarg.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace {

std::string FormatLog(const char* fmt, va_list args) {
  std::string msg = base::StringPrintV(fmt, args);
  if (!msg.empty() && msg.back() == '\n')
    msg.pop_back();
  return msg;
}

}  // namespace

void gestures_log(int verb, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (verb <= GESTURES_LOG_ERROR)
    LOG(ERROR) << "gestures: " << FormatLog(fmt, args);
  else if (verb <= GESTURES_LOG_INFO)
    VLOG(3) << "gestures: " << FormatLog(fmt, args);
  va_end(args);
}
