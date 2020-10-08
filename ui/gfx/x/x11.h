// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file replaces <X11/Xlib.h>.  https://crbug.com/1066670 is
// tracking removing usage of Xlib altogether.  Do not add more Xlib
// declarations here.  The intention is to incrementally remove declarations
// until there is nothing left, at which point this file will be removed.

#ifndef UI_GFX_X_X11_H_
#define UI_GFX_X_X11_H_

#include "ui/gfx/x/connection.h"

// Temporarily declare Xlib symbols we require.  Do not add more Xlib
// declarations here and do not include anything from <X11/*>.

extern "C" {

using Status = int;
using Bool = int;
using XID = unsigned long;
using KeySym = XID;
using KeyCode = unsigned char;
using Window = XID;
using Pixmap = XID;
using Font = XID;
using VisualID = unsigned long;
using XPointer = char*;
using Colormap = XID;
using Cursor = XID;
using Atom = unsigned long;
using Time = unsigned long;
using GC = struct _XGC*;
using Display = struct _XDisplay;
using xcb_connection_t = struct xcb_connection_t;

enum XEventQueueOwner { XlibOwnsEventQueue = 0, XCBOwnsEventQueue };

Status XInitThreads(void);
Display* XOpenDisplay(const char*);
int XCloseDisplay(Display*);
int XFlush(Display*);
xcb_connection_t* XGetXCBConnection(Display* dpy);
void XSetEventQueueOwner(Display* dpy, enum XEventQueueOwner owner);
int (*XSynchronize(Display*, Bool))(Display*);
}

#endif  // UI_GFX_X_X11_H_
