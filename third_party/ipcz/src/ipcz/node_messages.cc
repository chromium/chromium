// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generated from node_messages.cc.tmpl and checked-in. Change this
// file by editing the template then running:
//
// node_messages.py --dir={path to *_messages_generator.h}

#include "ipcz/node_messages.h"

#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz::msg {

// clang-format off

#pragma pack(push, 1)

ConnectFromBrokerToNonBroker_Params::ConnectFromBrokerToNonBroker_Params() = default;
ConnectFromBrokerToNonBroker_Params::~ConnectFromBrokerToNonBroker_Params() = default;
ConnectFromNonBrokerToBroker_Params::ConnectFromNonBrokerToBroker_Params() = default;
ConnectFromNonBrokerToBroker_Params::~ConnectFromNonBrokerToBroker_Params() = default;
ReferNonBroker_Params::ReferNonBroker_Params() = default;
ReferNonBroker_Params::~ReferNonBroker_Params() = default;
ConnectToReferredBroker_Params::ConnectToReferredBroker_Params() = default;
ConnectToReferredBroker_Params::~ConnectToReferredBroker_Params() = default;
ConnectToReferredNonBroker_Params::ConnectToReferredNonBroker_Params() = default;
ConnectToReferredNonBroker_Params::~ConnectToReferredNonBroker_Params() = default;
NonBrokerReferralAccepted_Params::NonBrokerReferralAccepted_Params() = default;
NonBrokerReferralAccepted_Params::~NonBrokerReferralAccepted_Params() = default;
NonBrokerReferralRejected_Params::NonBrokerReferralRejected_Params() = default;
NonBrokerReferralRejected_Params::~NonBrokerReferralRejected_Params() = default;
ConnectFromBrokerToBroker_Params::ConnectFromBrokerToBroker_Params() = default;
ConnectFromBrokerToBroker_Params::~ConnectFromBrokerToBroker_Params() = default;
RequestIntroduction_Params::RequestIntroduction_Params() = default;
RequestIntroduction_Params::~RequestIntroduction_Params() = default;
AcceptIntroduction_Params::AcceptIntroduction_Params() = default;
AcceptIntroduction_Params::~AcceptIntroduction_Params() = default;
RejectIntroduction_Params::RejectIntroduction_Params() = default;
RejectIntroduction_Params::~RejectIntroduction_Params() = default;
RequestIndirectIntroduction_Params::RequestIndirectIntroduction_Params() = default;
RequestIndirectIntroduction_Params::~RequestIndirectIntroduction_Params() = default;
AddBlockBuffer_Params::AddBlockBuffer_Params() = default;
AddBlockBuffer_Params::~AddBlockBuffer_Params() = default;
AcceptParcel_Params::AcceptParcel_Params() = default;
AcceptParcel_Params::~AcceptParcel_Params() = default;
AcceptParcelDriverObjects_Params::AcceptParcelDriverObjects_Params() = default;
AcceptParcelDriverObjects_Params::~AcceptParcelDriverObjects_Params() = default;
RouteClosed_Params::RouteClosed_Params() = default;
RouteClosed_Params::~RouteClosed_Params() = default;
RouteDisconnected_Params::RouteDisconnected_Params() = default;
RouteDisconnected_Params::~RouteDisconnected_Params() = default;
BypassPeer_Params::BypassPeer_Params() = default;
BypassPeer_Params::~BypassPeer_Params() = default;
AcceptBypassLink_Params::AcceptBypassLink_Params() = default;
AcceptBypassLink_Params::~AcceptBypassLink_Params() = default;
StopProxying_Params::StopProxying_Params() = default;
StopProxying_Params::~StopProxying_Params() = default;
ProxyWillStop_Params::ProxyWillStop_Params() = default;
ProxyWillStop_Params::~ProxyWillStop_Params() = default;
BypassPeerWithLink_Params::BypassPeerWithLink_Params() = default;
BypassPeerWithLink_Params::~BypassPeerWithLink_Params() = default;
StopProxyingToLocalPeer_Params::StopProxyingToLocalPeer_Params() = default;
StopProxyingToLocalPeer_Params::~StopProxyingToLocalPeer_Params() = default;
FlushRouter_Params::FlushRouter_Params() = default;
FlushRouter_Params::~FlushRouter_Params() = default;
RequestMemory_Params::RequestMemory_Params() = default;
RequestMemory_Params::~RequestMemory_Params() = default;
ProvideMemory_Params::ProvideMemory_Params() = default;
ProvideMemory_Params::~ProvideMemory_Params() = default;
RelayMessage_Params::RelayMessage_Params() = default;
RelayMessage_Params::~RelayMessage_Params() = default;
AcceptRelayedMessage_Params::AcceptRelayedMessage_Params() = default;
AcceptRelayedMessage_Params::~AcceptRelayedMessage_Params() = default;

