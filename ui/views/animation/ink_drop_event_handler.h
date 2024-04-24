// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_EVENT_HANDLER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_EVENT_HANDLER_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/events/event_handler.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace ui {
class LocatedEvent;
class ScopedTargetHandler;
}  // namespace ui

namespace views {
class InkDrop;
enum class InkDropState;
struct ViewHierarchyChangedDetails;

// This class handles ink-drop changes due to events on its host.
class VIEWS_EXPORT InkDropEventHandler : public ui::EventHandler,
                                         public ViewObserver {
 public:
  // Delegate class that allows InkDropEventHandler to be used with InkDrops
  // that are hosted in multiple ways.
  class Delegate {
   public:
    // Gets the InkDrop (or stub) that should react to incoming events.
    virtual InkDrop* GetInkDrop() = 0;

    virtual bool HasInkDrop() const = 0;

    // Returns true if gesture events should affect the InkDrop.
    virtual bool SupportsGestureEvents() const = 0;
  };

  InkDropEventHandler(View* host_view, Delegate* delegate);

  InkDropEventHandler(const InkDropEventHandler&) = delete;
  InkDropEventHandler& operator=(const InkDropEventHandler&) = delete;

  ~InkDropEventHandler() override;

  void AnimateToState(InkDropState state, const ui::LocatedEvent* event);
  ui::LocatedEvent* GetLastRippleTriggeringEvent() const;

 private:
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  std::string_view GetLogContext() const override;

  // ViewObserver:
  void OnViewVisibilityChanged(View* observed_view,
                               View* starting_view) override;
  void OnViewHierarchyChanged(
      View* observed_view,
      const ViewHierarchyChangedDetails& details) override;
  void OnViewBoundsChanged(View* observed_view) override;
  void OnViewFocused(View* observed_view) override;
  void OnViewBlurred(View* observed_view) override;
  void OnViewThemeChanged(View* observed_view) override;

  // Allows |this| to handle all GestureEvents on |host_view_|.
  std::unique_ptr<ui::ScopedTargetHandler> target_handler_;

  // The host view.
  const raw_ptr<View> host_view_;

  // Delegate used to get the InkDrop, etc.
  const raw_ptr<Delegate> delegate_;

  // The last user Event to trigger an InkDrop-ripple animation.
  std::unique_ptr<ui::LocatedEvent> last_ripple_triggering_event_;

  base::ScopedObservation<View, ViewObserver> observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_EVENT_HANDLER_H_
