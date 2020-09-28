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

using XErrorEvent = struct _XErrorEvent {
  int type;
  Display* display;
  XID resourceid;
  unsigned long serial;
  unsigned char error_code;
  unsigned char request_code;
  unsigned char minor_code;
};

using XRectangle = struct _XRectangle {
  short x, y;
  unsigned short width, height;
};

using XSetWindowAttributes = struct {
  Pixmap background_pixmap;
  unsigned long background_pixel;
  Pixmap border_pixmap;
  unsigned long border_pixel;
  int bit_gravity;
  int win_gravity;
  int backing_store;
  unsigned long backing_planes;
  unsigned long backing_pixel;
  Bool save_under;
  long event_mask;
  long do_not_propagate_mask;
  Bool override_redirect;
  Colormap colormap;
  Cursor cursor;
};

using XModifierKeymap = struct {
  int max_keypermod;
  KeyCode* modifiermap;
};

using XErrorHandler = int (*)(Display*, XErrorEvent*);
using XIOErrorHandler = int (*)(Display*);

Status XInitThreads(void);
Display* XOpenDisplay(const char*);
int XCloseDisplay(Display*);
char* XDisplayString(Display*);
int XFlush(Display*);
xcb_connection_t* XGetXCBConnection(Display* dpy);
void XSetEventQueueOwner(Display* dpy, enum XEventQueueOwner owner);
int XDefaultScreen(Display*);
Window XDefaultRootWindow(Display*);
unsigned long XLastKnownRequestProcessed(Display*);
int (*XSynchronize(Display*, Bool))(Display*);
int XGetErrorDatabaseText(Display*,
                          const char*,
                          const char*,
                          const char*,
                          char*,
                          int);
int XGetErrorText(Display*, int, char*, int);
XErrorHandler XSetErrorHandler(XErrorHandler);
XIOErrorHandler XSetIOErrorHandler(XIOErrorHandler);
void XLockDisplay(Display*);
extern void XUnlockDisplay(Display*);
int XConnectionNumber(Display*);
int XSelectInput(Display*, Window, long);
int XSetWindowBackgroundPixmap(Display*, Window, Pixmap);
int XResizeWindow(Display*, Window, unsigned int, unsigned int);
int XMapWindow(Display*, Window);
KeyCode XKeysymToKeycode(Display*, KeySym);
char* XKeysymToString(KeySym);
XModifierKeymap* XGetModifierMapping(Display*);
int XFreeModifiermap(XModifierKeymap*);
int XGrabServer(Display*);
int XUngrabServer(Display*);
unsigned long XBlackPixel(Display*, int);
int XStoreName(Display*, Window, const char*);
Status XIconifyWindow(Display*, Window, int);
int XConvertSelection(Display*, Atom, Atom, Atom, Window, Time);
Window XGetSelectionOwner(Display*, Atom);
int XSetSelectionOwner(Display*, Atom, Window, Time);
Status XInternAtoms(Display*, char**, int, Bool, Atom*);
int XDisplayKeycodes(Display*, int*, int*);
KeySym* XGetKeyboardMapping(Display*, KeyCode, int, int*);
KeySym XStringToKeysym(const char*);
int XChangeKeyboardMapping(Display*, int, int, KeySym*, int);
Bool XkbLookupKeySym(Display*, KeyCode, unsigned int, unsigned int*, KeySym*);
Bool XkbLockModifiers(Display*, unsigned int, unsigned int, unsigned int);
unsigned int XkbKeysymToModifiers(Display*, KeySym);
}

inline int XkbGroupForCoreState(int s) {
  return (s >> 13) & 0x3;
}

inline int XkbBuildCoreState(int m, int g) {
  return ((g & 0x3) << 13) | (m & 0xff);
}

#endif  // UI_GFX_X_X11_H_