ConnectFromBrokerToNonBroker::ConnectFromBrokerToNonBroker() = default;
ConnectFromBrokerToNonBroker::ConnectFromBrokerToNonBroker(decltype(kIncoming))
    : ConnectFromBrokerToNonBroker_Base(kIncoming) {}
ConnectFromBrokerToNonBroker::~ConnectFromBrokerToNonBroker() = default;

bool ConnectFromBrokerToNonBroker::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ConnectFromBrokerToNonBroker::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ConnectFromBrokerToNonBroker_Base::kVersions[];

ConnectFromNonBrokerToBroker::ConnectFromNonBrokerToBroker() = default;
ConnectFromNonBrokerToBroker::ConnectFromNonBrokerToBroker(decltype(kIncoming))
    : ConnectFromNonBrokerToBroker_Base(kIncoming) {}
ConnectFromNonBrokerToBroker::~ConnectFromNonBrokerToBroker() = default;

bool ConnectFromNonBrokerToBroker::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ConnectFromNonBrokerToBroker::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ConnectFromNonBrokerToBroker_Base::kVersions[];

ReferNonBroker::ReferNonBroker() = default;
ReferNonBroker::ReferNonBroker(decltype(kIncoming))
    : ReferNonBroker_Base(kIncoming) {}
ReferNonBroker::~ReferNonBroker() = default;

bool ReferNonBroker::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ReferNonBroker::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ReferNonBroker_Base::kVersions[];

ConnectToReferredBroker::ConnectToReferredBroker() = default;
ConnectToReferredBroker::ConnectToReferredBroker(decltype(kIncoming))
    : ConnectToReferredBroker_Base(kIncoming) {}
ConnectToReferredBroker::~ConnectToReferredBroker() = default;

bool ConnectToReferredBroker::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ConnectToReferredBroker::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ConnectToReferredBroker_Base::kVersions[];

ConnectToReferredNonBroker::ConnectToReferredNonBroker() = default;
ConnectToReferredNonBroker::ConnectToReferredNonBroker(decltype(kIncoming))
    : ConnectToReferredNonBroker_Base(kIncoming) {}
ConnectToReferredNonBroker::~ConnectToReferredNonBroker() = default;

bool ConnectToReferredNonBroker::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ConnectToReferredNonBroker::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ConnectToReferredNonBroker_Base::kVersions[];

NonBrokerReferralAccepted::NonBrokerReferralAccepted() = default;
NonBrokerReferralAccepted::NonBrokerReferralAccepted(decltype(kIncoming))
    : NonBrokerReferralAccepted_Base(kIncoming) {}
NonBrokerReferralAccepted::~NonBrokerReferralAccepted() = default;

bool NonBrokerReferralAccepted::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool NonBrokerReferralAccepted::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata NonBrokerReferralAccepted_Base::kVersions[];

NonBrokerReferralRejected::NonBrokerReferralRejected() = default;
NonBrokerReferralRejected::NonBrokerReferralRejected(decltype(kIncoming))
    : NonBrokerReferralRejected_Base(kIncoming) {}
NonBrokerReferralRejected::~NonBrokerReferralRejected() = default;

bool NonBrokerReferralRejected::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool NonBrokerReferralRejected::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata NonBrokerReferralRejected_Base::kVersions[];

ConnectFromBrokerToBroker::ConnectFromBrokerToBroker() = default;
ConnectFromBrokerToBroker::ConnectFromBrokerToBroker(decltype(kIncoming))
    : ConnectFromBrokerToBroker_Base(kIncoming) {}
ConnectFromBrokerToBroker::~ConnectFromBrokerToBroker() = default;

bool ConnectFromBrokerToBroker::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ConnectFromBrokerToBroker::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ConnectFromBrokerToBroker_Base::kVersions[];

RequestIntroduction::RequestIntroduction() = default;
RequestIntroduction::RequestIntroduction(decltype(kIncoming))
    : RequestIntroduction_Base(kIncoming) {}
RequestIntroduction::~RequestIntroduction() = default;

bool RequestIntroduction::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool RequestIntroduction::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata RequestIntroduction_Base::kVersions[];

AcceptIntroduction::AcceptIntroduction() = default;
AcceptIntroduction::AcceptIntroduction(decltype(kIncoming))
    : AcceptIntroduction_Base(kIncoming) {}
