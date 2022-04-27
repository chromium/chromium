// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_MESSAGE_INTERNAL_H_
#define IPCZ_SRC_IPCZ_MESSAGE_INTERNAL_H_

#include <cstdint>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_object.h"
#include "ipcz/ipcz.h"
#include "ipcz/sequence_number.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"
#include "util/safe_math.h"

namespace ipcz {

class DriverTransport;
class Node;

namespace internal {

// All wire structures defined in this header should be declared between this
// line and the corresponding #pragma pack(pop). Fields in these structures
// should manually aligned with explicit padding fields as needed.
#pragma pack(push, 1)

// Header which begins all messages. The header layout is versioned for
// extensibility and long-term support.
struct IPCZ_ALIGN(8) MessageHeader {
  // The size of the header in bytes.
  uint8_t size;

  // The header version in use by this message.
  uint8_t version;

  // Message ID assigned as part of a message's type definition via
  // IPCZ_MSG_BEGIN().
  uint8_t message_id;

  // Reserved for future use. Must be zero.
  uint8_t reserved[5];

  // Used for sequencing messages along a NodeLink to preserve end-to-end
  // ordering, as NodeLink messages may be transmitted either across a driver
  // transport or queues in shared memory.
  SequenceNumber sequence_number;
};
static_assert(sizeof(MessageHeader) == 16, "Unexpected size");

using MessageHeaderV0 = MessageHeader;
using LatestMessageHeaderVersion = MessageHeaderV0;

// Header encoding metadata about a structure within a message.
struct IPCZ_ALIGN(8) StructHeader {
  // The size of the structure in bytes.
  uint32_t size;

  // The version number of the structure, which may be used to differentiate
  // between different versions of the same encoded size.
  uint32_t version;
};
static_assert(sizeof(StructHeader) == 8, "Unexpected size");

// Header encoding metadata about any array within a message.
struct IPCZ_ALIGN(8) ArrayHeader {
  // The total number of bytes occupied by the array, including this header and
  // any padding for 8-byte alignment.
  uint32_t num_bytes;

  // The number of elements encoded for the array. Elements are packed into the
  // message immediately following this header.
  uint32_t num_elements;
};

// Each serialized driver object is represented as an array of bytes and an
// array of driver handles. This structure describes both arrays for a single
// driver object. For every object attached to a message, there one of these
// structs.
struct IPCZ_ALIGN(8) DriverObjectData {
  // Array index of the byte array which contains serialized data for this
  // driver object. This is specifically the byte index into the enclosing
  // message where the array's ArrayHeader can be found.
  uint32_t driver_data_array;

  // Every message carries a single unified array of attached driver handles.
  // This is index of the first driver handle relevant to a specific driver
  // object attachment.
  uint16_t first_driver_handle;

  // The number of driver handles belonging to this driver object, starting at
  // the index of `first_driver_handle` within the message's driver handle
  // array.
  uint16_t num_driver_handles;
};

// End of wire structure definitions. Anything below this line is not meant to
// be encoded into messages.
#pragma pack(pop)

// Message macros emit metadata structures which are used at runtime to help
// validate an encoded message during deserialization. This conveys the kind of
// each field within the message's parameter structure.
enum class ParamType {
  // A parameter encoded inline within the message's primary parameter struct.
  kData,

  // A parameter encoded as a 32-bit index elsewhere in the message. This index
  // points to encoded array contents, beginning with an ArrayHeader.
  kDataArray,

  // A parameter encoded as a single DriverObjectData structure, referring to
  // a single driver object attached to the message.
  kDriverObject,

  // A parameter encoded as a 32-bit index to an array elsewhere in the message.
  // The array contains zero or more DriverObjectData structures, and the
  // message parameter corresponds to a collection of driver objects attached to
  // the message.
  kDriverObjectArray,
};

// Metadata about a single parameter declared within a message via one of the
// IPCZ_MSG_PARAM* macros.
struct ParamMetadata {
  // The offset of this parameter from the start of the macro-generated
  // parameters structure, including the StructHeader itself.
  size_t offset;

  // The size of the data expected at `offset` in order for the field to be
  // deserializable.
  size_t size;

  // If this is an array-typed field, this is the encoded size of each array
  // element expected.
  size_t array_element_size;

  // The generic type of this parameter. See ParamType above.
  ParamType type;
};

// Base class for all ipcz-internal wire messages to be transmitted across a
// NodeLink. This provides helpers for appending and extracting dynamic message
// contents in addition to the base parameter structure for a given message.
// This should not be used directly, but should instead be used via a specific
// instance of the derived Message<T> helper below.
class IPCZ_ALIGN(8) MessageBase {
 public:
  MessageBase(uint8_t message_id, size_t params_size);
  ~MessageBase();

  MessageHeader& header() {
    return *reinterpret_cast<MessageHeader*>(data_.data());
  }

  const MessageHeader& header() const {
    return *reinterpret_cast<const MessageHeader*>(data_.data());
  }

