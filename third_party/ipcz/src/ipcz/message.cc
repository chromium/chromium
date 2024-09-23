// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/message.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/safe_math.h"

namespace ipcz {

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
bool SerializeDriverObject(
    DriverObject object,
    const DriverTransport& transport,
    Message& message,
    internal::DriverObjectData& data,
    absl::InlinedVector<IpczDriverHandle, 2>& transmissible_handles) {
  if (!object.is_valid()) {
    // This is not a valid driver handle and it cannot be serialized.
    data.num_driver_handles = 0;
    return false;
  }

  uint32_t driver_data_array = 0;
  DriverObject::SerializedDimensions dimensions =
      object.GetSerializedDimensions(transport);
  if (dimensions.num_bytes > 0) {
    driver_data_array = message.AllocateArray<uint8_t>(dimensions.num_bytes);
  }

  const uint32_t first_handle =
      static_cast<uint32_t>(transmissible_handles.size());
  absl::Span<uint8_t> driver_data =
      message.GetArrayView<uint8_t>(driver_data_array);
  data.driver_data_array = driver_data_array;
  data.num_driver_handles = dimensions.num_driver_handles;
  data.first_driver_handle = first_handle;

  transmissible_handles.resize(transmissible_handles.size() +
                               dimensions.num_driver_handles);

  auto handles_view = absl::MakeSpan(transmissible_handles);
  if (!object.Serialize(
          transport, driver_data,
          handles_view.subspan(first_handle, dimensions.num_driver_handles))) {
    return false;
  }

  return true;
}

// Returns `true` if and only if it will be safe to use GetArrayView() to access
// the contents of a serialized array beginning at `array_offset` bytes from
// the start of `message`, where each element is `element_size` bytes wide.
bool IsArrayValid(Message& message,
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
  if (bytes_available < sizeof(internal::ArrayHeader)) {
    return false;
  }

  auto& header = *reinterpret_cast<internal::ArrayHeader*>(&data[array_offset]);
  if (bytes_available < header.num_bytes ||
      header.num_bytes < sizeof(internal::ArrayHeader)) {
    return false;
  }

  size_t max_num_elements =
      (header.num_bytes - sizeof(internal::ArrayHeader)) / element_size;
  if (header.num_elements > max_num_elements) {
    return false;
  }

  return true;
}

// Deserializes a driver object encoded within `message`, returning the object
// on success. On failure, an invalid DriverObject is returned.
DriverObject DeserializeDriverObject(
    Message& message,
    const internal::DriverObjectData& object_data,
    absl::Span<const IpczDriverHandle> handles,
    const DriverTransport& transport) {
  if (!IsArrayValid(message, object_data.driver_data_array, sizeof(uint8_t))) {
    return {};
  }

  auto driver_data =
      message.GetArrayView<uint8_t>(object_data.driver_data_array);
  if (object_data.num_driver_handles > handles.size()) {
    return {};
  }

  if (handles.size() - object_data.num_driver_handles <
      object_data.first_driver_handle) {
    return {};
  }

  return DriverObject::Deserialize(
      transport, driver_data,
      handles.subspan(object_data.first_driver_handle,
                      object_data.num_driver_handles));
}

bool IsAligned(size_t n) {
  return n % 8 == 0;
}

}  // namespace

Message::ReceivedDataBuffer::ReceivedDataBuffer() = default;

// NOTE: This malloc'd buffer is intentionally NOT zero-initialized, because we
// will fully overwrite its contents.
Message::ReceivedDataBuffer::ReceivedDataBuffer(size_t size)
    : data_(static_cast<uint8_t*>(malloc(size))), size_(size) {}

Message::ReceivedDataBuffer::ReceivedDataBuffer(ReceivedDataBuffer&& other)
    : data_(std::move(other.data_)), size_(std::exchange(other.size_, 0)) {}

Message::ReceivedDataBuffer& Message::ReceivedDataBuffer::operator=(
    ReceivedDataBuffer&& other) {
  data_ = std::move(other.data_);
  size_ = std::exchange(other.size_, 0);
  return *this;
}

Message::ReceivedDataBuffer::~ReceivedDataBuffer() = default;

Message::Message() = default;

Message::Message(uint8_t message_id, size_t params_size)
    : inlined_data_(sizeof(internal::MessageHeader) + params_size),
      data_(absl::MakeSpan(*inlined_data_)) {
  internal::MessageHeader& h = header();
  h.size = sizeof(h);
  h.version = 0;
  h.message_id = message_id;
  h.driver_object_data_array = 0;

  ABSL_ASSERT(IsAligned(inlined_data_->size()));
}

Message::~Message() = default;

