// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/bridge_ice_controller.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/array_view.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/p2p/base/connection.h"
#include "third_party/webrtc/p2p/base/ice_agent_interface.h"
#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"
#include "third_party/webrtc/p2p/base/ice_transport_internal.h"
#include "third_party/webrtc/p2p/base/transport_description.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_controller_observer.h"
#include "third_party/webrtc_overrides/p2p/base/ice_ping_proposal.h"
#include "third_party/webrtc_overrides/p2p/base/ice_prune_proposal.h"
#include "third_party/webrtc_overrides/p2p/base/ice_switch_proposal.h"
#include "third_party/webrtc_overrides/rtc_base/diagnostic_logging.h"
#include "third_party/webrtc_overrides/rtc_base/logging.h"

namespace {
using cricket::Connection;
using cricket::IceAgentInterface;
using cricket::IceConfig;
using cricket::IceControllerInterface;
using cricket::IceMode;
using cricket::IceRole;
using cricket::NominationMode;
}  // unnamed namespace

namespace blink {

BridgeIceController::BridgeIceController(
    scoped_refptr<base::SequencedTaskRunner> network_task_runner,
    IceControllerObserverInterface* observer,
    IceAgentInterface* ice_agent,
    std::unique_ptr<cricket::IceControllerInterface> native_controller)
    : network_task_runner_(std::move(network_task_runner)),
      interaction_proxy_(
          base::MakeRefCounted<IceInteractionProxy>(this,
                                                    network_task_runner_)),
      native_controller_(std::move(native_controller)),
      agent_(*ice_agent),
      weak_factory_(this) {
  DCHECK(ice_agent != nullptr);
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (observer) {
    AttachObserver(observer);
  }
}

BridgeIceController::BridgeIceController(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    IceAgentInterface* ice_agent,
    std::unique_ptr<IceControllerInterface> native_controller)
    : BridgeIceController(std::move(task_runner),
                          /*observer=*/nullptr,
                          ice_agent,
                          std::move(native_controller)) {}

BridgeIceController::~BridgeIceController() = default;

void BridgeIceController::AttachObserver(
    IceControllerObserverInterface* observer) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  bool changed = observer_ != observer;
  IceControllerObserverInterface* previous_observer = observer_;
  observer_ = observer;
  if (previous_observer && changed) {
    previous_observer->OnObserverDetached();
  }
  if (observer_ && changed) {
    observer_->OnObserverAttached(interaction_proxy_);
  }
}

void BridgeIceController::SetIceConfig(const IceConfig& config) {
  native_controller_->SetIceConfig(config);
}

bool BridgeIceController::GetUseCandidateAttribute(
    const Connection* connection,
    NominationMode mode,
    IceMode remote_ice_mode) const {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  return native_controller_->GetUseCandidateAttr(connection, mode,
                                                 remote_ice_mode);
}

void BridgeIceController::OnConnectionAdded(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  native_controller_->AddConnection(connection);
  if (observer_) {
    observer_->OnConnectionAdded(IceConnection(connection));
  }
}

void BridgeIceController::OnConnectionPinged(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  native_controller_->MarkConnectionPinged(connection);
}

void BridgeIceController::OnConnectionUpdated(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (observer_) {
    observer_->OnConnectionUpdated(IceConnection(connection));
  }
}

void BridgeIceController::OnConnectionSwitched(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  selected_connection_ = connection;
  native_controller_->SetSelectedConnection(connection);
  if (observer_) {
    observer_->OnConnectionSwitched(IceConnection(connection));
  }
}

void BridgeIceController::OnConnectionDestroyed(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  IceConnection deleted_connection = IceConnection(connection);
  native_controller_->OnConnectionDestroyed(connection);
  if (observer_) {
    observer_->OnConnectionDestroyed(deleted_connection);
  }
}

void BridgeIceController::MaybeStartPinging() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (started_pinging_) {
    return;
  }

  if (native_controller_->HasPingableConnection()) {
    // Enqueue a task to select a connection and ping.
    // TODO(crbug.com/1369096): this can probably happen right away but retained
    // as a PostTask from the native WebRTC ICE controller. Remove if possible.
    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BridgeIceController::SelectAndPingConnection,
                                  weak_factory_.GetWeakPtr()));
    agent_.OnStartedPinging();
    started_pinging_ = true;
  }
}

