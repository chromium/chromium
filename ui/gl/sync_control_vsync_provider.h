// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_
#define UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_

#include <stdint.h>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/vsync_provider.h"

namespace gl {

// Base class for providers based on extensions like EGL_CHROMIUM_sync_control.
// TODO(344606399): Consider folding the EGL implementation in this class.
class SyncControlVSyncProvider : public gfx::VSyncProvider {
 public:
  SyncControlVSyncProvider();

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
