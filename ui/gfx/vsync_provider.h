// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_VSYNC_PROVIDER_H_
#define UI_GFX_VSYNC_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class GFX_EXPORT VSyncProvider {
 public:
  virtual ~VSyncProvider() {}

  typedef base::OnceCallback<void(const base::TimeTicks timebase,
                                  const base::TimeDelta interval)>
      UpdateVSyncCallback;

  // Get the time of the most recent screen refresh, along with the time
  // between consecutive refreshes. The callback is called as soon as
  // the data is available: it could be immediately from this method,
  // later via a PostTask to the current MessageLoop, or never (if we have
  // no data source). We provide the strong guarantee that the callback will
  // not be called once the instance of this class is destroyed.
  virtual void GetVSyncParameters(UpdateVSyncCallback callback) = 0;

  // Similar to GetVSyncParameters(). It returns true, if the data is available.
  // Otherwise false is returned.
  virtual bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                             base::TimeDelta* interval) = 0;

  // Returns true, if GetVSyncParametersIfAvailable is supported.
  virtual bool SupportGetVSyncParametersIfAvailable() const = 0;

  // Returns true, if VSyncProvider gets VSync timebase from HW.
  virtual bool IsHWClock() const = 0;
};

// Provides a constant timebase and interval.
class GFX_EXPORT FixedVSyncProvider : public VSyncProvider {
 public:
  FixedVSyncProvider(base::TimeTicks timebase, base::TimeDelta interval)
    : timebase_(timebase), interval_(interval) {
  }

  ~FixedVSyncProvider() override {}

  void GetVSyncParameters(UpdateVSyncCallback callback) override;
  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override;
  bool SupportGetVSyncParametersIfAvailable() const override;
  bool IsHWClock() const override;

 private:
  base::TimeTicks timebase_;
  base::TimeDelta interval_;
};

}  // namespace gfx

#endif  // UI_GFX_VSYNC_PROVIDER_H_
