// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_HIT_TEST_H_
#define UI_BASE_HIT_TEST_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)

// Define the HT* values so that this header can be included whether or not
// windows.h has been included. The definitions must exactly match to avoid
// redefinition errors when compiling source files which also include windows.h.
// Those source files conveniently provide a check that these values match.
// windows.h is not included here because of the namespace pollution it causes.
#define HTERROR (-2)
#define HTTRANSPARENT (-1)
#define HTNOWHERE 0
#define HTCLIENT 1
#define HTCAPTION 2
#define HTSYSMENU 3
#define HTGROWBOX 4
#define HTSIZE HTGROWBOX
#define HTMENU 5
#define HTHSCROLL 6
#define HTVSCROLL 7
#define HTMINBUTTON 8
#define HTMAXBUTTON 9
#define HTLEFT 10
#define HTRIGHT 11
#define HTTOP 12
#define HTTOPLEFT 13
#define HTTOPRIGHT 14
#define HTBOTTOM 15
#define HTBOTTOMLEFT 16
#define HTBOTTOMRIGHT 17
#define HTBORDER 18
#define HTREDUCE HTMINBUTTON
#define HTZOOM HTMAXBUTTON
#define HTSIZEFIRST HTLEFT
#define HTSIZELAST HTBOTTOMRIGHT
#define HTOBJECT 19
#define HTCLOSE 20
#define HTHELP 21

#else

// Defines the same symbolic names used by the WM_NCHITTEST Notification under
// win32 (the integer values are guaranteed to be equivalent). We do this
// because we have a whole bunch of code that deals with window resizing and
// such that requires these values.
enum HitTestCompat {
  HTNOWHERE = 0,
  HTBORDER = 18,
  HTBOTTOM = 15,
  HTBOTTOMLEFT = 16,
  HTBOTTOMRIGHT = 17,
  HTCAPTION = 2,
  HTCLIENT = 1,
  HTCLOSE = 20,
  HTERROR = -2,
  HTGROWBOX = 4,
  HTHELP = 21,
  HTHSCROLL = 6,
  HTLEFT = 10,
  HTMENU = 5,
  HTMAXBUTTON = 9,
  HTMINBUTTON = 8,
  HTREDUCE = HTMINBUTTON,
  HTRIGHT = 11,
  HTSIZE = HTGROWBOX,
  HTSYSMENU = 3,
  HTTOP = 12,
  HTTOPLEFT = 13,
  HTTOPRIGHT = 14,
  HTTRANSPARENT = -1,
  HTVSCROLL = 7,
  HTZOOM = HTMAXBUTTON
};

#endif  // BUILDFLAG(IS_WIN)

namespace ui {

// Returns true if the |component| is for resizing, like HTTOP or HTBOTTOM.
bool IsResizingComponent(int component);

// Returns true if the |component| is HTCAPTION or one of the resizing
// components.
bool CanPerformDragOrResize(int component);

}  // namespace ui

#endif  // UI_BASE_HIT_TEST_H_
