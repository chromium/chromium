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

static constexpr auto GrabModeAsync = 1;

static constexpr auto ShiftMask = 1 << 0;
static constexpr auto LockMask = 1 << 1;
static constexpr auto ControlMask = 1 << 2;
static constexpr auto Mod1Mask = 1 << 3;
static constexpr auto Mod2Mask = 1 << 4;
static constexpr auto Mod3Mask = 1 << 5;
static constexpr auto Mod4Mask = 1 << 6;
static constexpr auto Mod5Mask = 1 << 7;

static constexpr auto NoEventMask = 0L;
static constexpr auto KeyPressMask = 1L << 0;
static constexpr auto KeyReleaseMask = 1L << 1;
static constexpr auto ButtonPressMask = 1L << 2;
static constexpr auto ButtonReleaseMask = 1L << 3;
static constexpr auto EnterWindowMask = 1L << 4;
static constexpr auto LeaveWindowMask = 1L << 5;
static constexpr auto PointerMotionMask = 1L << 6;
static constexpr auto PointerMotionHintMask = 1L << 7;
static constexpr auto Button1MotionMask = 1L << 8;
static constexpr auto Button2MotionMask = 1L << 9;
static constexpr auto Button3MotionMask = 1L << 10;
static constexpr auto Button4MotionMask = 1L << 11;
static constexpr auto Button5MotionMask = 1L << 12;
static constexpr auto ButtonMotionMask = 1L << 13;
static constexpr auto KeymapStateMask = 1L << 14;
static constexpr auto ExposureMask = 1L << 15;
static constexpr auto VisibilityChangeMask = 1L << 16;
static constexpr auto StructureNotifyMask = 1L << 17;
static constexpr auto ResizeRedirectMask = 1L << 18;
static constexpr auto SubstructureNotifyMask = 1L << 19;
static constexpr auto SubstructureRedirectMask = 1L << 20;
static constexpr auto FocusChangeMask = 1L << 21;
static constexpr auto PropertyChangeMask = 1L << 22;
static constexpr auto ColormapChangeMask = 1L << 23;
static constexpr auto OwnerGrabButtonMask = 1L << 24;

static constexpr auto CWBackPixmap = 1L << 0;
static constexpr auto CWBackPixel = 1L << 1;
static constexpr auto CWBorderPixmap = 1L << 2;
static constexpr auto CWBorderPixel = 1L << 3;
static constexpr auto CWBitGravity = 1L << 4;
static constexpr auto CWWinGravity = 1L << 5;
static constexpr auto CWBackingStore = 1L << 6;
static constexpr auto CWBackingPlanes = 1L << 7;
static constexpr auto CWBackingPixel = 1L << 8;
static constexpr auto CWOverrideRedirect = 1L << 9;
static constexpr auto CWSaveUnder = 1L << 10;
static constexpr auto CWEventMask = 1L << 11;
static constexpr auto CWDontPropagate = 1L << 12;
static constexpr auto CWColormap = 1L << 13;
static constexpr auto CWCursor = 1L << 14;

static constexpr auto ForgetGravity = 0;
static constexpr auto NorthWestGravity = 1;
static constexpr auto NorthGravity = 2;
static constexpr auto NorthEastGravity = 3;
static constexpr auto WestGravity = 4;
static constexpr auto CenterGravity = 5;
static constexpr auto EastGravity = 6;
static constexpr auto SouthWestGravity = 7;
static constexpr auto SouthGravity = 8;
static constexpr auto SouthEastGravity = 9;
static constexpr auto StaticGravity = 10;

static constexpr auto Button1Mask = 1 << 8;
static constexpr auto Button2Mask = 1 << 9;
static constexpr auto Button3Mask = 1 << 10;
static constexpr auto Button4Mask = 1 << 11;
static constexpr auto Button5Mask = 1 << 12;

static constexpr auto CWX = 1 << 0;
static constexpr auto CWY = 1 << 1;
static constexpr auto CWWidth = 1 << 2;
static constexpr auto CWHeight = 1 << 3;
static constexpr auto CWBorderWidth = 1 << 4;
static constexpr auto CWSibling = 1 << 5;
static constexpr auto CWStackMode = 1 << 6;

static constexpr auto NotifyNormal = 0;
static constexpr auto NotifyGrab = 1;
static constexpr auto NotifyUngrab = 2;
static constexpr auto NotifyWhileGrabbed = 3;

static constexpr auto NotifyAncestor = 0;
static constexpr auto NotifyVirtual = 1;
static constexpr auto NotifyInferior = 2;
static constexpr auto NotifyNonlinear = 3;
static constexpr auto NotifyNonlinearVirtual = 4;
static constexpr auto NotifyPointer = 5;
static constexpr auto NotifyPointerRoot = 6;
static constexpr auto NotifyDetailNone = 7;

static constexpr auto PropModeReplace = 0;
static constexpr auto PropModePrepend = 1;
static constexpr auto PropModeAppend = 2;

static constexpr auto AnyPropertyType = 0L;

static constexpr auto NoSymbol = 0L;

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


using XWindowChanges = struct {
  int x, y;
  int width, height;
  int border_width;
  Window sibling;
  int stack_mode;
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
Bool XQueryExtension(Display*, const char*, int*, int*, int*);
int XSync(Display*, Bool);
XErrorHandler XSetErrorHandler(XErrorHandler);
XIOErrorHandler XSetIOErrorHandler(XIOErrorHandler);
void XLockDisplay(Display*);
extern void XUnlockDisplay(Display*);
int XConnectionNumber(Display*);
int XGrabKey(Display*, int, unsigned int, Window, Bool, int, int);
int XUngrabKey(Display*, int, unsigned int, Window);
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
int XConfigureWindow(Display*, Window, unsigned int, XWindowChanges*);
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
