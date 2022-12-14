// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_P2P_BASE_BRIDGE_ICE_CONTROLLER_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_P2P_BASE_BRIDGE_ICE_CONTROLLER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"

#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/p2p/base/active_ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/connection.h"
#include "third_party/webrtc/p2p/base/ice_agent_interface.h"
#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"
#include "third_party/webrtc/p2p/base/ice_transport_internal.h"
#include "third_party/webrtc/p2p/base/transport_description.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_interaction_interface.h"

namespace blink {

// BridgeIceController allows criculating ICE controller requests through blink
// before taking the necessary action. This enables blink to consult with the
// application before manipulating the ICE transport.
//
// BridgeIceController is constructed and owned for the entirety of its lifetime
// by the native ICE transport (i.e. P2PTransportChannel). It must be called on
// the same sequence (or thread) on which the ICE agent expects to be invoked.
class RTC_EXPORT BridgeIceController
    : public cricket::ActiveIceControllerInterface,
      public IceInteractionInterface {
 public:
  // Constructs an ICE controller wrapping an already constructed native webrtc
  // ICE controller. Does not take ownership of the ICE agent, which must
  // already exist and outlive the ICE controller. Task runner should be the
  // sequence on which the ICE agent expects to be invoked.
  BridgeIceController(
      scoped_refptr<base::SequencedTaskRunner> network_task_runner,
      cricket::IceAgentInterface* ice_agent,
      std::unique_ptr<cricket::IceControllerInterface> native_controller);
  ~BridgeIceController() override;

  // ActiveIceControllerInterface overrides.

  void SetIceConfig(const cricket::IceConfig& config) override;
  bool GetUseCandidateAttribute(
      const cricket::Connection* connection,
      cricket::NominationMode mode,
      cricket::IceMode remote_ice_mode) const override;

  void OnConnectionAdded(const cricket::Connection* connection) override;
  void OnConnectionPinged(const cricket::Connection* connection) override;
  void OnConnectionUpdated(const cricket::Connection* connection) override;
  void OnConnectionSwitched(const cricket::Connection* connection) override;
  void OnConnectionDestroyed(const cricket::Connection* connection) override;

  void OnSortAndSwitchRequest(cricket::IceSwitchReason reason) override;
  void OnImmediateSortAndSwitchRequest(
      cricket::IceSwitchReason reason) override;
  bool OnImmediateSwitchRequest(cricket::IceSwitchReason reason,
                                const cricket::Connection* selected) override;

  // Only for unit tests
  const cricket::Connection* FindNextPingableConnection() override;

  // IceInteractionInterface overrides.

  void AcceptPingProposal(const IcePingProposal& ping_proposal) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
  }
  void RejectPingProposal(const IcePingProposal& ping_proposal) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
  }

  void AcceptSwitchProposal(const IceSwitchProposal& switch_proposal) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
  }
  void RejectSwitchProposal(const IceSwitchProposal& switch_proposal) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
  }

  void AcceptPruneProposal(const IcePruneProposal& prune_proposal) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
  }
  void RejectPruneProposal(const IcePruneProposal& prune_proposal) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
  }

  webrtc::RTCError PingIceConnection(const IceConnection& connection) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
    return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
  }
  webrtc::RTCError SwitchToIceConnection(
      const IceConnection& connection) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
    return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
  }
  webrtc::RTCError PruneIceConnections(
      base::span<const IceConnection> connections_to_prune) override {
    NOTIMPLEMENTED();  // TODO(crbug.com/1369096) implement!
    return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
  }

 private:
  void MaybeStartPinging();
  void SelectAndPingConnection();
  void HandlePingResult(cricket::IceControllerInterface::PingResult result);

  void SortAndSwitchToBestConnection(cricket::IceSwitchReason reason);
  void DoSortAndSwitchToBestConnection(cricket::IceSwitchReason reason);
  void HandleSwitchResult(cricket::IceSwitchReason reason_for_switch,
                          cricket::IceControllerInterface::SwitchResult result);
  void UpdateStateOnConnectionsResorted();

  void PruneConnections();

  const scoped_refptr<base::SequencedTaskRunner> network_task_runner_;

  bool started_pinging_ = false;
  bool sort_pending_ = false;
  const cricket::Connection* selected_connection_ = nullptr;

  const std::unique_ptr<cricket::IceControllerInterface> native_controller_;
  cricket::IceAgentInterface& agent_;

  base::WeakPtrFactory<BridgeIceController> weak_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_P2P_BASE_BRIDGE_ICE_CONTROLLER_H_
