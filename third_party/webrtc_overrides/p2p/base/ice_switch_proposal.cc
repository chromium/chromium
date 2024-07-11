#include "third_party/webrtc_overrides/p2p/base/ice_switch_proposal.h"

#include "base/notreached.h"

#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"
#include "third_party/webrtc/rtc_base/strings/string_builder.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_proposal.h"

namespace blink {

IceSwitchReason ConvertFromWebrtcIceSwitchReason(
    cricket::IceSwitchReason reason) {
  switch (reason) {
    case cricket::IceSwitchReason::REMOTE_CANDIDATE_GENERATION_CHANGE:
      return IceSwitchReason::kRemoteCandidateGenerationChange;
    case cricket::IceSwitchReason::NETWORK_PREFERENCE_CHANGE:
      return IceSwitchReason::kNetworkPreferenceChange;
    case cricket::IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE:
      return IceSwitchReason::kNewConnectionFromLocalCandidate;
    case cricket::IceSwitchReason::NEW_CONNECTION_FROM_REMOTE_CANDIDATE:
      return IceSwitchReason::kNewConnectionFromRemoteCandidate;
    case cricket::IceSwitchReason::NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS:
      return IceSwitchReason::kNewConnectionFromUnknownRemoteAddress;
    case cricket::IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE:
      return IceSwitchReason::kNominationOnControlledSide;
    case cricket::IceSwitchReason::DATA_RECEIVED:
      return IceSwitchReason::kDataReceived;
    case cricket::IceSwitchReason::CONNECT_STATE_CHANGE:
      return IceSwitchReason::kConnectStateChange;
    case cricket::IceSwitchReason::SELECTED_CONNECTION_DESTROYED:
      return IceSwitchReason::kSelectedConnectionDestroyed;
    case cricket::IceSwitchReason::ICE_CONTROLLER_RECHECK:
      return IceSwitchReason::kIceControllerRecheck;
    case cricket::IceSwitchReason::APPLICATION_REQUESTED:
      return IceSwitchReason::kApplicationRequested;
    default:
      NOTREACHED_IN_MIGRATION();
      return IceSwitchReason::kUnknown;
  }
}

std::string IceSwitchReasonToString(IceSwitchReason reason) {
  switch (reason) {
    case IceSwitchReason::kUnknown:
      return "Unknown";
    case IceSwitchReason::kRemoteCandidateGenerationChange:
      return "RemoteCandidateGenerationChange";
    case IceSwitchReason::kNetworkPreferenceChange:
      return "NetworkPreferenceChange";
    case IceSwitchReason::kNewConnectionFromLocalCandidate:
      return "NewConnectionFromLocalCandidate";
    case IceSwitchReason::kNewConnectionFromRemoteCandidate:
      return "NewConnectionFromRemoteCandidate";
    case IceSwitchReason::kNewConnectionFromUnknownRemoteAddress:
      return "NewConnectionFromUnknownRemoteAddress";
    case IceSwitchReason::kNominationOnControlledSide:
      return "NominationOnControlledSide";
    case IceSwitchReason::kDataReceived:
      return "DataReceived";
    case IceSwitchReason::kConnectStateChange:
      return "ConnectStateChange";
    case IceSwitchReason::kSelectedConnectionDestroyed:
      return "SelectedConnectionDestroyed";
    case IceSwitchReason::kIceControllerRecheck:
      return "IceControllerRecheck";
    case IceSwitchReason::kApplicationRequested:
      return "ApplicationRequested";
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

cricket::IceSwitchReason ConvertToWebrtcIceSwitchReason(
    IceSwitchReason reason) {
  switch (reason) {
    case IceSwitchReason::kRemoteCandidateGenerationChange:
      return cricket::IceSwitchReason::REMOTE_CANDIDATE_GENERATION_CHANGE;
    case IceSwitchReason::kNetworkPreferenceChange:
      return cricket::IceSwitchReason::NETWORK_PREFERENCE_CHANGE;
    case IceSwitchReason::kNewConnectionFromLocalCandidate:
      return cricket::IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE;
    case IceSwitchReason::kNewConnectionFromRemoteCandidate:
      return cricket::IceSwitchReason::NEW_CONNECTION_FROM_REMOTE_CANDIDATE;
    case IceSwitchReason::kNewConnectionFromUnknownRemoteAddress:
      return cricket::IceSwitchReason::
          NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS;
    case IceSwitchReason::kNominationOnControlledSide:
      return cricket::IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE;
    case IceSwitchReason::kDataReceived:
      return cricket::IceSwitchReason::DATA_RECEIVED;
    case IceSwitchReason::kConnectStateChange:
      return cricket::IceSwitchReason::CONNECT_STATE_CHANGE;
    case IceSwitchReason::kSelectedConnectionDestroyed:
      return cricket::IceSwitchReason::SELECTED_CONNECTION_DESTROYED;
    case IceSwitchReason::kIceControllerRecheck:
      return cricket::IceSwitchReason::ICE_CONTROLLER_RECHECK;
    case IceSwitchReason::kApplicationRequested:
      return cricket::IceSwitchReason::APPLICATION_REQUESTED;
    case IceSwitchReason::kUnknown:
    default:
      NOTREACHED_IN_MIGRATION();
      return cricket::IceSwitchReason::UNKNOWN;
  }
}

IceRecheckEvent::IceRecheckEvent(const cricket::IceRecheckEvent& event)
    : reason(ConvertFromWebrtcIceSwitchReason(event.reason)),
      recheck_delay_ms(event.recheck_delay_ms) {}

IceSwitchProposal::IceSwitchProposal(
    cricket::IceSwitchReason reason,
    const cricket::IceControllerInterface::SwitchResult& switch_result,
    bool reply_expected)
    : IceProposal(reply_expected),
      reason_(ConvertFromWebrtcIceSwitchReason(reason)),
      recheck_event_(switch_result.recheck_event) {
  if (switch_result.connection.value_or(nullptr)) {
    connection_ = IceConnection(switch_result.connection.value());
  } else {
    connection_ = std::nullopt;
  }
  for (const cricket::Connection* conn :
       switch_result.connections_to_forget_state_on) {
    if (conn) {
      connections_to_forget_state_on_.emplace_back(conn);
    }
  }
}

std::string IceSwitchProposal::ToString() const {
  rtc::StringBuilder ss;
  ss << "SwitchProposal[" << IceSwitchReasonToString(reason_) << ":"
     << (connection_.has_value() ? connection_->ToString() : "<no connection>")
     << ":";
  if (recheck_event_.has_value()) {
    ss << IceSwitchReasonToString(recheck_event_->reason) << ":"
       << recheck_event_->recheck_delay_ms;
  } else {
    ss << "<no recheck>";
  }
  ss << ":";
  int ctr = 1;
  for (auto conn : connections_to_forget_state_on_) {
    ss << "(" << ctr++ << ":" << conn.ToString() << ")";
  }
  ss << "]";
  return ss.Release();
}

}  // namespace blink
