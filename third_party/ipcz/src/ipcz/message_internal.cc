// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/message_internal.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/safe_math.h"

namespace ipcz::internal {

namespace {

// Helper to transform a driver object attached to `message` into its serialized
// form within the message by running it through the driver's serializer.
//
// Metadata is placed into a DriverObjectData structure at `data_offset` bytes
// from the begining of the message. Serialized data bytes are stored in an
// array appended to `message` and referenced by the DriverObjectData, and any
// transmissible handles emitted by the driver are appended to
// `transmissible_handles`, with relevant index and count also stashed in the
// DriverObjectData.
IpczResult SerializeDriverObject(
    uint32_t data_offset,
    const DriverTransport& transport,
    MessageBase& message,
    absl::InlinedVector<IpczDriverHandle, 2>& transmissible_handles) {
  DriverObjectData* data =
      reinterpret_cast<DriverObjectData*>(&message.data_view()[data_offset]);
  DriverObject object =
      std::move(message.driver_objects()[data->first_driver_handle]);
  if (!object.is_valid()) {
    // This is not a valid driver handle and it cannot be serialized.
    data->num_driver_handles = 0;
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  // NOTE: `data` may be invalid after the allocation below. It's nulled here to
  // help catch accidental reuse.
  data = nullptr;

  uint32_t driver_data_array = 0;
  DriverObject::SerializedDimensions dimensions =
      object.GetSerializedDimensions(transport);
  if (dimensions.num_bytes > 0) {
    driver_data_array = message.AllocateArray<uint8_t>(dimensions.num_bytes);
  }

  const uint32_t first_handle =
      static_cast<uint32_t>(transmissible_handles.size());
  data = reinterpret_cast<DriverObjectData*>(&message.data_view()[data_offset]);
  absl::Span<uint8_t> driver_data =
      message.GetArrayView<uint8_t>(driver_data_array);
  data->driver_data_array = driver_data_array;
  data->num_driver_handles = dimensions.num_driver_handles;
  data->first_driver_handle = first_handle;

  transmissible_handles.resize(transmissible_handles.size() +
                               dimensions.num_driver_handles);

  auto handles_view = absl::MakeSpan(transmissible_handles);
  object.Serialize(
      transport, driver_data,
      handles_view.subspan(first_handle, dimensions.num_driver_handles));
  return IPCZ_RESULT_OK;
}

// Returns `true` if and only if it will be safe to use GetArrayView() to access
// the contents of a serialized array beginning at `array_offset` bytes from
// the start of `message`, where each element is `element_size` bytes wide.
bool IsArrayValid(MessageBase& message,
                  uint32_t array_offset,
                  size_t element_size) {
  if (array_offset == 0) {
    return true;
  }

  const absl::Span<uint8_t> data = message.data_view();
  if (array_offset >= data.size()) {
    return false;
  }

  size_t bytes_available = data.size() - array_offset;
  if (bytes_available < sizeof(ArrayHeader)) {
    return false;
  }

  ArrayHeader& header = *reinterpret_cast<ArrayHeader*>(&data[array_offset]);
  if (bytes_available < header.num_bytes ||
      header.num_bytes < sizeof(ArrayHeader)) {
    return false;
  }

  size_t max_num_elements =
      (header.num_bytes - sizeof(ArrayHeader)) / element_size;
  if (header.num_elements > max_num_elements) {
    return false;
  }

  return true;
}

// Deserializes a driver object encoded within `message`, appending the object
// for later retrieval via driver_objects() or TakeDriverObject().
bool DeserializeDriverObject(MessageBase& message,
                             DriverObjectData& object_data,
                             absl::Span<const IpczDriverHandle> handles,
                             const DriverTransport& transport) {
  if (!IsArrayValid(message, object_data.driver_data_array, sizeof(uint8_t))) {
    return false;
  }

  auto driver_data =
      message.GetArrayView<uint8_t>(object_data.driver_data_array);
  if (object_data.num_driver_handles > handles.size()) {
    return false;
  }

  if (handles.size() - object_data.num_driver_handles <
      object_data.first_driver_handle) {
    return false;
  }

  DriverObject object = DriverObject::Deserialize(
      transport, driver_data,
      handles.subspan(object_data.first_driver_handle,
                      object_data.num_driver_handles));
  if (!object.is_valid()) {
    return false;
  }

  message.AppendDriverObject(std::move(object), object_data);
  return true;
}

}  // namespace

MessageBase::MessageBase(uint8_t message_id, size_t params_size)
    : data_(sizeof(MessageHeader) + params_size),
      message_id_(message_id),
      params_size_(params_size) {
  MessageHeader& h = header();
  h.size = sizeof(h);
  h.version = 0;
  h.message_id = message_id;
}

MessageBase::~MessageBase() = default;

uint32_t MessageBase::AllocateGenericArray(size_t element_size,
                                           size_t num_elements) {
  if (num_elements == 0) {
    return 0;
  }
  size_t offset = Align(data_.size());
  size_t num_bytes = Align(
      CheckAdd(sizeof(ArrayHeader), CheckMul(element_size, num_elements)));
  data_.resize(CheckAdd(offset, num_bytes));
  ArrayHeader& header = *reinterpret_cast<ArrayHeader*>(&data_[offset]);
  header.num_bytes = checked_cast<uint32_t>(num_bytes);
  header.num_elements = checked_cast<uint32_t>(num_elements);
  return offset;
}

uint32_t MessageBase::AppendDriverObjects(absl::Span<DriverObject> objects) {
  const uint32_t array_param = AllocateArray<DriverObjectData>(objects.size());
  const absl::Span<DriverObjectData> object_data =
      GetArrayView<DriverObjectData>(array_param);
  for (size_t i = 0; i < objects.size(); ++i) {
    AppendDriverObject(std::move(objects[i]), object_data[i]);
  }
  return array_param;
}

void MessageBase::AppendDriverObject(DriverObject object,
                                     DriverObjectData& data) {
  // This is only a placeholder used later by Serialize() to locate the
  // serializable object within `driver_objects_`. Serialize() will then fill in
  // this structure with more appropriate metadata pertaining to the object's
  // serialized encoding.
  data.driver_data_array = 0;
  data.first_driver_handle = checked_cast<uint32_t>(driver_objects_.size());
  data.num_driver_handles = 1;
  driver_objects_.push_back(std::move(object));
}

DriverObject MessageBase::TakeDriverObject(const DriverObjectData& data) {
  // When properly deserialized, every logical driver object field in a message
  // should correspond to a single attached DriverObject. This is validated
  // during deserialization, so these assertions are safe.
  ABSL_ASSERT(data.num_driver_handles == 1);
  ABSL_ASSERT(driver_objects_.size() > data.first_driver_handle);
  return std::move(driver_objects_[data.first_driver_handle]);
}

bool MessageBase::CanTransmitOn(const DriverTransport& transport) {
  for (DriverObject& object : driver_objects_) {
    if (!object.CanTransmitOn(transport)) {
      return false;
    }
  }
  return true;
}

void MessageBase::Serialize(absl::Span<const ParamMetadata> params,
                            const DriverTransport& transport) {
  ABSL_ASSERT(CanTransmitOn(transport));
  absl::InlinedVector<IpczDriverHandle, 2> transmissible_handles;
  for (const auto& param : params) {
    switch (param.type) {
      case ParamType::kDriverObject: {
        IpczResult result = SerializeDriverObject(
            GetDataOffset(&GetParamValueAt<DriverObjectData>(param.offset)),
            transport, *this, transmissible_handles);
        ABSL_ASSERT(result == IPCZ_RESULT_OK);
        break;
      }

      case ParamType::kDriverObjectArray: {
        const uint32_t array_data_offset =
            GetParamValueAt<uint32_t>(param.offset);
        const size_t num_objects =
            GetArrayView<DriverObjectData>(array_data_offset).size();
        for (size_t i = 0; i < num_objects; ++i) {
          // Note that the address of this array can move on each iteration, as
          // SerializeDriverObject may need to reallocate the data buffer. Hence
          // we resolve it from the array offset each time.
          auto data = GetArrayView<DriverObjectData>(array_data_offset);
          IpczResult result = SerializeDriverObject(
              GetDataOffset(&data[i]), transport, *this, transmissible_handles);
          ABSL_ASSERT(result == IPCZ_RESULT_OK);
        }
        break;
      }

      default:
        // No additional work needed to serialize plain data or data array
        // fields.
        break;
    }
  }

  // Basic consistency check: all driver objects must have been taken and
  // serialized.
  for (const auto& object : driver_objects_) {
    ABSL_ASSERT(!object.is_valid());
  }

  transmissible_driver_handles_ = std::move(transmissible_handles);
}

bool MessageBase::DeserializeFromTransport(
    size_t params_size,
    uint32_t params_current_version,
    absl::Span<const ParamMetadata> params_metadata,
    absl::Span<const uint8_t> data,
    absl::Span<const IpczDriverHandle> handles,
    const DriverTransport& transport) {
  // Copy the data into a local message object to avoid any TOCTOU issues in
  // case `data` is in unsafe shared memory.
  data_.resize(data.size());
  memcpy(data_.data(), data.data(), data.size());

  // Validate the header. The message must at least be large enough to encode a
  // v0 MessageHeader, and the encoded header size and version must make sense
  // (e.g. version 0 size must be sizeof(MessageHeader))
  if (data_.size() < sizeof(MessageHeaderV0)) {
    return false;
  }

  const auto& message_header =
      *reinterpret_cast<const MessageHeaderV0*>(data_.data());
  if (message_header.version == 0) {
    if (message_header.size != sizeof(MessageHeaderV0)) {
      return false;
    }
  } else {
    if (message_header.size < sizeof(MessageHeaderV0)) {
      return false;
    }
  }

  if (message_header.size > data_.size()) {
    return false;
  }

  // Validate parameter data. There must be at least enough bytes following the
  // header to encode a StructHeader and to account for all parameter data.

  absl::Span<uint8_t> params_data = params_data_view();
  if (params_data.size() < sizeof(StructHeader)) {
    return false;
  }

  StructHeader& params_header =
      *reinterpret_cast<StructHeader*>(params_data.data());
  if (params_current_version < params_header.version) {
    params_header.version = params_current_version;
  }

  // The param struct's header claims to consist of more data than is present in
  // the message. Not good.
  if (params_data.size() < params_header.size) {
    return false;
  }

  // Finally, validate each parameter and unpack driver objects.
  for (const ParamMetadata& param : params_metadata) {
    if (param.offset >= params_header.size ||
        param.offset + param.size > params_header.size) {
      return false;
    }

    if (param.array_element_size > 0) {
      const uint32_t array_offset =
          *reinterpret_cast<uint32_t*>(&params_data[param.offset]);
      if (!IsArrayValid(*this, array_offset, param.array_element_size)) {
        return false;
      }
    }

    switch (param.type) {
      case ParamType::kDriverObject:
        if (!DeserializeDriverObject(
                *this, GetParamValueAt<DriverObjectData>(param.offset), handles,
                transport)) {
          return false;
        }
        break;

      case ParamType::kDriverObjectArray: {
        auto objects = GetArrayView<DriverObjectData>(
            GetParamValueAt<uint32_t>(param.offset));
        for (DriverObjectData& object : objects) {
          if (!DeserializeDriverObject(*this, object, handles, transport)) {
            return false;
          }
        }
        break;
      }

      default:
        break;
    }
  }

  return true;
}

}  // namespace ipcz::internal
