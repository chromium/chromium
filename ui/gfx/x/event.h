// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_EVENT_H_
#define UI_GFX_X_EVENT_H_

#include <cstdint>
#include <utility>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;
class Event;
struct ReadBuffer;

COMPONENT_EXPORT(X11)
void ReadEvent(Event* event, Connection* connection, ReadBuffer* buffer);

class COMPONENT_EXPORT(X11) Event {
 public:
  template <typename T>
  Event(bool send_event, T&& xproto_event) {
    using DecayT = std::decay_t<T>;
    send_event_ = send_event;
    sequence_ = xproto_event.sequence;
    type_id_ = DecayT::type_id;
    auto* event = new DecayT(std::forward<T>(xproto_event));
    event_ = {event, [](void* e) {
                if (e) {
                  delete reinterpret_cast<DecayT*>(e);
                }
              }};
    window_ = event->GetWindow();
  }

  Event();

  // |event_bytes| is modified and will not be valid after this call.
  // A copy is necessary if the original data is still needed.
  Event(scoped_refptr<base::RefCountedMemory> event_bytes,
        Connection* connection);

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;

  Event(Event&& event);
  Event& operator=(Event&& event);

  ~Event();

  template <typename T>
  T* As() {
    if (type_id_ == T::type_id)
      return reinterpret_cast<T*>(event_.get());
    return nullptr;
  }

  template <typename T>
  const T* As() const {
    return const_cast<Event*>(this)->As<T>();
  }

  bool send_event() const { return send_event_; }

  uint32_t sequence() const { return sequence_; }

  Window window() const { return window_ ? *window_ : Window::None; }
  void set_window(Window window) {
    if (window_)
      *window_ = window;
  }

  bool Initialized() const { return !!event_; }

 private:
  friend void ReadEvent(Event* event,
                        Connection* connection,
                        ReadBuffer* buffer);

  // True if this event was sent from another X client.  False if this event
  // was sent by the X server.
  bool send_event_ = false;
  uint16_t sequence_ = 0;

  // XProto event state.
  int type_id_ = 0;
  std::unique_ptr<void, void (*)(void*)> event_ = {nullptr, nullptr};

  // This member points to a field in |event_|, or may be nullptr if there's no
  // associated window for the event.  It's owned by |event_|, not us.
  raw_ptr<Window> window_ = nullptr;
};

}  // namespace x11

#endif  // UI_GFX_X_EVENT_H_