  absl::Span<uint8_t> data_view() { return absl::MakeSpan(data_); }
  absl::Span<uint8_t> params_data_view() {
    return absl::MakeSpan(&data_[header().size], data_.size() - header().size);
  }
  absl::Span<DriverObject> driver_objects() {
    return absl::MakeSpan(driver_objects_);
  }
  absl::Span<IpczDriverHandle> transmissible_driver_handles() {
    return absl::MakeSpan(transmissible_driver_handles_);
  }

  // Allocates additional storage in this message to hold an array of
  // `num_elements`, each with a size of `element_size` bytes. The allocated
  // storage includes space for an ArrayHeader and padding to an 8-byte
  // boundary. Returns the offset into the message payload where the ArrayHeader
  // begins. Storage for each element follows contiguously from there.
  uint32_t AllocateGenericArray(size_t element_size, size_t num_elements);

  // Simple template helper for AllocateGenericArray, using the size of the
  // type argument as the element size.
  template <typename ElementType>
  uint32_t AllocateArray(size_t num_elements) {
    return AllocateGenericArray(sizeof(ElementType), num_elements);
  }

  // Allocates additional storage in this message for an array of driver
  // objects, with each consisting of some number of bytes and driver handles.
  // Each driver object is described in the message by a DriverObjectData
  // structure, and this allocates an array of those structures. Similar to
  // AllocateGenericArray, this returns the index of that array's header within
  // the message.
  //
  // The objects in `objects` are stashed in this message and will not be fully
  // encoded until Serialize() is called.
  uint32_t AppendDriverObjects(absl::Span<DriverObject> objects);

  // Appends storage for a single driver object and stores it within this
  // message. `data` is updated to track the index of the attached object within
  // `driver_objects_`. This does not serialize `object` yet.
  //
  // When Serialize() is called on the message, any attached objects will be
  // serialized at that time, and any encoded DriverObjectData structures will
  // be updated to reflect details of the serialized object encoding.
  void AppendDriverObject(DriverObject object, DriverObjectData& data);

  // Takes ownership of a DriverObject that was attached to this message, given
  // an encoded DriverObjectData struct. This is only to be used on deserialized
  // messages.
  DriverObject TakeDriverObject(const DriverObjectData& data);

  // Returns the address of the first element of an array whose header begins
  // at `offset` bytes from the beginning of this message.
  void* GetArrayData(size_t offset) {
    // NOTE: Any offset plugged into this method must be validated ahead of
    // time.
    ABSL_ASSERT(CheckAdd(offset, sizeof(ArrayHeader)) <= data_.size());
    ArrayHeader& header = *reinterpret_cast<ArrayHeader*>(&data_[offset]);
    return &header + 1;
  }

  // Template helper which returns a view into a serialized array's contents,
  // given an array whose header begins at `offset` bytes from the beginning of
  // this message. If `offset` is zero, this returns an empty span.
  template <typename ElementType>
  absl::Span<ElementType> GetArrayView(size_t offset) {
    if (!offset) {
      return {};
    }

    // NOTE: Any offset plugged into this method must be validated ahead of
    // time.
    ABSL_ASSERT(CheckAdd(offset, sizeof(ArrayHeader)) <= data_.size());
    ArrayHeader& header = *reinterpret_cast<ArrayHeader*>(&data_[offset]);

    // The ArrayHeader itself must also have been validated already to ensure
    // that the span of array contents will not exceed the bounds of `data_`.
    ABSL_ASSERT(CheckAdd(CheckMul(sizeof(ElementType),
                                  static_cast<size_t>(header.num_elements)),
                         sizeof(ArrayHeader)) <= data_.size());
    return absl::MakeSpan(reinterpret_cast<ElementType*>(&header + 1),
                          header.num_elements);
  }

  // Helper to retrieve a typed value from the message given an absolute byte
  // offset from the start of the message.
  template <typename T>
  T& GetValueAt(size_t data_offset) {
    // NOTE: Any offset plugged into this method must be validated ahead of
    // time.
    ABSL_ASSERT(CheckAdd(data_offset, sizeof(T)) <= data_.size());
    return *reinterpret_cast<T*>(&data_[data_offset]);
  }

  // Helper to retrieve a typed value from the message given a byte offset from
  // the start of the message's parameter data. Note the distinction between
  // this and GetValueAt(), as this offset is taken from the end of the message
  // header, while GetValueAt() (and most other offset-diven methods here)
  // interprets the offset as relative to the beginning of the message itself.
  //
  // This method is used in conjunection with parameter metadata generated by
  // macros at compile-time.
  template <typename T>
  T& GetParamValueAt(size_t param_offset) {
    // NOTE: Any offset plugged into this method must be validated ahead of
    // time.
    ABSL_ASSERT(CheckAdd(param_offset, sizeof(T)) <= params_data_view().size());
    return GetValueAt<T>(GetDataOffset(&params_data_view()[param_offset]));
  }

  // Checks and indicates whether this message can be transmitted over
  // `transport`, which depends on whether the driver is able to transmit all of
  // the attached driver objects over that transport.
  bool CanTransmitOn(const DriverTransport& transport);

