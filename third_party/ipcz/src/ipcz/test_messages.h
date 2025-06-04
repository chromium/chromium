// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generated from node_messages.h.tmpl and checked-in. Change this
// file by editing the template then running:
//
// node_messages.py --dir={path to *_messages_generator.h}

#ifndef IPCZ_SRC_IPCZ_TEST_MESSAGES_H_
#define IPCZ_SRC_IPCZ_TEST_MESSAGES_H_

#include <cstddef>
#include <cstdint>

#include "ipcz/driver_transport.h"
#include "ipcz/message.h"

// Headers for included types.
#include "ipcz/driver_object.h"
#include "ipcz/message_test_types.h"

// clang-format off

namespace ipcz::test::msg {


#pragma pack(push, 1)

// Defines actual wire structures used to encode a message Foo. The main
// structure is Foo_Params, which consists first of a StructHeader, followed by
// a substructure for each defined version of the message. Version structures
// are concatenated in-order to form the full message at its latest known
// version, so a Foo with versions 0, 1, and 2 will have params laid out as:
//
//     struct Foo_Params {
//       internal::StructHeader header;
//       V0 v0;
//       V1 v1;
//       V2 v2;
//     };
//
// This is to say that V1 does not aggregate V0 and V2 does not aggregate V0 or
// V1. Each version structure contains only the fields added in that version.
//
// Macros in this header specifically emit declarations of each version struct's
// layout, as well as accessors for each version. In practice the whole output
// looks more like this, modulo some hidden utility methods and a sloppier
// arrangement because macros are macros:
//
//     struct Foo_Params {
//        struct V0 {
//          uint32_t some_field;
//          uint64_t some_other_field;
//        };
//
//        struct V1 {
//          uint32_t new_thing;
//        };
//
//        struct V2 {
//          uint64_t one_more_thing;
//          uint32_t and_another;
//        };
//
//        V0* v0() { return LargeEnoughForV0() ? &v0_ : nullptr; }
//        const V0* v0() const { return LargeEnoughForV0() ? &v0_ : nullptr; }
//
//        V1* v1() { return LargeEnoughForV1() ? &v1_ : nullptr; }
//        const V1* v1() const { return LargeEnoughForV1() ? &v1_ : nullptr; }
//
//        V2* v2() { return LargeEnoughForV2() ? &v2_ : nullptr; }
//        const V2* v2() const { return LargeEnoughForV2() ? &v2_ : nullptr; }
//
//        internal::StructHeader header;
//       private:
//        V0 v0_;
//        V1 v1_;
//        V2 v2_;
//      };
//
// Version structures are hidden behind pointer accessors because a validated
// message will always have its parameter payload mapped to this structure, and
// this structure represents the latest known version of the message layout. If
// If the message came from an older version however, memory beyond `v0_` may
// not actually belong to the structure and may not even be safe to address.
//
// Hiding versions >= 1 behind the above accessors ensures that versioned
// structures are used safely by the ipcz implementation.

struct IPCZ_ALIGN(8) BasicTestMessage_Params {
  friend class BasicTestMessage_Base;
  using TheseParams = BasicTestMessage_Params;
  BasicTestMessage_Params();
  ~BasicTestMessage_Params();
  static constexpr uint8_t kId = 0;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t foo;
    uint32_t bar;
  };

 private:
  V0 v0_;
  static constexpr size_t v0_offset() { return offsetof(TheseParams, v0_); }
  static constexpr size_t v0_required_size() {
    return v0_offset() + sizeof(v0_);
  }
  bool LargeEnoughForV0version() const {
    return header.size >= v0_required_size();
  }

 public:
  V0* v0() { return LargeEnoughForV0version() ? &v0_ : nullptr; }
  const V0* v0() const { return LargeEnoughForV0version() ? &v0_ : nullptr; }
};