// TODO(crbug.com/1369096) currently, proposals are applied when accepted even
// if another proposal arrived and was applied immediately while waiting for
// another response. This needs to be fixed by maintaining an ordered queue of
// proposals and ignoring any stale proposals.

void BridgeIceController::SelectAndPingConnection() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  agent_.UpdateConnectionStates();

  IceControllerInterface::PingResult result =
      native_controller_->SelectConnectionToPing(agent_.GetLastPingSentMs());
  bool reply_expected = false;
  if (observer_) {
    // Ping proposal without a connection may still be useful to indicate a
    // recheck delay.
    reply_expected =
        result.connection.has_value() && result.connection.value() != nullptr;
    observer_->OnPingProposal(IcePingProposal(result, reply_expected));
  }
  if (!reply_expected) {
    // No reply expected, so continue with the ping right away.
    DoPerformPing(result);
  }
}

void BridgeIceController::OnPingProposalAccepted(
    const IcePingProposal& proposal) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(proposal.reply_expected())
      << "Can't accept an unsolicited ping proposal";
  DCHECK(proposal.connection().has_value())
      << "Can't accept a ping proposal without a connection";

  const cricket::Connection* connection =
      FindConnection(proposal.connection().value().id());
  if (!connection) {
    RTC_LOG(LS_WARNING)
        << "Received unknown or destroyed connection in ping proposal id="
        << proposal.connection().value().id();
    return;
  }

  DoPerformPing(connection, proposal.recheck_delay_ms());
}

void BridgeIceController::OnPingProposalRejected(
    const IcePingProposal& proposal) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(proposal.reply_expected())
      << "Can't reject an unsolicited ping proposal";

  // Don't send a ping, but schedule a recheck anyway if it was proposed.
  DoSchedulePingRecheck(proposal.recheck_delay_ms());
}

void BridgeIceController::DoPerformPing(
    const cricket::IceControllerInterface::PingResult result) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DoPerformPing(result.connection.value_or(nullptr), result.recheck_delay_ms);
}

void BridgeIceController::DoPerformPing(const cricket::Connection* connection,
                                        std::optional<int> recheck_delay_ms) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  if (connection) {
    agent_.SendPingRequest(connection);
  }

  DoSchedulePingRecheck(recheck_delay_ms);
}

// TODO(crbug.com/1369096): rechecks could be batched to avoid a spike
// of recheck activity every recheck interval. Going further, ping proposals
// could probably also be batched to optimize the JS round trip.
void BridgeIceController::DoSchedulePingRecheck(
    std::optional<int> recheck_delay_ms) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (recheck_delay_ms.has_value()) {
    network_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BridgeIceController::SelectAndPingConnection,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(recheck_delay_ms.value()));
  }
}

void BridgeIceController::OnSortAndSwitchRequest(
    cricket::IceSwitchReason reason) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (!sort_pending_) {
    // To avoid recursion, enqueue a task to sort connections and check if a
    // better connection is available (this may lead to connection state changes
    // that trigger this request again). It is acceptable to perform other tasks
    // in between. In fact, tasks to send pings must be allowed to run as these
    // may affect the result of the sort operation and, consequently, which
    // connection is selected. It is also acceptable to perform a
    // sort-and-switch even if another sort-and-switch occurs right away, eg. in
    // response to a nomination from the peer.
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BridgeIceController::SortAndSwitchToBestConnection,
                       weak_factory_.GetWeakPtr(), reason));
    sort_pending_ = true;
  }
}

void BridgeIceController::SortAndSwitchToBestConnection(
    cricket::IceSwitchReason reason) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (!sort_pending_) {
    return;
  }

  // Any changes after this point will require a re-sort.
  sort_pending_ = false;
  DoSortAndSwitchToBestConnection(reason);
}

void BridgeIceController::OnImmediateSortAndSwitchRequest(
    cricket::IceSwitchReason reason) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DoSortAndSwitchToBestConnection(reason);
}

