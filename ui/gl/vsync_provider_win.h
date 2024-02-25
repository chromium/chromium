// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_PROVIDER_WIN_H_
#define UI_GL_VSYNC_PROVIDER_WIN_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT VSyncProviderWin : public gfx::VSyncProvider {
 public:
  explicit VSyncProviderWin(gfx::AcceleratedWidget window);

  VSyncProviderWin(const VSyncProviderWin&) = delete;
  VSyncProviderWin& operator=(const VSyncProviderWin&) = delete;

  ~VSyncProviderWin() override;

  static void InitializeOneOff();

  // gfx::VSyncProvider overrides;
  void GetVSyncParameters(UpdateVSyncCallback callback) override;
  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override;
  bool SupportGetVSyncParametersIfAvailable() const override;
  bool IsHWClock() const override;

  bool GetVSyncIntervalIfAvailable(base::TimeDelta* interval);

 private:
  gfx::AcceleratedWidget window_;
};

}  // namespace gl

#endif  // UI_GL_VSYNC_PROVIDER_WIN_H_
