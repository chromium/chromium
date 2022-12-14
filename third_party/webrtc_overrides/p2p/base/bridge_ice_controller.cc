// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/bridge_ice_controller.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"

#include "third_party/webrtc/p2p/base/active_ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/connection.h"
#include "third_party/webrtc/p2p/base/ice_agent_interface.h"
#include "third_party/webrtc/p2p/base/ice_controller_factory_interface.h"
#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"
#include "third_party/webrtc/p2p/base/ice_transport_internal.h"
#include "third_party/webrtc/p2p/base/transport_description.h"

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
    IceAgentInterface* ice_agent,
    std::unique_ptr<IceControllerInterface> native_controller)
    : network_task_runner_(std::move(network_task_runner)),
      native_controller_(std::move(native_controller)),
      agent_(*ice_agent),
      weak_factory_(this) {
  DCHECK(ice_agent != nullptr);
}

BridgeIceController::~BridgeIceController() = default;

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
}

void BridgeIceController::OnConnectionPinged(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  native_controller_->MarkConnectionPinged(connection);
}

void BridgeIceController::OnConnectionUpdated(const Connection* connection) {
  RTC_LOG(LS_VERBOSE) << "Connection report for " << connection->ToString();
  // Do nothing. Native ICE controllers have direct access to Connection, so no
  // need to update connection state separately.
}

void BridgeIceController::OnConnectionSwitched(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  selected_connection_ = connection;
  native_controller_->SetSelectedConnection(connection);
}

void BridgeIceController::OnConnectionDestroyed(const Connection* connection) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  native_controller_->OnConnectionDestroyed(connection);
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

void BridgeIceController::SelectAndPingConnection() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  agent_.UpdateConnectionStates();

  IceControllerInterface::PingResult result =
      native_controller_->SelectConnectionToPing(agent_.GetLastPingSentMs());
  HandlePingResult(result);
}

void BridgeIceController::HandlePingResult(
    IceControllerInterface::PingResult result) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  if (result.connection.has_value()) {
    agent_.SendPingRequest(result.connection.value());
  }

  network_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BridgeIceController::SelectAndPingConnection,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(result.recheck_delay_ms));
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
  HandleSwitchResult(reason, result);
  UpdateStateOnConnectionsResorted();
}

bool BridgeIceController::OnImmediateSwitchRequest(
    cricket::IceSwitchReason reason,
    const Connection* selected) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  IceControllerInterface::SwitchResult result =
      native_controller_->ShouldSwitchConnection(reason, selected);
  HandleSwitchResult(reason, result);
  return result.connection.has_value();
}

void BridgeIceController::HandleSwitchResult(
    cricket::IceSwitchReason reason_for_switch,
    IceControllerInterface::SwitchResult result) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (result.connection.has_value()) {
    RTC_LOG(LS_INFO) << "Switching selected connection due to: "
                     << IceSwitchReasonToString(reason_for_switch);
    agent_.SwitchSelectedConnection(result.connection.value(),
                                    reason_for_switch);
  }

  if (result.recheck_event.has_value()) {
    // If we do not switch to the connection because it missed the receiving
    // threshold, the new connection is in a better receiving state than the
    // currently selected connection. So we need to re-check whether it needs
    // to be switched at a later time.
    network_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BridgeIceController::DoSortAndSwitchToBestConnection,
                       weak_factory_.GetWeakPtr(),
                       result.recheck_event->reason),
        base::Milliseconds(result.recheck_event->recheck_delay_ms));
  }

  agent_.ForgetLearnedStateForConnections(
      result.connections_to_forget_state_on);
}

void BridgeIceController::UpdateStateOnConnectionsResorted() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  PruneConnections();

  // Update the internal state of the ICE agentl.
  agent_.UpdateState();

  // Also possibly start pinging.
  // We could start pinging if:
  // * The first connection was created.
  // * ICE credentials were provided.
  // * A TCP connection became connected.
  MaybeStartPinging();
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
    agent_.PruneConnections(connections_to_prune);
  }
}

// Only for unit tests
const Connection* BridgeIceController::FindNextPingableConnection() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  return native_controller_->FindNextPingableConnection();
}

}  // namespace blink