void BridgeIceController::DoSortAndSwitchToBestConnection(
    cricket::IceSwitchReason reason) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // Make sure the connection states are up-to-date since this affects how they
  // will be sorted.
  agent_.UpdateConnectionStates();

  IceControllerInterface::SwitchResult result =
      native_controller_->SortAndSwitchConnection(reason);
  bool reply_expected = false;
  if (observer_) {
    // Switch proposal without a proposed connection may still be useful to
    // indicate the reason.
    reply_expected =
        result.connection.has_value() && result.connection.value() != nullptr;
    observer_->OnSwitchProposal(
        IceSwitchProposal(reason, result, reply_expected));
  }
  if (!reply_expected) {
    // No reply expected, so continue with the switch right away.
    DoPerformSwitch(reason, result);
    UpdateStateOnConnectionsResorted();
  }
}

bool BridgeIceController::OnImmediateSwitchRequest(
    cricket::IceSwitchReason reason,
    const Connection* selected) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  IceControllerInterface::SwitchResult result =
      native_controller_->ShouldSwitchConnection(reason, selected);
  if (observer_) {
    observer_->OnSwitchProposal(
        IceSwitchProposal(reason, result, /*reply_expected=*/false));
  }
  // No reply expected, so continue with the switch right away.
  DoPerformSwitch(reason, result);
  return result.connection.has_value();
}

void BridgeIceController::OnSwitchProposalAccepted(
    const IceSwitchProposal& proposal) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(proposal.reply_expected())
      << "Can't accept an unsolicited switch proposal";
  DCHECK(proposal.connection().has_value())
      << "Can't accept a switch proposal without a connection";

  const cricket::Connection* connection =
      FindConnection(proposal.connection().value().id());
  if (!connection) {
    RTC_LOG(LS_WARNING)
        << "Received unknown or destroyed connection in switch proposal id="
        << proposal.connection().value().id();
    return;
  }

  std::optional<cricket::IceRecheckEvent> recheck_event = std::nullopt;
  if (proposal.recheck_event().has_value()) {
    recheck_event.emplace(
        ConvertToWebrtcIceSwitchReason(proposal.recheck_event()->reason),
        proposal.recheck_event()->recheck_delay_ms);
  }

  std::vector<const Connection*> connections_to_forget_state_on;
  for (const IceConnection& ice_connection :
       proposal.connections_to_forget_state_on()) {
    const cricket::Connection* conn = FindConnection(ice_connection.id());
    if (conn) {
      connections_to_forget_state_on.push_back(conn);
    } else {
      RTC_LOG(LS_WARNING) << "Received unknown or destroyed connection id="
                          << ice_connection.id() << " in switch proposal";
    }
  }

  DoPerformSwitch(ConvertToWebrtcIceSwitchReason(proposal.reason()), connection,
                  recheck_event, connections_to_forget_state_on);

  UpdateStateOnConnectionsResorted();
}

void BridgeIceController::OnSwitchProposalRejected(
    const IceSwitchProposal& proposal) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(proposal.reply_expected())
      << "Can't reject an unsolicited switch proposal";

  RTC_LOG(LS_INFO) << "Rejected proposal to switch connection";

  // Don't switch, but schedule a recheck if it was proposed.
  std::optional<cricket::IceRecheckEvent> recheck_event = std::nullopt;
  if (proposal.recheck_event().has_value()) {
    recheck_event.emplace(
        ConvertToWebrtcIceSwitchReason(proposal.recheck_event()->reason),
        proposal.recheck_event()->recheck_delay_ms);
  }
  DoScheduleSwitchRecheck(recheck_event);

  // Assume that the proposal occurred after a resort, so perform post-resort
  // tasks anyway.
  UpdateStateOnConnectionsResorted();
}

void BridgeIceController::DoPerformSwitch(
    cricket::IceSwitchReason reason_for_switch,
    const IceControllerInterface::SwitchResult result) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DoPerformSwitch(reason_for_switch, result.connection.value_or(nullptr),
                  result.recheck_event, result.connections_to_forget_state_on);
}

