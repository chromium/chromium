// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generated from node_messages.h.tmpl and checked-in. Change this
// file by editing the template then running:
//
// node_messages.py --dir={path to *_messages_generator.h}

#ifndef IPCZ_SRC_IPCZ_NODE_MESSAGES_H_
#define IPCZ_SRC_IPCZ_NODE_MESSAGES_H_

#include <cstddef>
#include <cstdint>

#include "ipcz/driver_transport.h"
#include "ipcz/message.h"

// Headers for included types.
#include "ipcz/buffer_id.h"
#include "ipcz/driver_object.h"
#include "ipcz/features.h"
#include "ipcz/fragment_descriptor.h"
#include "ipcz/handle_type.h"
#include "ipcz/link_side.h"
#include "ipcz/node_name.h"
#include "ipcz/node_type.h"
#include "ipcz/router_descriptor.h"
#include "ipcz/sequence_number.h"
#include "ipcz/sublink_id.h"

// clang-format off

namespace ipcz::msg {

// Bump this version number up by 1 when adding new protocol features so that
// they can be detected during NodeLink establishment.
constexpr uint32_t kProtocolVersion = 0;

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

struct IPCZ_ALIGN(8) ConnectFromBrokerToNonBroker_Params {
  friend class ConnectFromBrokerToNonBroker_Base;
  using TheseParams = ConnectFromBrokerToNonBroker_Params;
  ConnectFromBrokerToNonBroker_Params();
  ~ConnectFromBrokerToNonBroker_Params();
  static constexpr uint8_t kId = 0;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName broker_name;
    NodeName receiver_name;
    uint32_t protocol_version;
    uint32_t num_initial_portals;
    uint32_t buffer;
    uint32_t padding;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint32_t features;
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
};

struct IPCZ_ALIGN(8) ConnectFromNonBrokerToBroker_Params {
  friend class ConnectFromNonBrokerToBroker_Base;
  using TheseParams = ConnectFromNonBrokerToBroker_Params;
  ConnectFromNonBrokerToBroker_Params();
  ~ConnectFromNonBrokerToBroker_Params();
  static constexpr uint8_t kId = 1;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t protocol_version;
    uint32_t num_initial_portals;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint32_t features;
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
};

struct IPCZ_ALIGN(8) ReferNonBroker_Params {
  friend class ReferNonBroker_Base;
  using TheseParams = ReferNonBroker_Params;
  ReferNonBroker_Params();
  ~ReferNonBroker_Params();
  static constexpr uint8_t kId = 2;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint64_t referral_id;
    uint32_t num_initial_portals;
    uint32_t transport;
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

struct IPCZ_ALIGN(8) ConnectToReferredBroker_Params {
  friend class ConnectToReferredBroker_Base;
  using TheseParams = ConnectToReferredBroker_Params;
  ConnectToReferredBroker_Params();
  ~ConnectToReferredBroker_Params();
  static constexpr uint8_t kId = 3;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t protocol_version;
    uint32_t num_initial_portals;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint32_t features;
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
};

struct IPCZ_ALIGN(8) ConnectToReferredNonBroker_Params {
  friend class ConnectToReferredNonBroker_Base;
  using TheseParams = ConnectToReferredNonBroker_Params;
  ConnectToReferredNonBroker_Params();
  ~ConnectToReferredNonBroker_Params();
  static constexpr uint8_t kId = 4;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName name;
    NodeName broker_name;
    NodeName referrer_name;
    uint32_t broker_protocol_version;
    uint32_t referrer_protocol_version;
    uint32_t num_initial_portals;
    uint32_t broker_link_buffer;
    uint32_t referrer_link_transport;
    uint32_t referrer_link_buffer;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint32_t broker_features;
    uint32_t referrer_features;
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
};

struct IPCZ_ALIGN(8) NonBrokerReferralAccepted_Params {
  friend class NonBrokerReferralAccepted_Base;
  using TheseParams = NonBrokerReferralAccepted_Params;
  NonBrokerReferralAccepted_Params();
  ~NonBrokerReferralAccepted_Params();
  static constexpr uint8_t kId = 5;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint64_t referral_id;
    uint32_t protocol_version;
    uint32_t num_initial_portals;
    NodeName name;
    uint32_t transport;
    uint32_t buffer;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint32_t features;
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
};

struct IPCZ_ALIGN(8) NonBrokerReferralRejected_Params {
  friend class NonBrokerReferralRejected_Base;
  using TheseParams = NonBrokerReferralRejected_Params;
  NonBrokerReferralRejected_Params();
  ~NonBrokerReferralRejected_Params();
  static constexpr uint8_t kId = 6;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint64_t referral_id;
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

struct IPCZ_ALIGN(8) ConnectFromBrokerToBroker_Params {
  friend class ConnectFromBrokerToBroker_Base;
  using TheseParams = ConnectFromBrokerToBroker_Params;
  ConnectFromBrokerToBroker_Params();
  ~ConnectFromBrokerToBroker_Params();
  static constexpr uint8_t kId = 7;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName name;
    uint32_t protocol_version;
    uint32_t num_initial_portals;
    uint32_t buffer;
    uint32_t padding;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint32_t features;
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
};

struct IPCZ_ALIGN(8) RequestIntroduction_Params {
  friend class RequestIntroduction_Base;
  using TheseParams = RequestIntroduction_Params;
  RequestIntroduction_Params();
  ~RequestIntroduction_Params();
  static constexpr uint8_t kId = 10;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName name;
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

struct IPCZ_ALIGN(8) AcceptIntroduction_Params {
  friend class AcceptIntroduction_Base;
  using TheseParams = AcceptIntroduction_Params;
  AcceptIntroduction_Params();
  ~AcceptIntroduction_Params();
  static constexpr uint8_t kId = 11;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName name;
    LinkSide link_side;
    NodeType remote_node_type;
    uint16_t padding;
    uint32_t remote_protocol_version;
    uint32_t transport;
    uint32_t memory;
  };
  struct IPCZ_ALIGN(8) V1 {
    uint32_t remote_features;
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
};

struct IPCZ_ALIGN(8) RejectIntroduction_Params {
  friend class RejectIntroduction_Base;
  using TheseParams = RejectIntroduction_Params;
  RejectIntroduction_Params();
  ~RejectIntroduction_Params();
  static constexpr uint8_t kId = 12;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName name;
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

struct IPCZ_ALIGN(8) RequestIndirectIntroduction_Params {
  friend class RequestIndirectIntroduction_Base;
  using TheseParams = RequestIndirectIntroduction_Params;
  RequestIndirectIntroduction_Params();
  ~RequestIndirectIntroduction_Params();
  static constexpr uint8_t kId = 13;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName source_node;
    NodeName target_node;
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

struct IPCZ_ALIGN(8) AddBlockBuffer_Params {
  friend class AddBlockBuffer_Base;
  using TheseParams = AddBlockBuffer_Params;
  AddBlockBuffer_Params();
  ~AddBlockBuffer_Params();
  static constexpr uint8_t kId = 14;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    BufferId id;
    uint32_t block_size;
    uint32_t buffer;
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

struct IPCZ_ALIGN(8) AcceptParcel_Params {
  friend class AcceptParcel_Base;
  using TheseParams = AcceptParcel_Params;
  AcceptParcel_Params();
  ~AcceptParcel_Params();
  static constexpr uint8_t kId = 20;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    SequenceNumber sequence_number;
    uint32_t subparcel_index;
    uint32_t num_subparcels;
    FragmentDescriptor parcel_fragment;
    uint32_t parcel_data;
    uint32_t handle_types;
    uint32_t new_routers;
    uint32_t padding;
    internal::DriverObjectArrayData driver_objects;
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

struct IPCZ_ALIGN(8) AcceptParcelDriverObjects_Params {
  friend class AcceptParcelDriverObjects_Base;
  using TheseParams = AcceptParcelDriverObjects_Params;
  AcceptParcelDriverObjects_Params();
  ~AcceptParcelDriverObjects_Params();
  static constexpr uint8_t kId = 21;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    SequenceNumber sequence_number;
    internal::DriverObjectArrayData driver_objects;
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

struct IPCZ_ALIGN(8) RouteClosed_Params {
  friend class RouteClosed_Base;
  using TheseParams = RouteClosed_Params;
  RouteClosed_Params();
  ~RouteClosed_Params();
  static constexpr uint8_t kId = 22;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    SequenceNumber sequence_length;
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

struct IPCZ_ALIGN(8) RouteDisconnected_Params {
  friend class RouteDisconnected_Base;
  using TheseParams = RouteDisconnected_Params;
  RouteDisconnected_Params();
  ~RouteDisconnected_Params();
  static constexpr uint8_t kId = 23;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
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

struct IPCZ_ALIGN(8) BypassPeer_Params {
  friend class BypassPeer_Base;
  using TheseParams = BypassPeer_Params;
  BypassPeer_Params();
  ~BypassPeer_Params();
  static constexpr uint8_t kId = 30;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    uint64_t reserved0;
    NodeName bypass_target_node;
    SublinkId bypass_target_sublink;
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

struct IPCZ_ALIGN(8) AcceptBypassLink_Params {
  friend class AcceptBypassLink_Base;
  using TheseParams = AcceptBypassLink_Params;
  AcceptBypassLink_Params();
  ~AcceptBypassLink_Params();
  static constexpr uint8_t kId = 31;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName current_peer_node;
    SublinkId current_peer_sublink;
    SequenceNumber inbound_sequence_length_from_bypassed_link;
    SublinkId new_sublink;
    FragmentDescriptor new_link_state_fragment;
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

struct IPCZ_ALIGN(8) StopProxying_Params {
  friend class StopProxying_Base;
  using TheseParams = StopProxying_Params;
  StopProxying_Params();
  ~StopProxying_Params();
  static constexpr uint8_t kId = 32;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    SequenceNumber inbound_sequence_length;
    SequenceNumber outbound_sequence_length;
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

struct IPCZ_ALIGN(8) ProxyWillStop_Params {
  friend class ProxyWillStop_Base;
  using TheseParams = ProxyWillStop_Params;
  ProxyWillStop_Params();
  ~ProxyWillStop_Params();
  static constexpr uint8_t kId = 33;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    SequenceNumber inbound_sequence_length;
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

struct IPCZ_ALIGN(8) BypassPeerWithLink_Params {
  friend class BypassPeerWithLink_Base;
  using TheseParams = BypassPeerWithLink_Params;
  BypassPeerWithLink_Params();
  ~BypassPeerWithLink_Params();
  static constexpr uint8_t kId = 34;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    SublinkId new_sublink;
    FragmentDescriptor new_link_state_fragment;
    SequenceNumber inbound_sequence_length;
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

struct IPCZ_ALIGN(8) StopProxyingToLocalPeer_Params {
  friend class StopProxyingToLocalPeer_Base;
  using TheseParams = StopProxyingToLocalPeer_Params;
  StopProxyingToLocalPeer_Params();
  ~StopProxyingToLocalPeer_Params();
  static constexpr uint8_t kId = 35;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
    SequenceNumber outbound_sequence_length;
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

struct IPCZ_ALIGN(8) FlushRouter_Params {
  friend class FlushRouter_Base;
  using TheseParams = FlushRouter_Params;
  FlushRouter_Params();
  ~FlushRouter_Params();
  static constexpr uint8_t kId = 36;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    SublinkId sublink;
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

struct IPCZ_ALIGN(8) RequestMemory_Params {
  friend class RequestMemory_Base;
  using TheseParams = RequestMemory_Params;
  RequestMemory_Params();
  ~RequestMemory_Params();
  static constexpr uint8_t kId = 64;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t size;
    uint32_t padding;
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

struct IPCZ_ALIGN(8) ProvideMemory_Params {
  friend class ProvideMemory_Base;
  using TheseParams = ProvideMemory_Params;
  ProvideMemory_Params();
  ~ProvideMemory_Params();
  static constexpr uint8_t kId = 65;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    uint32_t size;
    uint32_t buffer;
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

struct IPCZ_ALIGN(8) RelayMessage_Params {
  friend class RelayMessage_Base;
  using TheseParams = RelayMessage_Params;
  RelayMessage_Params();
  ~RelayMessage_Params();
  static constexpr uint8_t kId = 66;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName destination;
    uint32_t data;
    uint32_t padding;
    internal::DriverObjectArrayData driver_objects;
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

struct IPCZ_ALIGN(8) AcceptRelayedMessage_Params {
  friend class AcceptRelayedMessage_Base;
  using TheseParams = AcceptRelayedMessage_Params;
  AcceptRelayedMessage_Params();
  ~AcceptRelayedMessage_Params();
  static constexpr uint8_t kId = 67;
  internal::StructHeader header;
  struct IPCZ_ALIGN(8) V0 {
    NodeName source;
    uint32_t data;
    uint32_t padding;
    internal::DriverObjectArrayData driver_objects;
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
static_assert(static_cast<uint32_t>(LinkSide::kMinValue) == 0);
static_assert(sizeof(LinkSide) == 1 || sizeof(LinkSide) == 4);
static_assert(static_cast<uint32_t>(NodeType::kMinValue) == 0);
static_assert(sizeof(NodeType) == 1 || sizeof(NodeType) == 4);

struct ConnectFromBrokerToNonBroker_Versions {
  using ParamsType = ConnectFromBrokerToNonBroker_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, broker_name), sizeof(VersionParams::broker_name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, receiver_name), sizeof(VersionParams::receiver_name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, protocol_version), sizeof(VersionParams::protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_initial_portals), sizeof(VersionParams::num_initial_portals), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, buffer), sizeof(VersionParams::buffer), 0,
         0, internal::ParamType::kDriverObject},
        {offsetof(VersionParams, padding), sizeof(VersionParams::padding), 0,
         0, internal::ParamType::kData},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, features), sizeof(VersionParams::features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
    };
  };
};
struct ConnectFromNonBrokerToBroker_Versions {
  using ParamsType = ConnectFromNonBrokerToBroker_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, protocol_version), sizeof(VersionParams::protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_initial_portals), sizeof(VersionParams::num_initial_portals), 0,
         0, internal::ParamType::kData},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, features), sizeof(VersionParams::features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
    };
  };
};
struct ReferNonBroker_Versions {
  using ParamsType = ReferNonBroker_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, referral_id), sizeof(VersionParams::referral_id), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_initial_portals), sizeof(VersionParams::num_initial_portals), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, transport), sizeof(VersionParams::transport), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
};
struct ConnectToReferredBroker_Versions {
  using ParamsType = ConnectToReferredBroker_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, protocol_version), sizeof(VersionParams::protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_initial_portals), sizeof(VersionParams::num_initial_portals), 0,
         0, internal::ParamType::kData},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, features), sizeof(VersionParams::features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
    };
  };
};
struct ConnectToReferredNonBroker_Versions {
  using ParamsType = ConnectToReferredNonBroker_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, name), sizeof(VersionParams::name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, broker_name), sizeof(VersionParams::broker_name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, referrer_name), sizeof(VersionParams::referrer_name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, broker_protocol_version), sizeof(VersionParams::broker_protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, referrer_protocol_version), sizeof(VersionParams::referrer_protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_initial_portals), sizeof(VersionParams::num_initial_portals), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, broker_link_buffer), sizeof(VersionParams::broker_link_buffer), 0,
         0, internal::ParamType::kDriverObject},
        {offsetof(VersionParams, referrer_link_transport), sizeof(VersionParams::referrer_link_transport), 0,
         0, internal::ParamType::kDriverObject},
        {offsetof(VersionParams, referrer_link_buffer), sizeof(VersionParams::referrer_link_buffer), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, broker_features), sizeof(VersionParams::broker_features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
        {offsetof(VersionParams, referrer_features), sizeof(VersionParams::referrer_features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
    };
  };
};
struct NonBrokerReferralAccepted_Versions {
  using ParamsType = NonBrokerReferralAccepted_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, referral_id), sizeof(VersionParams::referral_id), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, protocol_version), sizeof(VersionParams::protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_initial_portals), sizeof(VersionParams::num_initial_portals), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, name), sizeof(VersionParams::name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, transport), sizeof(VersionParams::transport), 0,
         0, internal::ParamType::kDriverObject},
        {offsetof(VersionParams, buffer), sizeof(VersionParams::buffer), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, features), sizeof(VersionParams::features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
    };
  };
};
struct NonBrokerReferralRejected_Versions {
  using ParamsType = NonBrokerReferralRejected_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, referral_id), sizeof(VersionParams::referral_id), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct ConnectFromBrokerToBroker_Versions {
  using ParamsType = ConnectFromBrokerToBroker_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, name), sizeof(VersionParams::name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, protocol_version), sizeof(VersionParams::protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_initial_portals), sizeof(VersionParams::num_initial_portals), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, buffer), sizeof(VersionParams::buffer), 0,
         0, internal::ParamType::kDriverObject},
        {offsetof(VersionParams, padding), sizeof(VersionParams::padding), 0,
         0, internal::ParamType::kData},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, features), sizeof(VersionParams::features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
    };
  };
};
struct RequestIntroduction_Versions {
  using ParamsType = RequestIntroduction_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, name), sizeof(VersionParams::name), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct AcceptIntroduction_Versions {
  using ParamsType = AcceptIntroduction_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, name), sizeof(VersionParams::name), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, link_side), sizeof(VersionParams::link_side), 0,
         static_cast<uint32_t>(LinkSide::kMaxValue), internal::ParamType::kEnum},
        {offsetof(VersionParams, remote_node_type), sizeof(VersionParams::remote_node_type), 0,
         static_cast<uint32_t>(NodeType::kMaxValue), internal::ParamType::kEnum},
        {offsetof(VersionParams, padding), sizeof(VersionParams::padding), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, remote_protocol_version), sizeof(VersionParams::remote_protocol_version), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, transport), sizeof(VersionParams::transport), 0,
         0, internal::ParamType::kDriverObject},
        {offsetof(VersionParams, memory), sizeof(VersionParams::memory), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
  struct V1 {
    static constexpr int kVersion = 1;
    using VersionParams = ParamsType::V1;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, remote_features), sizeof(VersionParams::remote_features),
         sizeof(Features::Bitfield), 0, internal::ParamType::kDataArray},
    };
  };
};
struct RejectIntroduction_Versions {
  using ParamsType = RejectIntroduction_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, name), sizeof(VersionParams::name), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct RequestIndirectIntroduction_Versions {
  using ParamsType = RequestIndirectIntroduction_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, source_node), sizeof(VersionParams::source_node), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, target_node), sizeof(VersionParams::target_node), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct AddBlockBuffer_Versions {
  using ParamsType = AddBlockBuffer_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, id), sizeof(VersionParams::id), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, block_size), sizeof(VersionParams::block_size), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, buffer), sizeof(VersionParams::buffer), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
};
struct AcceptParcel_Versions {
  using ParamsType = AcceptParcel_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, sequence_number), sizeof(VersionParams::sequence_number), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, subparcel_index), sizeof(VersionParams::subparcel_index), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, num_subparcels), sizeof(VersionParams::num_subparcels), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, parcel_fragment), sizeof(VersionParams::parcel_fragment), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, parcel_data), sizeof(VersionParams::parcel_data),
         sizeof(uint8_t), 0, internal::ParamType::kDataArray},
        {offsetof(VersionParams, handle_types), sizeof(VersionParams::handle_types),
         sizeof(HandleType), 0, internal::ParamType::kDataArray},
        {offsetof(VersionParams, new_routers), sizeof(VersionParams::new_routers),
         sizeof(RouterDescriptor), 0, internal::ParamType::kDataArray},
        {offsetof(VersionParams, padding), sizeof(VersionParams::padding), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, driver_objects), sizeof(VersionParams::driver_objects), 0,
         0, internal::ParamType::kDriverObjectArray},
    };
  };
};
struct AcceptParcelDriverObjects_Versions {
  using ParamsType = AcceptParcelDriverObjects_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, sequence_number), sizeof(VersionParams::sequence_number), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, driver_objects), sizeof(VersionParams::driver_objects), 0,
         0, internal::ParamType::kDriverObjectArray},
    };
  };
};
struct RouteClosed_Versions {
  using ParamsType = RouteClosed_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, sequence_length), sizeof(VersionParams::sequence_length), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct RouteDisconnected_Versions {
  using ParamsType = RouteDisconnected_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct BypassPeer_Versions {
  using ParamsType = BypassPeer_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, reserved0), sizeof(VersionParams::reserved0), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, bypass_target_node), sizeof(VersionParams::bypass_target_node), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, bypass_target_sublink), sizeof(VersionParams::bypass_target_sublink), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct AcceptBypassLink_Versions {
  using ParamsType = AcceptBypassLink_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, current_peer_node), sizeof(VersionParams::current_peer_node), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, current_peer_sublink), sizeof(VersionParams::current_peer_sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, inbound_sequence_length_from_bypassed_link), sizeof(VersionParams::inbound_sequence_length_from_bypassed_link), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, new_sublink), sizeof(VersionParams::new_sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, new_link_state_fragment), sizeof(VersionParams::new_link_state_fragment), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct StopProxying_Versions {
  using ParamsType = StopProxying_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, inbound_sequence_length), sizeof(VersionParams::inbound_sequence_length), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, outbound_sequence_length), sizeof(VersionParams::outbound_sequence_length), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct ProxyWillStop_Versions {
  using ParamsType = ProxyWillStop_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, inbound_sequence_length), sizeof(VersionParams::inbound_sequence_length), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct BypassPeerWithLink_Versions {
  using ParamsType = BypassPeerWithLink_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, new_sublink), sizeof(VersionParams::new_sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, new_link_state_fragment), sizeof(VersionParams::new_link_state_fragment), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, inbound_sequence_length), sizeof(VersionParams::inbound_sequence_length), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct StopProxyingToLocalPeer_Versions {
  using ParamsType = StopProxyingToLocalPeer_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, outbound_sequence_length), sizeof(VersionParams::outbound_sequence_length), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct FlushRouter_Versions {
  using ParamsType = FlushRouter_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, sublink), sizeof(VersionParams::sublink), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct RequestMemory_Versions {
  using ParamsType = RequestMemory_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, size), sizeof(VersionParams::size), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, padding), sizeof(VersionParams::padding), 0,
         0, internal::ParamType::kData},
    };
  };
};
struct ProvideMemory_Versions {
  using ParamsType = ProvideMemory_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, size), sizeof(VersionParams::size), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, buffer), sizeof(VersionParams::buffer), 0,
         0, internal::ParamType::kDriverObject},
    };
  };
};
struct RelayMessage_Versions {
  using ParamsType = RelayMessage_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, destination), sizeof(VersionParams::destination), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, data), sizeof(VersionParams::data),
         sizeof(uint8_t), 0, internal::ParamType::kDataArray},
        {offsetof(VersionParams, padding), sizeof(VersionParams::padding), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, driver_objects), sizeof(VersionParams::driver_objects), 0,
         0, internal::ParamType::kDriverObjectArray},
    };
  };
};
struct AcceptRelayedMessage_Versions {
  using ParamsType = AcceptRelayedMessage_Params;
  struct V0 {
    static constexpr int kVersion = 0;
    using VersionParams = ParamsType::V0;
    static constexpr internal::ParamMetadata kParams[] = {
        {offsetof(VersionParams, source), sizeof(VersionParams::source), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, data), sizeof(VersionParams::data),
         sizeof(uint8_t), 0, internal::ParamType::kDataArray},
        {offsetof(VersionParams, padding), sizeof(VersionParams::padding), 0,
         0, internal::ParamType::kData},
        {offsetof(VersionParams, driver_objects), sizeof(VersionParams::driver_objects), 0,
         0, internal::ParamType::kDriverObjectArray},
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
class ConnectFromBrokerToNonBroker_Base
    : public MessageWithParams<ConnectFromBrokerToNonBroker_Params> {
 public:
  using ParamsType = ConnectFromBrokerToNonBroker_Params;
  using VersionsType = ConnectFromBrokerToNonBroker_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ConnectFromBrokerToNonBroker_Base() = default;
  explicit ConnectFromBrokerToNonBroker_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ConnectFromBrokerToNonBroker_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
  };
};
class ConnectFromNonBrokerToBroker_Base
    : public MessageWithParams<ConnectFromNonBrokerToBroker_Params> {
 public:
  using ParamsType = ConnectFromNonBrokerToBroker_Params;
  using VersionsType = ConnectFromNonBrokerToBroker_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ConnectFromNonBrokerToBroker_Base() = default;
  explicit ConnectFromNonBrokerToBroker_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ConnectFromNonBrokerToBroker_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
  };
};
class ReferNonBroker_Base
    : public MessageWithParams<ReferNonBroker_Params> {
 public:
  using ParamsType = ReferNonBroker_Params;
  using VersionsType = ReferNonBroker_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ReferNonBroker_Base() = default;
  explicit ReferNonBroker_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ReferNonBroker_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class ConnectToReferredBroker_Base
    : public MessageWithParams<ConnectToReferredBroker_Params> {
 public:
  using ParamsType = ConnectToReferredBroker_Params;
  using VersionsType = ConnectToReferredBroker_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ConnectToReferredBroker_Base() = default;
  explicit ConnectToReferredBroker_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ConnectToReferredBroker_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
  };
};
class ConnectToReferredNonBroker_Base
    : public MessageWithParams<ConnectToReferredNonBroker_Params> {
 public:
  using ParamsType = ConnectToReferredNonBroker_Params;
  using VersionsType = ConnectToReferredNonBroker_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ConnectToReferredNonBroker_Base() = default;
  explicit ConnectToReferredNonBroker_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ConnectToReferredNonBroker_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
  };
};
class NonBrokerReferralAccepted_Base
    : public MessageWithParams<NonBrokerReferralAccepted_Params> {
 public:
  using ParamsType = NonBrokerReferralAccepted_Params;
  using VersionsType = NonBrokerReferralAccepted_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  NonBrokerReferralAccepted_Base() = default;
  explicit NonBrokerReferralAccepted_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~NonBrokerReferralAccepted_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
  };
};
class NonBrokerReferralRejected_Base
    : public MessageWithParams<NonBrokerReferralRejected_Params> {
 public:
  using ParamsType = NonBrokerReferralRejected_Params;
  using VersionsType = NonBrokerReferralRejected_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  NonBrokerReferralRejected_Base() = default;
  explicit NonBrokerReferralRejected_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~NonBrokerReferralRejected_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class ConnectFromBrokerToBroker_Base
    : public MessageWithParams<ConnectFromBrokerToBroker_Params> {
 public:
  using ParamsType = ConnectFromBrokerToBroker_Params;
  using VersionsType = ConnectFromBrokerToBroker_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ConnectFromBrokerToBroker_Base() = default;
  explicit ConnectFromBrokerToBroker_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ConnectFromBrokerToBroker_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
  };
};
class RequestIntroduction_Base
    : public MessageWithParams<RequestIntroduction_Params> {
 public:
  using ParamsType = RequestIntroduction_Params;
  using VersionsType = RequestIntroduction_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  RequestIntroduction_Base() = default;
  explicit RequestIntroduction_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~RequestIntroduction_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class AcceptIntroduction_Base
    : public MessageWithParams<AcceptIntroduction_Params> {
 public:
  using ParamsType = AcceptIntroduction_Params;
  using VersionsType = AcceptIntroduction_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  AcceptIntroduction_Base() = default;
  explicit AcceptIntroduction_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~AcceptIntroduction_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
      {VersionsType::V1::kVersion, ParamsType::v1_offset(),
       sizeof(ParamsType::V1), absl::MakeSpan(VersionsType::V1::kParams)},
  };
};
class RejectIntroduction_Base
    : public MessageWithParams<RejectIntroduction_Params> {
 public:
  using ParamsType = RejectIntroduction_Params;
  using VersionsType = RejectIntroduction_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  RejectIntroduction_Base() = default;
  explicit RejectIntroduction_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~RejectIntroduction_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class RequestIndirectIntroduction_Base
    : public MessageWithParams<RequestIndirectIntroduction_Params> {
 public:
  using ParamsType = RequestIndirectIntroduction_Params;
  using VersionsType = RequestIndirectIntroduction_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  RequestIndirectIntroduction_Base() = default;
  explicit RequestIndirectIntroduction_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~RequestIndirectIntroduction_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class AddBlockBuffer_Base
    : public MessageWithParams<AddBlockBuffer_Params> {
 public:
  using ParamsType = AddBlockBuffer_Params;
  using VersionsType = AddBlockBuffer_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  AddBlockBuffer_Base() = default;
  explicit AddBlockBuffer_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~AddBlockBuffer_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class AcceptParcel_Base
    : public MessageWithParams<AcceptParcel_Params> {
 public:
  using ParamsType = AcceptParcel_Params;
  using VersionsType = AcceptParcel_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  AcceptParcel_Base() = default;
  explicit AcceptParcel_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~AcceptParcel_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class AcceptParcelDriverObjects_Base
    : public MessageWithParams<AcceptParcelDriverObjects_Params> {
 public:
  using ParamsType = AcceptParcelDriverObjects_Params;
  using VersionsType = AcceptParcelDriverObjects_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  AcceptParcelDriverObjects_Base() = default;
  explicit AcceptParcelDriverObjects_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~AcceptParcelDriverObjects_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class RouteClosed_Base
    : public MessageWithParams<RouteClosed_Params> {
 public:
  using ParamsType = RouteClosed_Params;
  using VersionsType = RouteClosed_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  RouteClosed_Base() = default;
  explicit RouteClosed_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~RouteClosed_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class RouteDisconnected_Base
    : public MessageWithParams<RouteDisconnected_Params> {
 public:
  using ParamsType = RouteDisconnected_Params;
  using VersionsType = RouteDisconnected_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  RouteDisconnected_Base() = default;
  explicit RouteDisconnected_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~RouteDisconnected_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class BypassPeer_Base
    : public MessageWithParams<BypassPeer_Params> {
 public:
  using ParamsType = BypassPeer_Params;
  using VersionsType = BypassPeer_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  BypassPeer_Base() = default;
  explicit BypassPeer_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~BypassPeer_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class AcceptBypassLink_Base
    : public MessageWithParams<AcceptBypassLink_Params> {
 public:
  using ParamsType = AcceptBypassLink_Params;
  using VersionsType = AcceptBypassLink_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  AcceptBypassLink_Base() = default;
  explicit AcceptBypassLink_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~AcceptBypassLink_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class StopProxying_Base
    : public MessageWithParams<StopProxying_Params> {
 public:
  using ParamsType = StopProxying_Params;
  using VersionsType = StopProxying_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  StopProxying_Base() = default;
  explicit StopProxying_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~StopProxying_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class ProxyWillStop_Base
    : public MessageWithParams<ProxyWillStop_Params> {
 public:
  using ParamsType = ProxyWillStop_Params;
  using VersionsType = ProxyWillStop_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ProxyWillStop_Base() = default;
  explicit ProxyWillStop_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ProxyWillStop_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class BypassPeerWithLink_Base
    : public MessageWithParams<BypassPeerWithLink_Params> {
 public:
  using ParamsType = BypassPeerWithLink_Params;
  using VersionsType = BypassPeerWithLink_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  BypassPeerWithLink_Base() = default;
  explicit BypassPeerWithLink_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~BypassPeerWithLink_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class StopProxyingToLocalPeer_Base
    : public MessageWithParams<StopProxyingToLocalPeer_Params> {
 public:
  using ParamsType = StopProxyingToLocalPeer_Params;
  using VersionsType = StopProxyingToLocalPeer_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  StopProxyingToLocalPeer_Base() = default;
  explicit StopProxyingToLocalPeer_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~StopProxyingToLocalPeer_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class FlushRouter_Base
    : public MessageWithParams<FlushRouter_Params> {
 public:
  using ParamsType = FlushRouter_Params;
  using VersionsType = FlushRouter_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  FlushRouter_Base() = default;
  explicit FlushRouter_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~FlushRouter_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class RequestMemory_Base
    : public MessageWithParams<RequestMemory_Params> {
 public:
  using ParamsType = RequestMemory_Params;
  using VersionsType = RequestMemory_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  RequestMemory_Base() = default;
  explicit RequestMemory_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~RequestMemory_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class ProvideMemory_Base
    : public MessageWithParams<ProvideMemory_Params> {
 public:
  using ParamsType = ProvideMemory_Params;
  using VersionsType = ProvideMemory_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  ProvideMemory_Base() = default;
  explicit ProvideMemory_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~ProvideMemory_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class RelayMessage_Base
    : public MessageWithParams<RelayMessage_Params> {
 public:
  using ParamsType = RelayMessage_Params;
  using VersionsType = RelayMessage_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  RelayMessage_Base() = default;
  explicit RelayMessage_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~RelayMessage_Base() = default;
  static constexpr internal::VersionMetadata kVersions[] = {
      {VersionsType::V0::kVersion, ParamsType::v0_offset(),
       sizeof(ParamsType::V0), absl::MakeSpan(VersionsType::V0::kParams)},
  };
};
class AcceptRelayedMessage_Base
    : public MessageWithParams<AcceptRelayedMessage_Params> {
 public:
  using ParamsType = AcceptRelayedMessage_Params;
  using VersionsType = AcceptRelayedMessage_Versions;
  static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");
  AcceptRelayedMessage_Base() = default;
  explicit AcceptRelayedMessage_Base(decltype(kIncoming))
      : MessageWithParams(kIncoming) {}
  ~AcceptRelayedMessage_Base() = default;
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
class ConnectFromBrokerToNonBroker : public ConnectFromBrokerToNonBroker_Base {
 public:
  static constexpr uint8_t kId = 0;
  ConnectFromBrokerToNonBroker();
  explicit ConnectFromBrokerToNonBroker(decltype(kIncoming));
  ~ConnectFromBrokerToNonBroker();
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
};

class ConnectFromNonBrokerToBroker : public ConnectFromNonBrokerToBroker_Base {
 public:
  static constexpr uint8_t kId = 1;
  ConnectFromNonBrokerToBroker();
  explicit ConnectFromNonBrokerToBroker(decltype(kIncoming));
  ~ConnectFromNonBrokerToBroker();
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
};

class ReferNonBroker : public ReferNonBroker_Base {
 public:
  static constexpr uint8_t kId = 2;
  ReferNonBroker();
  explicit ReferNonBroker(decltype(kIncoming));
  ~ReferNonBroker();
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

class ConnectToReferredBroker : public ConnectToReferredBroker_Base {
 public:
  static constexpr uint8_t kId = 3;
  ConnectToReferredBroker();
  explicit ConnectToReferredBroker(decltype(kIncoming));
  ~ConnectToReferredBroker();
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
};

class ConnectToReferredNonBroker : public ConnectToReferredNonBroker_Base {
 public:
  static constexpr uint8_t kId = 4;
  ConnectToReferredNonBroker();
  explicit ConnectToReferredNonBroker(decltype(kIncoming));
  ~ConnectToReferredNonBroker();
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
};

class NonBrokerReferralAccepted : public NonBrokerReferralAccepted_Base {
 public:
  static constexpr uint8_t kId = 5;
  NonBrokerReferralAccepted();
  explicit NonBrokerReferralAccepted(decltype(kIncoming));
  ~NonBrokerReferralAccepted();
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
};

class NonBrokerReferralRejected : public NonBrokerReferralRejected_Base {
 public:
  static constexpr uint8_t kId = 6;
  NonBrokerReferralRejected();
  explicit NonBrokerReferralRejected(decltype(kIncoming));
  ~NonBrokerReferralRejected();
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

class ConnectFromBrokerToBroker : public ConnectFromBrokerToBroker_Base {
 public:
  static constexpr uint8_t kId = 7;
  ConnectFromBrokerToBroker();
  explicit ConnectFromBrokerToBroker(decltype(kIncoming));
  ~ConnectFromBrokerToBroker();
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
};

class RequestIntroduction : public RequestIntroduction_Base {
 public:
  static constexpr uint8_t kId = 10;
  RequestIntroduction();
  explicit RequestIntroduction(decltype(kIncoming));
  ~RequestIntroduction();
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

class AcceptIntroduction : public AcceptIntroduction_Base {
 public:
  static constexpr uint8_t kId = 11;
  AcceptIntroduction();
  explicit AcceptIntroduction(decltype(kIncoming));
  ~AcceptIntroduction();
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
};

class RejectIntroduction : public RejectIntroduction_Base {
 public:
  static constexpr uint8_t kId = 12;
  RejectIntroduction();
  explicit RejectIntroduction(decltype(kIncoming));
  ~RejectIntroduction();
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

class RequestIndirectIntroduction : public RequestIndirectIntroduction_Base {
 public:
  static constexpr uint8_t kId = 13;
  RequestIndirectIntroduction();
  explicit RequestIndirectIntroduction(decltype(kIncoming));
  ~RequestIndirectIntroduction();
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

class AddBlockBuffer : public AddBlockBuffer_Base {
 public:
  static constexpr uint8_t kId = 14;
  AddBlockBuffer();
  explicit AddBlockBuffer(decltype(kIncoming));
  ~AddBlockBuffer();
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

class AcceptParcel : public AcceptParcel_Base {
 public:
  static constexpr uint8_t kId = 20;
  AcceptParcel();
  explicit AcceptParcel(decltype(kIncoming));
  ~AcceptParcel();
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

class AcceptParcelDriverObjects : public AcceptParcelDriverObjects_Base {
 public:
  static constexpr uint8_t kId = 21;
  AcceptParcelDriverObjects();
  explicit AcceptParcelDriverObjects(decltype(kIncoming));
  ~AcceptParcelDriverObjects();
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

class RouteClosed : public RouteClosed_Base {
 public:
  static constexpr uint8_t kId = 22;
  RouteClosed();
  explicit RouteClosed(decltype(kIncoming));
  ~RouteClosed();
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

class RouteDisconnected : public RouteDisconnected_Base {
 public:
  static constexpr uint8_t kId = 23;
  RouteDisconnected();
  explicit RouteDisconnected(decltype(kIncoming));
  ~RouteDisconnected();
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

class BypassPeer : public BypassPeer_Base {
 public:
  static constexpr uint8_t kId = 30;
  BypassPeer();
  explicit BypassPeer(decltype(kIncoming));
  ~BypassPeer();
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

class AcceptBypassLink : public AcceptBypassLink_Base {
 public:
  static constexpr uint8_t kId = 31;
  AcceptBypassLink();
  explicit AcceptBypassLink(decltype(kIncoming));
  ~AcceptBypassLink();
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

class StopProxying : public StopProxying_Base {
 public:
  static constexpr uint8_t kId = 32;
  StopProxying();
  explicit StopProxying(decltype(kIncoming));
  ~StopProxying();
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

class ProxyWillStop : public ProxyWillStop_Base {
 public:
  static constexpr uint8_t kId = 33;
  ProxyWillStop();
  explicit ProxyWillStop(decltype(kIncoming));
  ~ProxyWillStop();
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

class BypassPeerWithLink : public BypassPeerWithLink_Base {
 public:
  static constexpr uint8_t kId = 34;
  BypassPeerWithLink();
  explicit BypassPeerWithLink(decltype(kIncoming));
  ~BypassPeerWithLink();
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

class StopProxyingToLocalPeer : public StopProxyingToLocalPeer_Base {
 public:
  static constexpr uint8_t kId = 35;
  StopProxyingToLocalPeer();
  explicit StopProxyingToLocalPeer(decltype(kIncoming));
  ~StopProxyingToLocalPeer();
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

class FlushRouter : public FlushRouter_Base {
 public:
  static constexpr uint8_t kId = 36;
  FlushRouter();
  explicit FlushRouter(decltype(kIncoming));
  ~FlushRouter();
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

class RequestMemory : public RequestMemory_Base {
 public:
  static constexpr uint8_t kId = 64;
  RequestMemory();
  explicit RequestMemory(decltype(kIncoming));
  ~RequestMemory();
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

class ProvideMemory : public ProvideMemory_Base {
 public:
  static constexpr uint8_t kId = 65;
  ProvideMemory();
  explicit ProvideMemory(decltype(kIncoming));
  ~ProvideMemory();
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

class RelayMessage : public RelayMessage_Base {
 public:
  static constexpr uint8_t kId = 66;
  RelayMessage();
  explicit RelayMessage(decltype(kIncoming));
  ~RelayMessage();
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

class AcceptRelayedMessage : public AcceptRelayedMessage_Base {
 public:
  static constexpr uint8_t kId = 67;
  AcceptRelayedMessage();
  explicit AcceptRelayedMessage(decltype(kIncoming));
  ~AcceptRelayedMessage();
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

class NodeMessageListener : public DriverTransport::Listener {
 public:
  virtual ~NodeMessageListener() = default;
  virtual bool OnMessage(Message& message);

 protected:
  virtual bool DispatchMessage(Message& message);
  virtual bool OnConnectFromBrokerToNonBroker(ConnectFromBrokerToNonBroker&) { return false; }
  virtual bool OnConnectFromNonBrokerToBroker(ConnectFromNonBrokerToBroker&) { return false; }
  virtual bool OnReferNonBroker(ReferNonBroker&) { return false; }
  virtual bool OnConnectToReferredBroker(ConnectToReferredBroker&) { return false; }
  virtual bool OnConnectToReferredNonBroker(ConnectToReferredNonBroker&) { return false; }
  virtual bool OnNonBrokerReferralAccepted(NonBrokerReferralAccepted&) { return false; }
  virtual bool OnNonBrokerReferralRejected(NonBrokerReferralRejected&) { return false; }
  virtual bool OnConnectFromBrokerToBroker(ConnectFromBrokerToBroker&) { return false; }
  virtual bool OnRequestIntroduction(RequestIntroduction&) { return false; }
  virtual bool OnAcceptIntroduction(AcceptIntroduction&) { return false; }
  virtual bool OnRejectIntroduction(RejectIntroduction&) { return false; }
  virtual bool OnRequestIndirectIntroduction(RequestIndirectIntroduction&) { return false; }
  virtual bool OnAddBlockBuffer(AddBlockBuffer&) { return false; }
  virtual bool OnAcceptParcel(AcceptParcel&) { return false; }
  virtual bool OnAcceptParcelDriverObjects(AcceptParcelDriverObjects&) { return false; }
  virtual bool OnRouteClosed(RouteClosed&) { return false; }
  virtual bool OnRouteDisconnected(RouteDisconnected&) { return false; }
  virtual bool OnBypassPeer(BypassPeer&) { return false; }
  virtual bool OnAcceptBypassLink(AcceptBypassLink&) { return false; }
  virtual bool OnStopProxying(StopProxying&) { return false; }
  virtual bool OnProxyWillStop(ProxyWillStop&) { return false; }
  virtual bool OnBypassPeerWithLink(BypassPeerWithLink&) { return false; }
  virtual bool OnStopProxyingToLocalPeer(StopProxyingToLocalPeer&) { return false; }
  virtual bool OnFlushRouter(FlushRouter&) { return false; }
  virtual bool OnRequestMemory(RequestMemory&) { return false; }
  virtual bool OnProvideMemory(ProvideMemory&) { return false; }
  virtual bool OnRelayMessage(RelayMessage&) { return false; }
  virtual bool OnAcceptRelayedMessage(AcceptRelayedMessage&) { return false; }
 private:
  bool OnTransportMessage(const DriverTransport::RawMessage& message,
                          const DriverTransport& transport,
                          IpczDriverHandle envelope) final;
  void OnTransportError() override {}
};

#pragma pack(pop)

}  // namespace ipcz::msg

// clang-format on

#endif  // IPCZ_SRC_IPCZ_NODE_MESSAGES_H_