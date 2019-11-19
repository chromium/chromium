// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_WINDOW_ANDROID_COMPOSITOR_H_
#define UI_ANDROID_WINDOW_ANDROID_COMPOSITOR_H_

#include <memory>

#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/android/ui_android_export.h"
#include "ui/compositor/compositor_lock.h"

namespace cc {
class Layer;
}

namespace ui {

class ResourceManager;

// Android interface for compositor-related tasks.
class UI_ANDROID_EXPORT WindowAndroidCompositor {
 public:
  virtual ~WindowAndroidCompositor() {}

  virtual void AttachLayerForReadback(scoped_refptr<cc::Layer> layer) = 0;
  virtual void RequestCopyOfOutputOnRootLayer(
      std::unique_ptr<viz::CopyOutputRequest> request) = 0;
  virtual void SetNeedsAnimate() = 0;
  virtual ResourceManager& GetResourceManager() = 0;
  virtual viz::FrameSinkId GetFrameSinkId() = 0;
  virtual void AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) = 0;
  virtual void RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id) = 0;
  virtual bool IsDrawingFirstVisibleFrame() const = 0;
  virtual void SetVSyncPaused(bool paused) = 0;
  virtual void OnUpdateRefreshRate(float refresh_rate) = 0;
  virtual void OnUpdateSupportedRefreshRates(
      const std::vector<float>& supported_refresh_rates) = 0;
};

}  // namespace ui

#endif  // UI_ANDROID_WINDOW_ANDROID_COMPOSITOR_H_
