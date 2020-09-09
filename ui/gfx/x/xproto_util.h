// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_UTIL_H_
#define UI_GFX_X_XPROTO_UTIL_H_

#include <cstdint>

#include "base/component_export.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

template <typename T>
x11::Future<void> SendEvent(
    const T& event,
    x11::Window target,
    x11::EventMask mask,
    x11::Connection* connection = x11::Connection::Get()) {
  static_assert(T::type_id > 0, "T must be an x11::*Event type");
  auto write_buffer = x11::Write(event);
  DCHECK_EQ(write_buffer.GetBuffers().size(), 1ul);
  auto& first_buffer = write_buffer.GetBuffers()[0];
  DCHECK_LE(first_buffer->size(), 32ul);
  std::vector<uint8_t> event_bytes(32);
  memcpy(event_bytes.data(), first_buffer->data(), first_buffer->size());

  x11::SendEventRequest send_event{false, target, mask};
  std::copy(event_bytes.begin(), event_bytes.end(), send_event.event.begin());
  return connection->SendEvent(send_event);
}

COMPONENT_EXPORT(X11)
void LogErrorEventDescription(unsigned long serial,
                              uint8_t error_code,
                              uint8_t request_code,
                              uint8_t minor_code);

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_UTIL_H_
