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

enum XEventQueueOwner { XlibOwnsEventQueue = 0, XCBOwnsEventQueue };

int XInitThreads(void);
struct _XDisplay* XOpenDisplay(const char*);
int XCloseDisplay(struct _XDisplay*);
int XFlush(struct _XDisplay*);
struct xcb_connection_t* XGetXCBConnection(struct _XDisplay* dpy);
void XSetEventQueueOwner(struct _XDisplay* dpy, enum XEventQueueOwner owner);
int (*XSynchronize(struct _XDisplay*, int))(struct _XDisplay*);
}

#endif  // UI_GFX_X_X11_H_