AcceptIntroduction::~AcceptIntroduction() = default;

bool AcceptIntroduction::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool AcceptIntroduction::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata AcceptIntroduction_Base::kVersions[];

RejectIntroduction::RejectIntroduction() = default;
RejectIntroduction::RejectIntroduction(decltype(kIncoming))
    : RejectIntroduction_Base(kIncoming) {}
RejectIntroduction::~RejectIntroduction() = default;

bool RejectIntroduction::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool RejectIntroduction::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata RejectIntroduction_Base::kVersions[];

RequestIndirectIntroduction::RequestIndirectIntroduction() = default;
RequestIndirectIntroduction::RequestIndirectIntroduction(decltype(kIncoming))
    : RequestIndirectIntroduction_Base(kIncoming) {}
RequestIndirectIntroduction::~RequestIndirectIntroduction() = default;

bool RequestIndirectIntroduction::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool RequestIndirectIntroduction::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata RequestIndirectIntroduction_Base::kVersions[];

AddBlockBuffer::AddBlockBuffer() = default;
AddBlockBuffer::AddBlockBuffer(decltype(kIncoming))
    : AddBlockBuffer_Base(kIncoming) {}
AddBlockBuffer::~AddBlockBuffer() = default;

bool AddBlockBuffer::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool AddBlockBuffer::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata AddBlockBuffer_Base::kVersions[];

AcceptParcel::AcceptParcel() = default;
AcceptParcel::AcceptParcel(decltype(kIncoming))
    : AcceptParcel_Base(kIncoming) {}
AcceptParcel::~AcceptParcel() = default;

bool AcceptParcel::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool AcceptParcel::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata AcceptParcel_Base::kVersions[];

AcceptParcelDriverObjects::AcceptParcelDriverObjects() = default;
AcceptParcelDriverObjects::AcceptParcelDriverObjects(decltype(kIncoming))
    : AcceptParcelDriverObjects_Base(kIncoming) {}
AcceptParcelDriverObjects::~AcceptParcelDriverObjects() = default;

bool AcceptParcelDriverObjects::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool AcceptParcelDriverObjects::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata AcceptParcelDriverObjects_Base::kVersions[];

RouteClosed::RouteClosed() = default;
RouteClosed::RouteClosed(decltype(kIncoming))
    : RouteClosed_Base(kIncoming) {}
RouteClosed::~RouteClosed() = default;

bool RouteClosed::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool RouteClosed::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata RouteClosed_Base::kVersions[];

RouteDisconnected::RouteDisconnected() = default;
RouteDisconnected::RouteDisconnected(decltype(kIncoming))
    : RouteDisconnected_Base(kIncoming) {}
RouteDisconnected::~RouteDisconnected() = default;

bool RouteDisconnected::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool RouteDisconnected::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata RouteDisconnected_Base::kVersions[];

BypassPeer::BypassPeer() = default;
BypassPeer::BypassPeer(decltype(kIncoming))
    : BypassPeer_Base(kIncoming) {}
BypassPeer::~BypassPeer() = default;

bool BypassPeer::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool BypassPeer::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata BypassPeer_Base::kVersions[];

AcceptBypassLink::AcceptBypassLink() = default;
AcceptBypassLink::AcceptBypassLink(decltype(kIncoming))
    : AcceptBypassLink_Base(kIncoming) {}
AcceptBypassLink::~AcceptBypassLink() = default;

bool AcceptBypassLink::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool AcceptBypassLink::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata AcceptBypassLink_Base::kVersions[];

StopProxying::StopProxying() = default;
StopProxying::StopProxying(decltype(kIncoming))
    : StopProxying_Base(kIncoming) {}
StopProxying::~StopProxying() = default;

bool StopProxying::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool StopProxying::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata StopProxying_Base::kVersions[];

ProxyWillStop::ProxyWillStop() = default;
ProxyWillStop::ProxyWillStop(decltype(kIncoming))
    : ProxyWillStop_Base(kIncoming) {}
ProxyWillStop::~ProxyWillStop() = default;

bool ProxyWillStop::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ProxyWillStop::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ProxyWillStop_Base::kVersions[];

BypassPeerWithLink::BypassPeerWithLink() = default;
BypassPeerWithLink::BypassPeerWithLink(decltype(kIncoming))
    : BypassPeerWithLink_Base(kIncoming) {}
BypassPeerWithLink::~BypassPeerWithLink() = default;

bool BypassPeerWithLink::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool BypassPeerWithLink::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata BypassPeerWithLink_Base::kVersions[];

