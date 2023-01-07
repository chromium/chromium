// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    composite \
//    damage \
//    dpms \
//    dri2 \
//    dri3 \
//    ge \
//    glx \
//    present \
//    randr \
//    record \
//    render \
//    res \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xc_misc \
//    xevie \
//    xf86dri \
//    xf86vidmode \
//    xfixes \
//    xinerama \
//    xinput \
//    xkb \
//    xprint \
//    xproto \
//    xselinux \
//    xtest \
//    xv \
//    xvmc

#include "xselinux.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

SELinux::SELinux(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

Future<SELinux::QueryVersionReply> SELinux::QueryVersion(
    const SELinux::QueryVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& client_major = request.client_major;
  auto& client_minor = request.client_minor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // client_major
  buf.Write(&client_major);

  // client_minor
  buf.Write(&client_minor);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::QueryVersionReply>(
      &buf, "SELinux::QueryVersion", false);
}

Future<SELinux::QueryVersionReply> SELinux::QueryVersion(
    const uint8_t& client_major,
    const uint8_t& client_minor) {
  return SELinux::QueryVersion(
      SELinux::QueryVersionRequest{client_major, client_minor});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::QueryVersionReply> detail::ReadReply<
    SELinux::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::QueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& server_major = (*reply).server_major;
  auto& server_minor = (*reply).server_minor;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // server_major
  Read(&server_major, &buf);

  // server_minor
  Read(&server_minor, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> SELinux::SetDeviceCreateContext(
    const SELinux::SetDeviceCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t context_len{};
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_len
  context_len = context.size();
  buf.Write(&context_len);

  // context
  DCHECK_EQ(static_cast<size_t>(context_len), context.size());
  for (auto& context_elem : context) {
    // context_elem
    buf.Write(&context_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SELinux::SetDeviceCreateContext",
                                        false);
}

Future<void> SELinux::SetDeviceCreateContext(const std::string& context) {
  return SELinux::SetDeviceCreateContext(
      SELinux::SetDeviceCreateContextRequest{context});
}

Future<SELinux::GetDeviceCreateContextReply> SELinux::GetDeviceCreateContext(
    const SELinux::GetDeviceCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetDeviceCreateContextReply>(
      &buf, "SELinux::GetDeviceCreateContext", false);
}

Future<SELinux::GetDeviceCreateContextReply> SELinux::GetDeviceCreateContext() {
  return SELinux::GetDeviceCreateContext(
      SELinux::GetDeviceCreateContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetDeviceCreateContextReply> detail::ReadReply<
    SELinux::GetDeviceCreateContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetDeviceCreateContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> SELinux::SetDeviceContext(
    const SELinux::SetDeviceContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device = request.device;
  uint32_t context_len{};
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device
  buf.Write(&device);

  // context_len
  context_len = context.size();
  buf.Write(&context_len);

  // context
  DCHECK_EQ(static_cast<size_t>(context_len), context.size());
  for (auto& context_elem : context) {
    // context_elem
    buf.Write(&context_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SELinux::SetDeviceContext",
                                        false);
}

Future<void> SELinux::SetDeviceContext(const uint32_t& device,
                                       const std::string& context) {
  return SELinux::SetDeviceContext(
      SELinux::SetDeviceContextRequest{device, context});
}

Future<SELinux::GetDeviceContextReply> SELinux::GetDeviceContext(
    const SELinux::GetDeviceContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device = request.device;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device
  buf.Write(&device);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetDeviceContextReply>(
      &buf, "SELinux::GetDeviceContext", false);
}

Future<SELinux::GetDeviceContextReply> SELinux::GetDeviceContext(
    const uint32_t& device) {
  return SELinux::GetDeviceContext(SELinux::GetDeviceContextRequest{device});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetDeviceContextReply> detail::ReadReply<
    SELinux::GetDeviceContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetDeviceContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> SELinux::SetWindowCreateContext(
    const SELinux::SetWindowCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t context_len{};
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_len
  context_len = context.size();
  buf.Write(&context_len);

  // context
  DCHECK_EQ(static_cast<size_t>(context_len), context.size());
  for (auto& context_elem : context) {
    // context_elem
    buf.Write(&context_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SELinux::SetWindowCreateContext",
                                        false);
}

Future<void> SELinux::SetWindowCreateContext(const std::string& context) {
  return SELinux::SetWindowCreateContext(
      SELinux::SetWindowCreateContextRequest{context});
}

Future<SELinux::GetWindowCreateContextReply> SELinux::GetWindowCreateContext(
    const SELinux::GetWindowCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetWindowCreateContextReply>(
      &buf, "SELinux::GetWindowCreateContext", false);
}

Future<SELinux::GetWindowCreateContextReply> SELinux::GetWindowCreateContext() {
  return SELinux::GetWindowCreateContext(
      SELinux::GetWindowCreateContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetWindowCreateContextReply> detail::ReadReply<
    SELinux::GetWindowCreateContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetWindowCreateContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::GetWindowContextReply> SELinux::GetWindowContext(
    const SELinux::GetWindowContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetWindowContextReply>(
      &buf, "SELinux::GetWindowContext", false);
}

Future<SELinux::GetWindowContextReply> SELinux::GetWindowContext(
    const Window& window) {
  return SELinux::GetWindowContext(SELinux::GetWindowContextRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetWindowContextReply> detail::ReadReply<
    SELinux::GetWindowContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetWindowContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> SELinux::SetPropertyCreateContext(
    const SELinux::SetPropertyCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t context_len{};
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_len
  context_len = context.size();
  buf.Write(&context_len);

  // context
  DCHECK_EQ(static_cast<size_t>(context_len), context.size());
  for (auto& context_elem : context) {
    // context_elem
    buf.Write(&context_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(
      &buf, "SELinux::SetPropertyCreateContext", false);
}

Future<void> SELinux::SetPropertyCreateContext(const std::string& context) {
  return SELinux::SetPropertyCreateContext(
      SELinux::SetPropertyCreateContextRequest{context});
}

Future<SELinux::GetPropertyCreateContextReply>
SELinux::GetPropertyCreateContext(
    const SELinux::GetPropertyCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetPropertyCreateContextReply>(
      &buf, "SELinux::GetPropertyCreateContext", false);
}

Future<SELinux::GetPropertyCreateContextReply>
SELinux::GetPropertyCreateContext() {
  return SELinux::GetPropertyCreateContext(
      SELinux::GetPropertyCreateContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetPropertyCreateContextReply> detail::ReadReply<
    SELinux::GetPropertyCreateContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetPropertyCreateContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> SELinux::SetPropertyUseContext(
    const SELinux::SetPropertyUseContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t context_len{};
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_len
  context_len = context.size();
  buf.Write(&context_len);

  // context
  DCHECK_EQ(static_cast<size_t>(context_len), context.size());
  for (auto& context_elem : context) {
    // context_elem
    buf.Write(&context_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SELinux::SetPropertyUseContext",
                                        false);
}

Future<void> SELinux::SetPropertyUseContext(const std::string& context) {
  return SELinux::SetPropertyUseContext(
      SELinux::SetPropertyUseContextRequest{context});
}

Future<SELinux::GetPropertyUseContextReply> SELinux::GetPropertyUseContext(
    const SELinux::GetPropertyUseContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetPropertyUseContextReply>(
      &buf, "SELinux::GetPropertyUseContext", false);
}

Future<SELinux::GetPropertyUseContextReply> SELinux::GetPropertyUseContext() {
  return SELinux::GetPropertyUseContext(
      SELinux::GetPropertyUseContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetPropertyUseContextReply> detail::ReadReply<
    SELinux::GetPropertyUseContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetPropertyUseContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::GetPropertyContextReply> SELinux::GetPropertyContext(
    const SELinux::GetPropertyContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetPropertyContextReply>(
      &buf, "SELinux::GetPropertyContext", false);
}

Future<SELinux::GetPropertyContextReply> SELinux::GetPropertyContext(
    const Window& window,
    const Atom& property) {
  return SELinux::GetPropertyContext(
      SELinux::GetPropertyContextRequest{window, property});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetPropertyContextReply> detail::ReadReply<
    SELinux::GetPropertyContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetPropertyContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::GetPropertyDataContextReply> SELinux::GetPropertyDataContext(
    const SELinux::GetPropertyDataContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetPropertyDataContextReply>(
      &buf, "SELinux::GetPropertyDataContext", false);
}

Future<SELinux::GetPropertyDataContextReply> SELinux::GetPropertyDataContext(
    const Window& window,
    const Atom& property) {
  return SELinux::GetPropertyDataContext(
      SELinux::GetPropertyDataContextRequest{window, property});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetPropertyDataContextReply> detail::ReadReply<
    SELinux::GetPropertyDataContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetPropertyDataContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::ListPropertiesReply> SELinux::ListProperties(
    const SELinux::ListPropertiesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::ListPropertiesReply>(
      &buf, "SELinux::ListProperties", false);
}

Future<SELinux::ListPropertiesReply> SELinux::ListProperties(
    const Window& window) {
  return SELinux::ListProperties(SELinux::ListPropertiesRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::ListPropertiesReply> detail::ReadReply<
    SELinux::ListPropertiesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::ListPropertiesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t properties_len{};
  auto& properties = (*reply).properties;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // properties_len
  Read(&properties_len, &buf);

  // pad1
  Pad(&buf, 20);

  // properties
  properties.resize(properties_len);
  for (auto& properties_elem : properties) {
    // properties_elem
    {
      auto& name = properties_elem.name;
      uint32_t object_context_len{};
      uint32_t data_context_len{};
      auto& object_context = properties_elem.object_context;
      auto& data_context = properties_elem.data_context;

      // name
      Read(&name, &buf);

      // object_context_len
      Read(&object_context_len, &buf);

      // data_context_len
      Read(&data_context_len, &buf);

      // object_context
      object_context.resize(object_context_len);
      for (auto& object_context_elem : object_context) {
        // object_context_elem
        Read(&object_context_elem, &buf);
      }

      // pad0
      Align(&buf, 4);

      // data_context
      data_context.resize(data_context_len);
      for (auto& data_context_elem : data_context) {
        // data_context_elem
        Read(&data_context_elem, &buf);
      }

      // pad1
      Align(&buf, 4);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> SELinux::SetSelectionCreateContext(
    const SELinux::SetSelectionCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t context_len{};
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_len
  context_len = context.size();
  buf.Write(&context_len);

  // context
  DCHECK_EQ(static_cast<size_t>(context_len), context.size());
  for (auto& context_elem : context) {
    // context_elem
    buf.Write(&context_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(
      &buf, "SELinux::SetSelectionCreateContext", false);
}

Future<void> SELinux::SetSelectionCreateContext(const std::string& context) {
  return SELinux::SetSelectionCreateContext(
      SELinux::SetSelectionCreateContextRequest{context});
}

Future<SELinux::GetSelectionCreateContextReply>
SELinux::GetSelectionCreateContext(
    const SELinux::GetSelectionCreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetSelectionCreateContextReply>(
      &buf, "SELinux::GetSelectionCreateContext", false);
}

Future<SELinux::GetSelectionCreateContextReply>
SELinux::GetSelectionCreateContext() {
  return SELinux::GetSelectionCreateContext(
      SELinux::GetSelectionCreateContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetSelectionCreateContextReply> detail::ReadReply<
    SELinux::GetSelectionCreateContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetSelectionCreateContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> SELinux::SetSelectionUseContext(
    const SELinux::SetSelectionUseContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t context_len{};
  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_len
  context_len = context.size();
  buf.Write(&context_len);

  // context
  DCHECK_EQ(static_cast<size_t>(context_len), context.size());
  for (auto& context_elem : context) {
    // context_elem
    buf.Write(&context_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SELinux::SetSelectionUseContext",
                                        false);
}

Future<void> SELinux::SetSelectionUseContext(const std::string& context) {
  return SELinux::SetSelectionUseContext(
      SELinux::SetSelectionUseContextRequest{context});
}

Future<SELinux::GetSelectionUseContextReply> SELinux::GetSelectionUseContext(
    const SELinux::GetSelectionUseContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetSelectionUseContextReply>(
      &buf, "SELinux::GetSelectionUseContext", false);
}

Future<SELinux::GetSelectionUseContextReply> SELinux::GetSelectionUseContext() {
  return SELinux::GetSelectionUseContext(
      SELinux::GetSelectionUseContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetSelectionUseContextReply> detail::ReadReply<
    SELinux::GetSelectionUseContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetSelectionUseContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::GetSelectionContextReply> SELinux::GetSelectionContext(
    const SELinux::GetSelectionContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& selection = request.selection;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // selection
  buf.Write(&selection);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetSelectionContextReply>(
      &buf, "SELinux::GetSelectionContext", false);
}

Future<SELinux::GetSelectionContextReply> SELinux::GetSelectionContext(
    const Atom& selection) {
  return SELinux::GetSelectionContext(
      SELinux::GetSelectionContextRequest{selection});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetSelectionContextReply> detail::ReadReply<
    SELinux::GetSelectionContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetSelectionContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::GetSelectionDataContextReply> SELinux::GetSelectionDataContext(
    const SELinux::GetSelectionDataContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& selection = request.selection;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // selection
  buf.Write(&selection);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetSelectionDataContextReply>(
      &buf, "SELinux::GetSelectionDataContext", false);
}

Future<SELinux::GetSelectionDataContextReply> SELinux::GetSelectionDataContext(
    const Atom& selection) {
  return SELinux::GetSelectionDataContext(
      SELinux::GetSelectionDataContextRequest{selection});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetSelectionDataContextReply> detail::ReadReply<
    SELinux::GetSelectionDataContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetSelectionDataContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::ListSelectionsReply> SELinux::ListSelections(
    const SELinux::ListSelectionsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 21;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::ListSelectionsReply>(
      &buf, "SELinux::ListSelections", false);
}

Future<SELinux::ListSelectionsReply> SELinux::ListSelections() {
  return SELinux::ListSelections(SELinux::ListSelectionsRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::ListSelectionsReply> detail::ReadReply<
    SELinux::ListSelectionsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::ListSelectionsReply>();

  auto& sequence = (*reply).sequence;
  uint32_t selections_len{};
  auto& selections = (*reply).selections;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // selections_len
  Read(&selections_len, &buf);

  // pad1
  Pad(&buf, 20);

  // selections
  selections.resize(selections_len);
  for (auto& selections_elem : selections) {
    // selections_elem
    {
      auto& name = selections_elem.name;
      uint32_t object_context_len{};
      uint32_t data_context_len{};
      auto& object_context = selections_elem.object_context;
      auto& data_context = selections_elem.data_context;

      // name
      Read(&name, &buf);

      // object_context_len
      Read(&object_context_len, &buf);

      // data_context_len
      Read(&data_context_len, &buf);

      // object_context
      object_context.resize(object_context_len);
      for (auto& object_context_elem : object_context) {
        // object_context_elem
        Read(&object_context_elem, &buf);
      }

      // pad0
      Align(&buf, 4);

      // data_context
      data_context.resize(data_context_len);
      for (auto& data_context_elem : data_context) {
        // data_context_elem
        Read(&data_context_elem, &buf);
      }

      // pad1
      Align(&buf, 4);
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SELinux::GetClientContextReply> SELinux::GetClientContext(
    const SELinux::GetClientContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& resource = request.resource;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // resource
  buf.Write(&resource);

  Align(&buf, 4);

  return connection_->SendRequest<SELinux::GetClientContextReply>(
      &buf, "SELinux::GetClientContext", false);
}

Future<SELinux::GetClientContextReply> SELinux::GetClientContext(
    const uint32_t& resource) {
  return SELinux::GetClientContext(SELinux::GetClientContextRequest{resource});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SELinux::GetClientContextReply> detail::ReadReply<
    SELinux::GetClientContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SELinux::GetClientContextReply>();

  auto& sequence = (*reply).sequence;
  uint32_t context_len{};
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context_len
  Read(&context_len, &buf);

  // pad1
  Pad(&buf, 20);

  // context
  context.resize(context_len);
  for (auto& context_elem : context) {
    // context_elem
    Read(&context_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
