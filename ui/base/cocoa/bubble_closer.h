// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_BUBBLE_CLOSER_H_
#define UI_BASE_COCOA_BUBBLE_CLOSER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Monitors mouse events to allow a regular window to have menu-like popup
// behavior when clicking outside the window. This is needed because macOS
// suppresses window activation events when clicking rapidly.
class COMPONENT_EXPORT(UI_BASE) BubbleCloser {
 public:
  // Installs an event monitor watching for mouse clicks outside of |window| or
  // any of its child windows. Invokes |on_click_outside| on each event.
  BubbleCloser(NSWindow* window, base::RepeatingClosure on_click_outside);

  BubbleCloser(const BubbleCloser&) = delete;
  BubbleCloser& operator=(const BubbleCloser&) = delete;

  ~BubbleCloser();

 private:
  void OnClickOutside();

  base::RepeatingClosure on_click_outside_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;

  base::WeakPtrFactory<BubbleCloser> factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_COCOA_BUBBLE_CLOSER_H_
