// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_PROVIDER_WIN_DCOMP_H_
#define UI_GL_VSYNC_PROVIDER_WIN_DCOMP_H_

#include "ui/gl/direct_composition_support.h"
#include "ui/gl/vsync_provider_win.h"

namespace gl {
// gfx::VSyncProvider implementation that utilizes the compositor clock to
// determine vsync parameters (as opposed to VSyncProviderWin, where parameters
// are calculated via DWM or QueryDisplayConfig)
class GL_EXPORT VSyncProviderWinDComp : public gfx::VSyncProvider {
 public:
  VSyncProviderWinDComp();

  VSyncProviderWinDComp(const VSyncProviderWinDComp&) = delete;
  VSyncProviderWinDComp& operator=(const VSyncProviderWinDComp&) = delete;

  ~VSyncProviderWinDComp() override;

  // gfx::VSyncProvider overrides;
  void GetVSyncParameters(UpdateVSyncCallback callback) override;
  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override;
  bool SupportGetVSyncParametersIfAvailable() const override;
  bool IsHWClock() const override;

 private:
  std::optional<COMPOSITION_FRAME_STATS> prev_frame_stats_;
};

}  // namespace gl

#endif  // UI_GL_VSYNC_PROVIDER_WIN_DCOMP_H_
