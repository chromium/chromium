// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/route_edge.h"

#include <tuple>

#include "ipcz/ipcz.h"
#include "ipcz/link_type.h"
#include "ipcz/local_router_link.h"
#include "ipcz/router.h"
#include "ipcz/sequence_number.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

class RouteEdgeTest : public testing::Test {
 public:
  Ref<RouterLink> CreateLink() {
    // This link does not need to be usable, it just needs to be unique. Hence
    // we create and link two throwaway routers. and return one of their links.
    auto a = MakeRefCounted<Router>();
    auto b = MakeRefCounted<Router>();
    auto [a_link, b_blink] =
        LocalRouterLink::CreatePair(LinkType::kCentral, {a, b});
    return a_link;
  }
};

TEST_F(RouteEdgeTest, Stability) {
  RouteEdge edge;

  // Fresh edges are stable.
  EXPECT_TRUE(edge.is_stable());

  // Edges with only a primary link are stable.
  auto link = CreateLink();
  edge.SetPrimaryLink(link);
  EXPECT_TRUE(edge.is_stable());
  edge.ReleasePrimaryLink();

  // Edges with a deferred decaying link are not stable.
  edge.BeginPrimaryLinkDecay();
  EXPECT_FALSE(edge.is_stable());

  // Edges with only a decaying link are not stable. This link will be set to
  // decay immediately due to the deferred decay from above.
  edge.SetPrimaryLink(link);
  EXPECT_FALSE(edge.is_stable());

  // Edges with both a primary and decaying link are still not stable.
  auto new_link = CreateLink();
  edge.SetPrimaryLink(new_link);
  EXPECT_FALSE(edge.is_stable());

  // But once the decaying link is dropped, the edge is stable again.
  edge.ReleaseDecayingLink();
  EXPECT_TRUE(edge.is_stable());
}

TEST_F(RouteEdgeTest, LinkSelection) {
  RouteEdge edge;
  auto first_link = CreateLink();
  auto second_link = CreateLink();

  // With no primary or decaying link, the primary link is the default choice.
  EXPECT_FALSE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(0)));

  // Now with only a primary link, that link is always selected.
  edge.SetPrimaryLink(first_link);
  EXPECT_FALSE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(0)));
  EXPECT_FALSE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(5)));
  EXPECT_FALSE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(10)));

  // With a decaying link but no outgoing sequence length limit, the decaying
  // link is always selected.
  edge.BeginPrimaryLinkDecay();
  edge.SetPrimaryLink(second_link);
  EXPECT_EQ(second_link, edge.primary_link());
  EXPECT_EQ(first_link, edge.decaying_link());
  EXPECT_TRUE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(0)));
  EXPECT_TRUE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(5)));
  EXPECT_TRUE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(10)));

  // Finally, with a limit on the decaying link's sequence length, selection now
  // depends on the specific SequenceNumber being transmitted.
  edge.set_length_to_decaying_link(SequenceNumber(5));
  EXPECT_TRUE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(0)));
  EXPECT_TRUE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(4)));
  EXPECT_FALSE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(5)));
  EXPECT_FALSE(edge.ShouldTransmitOnDecayingLink(SequenceNumber(10)));
}

TEST_F(RouteEdgeTest, FinishDecay) {
  RouteEdge edge;
  auto link = CreateLink();
  edge.SetPrimaryLink(link);
  edge.BeginPrimaryLinkDecay();

  // Decay cannot finish until inbound and outbound sequence length limits are
  // set.
  EXPECT_FALSE(edge.MaybeFinishDecay(SequenceNumber(0), SequenceNumber(0)));
  edge.set_length_to_decaying_link(SequenceNumber(0));
  EXPECT_FALSE(edge.MaybeFinishDecay(SequenceNumber(0), SequenceNumber(0)));
  edge.set_length_from_decaying_link(SequenceNumber(0));
  EXPECT_TRUE(edge.decaying_link());
  EXPECT_TRUE(edge.MaybeFinishDecay(SequenceNumber(0), SequenceNumber(0)));
  EXPECT_FALSE(edge.decaying_link());

  // Decay also cannot finish while the sequence length limits have not yet
  // been met by messages transmitted and received over the decaying link.
  edge.SetPrimaryLink(link);
  edge.BeginPrimaryLinkDecay();
  edge.set_length_to_decaying_link(SequenceNumber(2));
  edge.set_length_from_decaying_link(SequenceNumber(4));
  EXPECT_FALSE(edge.MaybeFinishDecay(SequenceNumber(1), SequenceNumber(3)));
  EXPECT_FALSE(edge.MaybeFinishDecay(SequenceNumber(1), SequenceNumber(4)));
  EXPECT_TRUE(edge.decaying_link());
  EXPECT_TRUE(edge.MaybeFinishDecay(SequenceNumber(2), SequenceNumber(4)));
  EXPECT_FALSE(edge.decaying_link());
}

}  // namespace
}  // namespace ipcz
