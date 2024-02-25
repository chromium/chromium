// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_MODE_H_
#define UI_DISPLAY_TYPES_DISPLAY_MODE_H_

#include <memory>
#include <ostream>
#include <string>

#include "ui/display/types/display_types_export.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace display {

namespace mojom {
class DisplayModeDataView;
}  // namespace mojom

// This class represents the basic information for a native mode. Platforms may
// extend this class to add platform specific information about the mode.
class DISPLAY_TYPES_EXPORT DisplayMode {
 public:
  DisplayMode(const gfx::Size& size,
              bool interlaced,
              float refresh_rate,
              int htotal,
              int vtotal,
              int clock);

  DisplayMode(const DisplayMode&) = delete;
  DisplayMode& operator=(const DisplayMode&) = delete;

  ~DisplayMode();
  std::unique_ptr<DisplayMode> Clone() const;

  const gfx::Size& size() const { return size_; }
  bool is_interlaced() const { return is_interlaced_; }
  float refresh_rate() const { return refresh_rate_; }

  bool operator<(const DisplayMode& other) const;
  bool operator>(const DisplayMode& other) const;
  bool operator==(const DisplayMode& other) const;

  // Computes the precise minimum vsync rate using the mode's timing details.
  // The value obtained from the EDID has a loss of precision due to being an
  // integer. The precise rate must correspond to an integer valued vtotal.
  float GetVSyncRateMin(int vsync_rate_min_from_edid) const;

  std::string ToString() const;

 private:
  friend struct mojo::StructTraits<display::mojom::DisplayModeDataView,
                                   std::unique_ptr<DisplayMode>>;

  // Display size of the mode (i.e. hdisplay x vdisplay).
  const gfx::Size size_;
  // Precise refresh rate of the mode in Hz (not necessarily equal to vrefresh).
  const float refresh_rate_;
  // True if the mode is interlaced.
  const bool is_interlaced_;
  // Total horizontal size of the mode.
  const int htotal_;
  // Total vertical size of the mode.
  const int vtotal_;
  // Pixel clock in kHz.
  const int clock_;
};

// Used by gtest to print readable errors.
DISPLAY_TYPES_EXPORT void PrintTo(const DisplayMode& mode, std::ostream* os);

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_MODE_H_
