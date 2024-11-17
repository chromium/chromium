// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/menu_source_utils.h"

#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/event.h"

namespace ui {

mojom::MenuSourceType GetMenuSourceTypeForEvent(const Event& event) {
  if (event.IsKeyEvent())
    return mojom::MenuSourceType::kKeyboard;
  if (event.IsTouchEvent() || event.IsGestureEvent())
    return mojom::MenuSourceType::kTouch;
  return mojom::MenuSourceType::kMouse;
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
mojom::MenuSourceType GetMenuSourceType(int event_flags) {
  if (event_flags & EF_MOUSE_BUTTON) {
    return mojom::MenuSourceType::kMouse;
  }
  if (event_flags & EF_FROM_TOUCH) {
    return mojom::MenuSourceType::kTouch;
  }
  return mojom::MenuSourceType::kKeyboard;
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace ui
