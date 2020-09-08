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
#include <X11/XKBlib.h>
#include <X11/Xlib-xcb.h>

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
}

#include "ui/gfx/x/xproto_undef.h"

#include "ui/gfx/x/connection.h"

// These commonly used names are undefined and if necessary recreated
// in the x11 namespace below. This is the main purpose of this header
// file.

// Not using common words is extra important for jumbo builds
// where cc files are merged. Those merged filed get to see many more
// headers than initially expected, including system headers like
// those from X11.

#undef Status       // Defined by X11/Xlib.h to int
#undef Bool         // Defined by X11/Xlib.h to int
#undef RootWindow   // Defined by X11/Xlib.h
#undef DestroyAll   // Defined by X11/X.h to 0
#undef Always       // Defined by X11/X.h to 2
#undef FocusIn      // Defined by X.h to 9
#undef FocusOut     // Defined by X.h to 10
#undef None         // Defined by X11/X.h to 0L
#undef True         // Defined by X11/Xlib.h to 1
#undef False        // Defined by X11/Xlib.h to 0
#undef CurrentTime  // Defined by X11/X.h to 0L
#undef Success      // Defined by X11/X.h to 0

// The x11 namespace allows to scope X11 constants and types that
// would be problematic at the default preprocessor level.
namespace x11 {
static constexpr unsigned long None = 0L;
static constexpr long CurrentTime = 0L;
static constexpr int False = 0;
static constexpr int True = 1;
static constexpr int Success = 0;
typedef int Bool;
typedef int Status;
}  // namespace x11

#endif  // UI_GFX_X_X11
