// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/pen_id_handler.h"

#include "base/win/scoped_winrt_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/win/test_support/fake_ipen_device.h"
#include "ui/views/win/test_support/fake_ipen_device_statics.h"

namespace views {

namespace {

constexpr int kPointerId1 = 1111;
constexpr int kPointerId2 = 2222;
constexpr int kPointerId3 = 3333;
constexpr int kPointerId4 = 4444;

}  // namespace

class FakePenIdHandler : public PenIdHandler {
 public:
  FakePenIdHandler(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDeviceStatics>
          pen_device_statics = nullptr) {
    pen_device_statics_ = pen_device_statics;
  }
};

class PenIdHandlerTest : public ::testing::Test {
 public:
  PenIdHandlerTest() = default;
  ~PenIdHandlerTest() override = default;

  // testing::Test overrides.
  void SetUp() override;
  void TearDown() override;

 private:
  base::win::ScopedWinrtInitializer scoped_winrt_initializer_;
};

void PenIdHandlerTest::SetUp() {
  ASSERT_TRUE(scoped_winrt_initializer_.Succeeded());
}

void PenIdHandlerTest::TearDown() {
  FakeIPenDeviceStatics::GetInstance()->SimulateAllPenDevicesRemoved();
}

// Tests TryGetPenUniqueId for devices that have a guid. The unique guid should
// be correctly mapped to a unique pen id, which is the value that is returned
// by TryGetPenUniqueId.
TEST_F(PenIdHandlerTest, GetGuidMapping) {
  Microsoft::WRL::ComPtr<FakeIPenDeviceStatics> pen_device_statics =
      FakeIPenDeviceStatics::GetInstance();
  FakePenIdHandler pen_id_handler(pen_device_statics);

  // Make sure Get GUID works correctly.
  const auto fake_pen_device_1 = Microsoft::WRL::Make<FakeIPenDevice>();
  const auto fake_pen_device_2 = Microsoft::WRL::Make<FakeIPenDevice>();
  const auto fake_pen_device_3 = Microsoft::WRL::Make<FakeIPenDevice>();

  pen_device_statics->SimulatePenEventGenerated(kPointerId1, fake_pen_device_1);
  pen_device_statics->SimulatePenEventGenerated(kPointerId2, fake_pen_device_2);
  pen_device_statics->SimulatePenEventGenerated(kPointerId3, fake_pen_device_3);
  pen_device_statics->SimulatePenEventGenerated(kPointerId4, fake_pen_device_1);

  const absl::optional<int32_t> id =
      pen_id_handler.TryGetPenUniqueId(kPointerId1);

  const absl::optional<int32_t> id2 =
      pen_id_handler.TryGetPenUniqueId(kPointerId2);
  EXPECT_NE(id, id2);

  const absl::optional<int32_t> id3 =
      pen_id_handler.TryGetPenUniqueId(kPointerId3);
  EXPECT_NE(id2, id3);
  EXPECT_NE(id, id3);

  // Different pointer id generated from a previously seen device should return
  // that device's unique id.
  EXPECT_EQ(id, pen_id_handler.TryGetPenUniqueId(kPointerId4));
}

// Simulate statics not being set. This should result in TryGetGuid returning
// absl::nullopt and TryGetTransducerId returning an invalid Transducer ID.
// Ultimately TryGetPenUniqueId should return null.
TEST_F(PenIdHandlerTest, PenDeviceStaticsFailedToSet) {
  FakePenIdHandler pen_id_handler;
  EXPECT_EQ(pen_id_handler.TryGetGuid(kPointerId1), absl::nullopt);
  EXPECT_EQ(pen_id_handler.TryGetPenUniqueId(kPointerId1), absl::nullopt);
}

TEST_F(PenIdHandlerTest, TryGetGuidHandlesBadStatics) {
  // Make sure `TryGetGUID` fails when there is no ID.
  Microsoft::WRL::ComPtr<FakeIPenDeviceStatics> pen_device_statics =
      FakeIPenDeviceStatics::GetInstance();
  FakePenIdHandler pen_id_handler(pen_device_statics);
  EXPECT_EQ(pen_id_handler.TryGetGuid(kPointerId1), absl::nullopt);

  // When there is a GUID, it should be plumbed.
  const auto fake_pen_device = Microsoft::WRL::Make<FakeIPenDevice>();
  pen_device_statics->SimulatePenEventGenerated(kPointerId1, fake_pen_device);
  EXPECT_EQ(pen_id_handler.TryGetGuid(kPointerId1), fake_pen_device->GetGuid());
}

}  // namespace views