struct IPCZ_ALIGN(8) MessageWithDataArray_Params {
  friend class MessageWithDataArray_Base;
  using TheseParams = MessageWithDataArray_Params;
  MessageWithDataArray_Params();
  ~MessageWithDataArray_Params();
  static constexpr uint8_t kId = 1;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t values;
  };

 private:
  V0 v0_;
  static constexpr size_t v0_offset() { return offsetof(TheseParams, v0_); }
  static constexpr size_t v0_required_size() {
    return v0_offset() + sizeof(v0_);
  }
  bool LargeEnoughForV0version() const {
    return header.size >= v0_required_size();
  }

 public:
  V0* v0() { return LargeEnoughForV0version() ? &v0_ : nullptr; }
  const V0* v0() const { return LargeEnoughForV0version() ? &v0_ : nullptr; }
};

struct IPCZ_ALIGN(8) MessageWithDriverObject_Params {
  friend class MessageWithDriverObject_Base;
  using TheseParams = MessageWithDriverObject_Params;
  MessageWithDriverObject_Params();
  ~MessageWithDriverObject_Params();
  static constexpr uint8_t kId = 2;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t object;
  };

 private:
  V0 v0_;
  static constexpr size_t v0_offset() { return offsetof(TheseParams, v0_); }
  static constexpr size_t v0_required_size() {
    return v0_offset() + sizeof(v0_);
  }
  bool LargeEnoughForV0version() const {
    return header.size >= v0_required_size();
  }

 public:
  V0* v0() { return LargeEnoughForV0version() ? &v0_ : nullptr; }
  const V0* v0() const { return LargeEnoughForV0version() ? &v0_ : nullptr; }
};

struct IPCZ_ALIGN(8) MessageWithDriverObjectArray_Params {
  friend class MessageWithDriverObjectArray_Base;
  using TheseParams = MessageWithDriverObjectArray_Params;
  MessageWithDriverObjectArray_Params();
  ~MessageWithDriverObjectArray_Params();
  static constexpr uint8_t kId = 3;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    internal::DriverObjectArrayData objects;
  };

 private:
  V0 v0_;
  static constexpr size_t v0_offset() { return offsetof(TheseParams, v0_); }
  static constexpr size_t v0_required_size() {
    return v0_offset() + sizeof(v0_);
  }
  bool LargeEnoughForV0version() const {
    return header.size >= v0_required_size();
  }

 public:
  V0* v0() { return LargeEnoughForV0version() ? &v0_ : nullptr; }
  const V0* v0() const { return LargeEnoughForV0version() ? &v0_ : nullptr; }
};

struct IPCZ_ALIGN(8) MessageWithDriverArrayAndExtraObject_Params {
  friend class MessageWithDriverArrayAndExtraObject_Base;
  using TheseParams = MessageWithDriverArrayAndExtraObject_Params;
  MessageWithDriverArrayAndExtraObject_Params();
  ~MessageWithDriverArrayAndExtraObject_Params();
  static constexpr uint8_t kId = 4;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    internal::DriverObjectArrayData objects;
    uint32_t extra_object;
  };

 private:
  V0 v0_;
  static constexpr size_t v0_offset() { return offsetof(TheseParams, v0_); }
  static constexpr size_t v0_required_size() {
    return v0_offset() + sizeof(v0_);
  }
  bool LargeEnoughForV0version() const {
    return header.size >= v0_required_size();
  }

 public:
  V0* v0() { return LargeEnoughForV0version() ? &v0_ : nullptr; }
  const V0* v0() const { return LargeEnoughForV0version() ? &v0_ : nullptr; }
};

