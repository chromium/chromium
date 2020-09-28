// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_util.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

void LogErrorEventDescription(unsigned long serial,
                              uint8_t error_code,
                              uint8_t request_code,
                              uint8_t minor_code) {
  // This function may make some expensive round trips (XListExtensions,
  // XQueryExtension), but the only effect this function has is LOG(WARNING),
  // so early-return if the log would never be sent anyway.
  if (!LOG_IS_ON(WARNING))
    return;

  char error_str[256];
  char request_str[256];

  x11::Connection* conn = x11::Connection::Get();
  auto* dpy = conn->display();
  XGetErrorText(dpy, error_code, error_str, sizeof(error_str));

  strncpy(request_str, "Unknown", sizeof(request_str));
  if (request_code < 128) {
    std::string num = base::NumberToString(request_code);
    XGetErrorDatabaseText(dpy, "XRequest", num.c_str(), "Unknown", request_str,
                          sizeof(request_str));
  } else {
    if (auto response = conn->ListExtensions({}).Sync()) {
      for (const auto& str : response->names) {
        const char* name = str.name.c_str();
        auto query = conn->QueryExtension({name}).Sync();
        if (query && request_code == query->major_opcode) {
          std::string msg = base::StringPrintf("%s.%d", name, minor_code);
          XGetErrorDatabaseText(dpy, "XRequest", msg.c_str(), "Unknown",
                                request_str, sizeof(request_str));
          break;
        }
      }
    }
  }

  LOG(WARNING) << "X error received: "
               << "serial " << serial << ", "
               << "error_code " << static_cast<int>(error_code) << " ("
               << error_str << "), "
               << "request_code " << static_cast<int>(request_code) << ", "
               << "minor_code " << static_cast<int>(minor_code) << " ("
               << request_str << ")";
}

}  // namespace x11
