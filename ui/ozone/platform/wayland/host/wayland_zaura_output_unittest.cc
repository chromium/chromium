// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output.h"

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/test/mock_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

using ::testing::Values;
namespace {
class WaylandZAuraOutputTest : public WaylandTest {
 public:
  WaylandZAuraOutputTest() : WaylandTest(TestServerMode::kAsync) {}
  WaylandZAuraOutputTest(const WaylandZAuraOutputTest&) = delete;
  WaylandZAuraOutputTest& operator=(const WaylandZAuraOutputTest&) = delete;
  ~WaylandZAuraOutputTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    // Set default values for the output.
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl::TestOutput* output = server->output();
      output->SetRect({800, 600});
      output->SetScale(1);
      output->Flush();
    });

    output_manager_ = connection_->wayland_output_manager();
    ASSERT_TRUE(output_manager_);
    EXPECT_TRUE(output_manager_->IsOutputReady());

    // Initializing the screen also connects it to the primary output, so it's
    // easier for us to get the associated WaylandOutput object later.
    platform_screen_ = output_manager_->CreateWaylandScreen();
    output_manager_->InitWaylandScreen(platform_screen_.get());
  }

 protected:
  raw_ptr<WaylandOutputManager> output_manager_ = nullptr;
  std::unique_ptr<WaylandScreen> platform_screen_;
};

}  // namespace

TEST_P(WaylandZAuraOutputTest, HandleInsets) {
  WaylandOutput* wayland_output = output_manager_->GetPrimaryOutput();
  ASSERT_TRUE(wayland_output);
  EXPECT_TRUE(wayland_output->IsReady());
  EXPECT_EQ(wayland_output->physical_size(), gfx::Size(800, 600));
  EXPECT_TRUE(wayland_output->insets().IsEmpty());
  EXPECT_TRUE(wayland_output->get_zaura_output());

  const gfx::Insets insets =
      gfx::Rect(800, 600).InsetsFrom(gfx::Rect(10, 10, 500, 400));

  // Simulate server sending updated insets to the client.
  PostToServerAndWait([&insets](wl::TestWaylandServerThread* server) {
    auto* const zaura_output = server->output()->GetAuraOutput()->resource();

    ASSERT_TRUE(zaura_output);
    EXPECT_FALSE(insets.IsEmpty());
    zaura_output_send_insets(zaura_output, insets.top(), insets.left(),
                             insets.bottom(), insets.right());
  });

  // Verify that insets is updated.
  EXPECT_TRUE(wayland_output->IsReady());
  EXPECT_EQ(wayland_output->physical_size(), gfx::Size(800, 600));
  EXPECT_EQ(wayland_output->insets(), insets);
}

TEST_P(WaylandZAuraOutputTest, HandleLogicalTransform) {
  WaylandOutput* wayland_output = output_manager_->GetPrimaryOutput();
  ASSERT_TRUE(wayland_output);
  EXPECT_TRUE(wayland_output->IsReady());
  EXPECT_FALSE(wayland_output->logical_transform());
  EXPECT_TRUE(wayland_output->get_zaura_output());

  // Simulate server sending updated transform offset to the client.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const zaura_output = server->output()->GetAuraOutput()->resource();

    zaura_output_send_logical_transform(zaura_output, WL_OUTPUT_TRANSFORM_270);
  });

  EXPECT_TRUE(wayland_output->IsReady());
  EXPECT_EQ(wayland_output->logical_transform(), WL_OUTPUT_TRANSFORM_270);
}

// Test edge case display ids are converted correctly.
TEST_P(WaylandZAuraOutputTest, DisplayIdConversions) {
  const int64_t kTestIds[] = {
      std::numeric_limits<int64_t>::min(),
      std::numeric_limits<int64_t>::min() + 1,
      static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1,
      std::numeric_limits<int32_t>::min(),
      std::numeric_limits<int32_t>::min() + 1,
      0,
      std::numeric_limits<int32_t>::max() - 1,
      std::numeric_limits<int32_t>::max(),
      static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1,
      std::numeric_limits<int64_t>::max() - 1,
      std::numeric_limits<int64_t>::max()};

  for (int64_t id : kTestIds) {
    uint32_t display_id_hi = static_cast<uint32_t>(id >> 32);
    uint32_t display_id_lo = static_cast<uint32_t>(id);
    WaylandZAuraOutput aura_output;
    WaylandZAuraOutput::OnDisplayId(&aura_output, nullptr, display_id_hi,
                                    display_id_lo);
    EXPECT_EQ(id, aura_output.display_id().value());
  }
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandZAuraOutputTest,
                         Values(wl::ServerConfig{}));

}  // namespace ui
