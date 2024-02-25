// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XLIB_SUPPORT_H_
#define UI_GFX_X_XLIB_SUPPORT_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

struct _XDisplay;
struct xcb_connection_t;

namespace x11 {

// Loads Xlib, initializes threads, and sets a default error handler.
COMPONENT_EXPORT(X11) void InitXlib();

// Sets an async error handler which only logs an error message.
COMPONENT_EXPORT(X11) void SetXlibErrorHandler();

// Wraps XFree().
COMPONENT_EXPORT(X11) void XlibFree(void* data);

// A scoped Xlib display.
class COMPONENT_EXPORT(X11) XlibDisplay {
 public:
  ~XlibDisplay();

  struct _XDisplay* display() { return display_; }

  operator struct _XDisplay *() { return display_; }

  struct xcb_connection_t* GetXcbConnection();

 private:
  friend class Connection;

  explicit XlibDisplay(const std::string& address);

  raw_ptr<struct _XDisplay> display_ = nullptr;
};

}  // namespace x11

#endif  // UI_GFX_X_XLIB_SUPPORT_H_
