// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_WEB_UI_H_
#define UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_WEB_UI_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

class TrackedElementHandler;

class TrackedElementWebUI : public ui::TrackedElement {
 public:
  TrackedElementWebUI(TrackedElementHandler* handler,
                      ui::ElementIdentifier identifier,
                      ui::ElementContext context);
  ~TrackedElementWebUI() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  TrackedElementHandler* handler() const { return handler_; }

  // ui::TrackedElement:
  gfx::Rect GetScreenBounds() const override;
  gfx::NativeView GetNativeView() const override;

 private:
  friend class TrackedElementHandler;

  void SetVisible(bool visible, gfx::RectF bounds = gfx::RectF());
  void Activate();
  void CustomEvent(ui::CustomElementEventType event_type);
  bool visible() const { return visible_; }

  const raw_ptr<TrackedElementHandler> handler_;
  bool visible_ = false;
  gfx::RectF last_known_bounds_;
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_WEB_UI_H_
