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

// Wraps XFree().
COMPONENT_EXPORT(X11) void XlibFree(void* data);

// A scoped Xlib display.
class COMPONENT_EXPORT(X11) XlibDisplay {
 public:
  ~XlibDisplay();

 private:
  friend class Connection;
  friend class XlibDisplayWrapper;

  explicit XlibDisplay(const std::string& address);

  raw_ptr<struct _XDisplay> display_ = nullptr;
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

  struct xcb_connection_t* GetXcbConnection();

  XlibDisplayWrapper(XlibDisplayWrapper&& other);
  XlibDisplayWrapper& operator=(XlibDisplayWrapper&& other);

 private:
  XlibDisplayWrapper(struct _XDisplay* display, XlibDisplayType type);

  friend class Connection;

  raw_ptr<struct _XDisplay> display_;
  XlibDisplayType type_;
};

}  // namespace x11

#endif  // UI_GFX_X_XLIB_SUPPORT_H_
