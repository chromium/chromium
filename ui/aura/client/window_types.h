// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_WINDOW_TYPES_H_
#define UI_AURA_CLIENT_WINDOW_TYPES_H_

namespace aura {
namespace client {

// This isn't a property because it can't change after the window has been
// initialized.
enum WindowType {
  WINDOW_TYPE_UNKNOWN = 0,

  // Regular windows that should be laid out by the client.
  WINDOW_TYPE_NORMAL,

  // Miscellaneous windows that should not be laid out by the shell.
  WINDOW_TYPE_POPUP,

  // A window intended as a control. Not laid out by the shell.
  WINDOW_TYPE_CONTROL,

  WINDOW_TYPE_MENU,

  WINDOW_TYPE_TOOLTIP,
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_WINDOW_TYPES_H_
