// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/pen_id_handler.h"

#include "base/win/scoped_winrt_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/win/test_support/fake_ipen_device.h"
#include "ui/views/win/test_support/fake_ipen_device_statics.h"
#include "ui/views/win/test_support/fake_ipen_pointer_point_statics.h"
#include "ui/views/win/test_support/fake_ipointer_point.h"
#include "ui/views/win/test_support/fake_ipointer_point_properties.h"

namespace views {

using ABI::Windows::Devices::Input::IPenDeviceStatics;
using ABI::Windows::UI::Input::IPointerPointStatics;

constexpr int kPenId0 = 0;
constexpr int kPenId1 = 1;
constexpr int kPenId2 = 2;

constexpr int kPointerId1 = 1111;
constexpr int kPointerId2 = 2222;
constexpr int kPointerId3 = 3333;
constexpr int kPointerId4 = 4444;
constexpr int kPointerId5 = 5555;

class FakePenIdHandler : public PenIdHandler {
 public:
  FakePenIdHandler(
      Microsoft::WRL::ComPtr<IPenDeviceStatics> pen_device_statics,
      Microsoft::WRL::ComPtr<IPointerPointStatics> pointer_point_statics) {
    pen_device_statics_ = pen_device_statics;
    pointer_point_statics_ = pointer_point_statics;
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
  FakeIPenPointerPointStatics::GetInstance()->ClearPointerPointsMap();
}
// Tests TryGetPenUniqueId for devices that have a guid. The unique guid should
// be correctly maped to a unique pen id, which is the value that is returned
// by TryGetPenUniqueId.
TEST_F(PenIdHandlerTest, GetGuidMapping) {
  Microsoft::WRL::ComPtr<FakeIPenDeviceStatics> pen_device_statics =
      FakeIPenDeviceStatics::GetInstance();
  FakePenIdHandler pen_id_handler(pen_device_statics, nullptr);

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

// Tests TryGetPenUniqueId for devices that don't have a guid, but do have
// a transducer id. Makes sure the correct TransducerId is returned given a
// pointer id.
TEST_F(PenIdHandlerTest, GetTransducerIdMapping) {
  Microsoft::WRL::ComPtr<FakeIPenPointerPointStatics> pointer_point_statics =
      FakeIPenPointerPointStatics::GetInstance();
  FakePenIdHandler pen_id_handler(nullptr, pointer_point_statics);

  // Make sure Get GUID works correctly.

  const auto p1 = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ false,
      /*has usage error*/ false,
      /*get usage error*/ false,
      /*tsn*/ 100,
      /*tvid*/ 1);
  const auto p2 = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ false,
      /*has usage error*/ false,
      /*get usage error*/ false,
      /*tsn*/ 200,
      /*tvid*/ 1);
  const auto p3 = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ false,
      /*has usage error*/ false,
      /*get usage error*/ false,
      /*tsn*/ 100,
      /*tvid*/ 2);
  const auto p4 = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ false,
      /*has usage error*/ false,
      /*get usage error*/ false,
      /*tsn*/ 100,
      /*tvid*/ 1);

  pointer_point_statics->AddPointerPoint(kPointerId1, p1);
  pointer_point_statics->AddPointerPoint(kPointerId2, p2);
  pointer_point_statics->AddPointerPoint(kPointerId3, p3);
  pointer_point_statics->AddPointerPoint(kPointerId4, p4);

  absl::optional<int32_t> id = pen_id_handler.TryGetPenUniqueId(kPointerId1);
  EXPECT_EQ(id, kPenId0);

  // Different serial number to previous should return a new unique id.
  id = pen_id_handler.TryGetPenUniqueId(kPointerId2);
  EXPECT_EQ(id, kPenId1);

  // Same serial number but different vendor id should result in a different
  // returned unique id.
  id = pen_id_handler.TryGetPenUniqueId(kPointerId3);
  EXPECT_EQ(id, kPenId2);

  // Persisted id should be returned if transducer id is recognized.
  id = pen_id_handler.TryGetPenUniqueId(kPointerId4);
  EXPECT_EQ(id, kPenId0);

  // Unrecognized id should return a null optional.
  id = pen_id_handler.TryGetPenUniqueId(kPointerId5);
  EXPECT_EQ(id, absl::nullopt);
}

