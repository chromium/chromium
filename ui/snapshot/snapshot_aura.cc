// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot_aura.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/snapshot/snapshot_async.h"

namespace ui {

static void MakeAsyncCopyRequest(
    Layer* layer,
    const gfx::Rect& source_rect,
    viz::CopyOutputRequest::CopyOutputRequestCallback callback) {
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          std::move(callback));
  request->set_area(source_rect);
  request->set_result_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());
  layer->RequestCopyOfOutput(std::move(request));
}

static void FinishedAsyncCopyRequest(
    std::unique_ptr<aura::WindowTracker> tracker,
    const gfx::Rect& source_rect,
    viz::CopyOutputRequest::CopyOutputRequestCallback callback,
    int retry_count,
    std::unique_ptr<viz::CopyOutputResult> result) {
  static const int kMaxRetries = 5;
  // Retry the copy request if the previous one failed for some reason.
  if (!tracker->windows().empty() && (retry_count < kMaxRetries) &&
      result->IsEmpty()) {
    // Look up window before calling MakeAsyncRequest. Otherwise, due
    // to undefined (favorably right to left) argument evaluation
    // order, the tracker might have been passed and set to NULL
    // before the window is looked up which results in a NULL pointer
    // dereference.
    aura::Window* window = tracker->windows()[0];
    MakeAsyncCopyRequest(
        window->layer(), source_rect,
        base::BindOnce(&FinishedAsyncCopyRequest, std::move(tracker),
                       source_rect, std::move(callback), retry_count + 1));
    return;
  }

  std::move(callback).Run(std::move(result));
}

static void MakeInitialAsyncCopyRequest(
    aura::Window* window,
    const gfx::Rect& source_rect,
    viz::CopyOutputRequest::CopyOutputRequestCallback callback) {
  auto tracker = std::make_unique<aura::WindowTracker>();
  tracker->Add(window);
  MakeAsyncCopyRequest(
      window->layer(), source_rect,
      base::BindOnce(&FinishedAsyncCopyRequest, std::move(tracker), source_rect,
                     std::move(callback), 0));
}

void GrabWindowSnapshotAndScaleAura(aura::Window* window,
                                    const gfx::Rect& source_rect,
                                    const gfx::Size& target_size,
                                    GrabSnapshotImageCallback callback) {
  MakeInitialAsyncCopyRequest(
      window, source_rect,
      base::BindOnce(&SnapshotAsync::ScaleCopyOutputResult, std::move(callback),
                     target_size));
}

void GrabWindowSnapshotAura(aura::Window* window,
                            const gfx::Rect& source_rect,
                            GrabSnapshotImageCallback callback) {
  MakeInitialAsyncCopyRequest(
      window, source_rect,
      base::BindOnce(&SnapshotAsync::RunCallbackWithCopyOutputResult,
                     std::move(callback)));
}

#if !BUILDFLAG(IS_WIN)

void GrabWindowSnapshotAndScale(gfx::NativeWindow window,
                                const gfx::Rect& source_rect,
                                const gfx::Size& target_size,
                                GrabSnapshotImageCallback callback) {
  GrabWindowSnapshotAndScaleAura(window, source_rect, target_size,
                                 std::move(callback));
}

void GrabWindowSnapshot(gfx::NativeWindow window,
                        const gfx::Rect& source_rect,
                        GrabSnapshotImageCallback callback) {
  GrabWindowSnapshotAura(window, source_rect, std::move(callback));
}

void GrabViewSnapshot(gfx::NativeView view,
                      const gfx::Rect& source_rect,
                      GrabSnapshotImageCallback callback) {
  GrabWindowSnapshotAura(view, source_rect, std::move(callback));
}

void GrabLayerSnapshot(ui::Layer* layer,
                       const gfx::Rect& source_rect,
                       GrabSnapshotImageCallback callback) {
  MakeAsyncCopyRequest(
      layer, source_rect,
      base::BindOnce(&SnapshotAsync::RunCallbackWithCopyOutputResult,
                     std::move(callback)));
}

#endif

}  // namespace ui
