// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XLIB_SUPPORT_H_
#define UI_GFX_X_XLIB_SUPPORT_H_

#include <string>

#include "base/component_export.h"

struct _XDisplay;

namespace x11 {

// Specifies the behavior of XlibDisplayWrapper.
enum class XlibDisplayType {
  // No action taken on wrapper construction or destruction.
  kNormal,

  // Flushes the connection on destruction.
  kFlushing,

  // Synchronizes all requests while the wrapper is alive.
  kSyncing,
};

// Loads Xlib, initializes threads, and sets a default error handler.
COMPONENT_EXPORT(X11) void InitXlib();

// Sets an async error handler which only logs an error message.
COMPONENT_EXPORT(X11) void SetXlibErrorHandler();

// A scoped Xlib display.
class COMPONENT_EXPORT(X11) XlibDisplay {
 public:
  ~XlibDisplay();

 private:
  friend class Connection;
  friend class XlibDisplayWrapper;

  explicit XlibDisplay(const std::string& address);

  struct _XDisplay* display_ = nullptr;
};

// A temporary wrapper around an unowned Xlib display that adds behavior
// on construction and destruction (see XlibDisplayType).
class COMPONENT_EXPORT(X11) XlibDisplayWrapper {
 public:
  ~XlibDisplayWrapper();

  struct _XDisplay* display() {
    return display_;
  }
  operator struct _XDisplay *() { return display_; }

  XlibDisplayWrapper(XlibDisplayWrapper&& other);
  XlibDisplayWrapper& operator=(XlibDisplayWrapper&& other);

 private:
  XlibDisplayWrapper(struct _XDisplay* display, XlibDisplayType type);

  friend class Connection;

  struct _XDisplay* display_;
  XlibDisplayType type_;
};

}  // namespace x11

#endif  // UI_GFX_X_XLIB_SUPPORT_H_
