// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_UTIL_H_
#define UI_GFX_X_X11_UTIL_H_

#include <stdint.h>

#include <memory>

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/x/connection.h"

typedef unsigned long XID;
typedef unsigned long VisualID;
typedef union _XEvent XEvent;
typedef struct _XImage XImage;
typedef struct _XGC* GC;
typedef struct _XDisplay XDisplay;
typedef struct _XRegion XRegion;
typedef struct __GLXFBConfigRec* GLXFBConfig;
typedef XID GLXWindow;
typedef XID GLXDrawable;

extern "C" {
int XFree(void*);
}

namespace gfx {

template <class T, class R, R (*F)(T*)>
struct XObjectDeleter {
  inline void operator()(void* ptr) const { F(static_cast<T*>(ptr)); }
};

template <class T, class D = XObjectDeleter<void, int, XFree>>
using XScopedPtr = std::unique_ptr<T, D>;

// Get the XDisplay singleton.  Prefer x11::Connection::Get() instead.
GFX_EXPORT XDisplay* GetXDisplay();

}  // namespace gfx

#endif  // UI_GFX_X_X11_UTIL_H_