struct IPCZ_ALIGN(8) MessageWithMultipleVersions_Params {
  friend class MessageWithMultipleVersions_Base;
  using TheseParams = MessageWithMultipleVersions_Params;
  MessageWithMultipleVersions_Params();
  ~MessageWithMultipleVersions_Params();
  static constexpr uint8_t kId = 5;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t a;
    uint32_t b;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint64_t c;
    uint64_t d;
  };
  struct IPCZ_ALIGN(8) V2 {
    uint32_t e;
  };

 private:
  V0 v0_;
  static constexpr size_t v0_offset() { return offsetof(TheseParams, v0_); }
  static constexpr size_t v0_required_size() {
    return v0_offset() + sizeof(v0_);
  }
  bool LargeEnoughForV0version() const {
    return header.size >= v0_required_size();
  }

 public:
  V0* v0() { return LargeEnoughForV0version() ? &v0_ : nullptr; }
  const V0* v0() const { return LargeEnoughForV0version() ? &v0_ : nullptr; }

 private:
  V1 v1_;
  static constexpr size_t v1_offset() { return offsetof(TheseParams, v1_); }
  static constexpr size_t v1_required_size() {
    return v1_offset() + sizeof(v1_);
  }
  bool LargeEnoughForV1version() const {
    return header.size >= v1_required_size();
  }

 public:
  V1* v1() { return LargeEnoughForV1version() ? &v1_ : nullptr; }
  const V1* v1() const { return LargeEnoughForV1version() ? &v1_ : nullptr; }

 private:
  V2 v2_;
  static constexpr size_t v2_offset() { return offsetof(TheseParams, v2_); }
  static constexpr size_t v2_required_size() {
    return v2_offset() + sizeof(v2_);
  }
  bool LargeEnoughForV2version() const {
    return header.size >= v2_required_size();
  }

 public:
  V2* v2() { return LargeEnoughForV2version() ? &v2_ : nullptr; }
  const V2* v2() const { return LargeEnoughForV2version() ? &v2_ : nullptr; }
};

struct IPCZ_ALIGN(8) MessageWithEnums_Params {
  friend class MessageWithEnums_Base;
  using TheseParams = MessageWithEnums_Params;
  MessageWithEnums_Params();
  ~MessageWithEnums_Params();
  static constexpr uint8_t kId = 6;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint16_t padding1;
    TestEnum8 foo;
    uint8_t padding2;
    TestEnum32 bar;
  };

 private:
  V0 v0_;
  static constexpr size_t v0_offset() { return offsetof(TheseParams, v0_); }
  static constexpr size_t v0_required_size() {
    return v0_offset() + sizeof(v0_);
  }
  bool LargeEnoughForV0version() const {
    return header.size >= v0_required_size();
  }

 public:
  V0* v0() { return LargeEnoughForV0version() ? &v0_ : nullptr; }
  const V0* v0() const { return LargeEnoughForV0version() ? &v0_ : nullptr; }
};


// This header is used to emit a Foo_Versions struct for each message Foo. The
// Foo_Versions struct contains parameter metadata for each defined version of a
// message. The structure looks something like this:
//
//     struct Foo_Versions {
//       using ParamsType = Foo_Params;
//
//       struct V0 {
//         using VersionParams = ParamsType::V0;
//         static constexpr internal::ParamMetadata kParams[] = {
//           {offsetof(VersionParams, field1), sizeof(VersionParams::field1),
//            ...},
//           {offsetof(VersionParams, field2), sizeof(VersionParams::field2),
//            ...},
//            ...etc.
//         };
//       };
//       struct V1 {
//         ...
//       };
//     };
//
// This structure is in turn used by message_base_declaration_macros.h to
// generated an aggregated array of version metadata that can be used at runtime
// for message validation.

// Validate enums start at 0 and finish at kMaxValue, and are size 1 or 4.
static_assert(static_cast<uint32_t>(TestEnum8::kMinValue) == 0);
static_assert(sizeof(TestEnum8) == 1 || sizeof(TestEnum8) == 4);
static_assert(static_cast<uint32_t>(TestEnum32::kMinValue) == 0);
static_assert(sizeof(TestEnum32) == 1 || sizeof(TestEnum32) == 4);

