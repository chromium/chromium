// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
#define UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_

#include "base/feature_list.h"
#include "ui/compositor/compositor_export.h"

namespace switches {

COMPOSITOR_EXPORT extern const char kEnablePixelOutputInTests[];
COMPOSITOR_EXPORT extern const char kUIEnableRGBA4444Textures[];
COMPOSITOR_EXPORT extern const char kUIEnableZeroCopy[];
COMPOSITOR_EXPORT extern const char kUIDisableZeroCopy[];
COMPOSITOR_EXPORT extern const char kUIShowPaintRects[];
COMPOSITOR_EXPORT extern const char kUISlowAnimations[];
COMPOSITOR_EXPORT extern const char kDisableVsyncForTests[];
COMPOSITOR_EXPORT extern const char kUiCompositorMemoryLimitWhenVisibleMB[];

}  // namespace switches

namespace features {

COMPOSITOR_EXPORT extern const base::Feature kEnablePixelCanvasRecording;

}  // namespace features

namespace ui {

bool IsUIZeroCopyEnabled();

bool COMPOSITOR_EXPORT IsPixelCanvasRecordingEnabled();

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_SWITCHES_H_
