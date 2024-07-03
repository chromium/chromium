// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_
#define UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_target.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/platform_event.h"

namespace ui {

class EventSource;

class EventTestApi {
 public:
  explicit EventTestApi(Event* event);

  EventTestApi(const EventTestApi&) = delete;
  EventTestApi& operator=(const EventTestApi&) = delete;

  virtual ~EventTestApi();

  void set_time_stamp(base::TimeTicks time_stamp) {
    event_->time_stamp_ = time_stamp;
  }

  void set_source_device_id(int source_device_id) {
    event_->source_device_id_ = source_device_id;
  }

  // PlatformEvents on most platforms are not copyable by default. The standard
  // `SetNativeEvent` API in the event object is a no-op on most platforms. This
  // API is exposed to set the PlatformEvent explicitly in tests.
  void set_native_event(PlatformEvent native_event) {
    event_->native_event_ = native_event;
  }

 private:
  EventTestApi();

  raw_ptr<Event> event_;
};

class LocatedEventTestApi : public EventTestApi {
 public:
  explicit LocatedEventTestApi(LocatedEvent* located_event);

  LocatedEventTestApi(const LocatedEventTestApi&) = delete;
  LocatedEventTestApi& operator=(const LocatedEventTestApi&) = delete;

  ~LocatedEventTestApi() override;

  void set_location(const gfx::Point& location) {
    located_event_->set_location(location);
  }
  void set_location_f(const gfx::PointF& location) {
    located_event_->set_location_f(location);
  }

 private:
  LocatedEventTestApi();

  raw_ptr<LocatedEvent> located_event_;
};

class KeyEventTestApi : public EventTestApi {
 public:
  explicit KeyEventTestApi(KeyEvent* key_event);

  KeyEventTestApi(const KeyEventTestApi&) = delete;
  KeyEventTestApi& operator=(const KeyEventTestApi&) = delete;

  ~KeyEventTestApi() override;

  void set_is_char(bool is_char) {
    key_event_->set_is_char(is_char);
  }

  DomKey dom_key() const { return key_event_->key_; }

 private:
  KeyEventTestApi();

  raw_ptr<KeyEvent> key_event_;
};

class EventTargetTestApi {
 public:
  explicit EventTargetTestApi(EventTarget* target);

  EventTargetTestApi(const EventTargetTestApi&) = delete;
  EventTargetTestApi& operator=(const EventTargetTestApi&) = delete;

  ui::EventHandlerList GetPreTargetHandlers() {
    ui::EventHandlerList list;
    target_->GetPreTargetHandlers(&list);
    return list;
  }

 private:
  EventTargetTestApi();

  raw_ptr<EventTarget> target_;
};

class EventSourceTestApi {
 public:
  explicit EventSourceTestApi(EventSource* event_source);

  EventSourceTestApi(const EventSourceTestApi&) = delete;
  EventSourceTestApi& operator=(const EventSourceTestApi&) = delete;

  [[nodiscard]] EventDispatchDetails SendEventToSink(Event* event);

 private:
  EventSourceTestApi();

  raw_ptr<EventSource> event_source_;
};

}  // namespace ui

#endif  // UI_EVENTS_TEST_EVENTS_TEST_UTILS_H_