StopProxyingToLocalPeer::StopProxyingToLocalPeer() = default;
StopProxyingToLocalPeer::StopProxyingToLocalPeer(decltype(kIncoming))
    : StopProxyingToLocalPeer_Base(kIncoming) {}
StopProxyingToLocalPeer::~StopProxyingToLocalPeer() = default;

bool StopProxyingToLocalPeer::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool StopProxyingToLocalPeer::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata StopProxyingToLocalPeer_Base::kVersions[];

FlushRouter::FlushRouter() = default;
FlushRouter::FlushRouter(decltype(kIncoming))
    : FlushRouter_Base(kIncoming) {}
FlushRouter::~FlushRouter() = default;

bool FlushRouter::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool FlushRouter::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata FlushRouter_Base::kVersions[];

RequestMemory::RequestMemory() = default;
RequestMemory::RequestMemory(decltype(kIncoming))
    : RequestMemory_Base(kIncoming) {}
RequestMemory::~RequestMemory() = default;

bool RequestMemory::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool RequestMemory::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata RequestMemory_Base::kVersions[];

ProvideMemory::ProvideMemory() = default;
ProvideMemory::ProvideMemory(decltype(kIncoming))
    : ProvideMemory_Base(kIncoming) {}
ProvideMemory::~ProvideMemory() = default;

bool ProvideMemory::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool ProvideMemory::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata ProvideMemory_Base::kVersions[];

RelayMessage::RelayMessage() = default;
RelayMessage::RelayMessage(decltype(kIncoming))
    : RelayMessage_Base(kIncoming) {}
RelayMessage::~RelayMessage() = default;

bool RelayMessage::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool RelayMessage::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata RelayMessage_Base::kVersions[];

AcceptRelayedMessage::AcceptRelayedMessage() = default;
AcceptRelayedMessage::AcceptRelayedMessage(decltype(kIncoming))
    : AcceptRelayedMessage_Base(kIncoming) {}
AcceptRelayedMessage::~AcceptRelayedMessage() = default;

bool AcceptRelayedMessage::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool AcceptRelayedMessage::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata AcceptRelayedMessage_Base::kVersions[];


bool NodeMessageListener::OnMessage(Message& message) {
  return DispatchMessage(message);
}