// Simulate statics not being set. This should result in TryGetGuid returning
// absl::nullopt and TryGetTransducerId returning an invalid Transducer ID.
// Ultimately TryGetPenUniqueId should return null.
TEST_F(PenIdHandlerTest, PenDeviceStaticsFailedToSet) {
  FakePenIdHandler pen_id_handler(nullptr, nullptr);
  EXPECT_EQ(pen_id_handler.TryGetGuid(kPointerId1), absl::nullopt);
  EXPECT_EQ(pen_id_handler.TryGetTransducerId(kPointerId1),
            PenIdHandler::TransducerId());
  EXPECT_EQ(pen_id_handler.TryGetPenUniqueId(kPointerId1), absl::nullopt);
}

TEST_F(PenIdHandlerTest, TryGetGuidHandlesBadStatics) {
  // Make sure `TryGetGUID` fails when there is no ID.
  Microsoft::WRL::ComPtr<FakeIPenDeviceStatics> pen_device_statics =
      FakeIPenDeviceStatics::GetInstance();
  FakePenIdHandler pen_id_handler(pen_device_statics, nullptr);
  EXPECT_EQ(pen_id_handler.TryGetGuid(kPointerId1), absl::nullopt);

  // When there is a GUID, it should be plumbed.
  const auto fake_pen_device = Microsoft::WRL::Make<FakeIPenDevice>();
  pen_device_statics->SimulatePenEventGenerated(kPointerId1, fake_pen_device);
  EXPECT_EQ(pen_id_handler.TryGetGuid(kPointerId1), fake_pen_device->GetGuid());
}

TEST_F(PenIdHandlerTest, TryGetTransducerIdHandlesErrors) {
  Microsoft::WRL::ComPtr<FakeIPenPointerPointStatics> pointer_point_statics =
      FakeIPenPointerPointStatics::GetInstance();
  FakePenIdHandler pen_id_handler(nullptr, pointer_point_statics);

  // No current point found.
  EXPECT_EQ(pen_id_handler.TryGetTransducerId(kPointerId1),
            PenIdHandler::TransducerId());

  // Current point found but point->GetProperties throws error.
  const auto p = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ true);
  pointer_point_statics->AddPointerPoint(kPointerId1, p);
  EXPECT_EQ(pen_id_handler.TryGetTransducerId(kPointerId1),
            PenIdHandler::TransducerId());

  // has usage throws error.
  const auto p1 = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ false,
      /*has usage error*/ true);
  pointer_point_statics->AddPointerPoint(kPointerId2, p1);
  EXPECT_EQ(pen_id_handler.TryGetTransducerId(kPointerId2),
            PenIdHandler::TransducerId());

  // get usage throws error.
  const auto p2 = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ false,
      /*has usage error*/ false,
      /*get usage error*/ true);
  pointer_point_statics->AddPointerPoint(kPointerId3, p2);
  EXPECT_EQ(pen_id_handler.TryGetTransducerId(kPointerId3),
            PenIdHandler::TransducerId());

  // Entire pipeline works correctly.
  const auto p3 = Microsoft::WRL::Make<FakeIPointerPoint>(
      /*getProperties throw error*/ false,
      /*has usage error*/ false,
      /*get usage error*/ false,
      /*tsn*/ 100,
      /*tvid*/ 200);
  pointer_point_statics->AddPointerPoint(kPointerId4, p3);
  EXPECT_EQ(pen_id_handler.TryGetTransducerId(kPointerId4),
            (PenIdHandler::TransducerId{/*tsn*/ 100, /*tvid*/ 200}));
}

}  // namespace views
