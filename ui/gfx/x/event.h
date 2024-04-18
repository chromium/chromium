// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_EVENT_H_
#define UI_GFX_X_EVENT_H_

#include <cstdint>
#include <type_traits>
#include <utility>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;
class Event;
struct ReadBuffer;

template <typename T>
void ReadEvent(T* event, ReadBuffer* buf);

class COMPONENT_EXPORT(X11) Event {
 public:
  template <typename T>
  Event(bool send_event, T&& xproto_event) {
    using DecayT = std::decay_t<T>;
    fabricated_ = true;
    send_event_ = send_event;
    type_id_ = DecayT::type_id;
    auto* event = new DecayT(std::forward<T>(xproto_event));
    event_ = {event, [](void* e) {
                if (e) {
                  delete reinterpret_cast<DecayT*>(e);
                }
              }};
  }

  Event();

  // |event_bytes| is modified and will not be valid after this call.
  // A copy is necessary if the original data is still needed.
  Event(scoped_refptr<UnsizedRefCountedMemory> event_bytes,
        Connection* connection);

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;

  Event(Event&& event);
  Event& operator=(Event&& event);

  ~Event();

  template <typename T>
  T* As() {
    if (type_id_ != T::type_id) {
      return nullptr;
    }
    if (!event_) {
      T* event = new T;
      Parse(
          event,
          [](void* e, ReadBuffer* r) { ReadEvent<T>(static_cast<T*>(e), r); },
          [](void* e) { delete static_cast<T*>(e); });
      if constexpr (std::is_member_pointer<decltype(&T::opcode)>()) {
        event->opcode = static_cast<decltype(event->opcode)>(opcode_);
      }
    }
    return static_cast<T*>(event_.get());
  }

  template <typename T>
  const T* As() const {
    return const_cast<Event*>(this)->As<T>();
  }

  bool send_event() const { return send_event_; }

  uint32_t sequence() const {
    CHECK(!fabricated_);
    return sequence_;
  }

  bool Initialized() const { return type_id_; }

 private:
  using Parser = void (*)(void*, ReadBuffer*);
  using Deleter = void (*)(void*);

  void Parse(void* event, Parser parser, Deleter deleter);

  // Indicates this event was sent from another client instead of the server.
  bool send_event_ = false;

  // Indicates this event was created, not sent by the server or a client.
  bool fabricated_ = false;

  // The value of the underlying event's `T::type_id`.
  uint8_t type_id_ = 0;

  // The value of the underlying event's `t->opcode`.
  uint8_t opcode_ = 0;

  // The (extended) event sequence provided by XCB.
  uint32_t sequence_ = 0;

  // The unparsed event, or nullptr if it's already parsed.
  scoped_refptr<UnsizedRefCountedMemory> raw_event_;

  // The type-erased parsed event, or nullptr if it hasn't been parsed yet.
  std::unique_ptr<void, Deleter> event_ = {nullptr, nullptr};
};

}  // namespace x11

#endif  // UI_GFX_X_EVENT_H_