uint32_t Message::AllocateGenericArray(size_t element_size,
                                       size_t num_elements) {
  if (num_elements == 0) {
    return 0;
  }
  size_t offset = Align(data_.size());
  size_t num_bytes = Align(CheckAdd(sizeof(internal::ArrayHeader),
                                    CheckMul(element_size, num_elements)));

  ABSL_ASSERT(inlined_data_);
  inlined_data_->resize(CheckAdd(offset, num_bytes));
  data_ = absl::MakeSpan(*inlined_data_);
  auto& header = *reinterpret_cast<internal::ArrayHeader*>(&data_[offset]);
  header.num_bytes = checked_cast<uint32_t>(num_bytes);
  header.num_elements = checked_cast<uint32_t>(num_elements);
  return offset;
}

uint32_t Message::AppendDriverObject(DriverObject object) {
  if (!object.is_valid()) {
    return internal::kInvalidDriverObjectIndex;
  }

  const uint32_t index = checked_cast<uint32_t>(driver_objects_.size());
  driver_objects_.push_back(std::move(object));
  return index;
}

internal::DriverObjectArrayData Message::AppendDriverObjects(
    absl::Span<DriverObject> objects) {
  const internal::DriverObjectArrayData data = {
      .first_object_index = checked_cast<uint32_t>(driver_objects_.size()),
      .num_objects = checked_cast<uint32_t>(objects.size()),
  };
  driver_objects_.reserve(driver_objects_.size() + objects.size());
  for (auto& object : objects) {
    ABSL_ASSERT(object.is_valid());
    driver_objects_.push_back(std::move(object));
  }
  return data;
}

DriverObject Message::TakeDriverObject(uint32_t index) {
  if (index == internal::kInvalidDriverObjectIndex) {
    return {};
  }

  // Note that `index` has already been validated by now.
  ABSL_HARDENING_ASSERT(index < driver_objects_.size());
  return std::move(driver_objects_[index]);
}

absl::Span<DriverObject> Message::GetDriverObjectArrayView(
    const internal::DriverObjectArrayData& data) {
  return absl::MakeSpan(driver_objects_)
      .subspan(data.first_object_index, data.num_objects);
}

bool Message::CanTransmitOn(const DriverTransport& transport) {
  for (DriverObject& object : driver_objects_) {
    if (!object.CanTransmitOn(transport)) {
      return false;
    }
  }
  return true;
}

bool Message::Serialize(const DriverTransport& transport) {
  ABSL_ASSERT(CanTransmitOn(transport));
  if (driver_objects_.empty()) {
    return true;
  }

  const uint32_t array_offset =
      AllocateArray<internal::DriverObjectData>(driver_objects_.size());
  header().driver_object_data_array = array_offset;

  // NOTE: In Chromium, a vast majority of IPC messages have 0, 1, or 2 OS
  // handles attached. Since these objects are small, we inline some storage on
  // the stack to avoid some heap allocation in the most common cases.
  absl::InlinedVector<IpczDriverHandle, 2> transmissible_handles;
  bool ok = true;
  for (size_t i = 0; i < driver_objects().size(); ++i) {
    internal::DriverObjectData data = {};
    ok &= SerializeDriverObject(std::move(driver_objects()[i]), transport,
                                *this, data, transmissible_handles);
    GetArrayView<internal::DriverObjectData>(array_offset)[i] = data;
  }

  if (ok) {
    transmissible_driver_handles_ = std::move(transmissible_handles);
    return true;
  }
  return false;
}

bool Message::DeserializeUnknownType(const DriverTransport::RawMessage& message,
                                     const DriverTransport& transport) {
  if (!CopyDataAndValidateHeader(message.data)) {
    return false;
  }

  // Validate and deserialize the DriverObject array.
  const uint32_t driver_object_array_offset = header().driver_object_data_array;
  bool all_driver_objects_ok = true;
  if (driver_object_array_offset > 0) {
    if (!IsArrayValid(*this, driver_object_array_offset,
                      sizeof(internal::DriverObjectData))) {
      // The header specified an invalid DriverObjectData array offset, or the
      // array itself was invalid or out-of-bounds.
      return false;
    }

    auto driver_object_data =
        GetArrayView<internal::DriverObjectData>(driver_object_array_offset);
    driver_objects_.reserve(driver_object_data.size());
    for (const internal::DriverObjectData& object_data : driver_object_data) {
      DriverObject object = DeserializeDriverObject(*this, object_data,
                                                    message.handles, transport);
      if (object.is_valid()) {
        driver_objects_.push_back(std::move(object));
      } else {
        // We don't fail immediately so we can try to deserialize the remaining
        // objects anyway, since doing so may free additional resources.
        all_driver_objects_ok = false;
      }
    }
  }

  return all_driver_objects_ok;
}

Message::ReceivedDataBuffer Message::TakeReceivedData() && {
  ABSL_ASSERT(received_data_.has_value());
  ReceivedDataBuffer buffer(std::move(*received_data_));
  received_data_.reset();
  data_ = {};
  return buffer;
}

