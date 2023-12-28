// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xlib_support.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "library_loaders/xlib_loader.h"
#include "library_loaders/xlib_xcb_loader.h"

namespace x11 {

namespace {

int XlibErrorHandler(void*, void*) {
  DVLOG(1) << "Xlib error received";
  return 0;
}

XlibLoader* GetXlibLoader() {
  static base::NoDestructor<XlibLoader> xlib_loader;
  return xlib_loader.get();
}

XlibXcbLoader* GetXlibXcbLoader() {
  static base::NoDestructor<XlibXcbLoader> xlib_xcb_loader;
  return xlib_xcb_loader.get();
}

}  // namespace

DISABLE_CFI_DLSYM
void InitXlib() {
  auto* xlib_loader = GetXlibLoader();
  if (xlib_loader->loaded()) {
    return;
  }

  CHECK(xlib_loader->Load("libX11.so.6"));

  auto* xlib_xcb_loader = GetXlibXcbLoader();
  CHECK(xlib_xcb_loader->Load("libX11-xcb.so.1"));

  CHECK(xlib_loader->XInitThreads());

  // The default Xlib error handler calls exit(1), which we don't want.  This
  // shouldn't happen in the browser process since only XProto requests are
  // made, but in the GPU process, GLX can make Xlib requests, so setting an
  // error handler is necessary.  Importantly, there's also an IO error handler,
  // and Xlib always calls exit(1) with no way to change this behavior.
  SetXlibErrorHandler();
}

DISABLE_CFI_DLSYM
void SetXlibErrorHandler() {
  GetXlibLoader()->XSetErrorHandler(XlibErrorHandler);
}

DISABLE_CFI_DLSYM
void XlibFree(void* data) {
  GetXlibLoader()->XFree(data);
}

DISABLE_CFI_DLSYM
XlibDisplay::XlibDisplay(const std::string& address) {
  InitXlib();

  display_ = GetXlibLoader()->XOpenDisplay(address.empty() ? nullptr
                                                           : address.c_str());
}

DISABLE_CFI_DLSYM
XlibDisplay::~XlibDisplay() {
  if (!display_) {
    return;
  }

  auto* loader = GetXlibLoader();
  // Events are not processed on |display_|, so if any client asks to receive
  // events, they will just queue up and leak memory.  This check makes sure
  // |display_| never had any pending events before it is closed.
  CHECK(!loader->XPending(display_));
  // ExtractAsDangling clears the underlying pointer and returns another raw_ptr
  // instance that is allowed to dangle.
  loader->XCloseDisplay(display_.ExtractAsDangling());
}

DISABLE_CFI_DLSYM
struct xcb_connection_t* XlibDisplay::GetXcbConnection() {
  return GetXlibXcbLoader()->XGetXCBConnection(display_);
}

}  // namespace x11
