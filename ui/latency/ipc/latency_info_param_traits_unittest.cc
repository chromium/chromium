// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/ipc/latency_info_param_traits.h"

#include <stddef.h>
#include <stdint.h>

#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/latency/ipc/latency_info_param_traits_macros.h"

namespace ui {

TEST(LatencyInfoParamTraitsTest, Basic) {
  LatencyInfo latency;
  latency.set_trace_id(5);
  ASSERT_FALSE(latency.terminated());
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT);

  EXPECT_EQ(5, latency.trace_id());
  EXPECT_TRUE(latency.terminated());

  base::Pickle msg;
  IPC::WriteParam(&msg, latency);

  LatencyInfo output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  EXPECT_EQ(latency.trace_id(), output.trace_id());
  EXPECT_EQ(latency.terminated(), output.terminated());
  EXPECT_TRUE(output.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                 nullptr));
}

TEST(LatencyInfoParamTraitsTest, InvalidData) {
  ui::LatencyInfo::LatencyMap components;
  base::Pickle msg;
  IPC::WriteParam(&msg, std::string());
  IPC::WriteParam(&msg, components);
  IPC::WriteParam(&msg, static_cast<int64_t>(1234));
  IPC::WriteParam(&msg, true);

  LatencyInfo output;
  base::PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ReadParam(&msg, &iter, &output));
}

}  // namespace ui
