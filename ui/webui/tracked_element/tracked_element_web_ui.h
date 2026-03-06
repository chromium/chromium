// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_WEB_UI_H_
#define UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_WEB_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

class TrackedElementHandler;

class TrackedElementWebUI : public ui::TrackedElement {
 public:
  // As long as this is alive, the other side will be asked to
  // highlight the element.
  class HighlightHandle : public base::RefCounted<HighlightHandle> {
   public:
    HighlightHandle(const HighlightHandle&) = delete;
    HighlightHandle(HighlightHandle&&) = delete;
    HighlightHandle& operator=(const HighlightHandle&) = delete;
    HighlightHandle& operator=(HighlightHandle&&) = delete;

   private:
    friend class base::RefCounted<HighlightHandle>;
    friend class TrackedElementWebUI;
    explicit HighlightHandle(base::WeakPtr<TrackedElementWebUI> element);
    ~HighlightHandle();

    base::WeakPtr<TrackedElementWebUI> element_;
  };

  TrackedElementWebUI(TrackedElementHandler* handler,
                      ui::ElementIdentifier identifier,
                      ui::ElementContext context);
  ~TrackedElementWebUI() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  TrackedElementHandler* handler() const { return handler_; }

  // ui::TrackedElement:
  gfx::Rect GetScreenBounds() const override;
  gfx::NativeView GetNativeView() const override;

  bool can_highlight() const { return can_highlight_; }

  // Precondition: can_highlight() must be true.
  // Returns any pre-existing highlight handle or makes a new one, asking the
  // other end to apply a highlight.
  scoped_refptr<HighlightHandle> GetOrMakeHighlightHandle();

 private:
  friend class TrackedElementHandler;

  void ReleaseHighlightHandle();

  void SetVisible(bool visible, gfx::RectF bounds = gfx::RectF());
  void Activate();
  void CustomEvent(ui::CustomElementEventType event_type);
  bool visible() const { return visible_; }
  void set_can_highlight(bool can_highlight) { can_highlight_ = can_highlight; }

  const raw_ptr<TrackedElementHandler> handler_;
  bool visible_ = false;
  bool can_highlight_ = false;
  gfx::RectF last_known_bounds_;
  raw_ptr<HighlightHandle> highlight_handle_ = nullptr;

  base::WeakPtrFactory<TrackedElementWebUI> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_TRACKED_ELEMENT_WEB_UI_H_
