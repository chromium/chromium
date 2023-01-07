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

#include "record.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Record::Record(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string Record::BadContextError::ToString() const {
  std::stringstream ss_;
  ss_ << "Record::BadContextError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".invalid_record = " << static_cast<uint64_t>(invalid_record) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Record::BadContextError>(Record::BadContextError* error_,
                                        ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& invalid_record = (*error_).invalid_record;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // invalid_record
  Read(&invalid_record, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  DCHECK_LE(buf.offset, 32ul);
}
Future<Record::QueryVersionReply> Record::QueryVersion(
    const Record::QueryVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major_version = request.major_version;
  auto& minor_version = request.minor_version;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major_version
  buf.Write(&major_version);

  // minor_version
  buf.Write(&minor_version);

  Align(&buf, 4);

  return connection_->SendRequest<Record::QueryVersionReply>(
      &buf, "Record::QueryVersion", false);
}

Future<Record::QueryVersionReply> Record::QueryVersion(
    const uint16_t& major_version,
    const uint16_t& minor_version) {
  return Record::QueryVersion(
      Record::QueryVersionRequest{major_version, minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Record::QueryVersionReply> detail::ReadReply<
    Record::QueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Record::QueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& major_version = (*reply).major_version;
  auto& minor_version = (*reply).minor_version;

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

  // major_version
  Read(&major_version, &buf);

  // minor_version
  Read(&minor_version, &buf);

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Record::CreateContext(
    const Record::CreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& element_header = request.element_header;
  uint32_t num_client_specs{};
  uint32_t num_ranges{};
  auto& client_specs = request.client_specs;
  size_t client_specs_len = client_specs.size();
  auto& ranges = request.ranges;
  size_t ranges_len = ranges.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // element_header
  buf.Write(&element_header);

  // pad0
  Pad(&buf, 3);

  // num_client_specs
  num_client_specs = client_specs.size();
  buf.Write(&num_client_specs);

  // num_ranges
  num_ranges = ranges.size();
  buf.Write(&num_ranges);

  // client_specs
  DCHECK_EQ(static_cast<size_t>(num_client_specs), client_specs.size());
  for (auto& client_specs_elem : client_specs) {
    // client_specs_elem
    buf.Write(&client_specs_elem);
  }

  // ranges
  DCHECK_EQ(static_cast<size_t>(num_ranges), ranges.size());
  for (auto& ranges_elem : ranges) {
    // ranges_elem
    {
      auto& core_requests = ranges_elem.core_requests;
      auto& core_replies = ranges_elem.core_replies;
      auto& ext_requests = ranges_elem.ext_requests;
      auto& ext_replies = ranges_elem.ext_replies;
      auto& delivered_events = ranges_elem.delivered_events;
      auto& device_events = ranges_elem.device_events;
      auto& errors = ranges_elem.errors;
      auto& client_started = ranges_elem.client_started;
      auto& client_died = ranges_elem.client_died;

      // core_requests
      {
        auto& first = core_requests.first;
        auto& last = core_requests.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // core_replies
      {
        auto& first = core_replies.first;
        auto& last = core_replies.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // ext_requests
      {
        auto& major = ext_requests.major;
        auto& minor = ext_requests.minor;

        // major
        {
          auto& first = major.first;
          auto& last = major.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }

        // minor
        {
          auto& first = minor.first;
          auto& last = minor.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }
      }

      // ext_replies
      {
        auto& major = ext_replies.major;
        auto& minor = ext_replies.minor;

        // major
        {
          auto& first = major.first;
          auto& last = major.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }

        // minor
        {
          auto& first = minor.first;
          auto& last = minor.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }
      }

      // delivered_events
      {
        auto& first = delivered_events.first;
        auto& last = delivered_events.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // device_events
      {
        auto& first = device_events.first;
        auto& last = device_events.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // errors
      {
        auto& first = errors.first;
        auto& last = errors.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // client_started
      buf.Write(&client_started);

      // client_died
      buf.Write(&client_died);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Record::CreateContext", false);
}

Future<void> Record::CreateContext(const Context& context,
                                   const ElementHeader& element_header,
                                   const std::vector<ClientSpec>& client_specs,
                                   const std::vector<Range>& ranges) {
  return Record::CreateContext(Record::CreateContextRequest{
      context, element_header, client_specs, ranges});
}

Future<void> Record::RegisterClients(
    const Record::RegisterClientsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& element_header = request.element_header;
  uint32_t num_client_specs{};
  uint32_t num_ranges{};
  auto& client_specs = request.client_specs;
  size_t client_specs_len = client_specs.size();
  auto& ranges = request.ranges;
  size_t ranges_len = ranges.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // element_header
  buf.Write(&element_header);

  // pad0
  Pad(&buf, 3);

  // num_client_specs
  num_client_specs = client_specs.size();
  buf.Write(&num_client_specs);

  // num_ranges
  num_ranges = ranges.size();
  buf.Write(&num_ranges);

  // client_specs
  DCHECK_EQ(static_cast<size_t>(num_client_specs), client_specs.size());
  for (auto& client_specs_elem : client_specs) {
    // client_specs_elem
    buf.Write(&client_specs_elem);
  }

  // ranges
  DCHECK_EQ(static_cast<size_t>(num_ranges), ranges.size());
  for (auto& ranges_elem : ranges) {
    // ranges_elem
    {
      auto& core_requests = ranges_elem.core_requests;
      auto& core_replies = ranges_elem.core_replies;
      auto& ext_requests = ranges_elem.ext_requests;
      auto& ext_replies = ranges_elem.ext_replies;
      auto& delivered_events = ranges_elem.delivered_events;
      auto& device_events = ranges_elem.device_events;
      auto& errors = ranges_elem.errors;
      auto& client_started = ranges_elem.client_started;
      auto& client_died = ranges_elem.client_died;

      // core_requests
      {
        auto& first = core_requests.first;
        auto& last = core_requests.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // core_replies
      {
        auto& first = core_replies.first;
        auto& last = core_replies.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // ext_requests
      {
        auto& major = ext_requests.major;
        auto& minor = ext_requests.minor;

        // major
        {
          auto& first = major.first;
          auto& last = major.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }

        // minor
        {
          auto& first = minor.first;
          auto& last = minor.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }
      }

      // ext_replies
      {
        auto& major = ext_replies.major;
        auto& minor = ext_replies.minor;

        // major
        {
          auto& first = major.first;
          auto& last = major.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }

        // minor
        {
          auto& first = minor.first;
          auto& last = minor.last;

          // first
          buf.Write(&first);

          // last
          buf.Write(&last);
        }
      }

      // delivered_events
      {
        auto& first = delivered_events.first;
        auto& last = delivered_events.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // device_events
      {
        auto& first = device_events.first;
        auto& last = device_events.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // errors
      {
        auto& first = errors.first;
        auto& last = errors.last;

        // first
        buf.Write(&first);

        // last
        buf.Write(&last);
      }

      // client_started
      buf.Write(&client_started);

      // client_died
      buf.Write(&client_died);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Record::RegisterClients", false);
}

Future<void> Record::RegisterClients(
    const Context& context,
    const ElementHeader& element_header,
    const std::vector<ClientSpec>& client_specs,
    const std::vector<Range>& ranges) {
  return Record::RegisterClients(Record::RegisterClientsRequest{
      context, element_header, client_specs, ranges});
}

Future<void> Record::UnregisterClients(
    const Record::UnregisterClientsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  uint32_t num_client_specs{};
  auto& client_specs = request.client_specs;
  size_t client_specs_len = client_specs.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // num_client_specs
  num_client_specs = client_specs.size();
  buf.Write(&num_client_specs);

  // client_specs
  DCHECK_EQ(static_cast<size_t>(num_client_specs), client_specs.size());
  for (auto& client_specs_elem : client_specs) {
    // client_specs_elem
    buf.Write(&client_specs_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Record::UnregisterClients",
                                        false);
}

Future<void> Record::UnregisterClients(
    const Context& context,
    const std::vector<ClientSpec>& client_specs) {
  return Record::UnregisterClients(
      Record::UnregisterClientsRequest{context, client_specs});
}

Future<Record::GetContextReply> Record::GetContext(
    const Record::GetContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<Record::GetContextReply>(
      &buf, "Record::GetContext", false);
}

Future<Record::GetContextReply> Record::GetContext(const Context& context) {
  return Record::GetContext(Record::GetContextRequest{context});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Record::GetContextReply> detail::ReadReply<
    Record::GetContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Record::GetContextReply>();

  auto& enabled = (*reply).enabled;
  auto& sequence = (*reply).sequence;
  auto& element_header = (*reply).element_header;
  uint32_t num_intercepted_clients{};
  auto& intercepted_clients = (*reply).intercepted_clients;
  size_t intercepted_clients_len = intercepted_clients.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // enabled
  Read(&enabled, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // element_header
  Read(&element_header, &buf);

  // pad0
  Pad(&buf, 3);

  // num_intercepted_clients
  Read(&num_intercepted_clients, &buf);

  // pad1
  Pad(&buf, 16);

  // intercepted_clients
  intercepted_clients.resize(num_intercepted_clients);
  for (auto& intercepted_clients_elem : intercepted_clients) {
    // intercepted_clients_elem
    {
      auto& client_resource = intercepted_clients_elem.client_resource;
      uint32_t num_ranges{};
      auto& ranges = intercepted_clients_elem.ranges;
      size_t ranges_len = ranges.size();

      // client_resource
      Read(&client_resource, &buf);

      // num_ranges
      Read(&num_ranges, &buf);

      // ranges
      ranges.resize(num_ranges);
      for (auto& ranges_elem : ranges) {
        // ranges_elem
        {
          auto& core_requests = ranges_elem.core_requests;
          auto& core_replies = ranges_elem.core_replies;
          auto& ext_requests = ranges_elem.ext_requests;
          auto& ext_replies = ranges_elem.ext_replies;
          auto& delivered_events = ranges_elem.delivered_events;
          auto& device_events = ranges_elem.device_events;
          auto& errors = ranges_elem.errors;
          auto& client_started = ranges_elem.client_started;
          auto& client_died = ranges_elem.client_died;

          // core_requests
          {
            auto& first = core_requests.first;
            auto& last = core_requests.last;

            // first
            Read(&first, &buf);

            // last
            Read(&last, &buf);
          }

          // core_replies
          {
            auto& first = core_replies.first;
            auto& last = core_replies.last;

            // first
            Read(&first, &buf);

            // last
            Read(&last, &buf);
          }

          // ext_requests
          {
            auto& major = ext_requests.major;
            auto& minor = ext_requests.minor;

            // major
            {
              auto& first = major.first;
              auto& last = major.last;

              // first
              Read(&first, &buf);

              // last
              Read(&last, &buf);
            }

            // minor
            {
              auto& first = minor.first;
              auto& last = minor.last;

              // first
              Read(&first, &buf);

              // last
              Read(&last, &buf);
            }
          }

          // ext_replies
          {
            auto& major = ext_replies.major;
            auto& minor = ext_replies.minor;

            // major
            {
              auto& first = major.first;
              auto& last = major.last;

              // first
              Read(&first, &buf);

              // last
              Read(&last, &buf);
            }

            // minor
            {
              auto& first = minor.first;
              auto& last = minor.last;

              // first
              Read(&first, &buf);

              // last
              Read(&last, &buf);
            }
          }

          // delivered_events
          {
            auto& first = delivered_events.first;
            auto& last = delivered_events.last;

            // first
            Read(&first, &buf);

            // last
            Read(&last, &buf);
          }

          // device_events
          {
            auto& first = device_events.first;
            auto& last = device_events.last;

            // first
            Read(&first, &buf);

            // last
            Read(&last, &buf);
          }

          // errors
          {
            auto& first = errors.first;
            auto& last = errors.last;

            // first
            Read(&first, &buf);

            // last
            Read(&last, &buf);
          }

          // client_started
          Read(&client_started, &buf);

          // client_died
          Read(&client_died, &buf);
        }
      }
    }
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Record::EnableContextReply> Record::EnableContext(
    const Record::EnableContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

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

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<Record::EnableContextReply>(
      &buf, "Record::EnableContext", false);
}

Future<Record::EnableContextReply> Record::EnableContext(
    const Context& context) {
  return Record::EnableContext(Record::EnableContextRequest{context});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Record::EnableContextReply> detail::ReadReply<
    Record::EnableContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Record::EnableContextReply>();

  auto& category = (*reply).category;
  auto& sequence = (*reply).sequence;
  auto& element_header = (*reply).element_header;
  auto& client_swapped = (*reply).client_swapped;
  auto& xid_base = (*reply).xid_base;
  auto& server_time = (*reply).server_time;
  auto& rec_sequence_num = (*reply).rec_sequence_num;
  auto& data = (*reply).data;
  size_t data_len = data.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // category
  Read(&category, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // element_header
  Read(&element_header, &buf);

  // client_swapped
  Read(&client_swapped, &buf);

  // pad0
  Pad(&buf, 2);

  // xid_base
  Read(&xid_base, &buf);

  // server_time
  Read(&server_time, &buf);

  // rec_sequence_num
  Read(&rec_sequence_num, &buf);

  // pad1
  Pad(&buf, 8);

  // data
  data.resize((length) * (4));
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Record::DisableContext(
    const Record::DisableContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Record::DisableContext", false);
}

Future<void> Record::DisableContext(const Context& context) {
  return Record::DisableContext(Record::DisableContextRequest{context});
}

Future<void> Record::FreeContext(const Record::FreeContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Record::FreeContext", false);
}

Future<void> Record::FreeContext(const Context& context) {
  return Record::FreeContext(Record::FreeContextRequest{context});
}

}  // namespace x11