void BridgeIceController::DoPerformSwitch(
    cricket::IceSwitchReason reason_for_switch,
    const cricket::Connection* connection,
    std::optional<cricket::IceRecheckEvent> recheck_event,
    base::span<const cricket::Connection* const>
        connections_to_forget_state_on) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // Perform the switch.
  if (connection) {
    RTC_LOG(LS_INFO) << "Switching selected connection due to: "
                     << IceSwitchReasonToString(reason_for_switch);
    agent_.SwitchSelectedConnection(connection, reason_for_switch);
  }

  // Schedule a recheck.
  DoScheduleSwitchRecheck(recheck_event);

  // Reset any connection state information where necessary.
  agent_.ForgetLearnedStateForConnections(connections_to_forget_state_on);
}

void BridgeIceController::DoScheduleSwitchRecheck(
    std::optional<cricket::IceRecheckEvent> recheck_event) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  if (recheck_event.has_value()) {
    // If we do not switch to the connection because it missed the receiving
    // threshold, the new connection is in a better receiving state than the
    // currently selected connection. So we need to re-check whether it needs
    // to be switched at a later time.
    network_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BridgeIceController::DoSortAndSwitchToBestConnection,
                       weak_factory_.GetWeakPtr(), recheck_event->reason),
        base::Milliseconds(recheck_event->recheck_delay_ms));
  }
}

void BridgeIceController::UpdateStateOnConnectionsResorted() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  PruneConnections();

  // Defer the rest of state update until after the pruning is complete, to
  // allow for pruning to round trip through the observer.
}

void BridgeIceController::PruneConnections() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // The controlled side can prune only if the selected connection has been
  // nominated because otherwise it may prune the connection that will be
  // selected by the controlling side.
  // TODO(honghaiz): This is not enough to prevent a connection from being
  // pruned too early because with aggressive nomination, the controlling side
  // will nominate every connection until it becomes writable.
  if (agent_.GetIceRole() == cricket::ICEROLE_CONTROLLING ||
      (selected_connection_ && selected_connection_->nominated())) {
    std::vector<const Connection*> connections_to_prune =
        native_controller_->PruneConnections();
    if (observer_ && !connections_to_prune.empty()) {
      observer_->OnPruneProposal(
          IcePruneProposal(connections_to_prune, /*reply_expected=*/true));
    } else {
      // No proposal, so continue with the prune right away.
      DoPerformPrune(connections_to_prune);
      UpdateStateOnPrune();
    }
  }
}

void BridgeIceController::UpdateStateOnPrune() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // Update the internal state of the ICE agentl.
  agent_.UpdateState();

  // Also possibly start pinging.
  // We could start pinging if:
  // * The first connection was created.
  // * ICE credentials were provided.
  // * A TCP connection became connected.
  MaybeStartPinging();
}

void BridgeIceController::OnPruneProposalAccepted(
    const IcePruneProposal& proposal) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(proposal.reply_expected())
      << "Can't reject an unsolicited prune proposal";

  std::vector<const cricket::Connection*> connections_to_prune;
  for (const IceConnection& ice_connection : proposal.connections_to_prune()) {
    const cricket::Connection* connection = FindConnection(ice_connection.id());
    if (connection) {
      connections_to_prune.push_back(connection);
    } else {
      RTC_LOG(LS_WARNING) << "Received unknown or destroyed connection id="
                          << ice_connection.id() << " in prune proposal";
    }
  }
  DoPerformPrune(connections_to_prune);

  UpdateStateOnPrune();
}

void BridgeIceController::OnPruneProposalRejected(
    const IcePruneProposal& proposal) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(proposal.reply_expected())
      << "Can't reject an unsolicited prune proposal";

  RTC_LOG(LS_INFO) << "Rejected proposal to prune "
                   << proposal.connections_to_prune().size() << "connections.";

  // Update state regardless in case something changed.
  UpdateStateOnPrune();
}

void BridgeIceController::DoPerformPrune(
    base::span<const cricket::Connection* const> connections) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  agent_.PruneConnections(connections);
}

// Only for unit tests
const Connection* BridgeIceController::FindNextPingableConnection() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  return native_controller_->FindNextPingableConnection();
}

