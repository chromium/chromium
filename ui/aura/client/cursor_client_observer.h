// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_CURSOR_CLIENT_OBSERVER_H_
#define UI_AURA_CLIENT_CURSOR_CLIENT_OBSERVER_H_

#include <optional>

#include "ui/aura/aura_export.h"
#include "ui/base/cursor/cursor.h"

namespace display {
class Display;
}

namespace gfx {
class Size;
}

namespace aura {
namespace client {

class AURA_EXPORT CursorClientObserver {
 public:
  virtual void OnCursorVisibilityChanged(bool is_visible) {}
  // Called when the custom cursor size changes. See
  // `CursorClient::SetCustomCursorSize()`.
  virtual void OnCustomCursorSizeChanged(
      std::optional<int> custom_cursor_size) {}
  virtual void OnCursorDisplayChanged(const display::Display& display) {}
  // System cursor size is the size, in DIP, of the cursor; according
  // to OS settings.
  virtual void OnSystemCursorSizeChanged(const gfx::Size& system_cursor_size) {}

 protected:
  virtual ~CursorClientObserver() {}
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_CURSOR_CLIENT_OBSERVER_H_