struct BasicTestMessage_Versions {
  using ParamsType = BasicTestMessage_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, foo), sizeof(VersionParams::foo), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, bar), sizeof(VersionParams::bar), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct MessageWithDataArray_Versions {
  using ParamsType = MessageWithDataArray_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, values), sizeof(VersionParams::values),
         sizeof(uint64_t), 0, internal::ParamType::kDataArray},
    };
  };
};
struct MessageWithDriverObject_Versions {
  using ParamsType = MessageWithDriverObject_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, object), sizeof(VersionParams::object), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
};
struct MessageWithDriverObjectArray_Versions {
  using ParamsType = MessageWithDriverObjectArray_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, objects), sizeof(VersionParams::objects), 0,
         0, internal::ParamType::kDriverObjectArray},
    };
  };
};
struct MessageWithDriverArrayAndExtraObject_Versions {
  using ParamsType = MessageWithDriverArrayAndExtraObject_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, objects), sizeof(VersionParams::objects), 0,
         0, internal::ParamType::kDriverObjectArray},
        {offsetof(VersionParams, extra_object), sizeof(VersionParams::extra_object), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
};
struct MessageWithMultipleVersions_Versions {
  using ParamsType = MessageWithMultipleVersions_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, a), sizeof(VersionParams::a), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, b), sizeof(VersionParams::b), 0,
         0, internal::ParamType::kData},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, c), sizeof(VersionParams::c), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, d), sizeof(VersionParams::d), 0,
         0, internal::ParamType::kData},
    };
  };
  struct V2 {
    static constexpr int kVersion = 2;
    using VersionParams = ParamsType::V2;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, e), sizeof(VersionParams::e),
         sizeof(uint32_t), 0, internal::ParamType::kDataArray},
    };
  };
};
struct MessageWithEnums_Versions {
  using ParamsType = MessageWithEnums_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, padding1), sizeof(VersionParams::padding1), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, foo), sizeof(VersionParams::foo), 0,
         static_cast<uint32_t>(TestEnum8::kMaxValue), internal::ParamType::kEnum},
        {offsetof(VersionParams, padding2), sizeof(VersionParams::padding2), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, bar), sizeof(VersionParams::bar), 0,
         static_cast<uint32_t>(TestEnum32::kMaxValue), internal::ParamType::kEnum},
    };
  };
};

