// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/android_surface_control_compat.h"

#include <android/data_space.h>

#include "base/android/build_info.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gfx {
namespace {

class SurfaceControlTransactionTest : public testing::Test {
 public:
  SurfaceControlTransactionTest() {
    gfx::SurfaceControl::SetStubImplementationForTesting();
  }

 protected:
  struct CallbackContext {
    CallbackContext(bool* called, bool* destroyed)
        : called(called), destroyed(destroyed) {}
    ~CallbackContext() { *destroyed = true; }
    raw_ptr<bool> called;
    raw_ptr<bool> destroyed;
  };

  SurfaceControl::Transaction::OnCompleteCb CreateOnCompleteCb(
      bool* called,
      bool* destroyed) {
    return base::BindOnce(
        [](std::unique_ptr<CallbackContext> context,
           SurfaceControl::TransactionStats stats) {
          DCHECK(!*context->called);
          *context->called = true;
        },
        std::make_unique<CallbackContext>(called, destroyed));
  }

  SurfaceControl::Transaction::OnCommitCb CreateOnCommitCb(bool* called,
                                                           bool* destroyed) {
    return base::BindOnce(
        [](std::unique_ptr<CallbackContext> context) {
          DCHECK(!*context->called);
          *context->called = true;
        },
        std::make_unique<CallbackContext>(called, destroyed));
  }

