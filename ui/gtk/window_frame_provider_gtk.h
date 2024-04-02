// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_
#define UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/linux/window_frame_provider.h"

typedef struct _GtkParamSpec GtkParamSpec;
typedef struct _GtkSettings GtkSettings;

namespace gtk {

class WindowFrameProviderGtk : public ui::WindowFrameProvider {
 public:
  WindowFrameProviderGtk(bool solid_frame, bool tiled);

  WindowFrameProviderGtk(const WindowFrameProviderGtk&) = delete;
  WindowFrameProviderGtk& operator=(const WindowFrameProviderGtk&) = delete;

  ~WindowFrameProviderGtk() override;

  // ui::WindowFrameProvider:
  int GetTopCornerRadiusDip() override;
  bool IsTopFrameTranslucent() override;
  gfx::Insets GetFrameThicknessDip() override;
  void PaintWindowFrame(gfx::Canvas* canvas,
                        const gfx::Rect& rect,
                        int top_area_height,
                        bool focused,
                        const gfx::Insets& input_insets) override;

 private:
  // Data and metrics that depend on the scale.
  struct Asset {
    Asset();
    Asset(const Asset&);
    Asset& operator=(const Asset&);
    ~Asset();

    int frame_size_px = 0;

    // These are texture maps that we will sample from to draw the frame.  The
    // corners are drawn directly and the edges are tiled.
    SkBitmap focused_bitmap;
    SkBitmap unfocused_bitmap;
  };

  // Paint the window frame and update any metrics (like the frame thickness)
  // based on it.  Bitmaps and metrics are cached in |assets_|, so this is a
  // no-op if there is a cache entry created earlier.
  Asset& GetOrCreateAsset(float scale);

  int BitmapSizePx(const Asset& asset) const;

  void OnThemeChanged(GtkSettings* settings, GtkParamSpec* param);

  // Input parameters used for drawing.
  const bool solid_frame_;
  const bool tiled_;

  // Scale-independent metric calculated based on the bitmaps.
  std::optional<gfx::Insets> frame_thickness_dip_;
  std::optional<int> top_corner_radius_dip_;
  std::optional<bool> top_frame_is_translucent_;

  // Cached bitmaps and metrics.
  base::flat_map<float, Asset> assets_;

  // These signals invalidate the cache on change.
  ScopedGSignal theme_name_signal_;
  ScopedGSignal prefer_dark_signal_;
};

}  // namespace gtk

#endif  // UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_
