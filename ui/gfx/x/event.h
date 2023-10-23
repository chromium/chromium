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
    flags_ = kParsedFlag | kFabricatedEventFlag;
    if (send_event) {
      flags_ |= kSendEventFlag;
    }
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
  Event(scoped_refptr<base::RefCountedMemory> event_bytes,
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
    if (!(flags_ & kParsedFlag)) {
      T* event = new T;
      ReadEvent(event, static_cast<ReadBuffer*>(event_.get()));
      if constexpr (std::is_member_pointer<decltype(&T::opcode)>()) {
        event->opcode = static_cast<decltype(event->opcode)>(opcode_);
      }
      event_ = {event, [](void* e) { delete static_cast<T*>(e); }};
      flags_ |= kParsedFlag;
    }
    return static_cast<T*>(event_.get());
  }

  template <typename T>
  const T* As() const {
    return const_cast<Event*>(this)->As<T>();
  }

  bool send_event() const { return flags_ & kSendEventFlag; }

  uint32_t sequence() const {
    DUMP_WILL_BE_CHECK(!(flags_ & kFabricatedEventFlag));
    return sequence_;
  }

  bool Initialized() const { return type_id_; }

 private:
  enum Flags : uint8_t {
    // Indicates `event_` has been deserialized.
    kParsedFlag = 1 << 0,
    // Indicates this event was sent from another client instead of the server.
    kSendEventFlag = 1 << 1,
    // Indicates this event was created, not sent by the server or a client.
    kFabricatedEventFlag = 1 << 2,
  };

  uint8_t flags_ = 0;

  // The value of the underlying event's `T::type_id`.
  uint8_t type_id_ = 0;

  // The value of the underlying event's `t->opcode`.
  uint8_t opcode_ = 0;

  // The (extended) event sequence provided by XCB.
  uint32_t sequence_ = 0;

  // If `flags_ & kParsedFlag`, this holds the underlying event, otherwise it
  // holds a `ReadBuffer`.
  std::unique_ptr<void, void (*)(void*)> event_ = {nullptr, nullptr};
};

}  // namespace x11

#endif  // UI_GFX_X_EVENT_H_
