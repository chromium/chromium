// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file replaces includes of X11 system includes while
// preventing them from redefining and making a mess of commonly used
// keywords like "None" and "Status". Instead those are placed inside
// an X11 namespace where they will not clash with other code.

#ifndef UI_GFX_X_X11
#define UI_GFX_X_X11

extern "C" {
// Xlib.h defines base types so it must be included before the less
// central X11 headers can be included.
#include <X11/Xlib.h>

// And the rest so that nobody needs to include them manually...
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xregion.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XIproto.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/record.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/sync.h>

// Define XK_xxx before the #include of <X11/keysym.h> so that <X11/keysym.h>
// defines all KeySyms we need.
#define XK_3270  // For XK_3270_BackTab in particular.
#define XK_MISCELLANY
#define XK_LATIN1
#define XK_LATIN2
#define XK_LATIN3
#define XK_LATIN4
#define XK_LATIN8
#define XK_LATIN9
#define XK_KATAKANA
#define XK_ARABIC
#define XK_CYRILLIC
#define XK_GREEK
#define XK_TECHNICAL
#define XK_SPECIAL
#define XK_PUBLISHING
#define XK_APL
#define XK_HEBREW
#define XK_THAI
#define XK_KOREAN
#define XK_ARMENIAN
#define XK_GEORGIAN
#define XK_CAUCASUS
#define XK_VIETNAMESE
#define XK_CURRENCY
#define XK_MATHEMATICAL
#define XK_BRAILLE
#define XK_SINHALA
#define XK_XKB_KEYS

#ifndef XK_dead_greek
#define XK_dead_greek 0xfe8c
#endif

#include <X11/Sunkeysym.h>
#include <X11/XF86keysym.h>
#include <X11/keysym.h>

// These commonly used names are undefined and if necessary recreated
// in the x11 namespace below. This is the main purpose of this header
// file.

// Not using common words is extra important for jumbo builds
// where cc files are merged. Those merged filed get to see many more
// headers than initially expected, including system headers like
// those from X11.

#undef Status         // Defined by X11/Xlib.h to int
#undef Bool           // Defined by X11/Xlib.h to int
#undef RootWindow     // Defined by X11/Xlib.h
#undef DestroyAll     // Defined by X11/X.h to 0
#undef Always         // Defined by X11/X.h to 2
#undef AddToList      // Defined by X11/extensions/XI.h to 0
#undef COUNT          // Defined by X11/extensions/XI.h to 0
#undef CREATE         // Defined by X11/extensions/XI.h to 1
#undef DeviceAdded    // Defined by X11/extensions/XI.h to 0
#undef DeviceMode     // Defined by X11/extensions/XI.h to 1
#undef DeviceRemoved  // Defined by X11/extensions/XI.h to 1

// The constants below are made available in the x11 namespace with
// their original values so we double check that the value is what we
// expect using static_assert.
static_assert(FocusIn == 9 && FocusOut == 10, "Unexpected focus constants");
#undef FocusIn   // Defined by X.h to 9
#undef FocusOut  // Defined by X.h to 10

static_assert(None == 0, "Unexpected value for X11 constant 'None'");
#undef None  // Defined by X11/X.h to 0L

static_assert(True == 1 && False == 0, "Unexpected X11 truth values");
#undef True   // Defined by X11/Xlib.h to 1
#undef False  // Defined by X11/Xlib.h to 0

static_assert(CurrentTime == 0, "Unexpected value for X11 'CurrentTime'");
#undef CurrentTime  // Defined by X11/X.h to 0L

static_assert(Success == 0, "Unexpected value for X11 'Success'");
#undef Success  // Defined by X11/X.h to 0
}

// The x11 namespace allows to scope X11 constants and types that
// would be problematic at the default preprocessor level.
namespace x11 {
static constexpr unsigned long None = 0L;
static constexpr long CurrentTime = 0L;
static constexpr int False = 0;
static constexpr int True = 1;
static constexpr int Success = 0;
static constexpr int FocusIn = 9;
static constexpr int FocusOut = 10;
typedef int Bool;
typedef int Status;
}  // namespace x11

#endif  // UI_GFX_X_X11