  void RunRemainingTasks() {
    base::RunLoop runloop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, runloop.QuitClosure());
    runloop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(SurfaceControlTransactionTest, CallbackCalledAfterApply) {
  bool on_complete_called = false;
  bool on_commit_called = false;
  bool on_commit_destroyed = false;
  bool on_complete_destroyed = false;

  gfx::SurfaceControl::Transaction transaction;
  transaction.SetOnCompleteCb(
      CreateOnCompleteCb(&on_complete_called, &on_complete_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  transaction.SetOnCommitCb(
      CreateOnCommitCb(&on_commit_called, &on_commit_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Nothing should have been called yet.
  EXPECT_FALSE(on_complete_called);
  EXPECT_FALSE(on_commit_called);

  transaction.Apply();
  RunRemainingTasks();

  // After apply callbacks should be called.
  EXPECT_TRUE(on_complete_called);
  EXPECT_TRUE(on_commit_called);

  // As this is Once callback naturally it's context should have been destroyed.
  EXPECT_TRUE(on_complete_destroyed);
  EXPECT_TRUE(on_commit_destroyed);
}

TEST_F(SurfaceControlTransactionTest, CallbackDestroyedWithoutApply) {
  bool on_complete_called = false;
  bool on_commit_called = false;
  bool on_commit_destroyed = false;
  bool on_complete_destroyed = false;

  {
    SurfaceControl::Transaction transaction;
    transaction.SetOnCompleteCb(
        CreateOnCompleteCb(&on_complete_called, &on_complete_destroyed),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    transaction.SetOnCommitCb(
        CreateOnCommitCb(&on_commit_called, &on_commit_destroyed),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Nothing should have been called yet.
    EXPECT_FALSE(on_complete_called);
    EXPECT_FALSE(on_commit_called);
  }

  RunRemainingTasks();

  // Apply wasn't called, but transaction left the scope, so the callback
  // contexts should have been destroyed.
  EXPECT_TRUE(on_complete_destroyed);
  EXPECT_TRUE(on_commit_destroyed);
}

TEST_F(SurfaceControlTransactionTest, CallbackSetupAfterGetTransaction) {
  bool on_complete_called = false;
  bool on_commit_called = false;
  bool on_commit_destroyed = false;
  bool on_complete_destroyed = false;

  gfx::SurfaceControl::Transaction transaction;
  transaction.SetOnCompleteCb(
      CreateOnCompleteCb(&on_complete_called, &on_complete_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  transaction.SetOnCommitCb(
      CreateOnCommitCb(&on_commit_called, &on_commit_destroyed),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Nothing should have been called yet.
  EXPECT_FALSE(on_complete_called);
  EXPECT_FALSE(on_commit_called);

  auto* asurfacetransaction = transaction.GetTransaction();

  // Should be no task to run, but calling this to make sure nothing is
  // scheduled that can call callbacks.
  RunRemainingTasks();

  // And not yet.
  EXPECT_FALSE(on_complete_called);
  EXPECT_FALSE(on_commit_called);

  // This is usually called by framework.
  SurfaceControl::ApplyTransaction(asurfacetransaction);
  RunRemainingTasks();

  // After apply callbacks should be called.
  EXPECT_TRUE(on_complete_called);
  EXPECT_TRUE(on_commit_called);

  // As this is Once callback naturally it's context should have been destroyed.
  EXPECT_TRUE(on_complete_destroyed);
  EXPECT_TRUE(on_commit_destroyed);
}

TEST(SurfaceControl, ColorSpaceToADataSpace) {
  // Invalid color spaces are mapped to sRGB.
  {
    ADataSpace dataspace = ADATASPACE_UNKNOWN;
    float extended_range_brightness_ratio = 0.f;
    EXPECT_TRUE(SurfaceControl::ColorSpaceToADataSpace(
        gfx::ColorSpace(), 1.f, dataspace, extended_range_brightness_ratio));
    EXPECT_EQ(dataspace, ADATASPACE_SRGB);
    EXPECT_EQ(extended_range_brightness_ratio, 1.f);
  }

  // sRGB.
  {
    ADataSpace dataspace = ADATASPACE_UNKNOWN;
    float extended_range_brightness_ratio = 0.f;
    EXPECT_TRUE(SurfaceControl::ColorSpaceToADataSpace(
        gfx::ColorSpace::CreateSRGB(), 1.f, dataspace,
        extended_range_brightness_ratio));
    EXPECT_EQ(dataspace, ADATASPACE_SRGB);
    EXPECT_EQ(extended_range_brightness_ratio, 1.f);
  }

  // Display P3.
  {
    ADataSpace dataspace = ADATASPACE_UNKNOWN;
    float extended_range_brightness_ratio = 0.f;
    EXPECT_TRUE(SurfaceControl::ColorSpaceToADataSpace(
        gfx::ColorSpace::CreateDisplayP3D65(), 1.f, dataspace,
        extended_range_brightness_ratio));
    EXPECT_EQ(dataspace, ADATASPACE_DISPLAY_P3);
    EXPECT_EQ(extended_range_brightness_ratio, 1.f);
  }

  // Before S, only sRGB and P3 are supported.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_S) {
    return;
  }

  // Rec2020 with an sRGB transfer function.
  {
    gfx::ColorSpace rec2020(gfx::ColorSpace::PrimaryID::BT2020,
                            gfx::ColorSpace::TransferID::SRGB);
    ADataSpace dataspace = ADATASPACE_UNKNOWN;
    float extended_range_brightness_ratio = 0.f;
    EXPECT_TRUE(SurfaceControl::ColorSpaceToADataSpace(
        rec2020, 1.f, dataspace, extended_range_brightness_ratio));
    EXPECT_EQ(dataspace, STANDARD_BT2020 | TRANSFER_SRGB | RANGE_FULL);
    EXPECT_EQ(extended_range_brightness_ratio, 1.f);
  }

  // sRGB, but it will come out as extended because there is a >1 desired
  // brightness ratio.
  {
    ADataSpace dataspace = ADATASPACE_UNKNOWN;
    float extended_range_brightness_ratio = 0.f;
    EXPECT_TRUE(SurfaceControl::ColorSpaceToADataSpace(
        gfx::ColorSpace::CreateSRGB(), 4.f, dataspace,
        extended_range_brightness_ratio));
    EXPECT_EQ(dataspace, STANDARD_BT709 | TRANSFER_SRGB | RANGE_EXTENDED);
    EXPECT_EQ(extended_range_brightness_ratio, 1.f);
  }

  // P3, extended by 2x.
  {
    skcms_TransferFunction trfn_srgb_scaled =
        skia::ScaleTransferFunction(SkNamedTransferFnExt::kSRGB, 2.f);
    gfx::ColorSpace p3_scaled(
        gfx::ColorSpace::PrimaryID::P3, gfx::ColorSpace::TransferID::CUSTOM_HDR,
        gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL, nullptr,
        &trfn_srgb_scaled);
    ADataSpace dataspace = ADATASPACE_UNKNOWN;
    float extended_range_brightness_ratio = 0.f;
    EXPECT_TRUE(SurfaceControl::ColorSpaceToADataSpace(
        p3_scaled, 1.f, dataspace, extended_range_brightness_ratio));
    EXPECT_EQ(dataspace, STANDARD_DCI_P3 | TRANSFER_SRGB | RANGE_EXTENDED);
    EXPECT_NEAR(extended_range_brightness_ratio, 2.f, 0.0001f);
  }
}

}  // namespace
}  // namespace gfx
