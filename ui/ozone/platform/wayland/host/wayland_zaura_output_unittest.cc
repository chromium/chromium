// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output.h"

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/display/test/test_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {
namespace {

// A Test Screen that uses Display info in WaylandOutputManager.  It has bare
// minimum impl to support the test case for {Set|Get|DisplayForNewWindows().
class WaylandTestScreen : public display::test::TestScreen {
 public:
  explicit WaylandTestScreen(WaylandScreen* wayland_screen)
      : display::test::TestScreen(/*create_display=*/false,
                                  /*register_screen=*/true),
        wayland_screen_(wayland_screen) {}

  const std::vector<display::Display>& GetAllDisplays() const override {
    return wayland_screen_->GetAllDisplays();
  }

 private:
  raw_ptr<WaylandScreen> wayland_screen_;
};

class WaylandZAuraOutputTest : public WaylandTestSimpleWithAuraShell {
 public:
  WaylandZAuraOutputTest() = default;
  WaylandZAuraOutputTest(const WaylandZAuraOutputTest&) = delete;
  WaylandZAuraOutputTest& operator=(const WaylandZAuraOutputTest&) = delete;
  ~WaylandZAuraOutputTest() override = default;

  void SetUp() override {
    WaylandTestSimpleWithAuraShell::SetUp();

    // Set default values for the output.
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl::TestOutput* output = server->output();
      output->SetPhysicalAndLogicalBounds({800, 600});
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

TEST_F(WaylandZAuraOutputTest, HandleInsets) {
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

TEST_F(WaylandZAuraOutputTest, HandleLogicalTransform) {
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
TEST_F(WaylandZAuraOutputTest, DisplayIdConversions) {
  const int64_t kTestIds[] = {
      std::numeric_limits<int64_t>::min(),
      std::numeric_limits<int64_t>::min() + 1,
      static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1,
      std::numeric_limits<int32_t>::min(),
      std::numeric_limits<int32_t>::min() + 1,
      -1,
      0,
      1,
      std::numeric_limits<int32_t>::max() - 1,
      std::numeric_limits<int32_t>::max(),
      static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1,
      std::numeric_limits<int64_t>::max() - 1,
      std::numeric_limits<int64_t>::max()};

  for (int64_t id : kTestIds) {
    auto display_id = ui::wayland::ToWaylandDisplayIdPair(id);
    WaylandZAuraOutput aura_output;
    WaylandZAuraOutput::OnDisplayId(&aura_output, nullptr, display_id.high,
                                    display_id.low);
    EXPECT_EQ(id, aura_output.display_id().value());
  }
}

TEST_F(WaylandZAuraOutputTest, ActiveDisplay) {
  WaylandTestScreen test_screen(output_manager_->wayland_screen());

  wl::TestOutput* primary = nullptr;
  wl::TestOutput* secondary = nullptr;
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    primary = server->output();
    secondary =
        server->CreateAndInitializeOutput(wl::TestOutputMetrics({100, 100}));
  });

  int64_t primary_id = display::kInvalidDisplayId;
  int64_t secondary_id = display::kInvalidDisplayId;
  // Wait so that the client creates xdg/aura outputs.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    primary_id = primary->GetDisplayId();
    secondary_id = secondary->GetDisplayId();
  });

  WaitForAllDisplaysReady();

  auto* platform_screen = output_manager_->wayland_screen();
  DCHECK(platform_screen);
  ASSERT_EQ(2u, platform_screen->GetAllDisplays().size());

  EXPECT_EQ(primary_id, platform_screen->GetAllDisplays()[0].id());
  EXPECT_EQ(secondary_id, platform_screen->GetAllDisplays()[1].id());

  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    secondary->GetAuraOutput()->SendActivated();
  });
  EXPECT_EQ(secondary_id,
            display::Screen::GetScreen()->GetDisplayForNewWindows().id());
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    primary->GetAuraOutput()->SendActivated();
  });
  EXPECT_EQ(primary_id,
            display::Screen::GetScreen()->GetDisplayForNewWindows().id());
}

TEST_F(WaylandZAuraOutputTest, ZAuraOutputIsReady) {
  auto* output_manager = connection_->wayland_output_manager();
  const auto* primary_output = output_manager->GetPrimaryOutput();

  // Create a new output but suppress metrics.
  wl::TestOutput* test_output = nullptr;
  PostToServerAndWait([&test_output](wl::TestWaylandServerThread* server) {
    test_output = server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({0, 0, 800, 600}));
    ASSERT_TRUE(test_output);
    test_output->set_suppress_implicit_flush(true);
  });
  const auto& all_outputs = output_manager->GetAllOutputs();
  ASSERT_EQ(2u, all_outputs.size());

  // Get the newly created WaylandOutput.
  auto pair_it = base::ranges::find_if_not(all_outputs, [&](auto& pair) {
    return pair.first == primary_output->output_id();
  });
  ASSERT_NE(all_outputs.end(), pair_it);

  auto* new_output = pair_it->second.get();
  EXPECT_NE(nullptr, new_output);

  auto* xdg_output = new_output->xdg_output_for_testing();
  EXPECT_NE(nullptr, xdg_output);

  auto* aura_output = new_output->aura_output_for_testing();
  EXPECT_NE(nullptr, aura_output);

  // The output should not be marked ready since metrics and specifically the
  // wl_output.done event has not yet been received.
  EXPECT_FALSE(new_output->IsReady());
  EXPECT_FALSE(xdg_output->IsReady());
  EXPECT_FALSE(aura_output->IsReady());

  // Flush metrics and the output should enter the ready state.
  PostToServerAndWait([&test_output](wl::TestWaylandServerThread* server) {
    test_output->Flush();
  });
  EXPECT_TRUE(new_output->IsReady());
  EXPECT_TRUE(xdg_output->IsReady());
  EXPECT_TRUE(aura_output->IsReady());
}

}  // namespace ui
