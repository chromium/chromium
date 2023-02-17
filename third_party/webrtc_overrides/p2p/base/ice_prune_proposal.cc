#include "third_party/webrtc_overrides/p2p/base/ice_prune_proposal.h"

#include <algorithm>
#include <iterator>

#include "third_party/webrtc/rtc_base/strings/string_builder.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_proposal.h"

namespace blink {

IcePruneProposal::IcePruneProposal(
    const rtc::ArrayView<const cricket::Connection*> connections_to_prune,
    bool reply_expected)
    : IceProposal(reply_expected) {
  for (const cricket::Connection* conn : connections_to_prune) {
    if (conn) {
      connections_to_prune_.emplace_back(conn);
    }
  }
}

std::string IcePruneProposal::ToString() const {
  rtc::StringBuilder ss;
  ss << "PruneProposal[";
  int ctr = 1;
  for (auto conn : connections_to_prune_) {
    ss << "(" << ctr++ << ":" << conn.ToString() << ")";
  }
  ss << "]";
  return ss.Release();
}

}  // namespace blink