webrtc::RTCError BridgeIceController::OnPingRequested(
    const IceConnection& ice_connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug.com/1369096) add rate-limiting checks.

  const cricket::Connection* connection = FindConnection(ice_connection.id());
  if (!connection) {
    RTC_LOG(LS_WARNING)
        << "Received ping request for unknown or destroyed connection id="
        << ice_connection.id();
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER);
  }

  DoPerformPing(connection, /*recheck_delay_ms=*/std::nullopt);
  return webrtc::RTCError::OK();
}

webrtc::RTCError BridgeIceController::OnSwitchRequested(
    const IceConnection& ice_connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug.com/1369096) should we check with the native controller about
  // this switch, similar to an ImmediateSwitchRequest?

  const cricket::Connection* connection = FindConnection(ice_connection.id());
  if (!connection) {
    RTC_LOG(LS_WARNING)
        << "Received switch request for unknown or destroyed connection id="
        << ice_connection.id();
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER);
  }

  DoPerformSwitch(cricket::IceSwitchReason::APPLICATION_REQUESTED, connection,
                  std::nullopt, base::span<const cricket::Connection* const>());
  return webrtc::RTCError::OK();
}

webrtc::RTCError BridgeIceController::OnPruneRequested(
    base::span<const IceConnection> ice_connections_to_prune) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  std::vector<const cricket::Connection*> connections_to_prune;
  for (const IceConnection& ice_connection : ice_connections_to_prune) {
    const cricket::Connection* connection = FindConnection(ice_connection.id());
    if (connection) {
      connections_to_prune.push_back(connection);
    } else {
      RTC_LOG(LS_WARNING)
          << "Received prune request unknown or destroyed connection id="
          << ice_connection.id();
    }
  }

  if (!connections_to_prune.empty()) {
    DoPerformPrune(connections_to_prune);
    UpdateStateOnPrune();
  }

  return webrtc::RTCError::OK();
}

const cricket::Connection* BridgeIceController::FindConnection(
    uint32_t id) const {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  rtc::ArrayView<const Connection* const> conns =
      native_controller_->GetConnections();
  auto it = absl::c_find_if(
      conns, [id](const Connection* c) { return c->id() == id; });
  if (it != conns.end()) {
    return *it;
  } else {
    return nullptr;
  }
}

BridgeIceController::IceInteractionProxy::IceInteractionProxy(
    BridgeIceController* controller,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : controller_(controller), task_runner_(std::move(task_runner)) {}

void BridgeIceController::IceInteractionProxy::AcceptPingProposal(
    const IcePingProposal& proposal) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  controller_->OnPingProposalAccepted(proposal);
}

void BridgeIceController::IceInteractionProxy::RejectPingProposal(
    const IcePingProposal& proposal) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  controller_->OnPingProposalRejected(proposal);
}

void BridgeIceController::IceInteractionProxy::AcceptSwitchProposal(
    const IceSwitchProposal& proposal) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  controller_->OnSwitchProposalAccepted(proposal);
}

void BridgeIceController::IceInteractionProxy::RejectSwitchProposal(
    const IceSwitchProposal& proposal) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  controller_->OnSwitchProposalRejected(proposal);
}

void BridgeIceController::IceInteractionProxy::AcceptPruneProposal(
    const IcePruneProposal& proposal) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  controller_->OnPruneProposalAccepted(proposal);
}

void BridgeIceController::IceInteractionProxy::RejectPruneProposal(
    const IcePruneProposal& proposal) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  controller_->OnPruneProposalRejected(proposal);
}

webrtc::RTCError BridgeIceController::IceInteractionProxy::PingIceConnection(
    const IceConnection& connection) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return controller_->OnPingRequested(connection);
}

webrtc::RTCError
BridgeIceController::IceInteractionProxy::SwitchToIceConnection(
    const IceConnection& connection) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return controller_->OnSwitchRequested(connection);
}

webrtc::RTCError BridgeIceController::IceInteractionProxy::PruneIceConnections(
    base::span<const IceConnection> connections_to_prune) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return controller_->OnPruneRequested(connections_to_prune);
}

}  // namespace blink
