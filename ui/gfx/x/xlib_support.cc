// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xlib_support.h"

#include "base/check.h"
#include "base/logging.h"

extern "C" {
int XInitThreads(void);
struct _XDisplay* XOpenDisplay(const char*);
int XCloseDisplay(struct _XDisplay*);
int XFlush(struct _XDisplay*);
int XSynchronize(struct _XDisplay*, int);
int XSetErrorHandler(int (*)(void*, void*));
}

namespace x11 {

namespace {

int XlibErrorHandler(void*, void*) {
  DVLOG(1) << "Xlib error received";
  return 0;
}

}  // namespace

XlibDisplay::XlibDisplay(const std::string& address) {
  CHECK(XInitThreads());

  // The default Xlib error handler calls exit(1), which we don't want.  This
  // shouldn't happen in the browser process since only XProto requests are
  // made, but in the GPU process, GLX can make Xlib requests, so setting an
  // error handler is necessary.  Importantly, there's also an IO error handler,
  // and Xlib always calls exit(1) with no way to change this behavior.
  XSetErrorHandler(XlibErrorHandler);

  display_ = XOpenDisplay(address.empty() ? nullptr : address.c_str());
}

XlibDisplay::~XlibDisplay() {
  if (display_)
    XCloseDisplay(display_);
}

XlibDisplayWrapper::XlibDisplayWrapper(struct _XDisplay* display,
                                       XlibDisplayType type)
    : display_(display), type_(type) {
  if (!display_)
    return;
  if (type == XlibDisplayType::kSyncing)
    XSynchronize(display_, true);
}

XlibDisplayWrapper::~XlibDisplayWrapper() {
  if (!display_)
    return;
  if (type_ == XlibDisplayType::kFlushing)
    XFlush(display_);
  else if (type_ == XlibDisplayType::kSyncing)
    XSynchronize(display_, false);
}

XlibDisplayWrapper::XlibDisplayWrapper(XlibDisplayWrapper&& other) {
  display_ = other.display_;
  type_ = other.type_;
  other.display_ = nullptr;
  other.type_ = XlibDisplayType::kNormal;
}

XlibDisplayWrapper& XlibDisplayWrapper::operator=(XlibDisplayWrapper&& other) {
  display_ = other.display_;
  type_ = other.type_;
  other.display_ = nullptr;
  other.type_ = XlibDisplayType::kNormal;
  return *this;
}

}  // namespace x11
