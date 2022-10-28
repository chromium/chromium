// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_UTIL_H_
#define UI_GFX_X_XPROTO_UTIL_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

template <typename T>
Future<void> SendEvent(const T& event,
                       Window target,
                       EventMask mask,
                       Connection* connection = Connection::Get()) {
  static_assert(T::type_id > 0, "T must be an *Event type");
  auto write_buffer = Write(event);
  DCHECK_EQ(write_buffer.GetBuffers().size(), 1ul);
  auto& first_buffer = write_buffer.GetBuffers()[0];
  DCHECK_LE(first_buffer->size(), 32ul);
  std::vector<uint8_t> event_bytes(32);
  memcpy(event_bytes.data(), first_buffer->data(), first_buffer->size());

  SendEventRequest send_event{false, target, mask};
  base::ranges::copy(event_bytes, send_event.event.begin());
  return connection->SendEvent(send_event);
}

template <typename T>
bool GetArrayProperty(Window window,
                      Atom name,
                      std::vector<T>* value,
                      Atom* out_type = nullptr,
                      size_t amount = 0,
                      Connection* connection = Connection::Get()) {
  static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "");

  size_t bytes = amount * sizeof(T);
  // The length field specifies the maximum amount of data we would like the
  // server to give us.  It's specified in units of 4 bytes, so divide by 4.
  // Add 3 before division to round up.
  size_t length = (bytes + 3) / 4;
  using lentype = decltype(GetPropertyRequest::long_length);
  auto response =
      connection
          ->GetProperty(GetPropertyRequest{
              .window = static_cast<Window>(window),
              .property = name,
              .long_length = static_cast<uint32_t>(
                  amount ? length : std::numeric_limits<lentype>::max())})
          .Sync();
  if (!response || response->format != CHAR_BIT * sizeof(T))
    return false;

  DCHECK_EQ(response->format / CHAR_BIT * response->value_len,
            response->value->size());
  value->resize(response->value_len);
  if (response->value_len > 0)
    memcpy(value->data(), response->value->data(), response->value->size());
  if (out_type)
    *out_type = response->type;
  return true;
}

template <typename T>
bool GetProperty(Window window,
                 const Atom name,
                 T* value,
                 Connection* connection = Connection::Get()) {
  std::vector<T> values;
  if (!GetArrayProperty(window, name, &values, nullptr, 1, connection) ||
      values.empty()) {
    return false;
  }
  *value = values[0];
  return true;
}

template <typename T>
Future<void> SetArrayProperty(Window window,
                              Atom name,
                              Atom type,
                              const std::vector<T>& values,
                              Connection* connection = Connection::Get()) {
  static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "");
  std::vector<uint8_t> data(sizeof(T) * values.size());
  if (values.size() > 0)
    memcpy(data.data(), values.data(), sizeof(T) * values.size());
  return connection->ChangeProperty(
      ChangePropertyRequest{.window = static_cast<Window>(window),
                            .property = name,
                            .type = type,
                            .format = CHAR_BIT * sizeof(T),
                            .data_len = static_cast<uint32_t>(values.size()),
                            .data = base::RefCountedBytes::TakeVector(&data)});
}

template <typename T>
Future<void> SetProperty(Window window,
                         Atom name,
                         Atom type,
                         const T& value,
                         Connection* connection = Connection::Get()) {
  return SetArrayProperty(window, name, type, std::vector<T>{value},
                          connection);
}

COMPONENT_EXPORT(X11)
void DeleteProperty(x11::Window window, x11::Atom name);

COMPONENT_EXPORT(X11)
void SetStringProperty(Window window,
                       Atom property,
                       Atom type,
                       const std::string& value,
                       Connection* connection = Connection::Get());

COMPONENT_EXPORT(X11)
Window CreateDummyWindow(const std::string& name = std::string(),
                         Connection* connection = Connection::Get());

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_UTIL_H_
