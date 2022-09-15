// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XLIB_H_
#define UI_GFX_X_XLIB_H_

extern "C" {
int XInitThreads(void);
struct _XDisplay* XOpenDisplay(const char*);
int XCloseDisplay(struct _XDisplay*);
int XFlush(struct _XDisplay*);
int XSynchronize(struct _XDisplay*, int);
int XSetErrorHandler(int (*)(void*, void*));
void XFree(void*);
int XPending(struct _XDisplay*);
}

#endif  // UI_GFX_X_XLIB_H_
