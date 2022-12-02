#include "third_party/webrtc_overrides/p2p/base/ice_prune_proposal.h"

#include <algorithm>
#include <iterator>

#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_proposal.h"

namespace blink {

IcePruneProposal::IcePruneProposal(
    const rtc::ArrayView<const cricket::Connection*> connections_to_prune,
    bool reply_expected)
    : IceProposal(reply_expected) {
  std::transform(
      connections_to_prune.cbegin(), connections_to_prune.cend(),
      std::back_inserter(connections_to_prune_),
      [](const cricket::Connection* conn) { return IceConnection(conn); });
}

}  // namespace blink