  // Attempts to finalize a message for transit over `transport`, potentially
  // mutating the message data in-place. Returns true iff sucessful.
  //
  // NOTE: It is invalid to call this on a message for which
  // `CanTransmitOn(transport)` does not return true and doing so results in
  // unspecified behavior.
  void Serialize(absl::Span<const ParamMetadata> params,
                 const DriverTransport& transport);

 protected:
  // Returns `x` aligned above to the nearest 8-byte boundary.
  constexpr size_t Align(size_t x) { return (x + 7) & ~7; }

  // Returns the relative offset of an address which falls within the message's
  // data payload.
  uint32_t GetDataOffset(const void* data) {
    return static_cast<uint32_t>(static_cast<const uint8_t*>(data) -
                                 data_.data());
  }

  // Attempts to deserialize a message from raw `data` and `handles` into `this`
  // message object, given the `params_size`, `params_current_version` and
  // `params_metadata`, which are all generated from message macros at build
  // time to describe a specific ipcz-internal message.
  //
  // `transport` is the transport from which the incoming data and handles were
  // received.
  bool DeserializeFromTransport(size_t params_size,
                                uint32_t params_current_version,
                                absl::Span<const ParamMetadata> params_metadata,
                                absl::Span<const uint8_t> data,
                                absl::Span<const IpczDriverHandle> handles,
                                const DriverTransport& transport);

  // Raw serialized data for this message. This always begins with MessageHeader
  // (or potentially some newer or older version thereof), whose actual size
  // is determined by the header's `size` field. After that many bytes, a
  // parameters structure immediately follows, as generated by an invocation of
  // IPCZ_MSG_BEGIN()/IPCZ_MSG_END(). After fixed parameters, any number of
  // dynamicaly inlined allocations may follow (e.g. for array contents,
  // driver objects, etc.)
  absl::InlinedVector<uint8_t, 128> data_;

  // Collection of DriverObjects attached to this message. These are attached
  // while building a message (e.g. by calling AppendDriverObject), and they are
  // consumed by Serialize() to encode the objects for transmission. Serialized
  // objects are represented by some combination of data within the message,
  // and zero or more transmissible driver handles which accumulate in
  // `transmissible_driver_handles_` during serialization.
  //
  // On deserialization, driver object data and transmissible handles are fed
  // back to the driver and used to reconstruct this list. Deserialized objects
  // may be extracted from the message by calling TakeDriverObject().
  //
  // Since each driver object may serialize to any number of bytes and
  // transmissible handles, there is generally NOT a 1:1 correpsondence between
  // this list and `transmissible_driver_handles_`.
  absl::InlinedVector<DriverObject, 2> driver_objects_;

  // Collection of driver handles which the driver knows how to transmit as-is,
  // in conjunction with (but out-of-band from) the payload in `data_`. This
  // is populated by Serialize() if the driver emits any transmissible handles
  // as outputs when serializing any of the objects in `driver_objects_`.
  //
  // On deserialization this set of handles is consumed; and in combination with
  // encoded object data, is used by the driver to reconstruct
  // `driver_objects_`.
  //
  // Since each driver object may serialize to any number of bytes and
  // transmissible handles, there is generally NOT a 1:1 correpsondence between
  // this list and `driver_objects_`.
  absl::InlinedVector<IpczDriverHandle, 2> transmissible_driver_handles_;

  // Basic constant attributes of this message, as constructed or deserialized.
  const uint8_t message_id_;
  const uint32_t params_size_;
};

// Template helper to wrap the MessageBase type for a specific macro-generated
// parameter structure. This primarily exists for safe, convenient construction
// of message payloads with correct header information and no leaky padding
// bits, as well as for convenient access to parameters within size-validated,
// deserialized messages.
//
// When an IPCZ_MSG_BEGIN() macro is used to declare a new Foo message, it will
// emit both a msg::Foo_Params structure for the fixed wire data of the message
// parameters, as well as a msg::Foo which is an alias for an instance of this
// template, namely Message<msg::Foo_Params>.
template <typename ParamDataType>
class Message : public MessageBase {
 public:
  Message() : MessageBase(ParamDataType::kId, sizeof(ParamDataType)) {
    ParamDataType& p = *(new (&params()) ParamDataType());
    p.header.size = sizeof(p);
    p.header.version = ParamDataType::kVersion;
  }

  ~Message() = default;

  // Convenient accessors for the message's main parameters struct, whose
  // location depends on the size of the header. Note that because this may be
  // used to access parameters within messages using a newer or older header
  // than what's defined above in MessageHeader, we index based on the header's
  // encoded size rather than the compile-time size of MessageHeader.
  //
  // If this Message was deserialized from the wire, it must already have been
  // validated to have an enough space for `header().size` bytes plus the size
  // if ParamDataType.
  ParamDataType& params() {
    return *reinterpret_cast<ParamDataType*>(&data_[header().size]);
  }

  const ParamDataType& params() const {
    return *reinterpret_cast<const ParamDataType*>(&data_[header().size]);
  }
};

}  // namespace internal
}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_MESSAGE_INTERNAL_H_
