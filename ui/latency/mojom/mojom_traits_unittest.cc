// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/latency/mojom/latency_info_mojom_traits.h"
#include "ui/latency/mojom/traits_test_service.mojom.h"

namespace ui {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojo::Remote<mojom::TraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::TraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  // TraitsTestService:
  void EchoLatencyInfo(const LatencyInfo& info,
                       EchoLatencyInfoCallback callback) override {
    std::move(callback).Run(info);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, LatencyInfo) {
  LatencyInfo latency;
  latency.set_trace_id(5);
  latency.set_ukm_source_id(10);
  ASSERT_FALSE(latency.terminated());
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT);

  EXPECT_EQ(5, latency.trace_id());
  EXPECT_EQ(10, latency.ukm_source_id());
  EXPECT_TRUE(latency.terminated());

  latency.set_source_event_type(ui::SourceEventType::TOUCH);

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  LatencyInfo output;
  remote->EchoLatencyInfo(latency, &output);

  EXPECT_EQ(latency.trace_id(), output.trace_id());
  EXPECT_EQ(latency.ukm_source_id(), output.ukm_source_id());
  EXPECT_EQ(latency.terminated(), output.terminated());
  EXPECT_EQ(latency.source_event_type(), output.source_event_type());

  EXPECT_TRUE(
      output.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
}

}  // namespace ui
