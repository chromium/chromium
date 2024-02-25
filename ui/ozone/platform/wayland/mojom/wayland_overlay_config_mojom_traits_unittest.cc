// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config_traits_test_service.mojom.h"

namespace wl {

namespace {

class WaylandOverlayConfigStructTraitsTest
    : public testing::Test,
      public mojom::ConfigTraitsTestService {
 public:
  WaylandOverlayConfigStructTraitsTest() = default;

  WaylandOverlayConfigStructTraitsTest(
      const WaylandOverlayConfigStructTraitsTest&) = delete;
  WaylandOverlayConfigStructTraitsTest& operator=(
      const WaylandOverlayConfigStructTraitsTest&) = delete;

 protected:
  mojo::Remote<mojom::ConfigTraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::ConfigTraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  // ConfigTraitsTestService:
  void EchoTransform(
      const absl::variant<gfx::OverlayTransform, gfx::Transform>& t,
      EchoTransformCallback callback) override {
    std::move(callback).Run(t);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<ConfigTraitsTestService> traits_test_receivers_;
};

}  // namespace

TEST_F(WaylandOverlayConfigStructTraitsTest, OverlayTransform) {
  const gfx::OverlayTransform t = gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
  absl::variant<gfx::OverlayTransform, gfx::Transform> input(t);
  mojo::Remote<mojom::ConfigTraitsTestService> remote = GetTraitsTestRemote();
  absl::variant<gfx::OverlayTransform, gfx::Transform> output;
  remote->EchoTransform(input, &output);
  EXPECT_TRUE(absl::holds_alternative<gfx::OverlayTransform>(output));
  EXPECT_EQ(t, absl::get<gfx::OverlayTransform>(output));
}

TEST_F(WaylandOverlayConfigStructTraitsTest, MatrixTransform) {
  const gfx::Transform t = gfx::Transform::MakeScale(2, 3);
  absl::variant<gfx::OverlayTransform, gfx::Transform> input(t);
  mojo::Remote<mojom::ConfigTraitsTestService> remote = GetTraitsTestRemote();
  absl::variant<gfx::OverlayTransform, gfx::Transform> output;
  remote->EchoTransform(input, &output);
  EXPECT_TRUE(absl::holds_alternative<gfx::Transform>(output));
  EXPECT_EQ(t, absl::get<gfx::Transform>(output));
}

}  // namespace wl
