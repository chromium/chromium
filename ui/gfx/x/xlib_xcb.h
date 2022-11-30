// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XLIB_XCB_H_
#define UI_GFX_X_XLIB_XCB_H_

extern "C" {
struct xcb_connection_t* XGetXCBConnection(struct _XDisplay*);
}

#endif  // UI_GFX_X_XLIB_XCB_H_