bool Message::CopyDataAndValidateHeader(absl::Span<const uint8_t> data) {
  // Copy the data into a local message object to avoid any TOCTOU issues in
  // case `data` is in unsafe shared memory.
  received_data_.emplace(data.size());
  memcpy(received_data_->data(), data.data(), data.size());
  data_ = received_data_->bytes();

  // The message must at least be large enough to encode a v0 MessageHeader.
  if (data_.size() < sizeof(internal::MessageHeaderV0)) {
    return false;
  }

  // Version 0 header must match MsesageHeaderV0 size exactly. Newer unknown
  // versions must not be smaller than that.
  const auto& header =
      *reinterpret_cast<const internal::MessageHeaderV0*>(data_.data());
  if (header.version == 0) {
    if (header.size != sizeof(internal::MessageHeaderV0)) {
      return false;
    }
  } else {
    if (header.size < sizeof(internal::MessageHeaderV0)) {
      return false;
    }
  }

  // The header's stated size (and thus the start of the parameter payload)
  // must not run over the edge of the message and must be 8-byte-aligned.
  if (header.size > data_.size() || !IsAligned(header.size)) {
    return false;
  }

  return true;
}

bool Message::ValidateParameters(
    size_t params_size,
    absl::Span<const internal::VersionMetadata> versions) {
  // Validate parameter data. There must be at least enough bytes following the
  // header to encode a StructHeader and to account for all parameter data for
  // some known version of the message.
  absl::Span<uint8_t> params_data = params_data_view();
  if (params_data.size() < sizeof(internal::StructHeader)) {
    return false;
  }

  auto& params_header =
      *reinterpret_cast<internal::StructHeader*>(params_data.data());

  // The param struct's header claims to consist of more data than is present in
  // the message. Not good.
  if (params_data.size() < params_header.size) {
    return false;
  }

  // Parameter struct sizes must be 8-byte-aligned.
  if (!IsAligned(params_header.size)) {
    return false;
  }

  // NOTE: In Chromium, a vast majority of IPC messages have 0, 1, or 2 OS
  // handles attached. Since these objects are small, we inline some storage on
  // the stack to avoid some heap allocation in the most common cases.
  absl::InlinedVector<bool, 2> is_object_claimed(driver_objects_.size());

  // Finally, validate each parameter and claim driver objects. We track the
  // index of every object claimed by a parameter to ensure that no object is
  // claimed more than once.
  //
  // It is not an error for some objects to go unclaimed, as they may have been
  // provided for fields from a newer version of the message that isn't known to
  // this receipient.
  //
  // NOTE: All VersionMetadata and ParamMetadata structures are preprocessor-
  // generated constants used to reflect message layouts. They are not received
  // over the wire and do not require validation themselves.
  for (const internal::VersionMetadata& version : versions) {
    if (version.offset >= params_header.size ||
        version.offset + version.size > params_header.size) {
      // It's not an error to fall short of any version above 0. Higher-
      // versioned fields are inaccessible to message consumers in this case.
      return &version != &versions[0];
    }

    for (const internal::ParamMetadata& param : version.params) {
      const size_t offset = version.offset + param.offset;
      if (param.array_element_size > 0) {
        const uint32_t array_offset =
            *reinterpret_cast<uint32_t*>(&params_data[offset]);
        if (!IsArrayValid(*this, array_offset, param.array_element_size)) {
          return false;
        }
      }

      switch (param.type) {
        case internal::ParamType::kDriverObject: {
          const uint32_t index = GetParamValueAt<uint32_t>(offset);
          if (index != internal::kInvalidDriverObjectIndex) {
            if (is_object_claimed[index]) {
              return false;
            }
            is_object_claimed[index] = true;
          }
          break;
        }

        case internal::ParamType::kDriverObjectArray: {
          const internal::DriverObjectArrayData array_data =
              GetParamValueAt<internal::DriverObjectArrayData>(offset);
          const size_t begin = array_data.first_object_index;
          for (size_t i = begin; i < begin + array_data.num_objects; ++i) {
            if (is_object_claimed[i]) {
              return false;
            }
            is_object_claimed[i] = true;
          }
          break;
        }

        default:
          break;
      }
    }
  }

  return true;
}

bool Message::DeserializeFromTransport(
    size_t params_size,
    absl::Span<const internal::VersionMetadata> versions,
    const DriverTransport::RawMessage& message,
    const DriverTransport& transport) {
  if (!DeserializeUnknownType(message, transport)) {
    return false;
  }

  return ValidateParameters(params_size, versions);
}

bool Message::DeserializeFromRelay(
    size_t params_size,
    absl::Span<const internal::VersionMetadata> versions,
    absl::Span<const uint8_t> data,
    absl::Span<DriverObject> objects) {
  if (!CopyDataAndValidateHeader(data)) {
    return false;
  }

  driver_objects_.resize(objects.size());
  std::move(objects.begin(), objects.end(), driver_objects_.begin());

  return ValidateParameters(params_size, versions);
}

}  // namespace ipcz
