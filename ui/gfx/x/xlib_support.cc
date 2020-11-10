// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xlib_support.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "library_loaders/xlib_loader.h"

namespace x11 {

namespace {

int XlibErrorHandler(void*, void*) {
  DVLOG(1) << "Xlib error received";
  return 0;
}

}  // namespace

DISABLE_CFI_ICALL
XlibDisplay::XlibDisplay(const std::string& address) {
  xlib_loader_ = std::make_unique<XlibLoader>();
  CHECK(xlib_loader_->Load("libX11.so.6"));

  CHECK(xlib_loader_->XInitThreads());

  // The default Xlib error handler calls exit(1), which we don't want.  This
  // shouldn't happen in the browser process since only XProto requests are
  // made, but in the GPU process, GLX can make Xlib requests, so setting an
  // error handler is necessary.  Importantly, there's also an IO error handler,
  // and Xlib always calls exit(1) with no way to change this behavior.
  xlib_loader_->XSetErrorHandler(XlibErrorHandler);

  display_ =
      xlib_loader_->XOpenDisplay(address.empty() ? nullptr : address.c_str());
}

DISABLE_CFI_ICALL
XlibDisplay::~XlibDisplay() {
  if (display_)
    xlib_loader_->XCloseDisplay(display_);
}

DISABLE_CFI_ICALL
XlibDisplayWrapper::XlibDisplayWrapper(XlibLoader* xlib_loader,
                                       struct _XDisplay* display,
                                       XlibDisplayType type)
    : xlib_loader_(xlib_loader), display_(display), type_(type) {
  if (!display_)
    return;
  if (type == XlibDisplayType::kSyncing)
    xlib_loader_->XSynchronize(display_, true);
}

DISABLE_CFI_ICALL
XlibDisplayWrapper::~XlibDisplayWrapper() {
  if (!display_)
    return;
  if (type_ == XlibDisplayType::kFlushing)
    xlib_loader_->XFlush(display_);
  else if (type_ == XlibDisplayType::kSyncing)
    xlib_loader_->XSynchronize(display_, false);
}

XlibDisplayWrapper::XlibDisplayWrapper(XlibDisplayWrapper&& other) {
  xlib_loader_ = other.xlib_loader_;
  display_ = other.display_;
  type_ = other.type_;
  other.xlib_loader_ = nullptr;
  other.display_ = nullptr;
  other.type_ = XlibDisplayType::kNormal;
}

XlibDisplayWrapper& XlibDisplayWrapper::operator=(XlibDisplayWrapper&& other) {
  xlib_loader_ = other.xlib_loader_;
  display_ = other.display_;
  type_ = other.type_;
  other.xlib_loader_ = nullptr;
  other.display_ = nullptr;
  other.type_ = XlibDisplayType::kNormal;
  return *this;
}

}  // namespace x11
