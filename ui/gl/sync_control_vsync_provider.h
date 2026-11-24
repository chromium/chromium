// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_
#define UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Base class for providers based on extensions like EGL_CHROMIUM_sync_control.
// TODO(344606399): Consider folding the EGL implementation in this class.
class GL_EXPORT SyncControlVSyncProvider : public gfx::VSyncProvider {
 public:
  // If `interval_provider` is non-null, its interval is used instead of
  // GetMscRate(), which may report the rate of the wrong output when the
  // display has several outputs with different refresh rates.
  explicit SyncControlVSyncProvider(
      std::unique_ptr<gfx::VSyncProvider> interval_provider = nullptr);

  SyncControlVSyncProvider(const SyncControlVSyncProvider&) = delete;
  SyncControlVSyncProvider& operator=(const SyncControlVSyncProvider&) = delete;

  ~SyncControlVSyncProvider() override;

  void GetVSyncParameters(UpdateVSyncCallback callback) override;
  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override;
  bool SupportGetVSyncParametersIfAvailable() const override;

  static constexpr bool IsSupported() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    return true;
#else
    return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  }

 protected:
  virtual bool GetSyncValues(int64_t* system_time,
                             int64_t* media_stream_counter,
                             int64_t* swap_buffer_counter) = 0;

  virtual bool GetMscRate(int32_t* numerator, int32_t* denominator) = 0;

 private:
  std::unique_ptr<gfx::VSyncProvider> interval_provider_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::TimeTicks last_timebase_;
  uint64_t last_media_stream_counter_ = 0;
  base::TimeDelta last_good_interval_;
  bool invalid_msc_ = false;

  // A short history of the last few computed intervals.
  // We use this to filter out the noise in the computation resulting
  // from configuration change (monitor reconfiguration, moving windows
  // between monitors, suspend and resume, etc.).
  base::queue<base::TimeDelta> last_computed_intervals_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
};

}  // namespace gl

#endif  // UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_
