// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/sync_control_vsync_provider.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vsync_provider.h"

namespace gl {

namespace {

constexpr int32_t kMscRate = 60;
constexpr base::TimeDelta kWindowInterval = base::Seconds(1) / 144;

class FixedIntervalVSyncProvider : public gfx::VSyncProvider {
 public:
  explicit FixedIntervalVSyncProvider(base::TimeDelta interval)
      : interval_(interval) {}

  void GetVSyncParameters(UpdateVSyncCallback callback) override {}
  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override {
    *interval = interval_;
    return true;
  }
  bool SupportGetVSyncParametersIfAvailable() const override { return true; }
  bool IsHWClock() const override { return false; }

 private:
  const base::TimeDelta interval_;
};

// Reports a vblank that just happened and an MSC rate of `kMscRate`, like
// glXGetMscRateOML does for the primary output of the X screen.
class FakeSyncControlVSyncProvider : public SyncControlVSyncProvider {
 public:
  using SyncControlVSyncProvider::SyncControlVSyncProvider;

  bool IsHWClock() const override { return true; }

 protected:
  bool GetSyncValues(int64_t* system_time,
                     int64_t* media_stream_counter,
                     int64_t* swap_buffer_counter) override {
    *system_time = base::TimeTicks::Now().since_origin().InMicroseconds();
    *media_stream_counter = ++msc_;
    *swap_buffer_counter = 0;
    return true;
  }

  bool GetMscRate(int32_t* numerator, int32_t* denominator) override {
    *numerator = kMscRate;
    *denominator = 1;
    return true;
  }

 private:
  int64_t msc_ = 0;
};

base::TimeDelta GetInterval(gfx::VSyncProvider& provider) {
  base::TimeTicks timebase;
  base::TimeDelta interval;
  // Two samples are needed before a computed interval is accepted.
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(provider.GetVSyncParametersIfAvailable(&timebase, &interval));
  }
  return interval;
}

}  // namespace

TEST(SyncControlVSyncProviderTest, UsesMscRate) {
  FakeSyncControlVSyncProvider provider;
  EXPECT_EQ(GetInterval(provider), base::Seconds(1) / kMscRate);
}

// The interval of the output showing the window takes precedence over the
// MSC rate, which may belong to another output (crbug.com/328849227).
TEST(SyncControlVSyncProviderTest, PrefersIntervalProvider) {
  FakeSyncControlVSyncProvider provider(
      std::make_unique<FixedIntervalVSyncProvider>(kWindowInterval));
  EXPECT_EQ(GetInterval(provider), kWindowInterval);
}

}  // namespace gl
