#include "third_party/webrtc_overrides/p2p/base/ice_prune_proposal.h"

#include <string>

#include "third_party/webrtc/api/array_view.h"
#include "third_party/webrtc/p2p/base/connection.h"
#include "third_party/webrtc/rtc_base/strings/string_builder.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_proposal.h"

namespace blink {

IcePruneProposal::IcePruneProposal(
    const webrtc::ArrayView<const webrtc::Connection*> connections_to_prune,
    bool reply_expected)
    : IceProposal(reply_expected) {
  for (const webrtc::Connection* conn : connections_to_prune) {
    if (conn) {
      connections_to_prune_.emplace_back(conn);
    }
  }
}

std::string IcePruneProposal::ToString() const {
  webrtc::StringBuilder ss;
  ss << "PruneProposal[";
  int ctr = 1;
  for (auto conn : connections_to_prune_) {
    ss << "(" << ctr++ << ":" << conn.ToString() << ")";
  }
  ss << "]";
  return ss.Release();
}

}  // namespace blink