// This header is used to emit a Foo_Base class declaration for each message
// Foo. The main purpose of Foo_Base is to define the list of version metadata
// for the Foo message, and to act as a base class for the generated Foo class
// (see message_declaration_macros.h) so that class can introspect its own
// version metadata. The version metadata cannot be defined by macros in that
// header, because that header already needs to emit accessor methods for each
// version.
class BasicTestMessage_Base
    : public MessageWithParams<BasicTestMessage_Params> {
 public:
  using ParamsType = BasicTestMessage_Params;
  using VersionsType = BasicTestMessage_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  BasicTestMessage_Base() = default;
  explicit BasicTestMessage_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~BasicTestMessage_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class MessageWithDataArray_Base
    : public MessageWithParams<MessageWithDataArray_Params> {
 public:
  using ParamsType = MessageWithDataArray_Params;
  using VersionsType = MessageWithDataArray_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  MessageWithDataArray_Base() = default;
  explicit MessageWithDataArray_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~MessageWithDataArray_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class MessageWithDriverObject_Base
    : public MessageWithParams<MessageWithDriverObject_Params> {
 public:
  using ParamsType = MessageWithDriverObject_Params;
  using VersionsType = MessageWithDriverObject_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  MessageWithDriverObject_Base() = default;
  explicit MessageWithDriverObject_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~MessageWithDriverObject_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class MessageWithDriverObjectArray_Base
    : public MessageWithParams<MessageWithDriverObjectArray_Params> {
 public:
  using ParamsType = MessageWithDriverObjectArray_Params;
  using VersionsType = MessageWithDriverObjectArray_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  MessageWithDriverObjectArray_Base() = default;
  explicit MessageWithDriverObjectArray_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~MessageWithDriverObjectArray_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class MessageWithDriverArrayAndExtraObject_Base
    : public MessageWithParams<MessageWithDriverArrayAndExtraObject_Params> {
 public:
  using ParamsType = MessageWithDriverArrayAndExtraObject_Params;
  using VersionsType = MessageWithDriverArrayAndExtraObject_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  MessageWithDriverArrayAndExtraObject_Base() = default;
  explicit MessageWithDriverArrayAndExtraObject_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~MessageWithDriverArrayAndExtraObject_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class MessageWithMultipleVersions_Base
    : public MessageWithParams<MessageWithMultipleVersions_Params> {
 public:
  using ParamsType = MessageWithMultipleVersions_Params;
  using VersionsType = MessageWithMultipleVersions_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  MessageWithMultipleVersions_Base() = default;
  explicit MessageWithMultipleVersions_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~MessageWithMultipleVersions_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
      {VersionsType::V2::kVersion, ParamsType::v2_offset(),
       sizeof(ParamsType::V2), absl::MakeSpan(VersionsType::V2::kParams)},
  };
};
class MessageWithEnums_Base
    : public MessageWithParams<MessageWithEnums_Params> {
 public:
  using ParamsType = MessageWithEnums_Params;
  using VersionsType = MessageWithEnums_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  MessageWithEnums_Base() = default;
  explicit MessageWithEnums_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~MessageWithEnums_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};

// This header emits the usable Foo class for each message Foo, which may be
// used directly by ipcz implementation. In particular this header is used to
// generate version struct accessors (v0, v1, etc.) which expose all available
// message parameters. See message_params_declaration_macros.h for their
// definitions.
class BasicTestMessage : public BasicTestMessage_Base {
 public:
  static constexpr uint8_t kId = 0;
  BasicTestMessage();
  explicit BasicTestMessage(decltype(kIncoming));
  ~BasicTestMessage();
  bool Deserialize(const DriverTransport::RawMessage& message,
                   const DriverTransport& transport);
  bool DeserializeRelayed(absl::Span<const uint8_t> data,
                          absl::Span<DriverObject> objects);
  static_assert(0 < std::size(kVersions) && kVersions[0].version_number == 0,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V0* v0() { return params().v0(); }
  const ParamsType::V0* v0() const { return params().v0(); }
};

class MessageWithDataArray : public MessageWithDataArray_Base {
 public:
  static constexpr uint8_t kId = 1;
  MessageWithDataArray();
  explicit MessageWithDataArray(decltype(kIncoming));
  ~MessageWithDataArray();
  bool Deserialize(const DriverTransport::RawMessage& message,
                   const DriverTransport& transport);
  bool DeserializeRelayed(absl::Span<const uint8_t> data,
                          absl::Span<DriverObject> objects);
  static_assert(0 < std::size(kVersions) && kVersions[0].version_number == 0,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V0* v0() { return params().v0(); }
  const ParamsType::V0* v0() const { return params().v0(); }
};

class MessageWithDriverObject : public MessageWithDriverObject_Base {
 public:
  static constexpr uint8_t kId = 2;
  MessageWithDriverObject();
  explicit MessageWithDriverObject(decltype(kIncoming));
  ~MessageWithDriverObject();
  bool Deserialize(const DriverTransport::RawMessage& message,
                   const DriverTransport& transport);
  bool DeserializeRelayed(absl::Span<const uint8_t> data,
                          absl::Span<DriverObject> objects);
  static_assert(0 < std::size(kVersions) && kVersions[0].version_number == 0,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V0* v0() { return params().v0(); }
  const ParamsType::V0* v0() const { return params().v0(); }
};

class MessageWithDriverObjectArray : public MessageWithDriverObjectArray_Base {
 public:
  static constexpr uint8_t kId = 3;
  MessageWithDriverObjectArray();
  explicit MessageWithDriverObjectArray(decltype(kIncoming));
  ~MessageWithDriverObjectArray();
  bool Deserialize(const DriverTransport::RawMessage& message,
                   const DriverTransport& transport);
  bool DeserializeRelayed(absl::Span<const uint8_t> data,
                          absl::Span<DriverObject> objects);
  static_assert(0 < std::size(kVersions) && kVersions[0].version_number == 0,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V0* v0() { return params().v0(); }
  const ParamsType::V0* v0() const { return params().v0(); }
};

class MessageWithDriverArrayAndExtraObject : public MessageWithDriverArrayAndExtraObject_Base {
 public:
  static constexpr uint8_t kId = 4;
  MessageWithDriverArrayAndExtraObject();
  explicit MessageWithDriverArrayAndExtraObject(decltype(kIncoming));
  ~MessageWithDriverArrayAndExtraObject();
  bool Deserialize(const DriverTransport::RawMessage& message,
                   const DriverTransport& transport);
  bool DeserializeRelayed(absl::Span<const uint8_t> data,
                          absl::Span<DriverObject> objects);
  static_assert(0 < std::size(kVersions) && kVersions[0].version_number == 0,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V0* v0() { return params().v0(); }
  const ParamsType::V0* v0() const { return params().v0(); }
};

class MessageWithMultipleVersions : public MessageWithMultipleVersions_Base {
 public:
  static constexpr uint8_t kId = 5;
  MessageWithMultipleVersions();
  explicit MessageWithMultipleVersions(decltype(kIncoming));
  ~MessageWithMultipleVersions();
  bool Deserialize(const DriverTransport::RawMessage& message,
                   const DriverTransport& transport);
  bool DeserializeRelayed(absl::Span<const uint8_t> data,
                          absl::Span<DriverObject> objects);
  static_assert(0 < std::size(kVersions) && kVersions[0].version_number == 0,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V0* v0() { return params().v0(); }
  const ParamsType::V0* v0() const { return params().v0(); }
  static_assert(1 < std::size(kVersions) && kVersions[1].version_number == 1,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V1* v1() { return params().v1(); }
  const ParamsType::V1* v1() const { return params().v1(); }
  static_assert(2 < std::size(kVersions) && kVersions[2].version_number == 2,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V2* v2() { return params().v2(); }
  const ParamsType::V2* v2() const { return params().v2(); }
};

class MessageWithEnums : public MessageWithEnums_Base {
 public:
  static constexpr uint8_t kId = 6;
  MessageWithEnums();
  explicit MessageWithEnums(decltype(kIncoming));
  ~MessageWithEnums();
  bool Deserialize(const DriverTransport::RawMessage& message,
                   const DriverTransport& transport);
  bool DeserializeRelayed(absl::Span<const uint8_t> data,
                          absl::Span<DriverObject> objects);
  static_assert(0 < std::size(kVersions) && kVersions[0].version_number == 0,
                "Invalid version declaration(s). Message versions must be "
                "declared sequentially starting from 0.");

  ParamsType::V0* v0() { return params().v0(); }
  const ParamsType::V0* v0() const { return params().v0(); }
};


// Declares the BarMessageListener class for a given interface Bar. In ipcz
// message parlance an interface is a collection of related messages. This class
// routes a generic message dispatches to generated virtual methods named for
// the messages they receive. e.g. a DoStuff message is routed (based on message
// ID) to OnDoStuff().
//
// ipcz objects may override this interface to receive messages from some
// transport endpoint which they control. For example a NodeLink implements
// the Node interface (see node_messages.h and node_message_generator.h) by
// subclassing generated NodeMessageListener class and implementing all its
// methods.
//
// Note that listeners separate message receipt (OnMessage) from message
// dispatch (DispatchMessage). By default, OnMessage() simply forwards to
// DispatchMessage(), but the split allows subclasses to override this behavior,
// for example to defer dispatch in some cases.
//
// All raw transport messages are fully validated and deserialized before
// hitting OnMessage(), so implementations do not need to do any protocol-level
// validation of their own.

class TestMessageListener : public DriverTransport::Listener {
 public:
  virtual ~TestMessageListener() = default;
  virtual bool OnMessage(Message& message);

 protected:
  virtual bool DispatchMessage(Message& message);
  virtual bool OnBasicTestMessage(BasicTestMessage&) { return false; }
  virtual bool OnMessageWithDataArray(MessageWithDataArray&) { return false; }
  virtual bool OnMessageWithDriverObject(MessageWithDriverObject&) { return false; }
  virtual bool OnMessageWithDriverObjectArray(MessageWithDriverObjectArray&) { return false; }
  virtual bool OnMessageWithDriverArrayAndExtraObject(MessageWithDriverArrayAndExtraObject&) { return false; }
  virtual bool OnMessageWithMultipleVersions(MessageWithMultipleVersions&) { return false; }
  virtual bool OnMessageWithEnums(MessageWithEnums&) { return false; }
 private:
  bool OnTransportMessage(const DriverTransport::RawMessage& message,
                          const DriverTransport& transport,
                          IpczDriverHandle envelope) final;
  void OnTransportError() override {}
};

#pragma pack(pop)

}  // namespace ipcz::test::msg

// clang-format on

#endif  // IPCZ_SRC_IPCZ_TEST_MESSAGES_H_