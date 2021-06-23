// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_TARGET_H_
#define UI_EVENTS_EVENT_TARGET_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/events/events_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

class EventDispatcher;
class EventTargeter;
class EventTargetIterator;
class LocatedEvent;

class EVENTS_EXPORT EventTarget {
 public:
  class DispatcherApi {
   public:
    explicit DispatcherApi(EventTarget* target) : target_(target) {}

   private:
    DispatcherApi();
    EventTarget* target_;

    DISALLOW_COPY_AND_ASSIGN(DispatcherApi);
  };

  EventTarget();
  virtual ~EventTarget();

  virtual bool CanAcceptEvent(const Event& event) = 0;

  // Returns the parent EventTarget in the event-target tree.
  virtual EventTarget* GetParentTarget() = 0;

  // Returns an iterator an EventTargeter can use to iterate over the list of
  // child EventTargets.
  virtual std::unique_ptr<EventTargetIterator> GetChildIterator() const = 0;

  // Returns the EventTargeter that should be used to find the target for an
  // event in the subtree rooted at this EventTarget.
  virtual EventTargeter* GetEventTargeter() = 0;

  // Updates the states in |event| (e.g. location) to be suitable for |target|,
  // so that |event| can be dispatched to |target|.
  virtual void ConvertEventToTarget(const EventTarget* target,
                                    LocatedEvent* event) const;

  // Get |event|'s screen location, using the EventTarget's screen location.
  virtual gfx::PointF GetScreenLocationF(const LocatedEvent& event) const;
  gfx::Point GetScreenLocation(const LocatedEvent& event) const;

  // Priority levels for PreTargetHandlers.
  enum class Priority {
    // The Accessibility level is the highest, and gets events before
    // other priority levels. This allows accessibility features to
    // modify events directly from the user.
    kAccessibility,

    // System priority EventHandlers get events before default level, and
    // should be used for drag and drop, menus, etc.
    kSystem,

    // The default level should be used by most EventHandlers.
    kDefault,
  };

  // Adds a handler to receive events before the target. The handler must be
  // explicitly removed from the target before the handler is destroyed. The
  // EventTarget does not take ownership of the handler.
  void AddPreTargetHandler(EventHandler* handler,
                           Priority priority = Priority::kDefault);
  void RemovePreTargetHandler(EventHandler* handler);

  // Adds a handler to receive events after the target. The handler must be
  // explicitly removed from the target before the handler is destroyed. The
  // EventTarget does not take ownership of the handler.
  void AddPostTargetHandler(EventHandler* handler);
  void RemovePostTargetHandler(EventHandler* handler);

  // Returns true if the event pre target list is empty.
  bool IsPreTargetListEmpty() const;

  // Sets |target_handler| as |target_handler_| and returns the old handler.
  EventHandler* SetTargetHandler(EventHandler* target_handler);

  bool HasTargetHandler() const { return target_handler_ != nullptr; }

 protected:
  EventHandler* target_handler() { return target_handler_; }

 private:
  friend class EventDispatcher;
  friend class EventTargetTestApi;

  // A handler with a priority.
  struct PrioritizedHandler {
    EventHandler* handler = nullptr;
    Priority priority = Priority::kDefault;

    bool operator<(const PrioritizedHandler& ph) const {
      return priority < ph.priority;
    }
  };
  using EventHandlerPriorityList = std::vector<PrioritizedHandler>;

  // Returns the list of handlers that should receive the event before the
  // target. The handlers from the outermost target are first in the list, and
  // the handlers on |this| are the last in the list.
  void GetPreTargetHandlers(EventHandlerList* list);

  // Returns the list of handlers that should receive the event after the
  // target. The handlers from the outermost target are last in the list, and
  // the handlers on |this| are the first in the list.
  void GetPostTargetHandlers(EventHandlerList* list);

  EventHandlerPriorityList pre_target_list_;
  EventHandlerList post_target_list_;
  EventHandler* target_handler_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EventTarget);
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_TARGET_H_