bool NodeMessageListener::OnTransportMessage(
    const DriverTransport::RawMessage& raw_message,
    const DriverTransport& transport,
    IpczDriverHandle envelope) {
  if (raw_message.data.size() >= sizeof(internal::MessageHeaderV0)) {
    const auto& header = *reinterpret_cast<const internal::MessageHeaderV0*>(
        raw_message.data.data());
    switch (header.message_id) {
      case ConnectFromBrokerToNonBroker::kId: {
        ConnectFromBrokerToNonBroker message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case ConnectFromNonBrokerToBroker::kId: {
        ConnectFromNonBrokerToBroker message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case ReferNonBroker::kId: {
        ReferNonBroker message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case ConnectToReferredBroker::kId: {
        ConnectToReferredBroker message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case ConnectToReferredNonBroker::kId: {
        ConnectToReferredNonBroker message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case NonBrokerReferralAccepted::kId: {
        NonBrokerReferralAccepted message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case NonBrokerReferralRejected::kId: {
        NonBrokerReferralRejected message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case ConnectFromBrokerToBroker::kId: {
        ConnectFromBrokerToBroker message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case RequestIntroduction::kId: {
        RequestIntroduction message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case AcceptIntroduction::kId: {
        AcceptIntroduction message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case RejectIntroduction::kId: {
        RejectIntroduction message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case RequestIndirectIntroduction::kId: {
        RequestIndirectIntroduction message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case AddBlockBuffer::kId: {
        AddBlockBuffer message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case AcceptParcel::kId: {
        AcceptParcel message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case AcceptParcelDriverObjects::kId: {
        AcceptParcelDriverObjects message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case RouteClosed::kId: {
        RouteClosed message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case RouteDisconnected::kId: {
        RouteDisconnected message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case BypassPeer::kId: {
        BypassPeer message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case AcceptBypassLink::kId: {
        AcceptBypassLink message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case StopProxying::kId: {
        StopProxying message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case ProxyWillStop::kId: {
        ProxyWillStop message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case BypassPeerWithLink::kId: {
        BypassPeerWithLink message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case StopProxyingToLocalPeer::kId: {
        StopProxyingToLocalPeer message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case FlushRouter::kId: {
        FlushRouter message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case RequestMemory::kId: {
        RequestMemory message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case ProvideMemory::kId: {
        ProvideMemory message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case RelayMessage::kId: {
        RelayMessage message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case AcceptRelayedMessage::kId: {
        AcceptRelayedMessage message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      default:
        break;
    }
  }
  Message message;
  message.SetEnvelope(
      DriverObject(*transport.driver_object().driver(), envelope));
  return message.DeserializeUnknownType(raw_message, transport) &&
         OnMessage(message);
}

bool NodeMessageListener::DispatchMessage(Message& message) {
  switch (message.header().message_id) {
    case msg::ConnectFromBrokerToNonBroker::kId:
      return OnConnectFromBrokerToNonBroker(static_cast<ConnectFromBrokerToNonBroker&>(message));
    case msg::ConnectFromNonBrokerToBroker::kId:
      return OnConnectFromNonBrokerToBroker(static_cast<ConnectFromNonBrokerToBroker&>(message));
    case msg::ReferNonBroker::kId:
      return OnReferNonBroker(static_cast<ReferNonBroker&>(message));
    case msg::ConnectToReferredBroker::kId:
      return OnConnectToReferredBroker(static_cast<ConnectToReferredBroker&>(message));
    case msg::ConnectToReferredNonBroker::kId:
      return OnConnectToReferredNonBroker(static_cast<ConnectToReferredNonBroker&>(message));
    case msg::NonBrokerReferralAccepted::kId:
      return OnNonBrokerReferralAccepted(static_cast<NonBrokerReferralAccepted&>(message));
    case msg::NonBrokerReferralRejected::kId:
      return OnNonBrokerReferralRejected(static_cast<NonBrokerReferralRejected&>(message));
    case msg::ConnectFromBrokerToBroker::kId:
      return OnConnectFromBrokerToBroker(static_cast<ConnectFromBrokerToBroker&>(message));
    case msg::RequestIntroduction::kId:
      return OnRequestIntroduction(static_cast<RequestIntroduction&>(message));
    case msg::AcceptIntroduction::kId:
      return OnAcceptIntroduction(static_cast<AcceptIntroduction&>(message));
    case msg::RejectIntroduction::kId:
      return OnRejectIntroduction(static_cast<RejectIntroduction&>(message));
    case msg::RequestIndirectIntroduction::kId:
      return OnRequestIndirectIntroduction(static_cast<RequestIndirectIntroduction&>(message));
    case msg::AddBlockBuffer::kId:
      return OnAddBlockBuffer(static_cast<AddBlockBuffer&>(message));
    case msg::AcceptParcel::kId:
      return OnAcceptParcel(static_cast<AcceptParcel&>(message));
    case msg::AcceptParcelDriverObjects::kId:
      return OnAcceptParcelDriverObjects(static_cast<AcceptParcelDriverObjects&>(message));
    case msg::RouteClosed::kId:
      return OnRouteClosed(static_cast<RouteClosed&>(message));
    case msg::RouteDisconnected::kId:
      return OnRouteDisconnected(static_cast<RouteDisconnected&>(message));
    case msg::BypassPeer::kId:
      return OnBypassPeer(static_cast<BypassPeer&>(message));
    case msg::AcceptBypassLink::kId:
      return OnAcceptBypassLink(static_cast<AcceptBypassLink&>(message));
    case msg::StopProxying::kId:
      return OnStopProxying(static_cast<StopProxying&>(message));
    case msg::ProxyWillStop::kId:
      return OnProxyWillStop(static_cast<ProxyWillStop&>(message));
    case msg::BypassPeerWithLink::kId:
      return OnBypassPeerWithLink(static_cast<BypassPeerWithLink&>(message));
    case msg::StopProxyingToLocalPeer::kId:
      return OnStopProxyingToLocalPeer(static_cast<StopProxyingToLocalPeer&>(message));
    case msg::FlushRouter::kId:
      return OnFlushRouter(static_cast<FlushRouter&>(message));
    case msg::RequestMemory::kId:
      return OnRequestMemory(static_cast<RequestMemory&>(message));
    case msg::ProvideMemory::kId:
      return OnProvideMemory(static_cast<ProvideMemory&>(message));
    case msg::RelayMessage::kId:
      return OnRelayMessage(static_cast<RelayMessage&>(message));
    case msg::AcceptRelayedMessage::kId:
      return OnAcceptRelayedMessage(static_cast<AcceptRelayedMessage&>(message));
    default:
      // Message might be from a newer version of ipcz so quietly ignore it.
      return true;
  }
}

#pragma pack(pop)

// clang-format on

}  // namespace ipcz::msg