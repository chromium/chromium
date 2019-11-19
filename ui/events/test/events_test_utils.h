// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_
#define UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_

#include "base/macros.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_target.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace ui {

class EventSource;

class EventTestApi {
 public:
  explicit EventTestApi(Event* event);
  virtual ~EventTestApi();

  void set_time_stamp(base::TimeTicks time_stamp) {
    event_->time_stamp_ = time_stamp;
  }

  void set_source_device_id(int source_device_id) {
    event_->source_device_id_ = source_device_id;
  }

 private:
  EventTestApi();

  Event* event_;

  DISALLOW_COPY_AND_ASSIGN(EventTestApi);
};

class LocatedEventTestApi : public EventTestApi {
 public:
  explicit LocatedEventTestApi(LocatedEvent* located_event);
  ~LocatedEventTestApi() override;

  void set_location(const gfx::Point& location) {
    located_event_->set_location(location);
  }
  void set_location_f(const gfx::PointF& location) {
    located_event_->set_location_f(location);
  }

 private:
  LocatedEventTestApi();

  LocatedEvent* located_event_;

  DISALLOW_COPY_AND_ASSIGN(LocatedEventTestApi);
};

class KeyEventTestApi : public EventTestApi {
 public:
  explicit KeyEventTestApi(KeyEvent* key_event);
  ~KeyEventTestApi() override;

  void set_is_char(bool is_char) {
    key_event_->set_is_char(is_char);
  }

  DomKey dom_key() const { return key_event_->key_; }

 private:
  KeyEventTestApi();

  KeyEvent* key_event_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventTestApi);
};

class EventTargetTestApi {
 public:
  explicit EventTargetTestApi(EventTarget* target);

  ui::EventHandlerList GetPreTargetHandlers() {
    ui::EventHandlerList list;
    target_->GetPreTargetHandlers(&list);
    return list;
  }

 private:
  EventTargetTestApi();

  EventTarget* target_;

  DISALLOW_COPY_AND_ASSIGN(EventTargetTestApi);
};

class EventSourceTestApi {
 public:
  explicit EventSourceTestApi(EventSource* event_source);

  EventDispatchDetails SendEventToSink(Event* event) WARN_UNUSED_RESULT;

 private:
  EventSourceTestApi();

  EventSource* event_source_;

  DISALLOW_COPY_AND_ASSIGN(EventSourceTestApi);
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_
