// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_MODE_H_
#define UI_DISPLAY_TYPES_DISPLAY_MODE_H_

#include <iosfwd>
#include <memory>
#include <optional>
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
              const std::optional<float>& vsync_rate_min);

  DisplayMode(const gfx::Size& size, bool interlaced, float refresh_rate);

  DisplayMode(const DisplayMode&) = delete;
  DisplayMode& operator=(const DisplayMode&) = delete;

  ~DisplayMode();
  std::unique_ptr<DisplayMode> Clone() const;
  std::unique_ptr<DisplayMode> CopyWithSize(const gfx::Size& size) const;

  const gfx::Size& size() const { return size_; }
  bool is_interlaced() const { return is_interlaced_; }
  float refresh_rate() const { return refresh_rate_; }
  const std::optional<float>& vsync_rate_min() const { return vsync_rate_min_; }

  bool operator<(const DisplayMode& other) const;
  bool operator>(const DisplayMode& other) const;
  bool operator==(const DisplayMode& other) const;

  std::string ToString() const;
  // Returns a string representation of this mode's properties excluding those
  // which may change during a configuration request. This is convenient for
  // test expectations which only need to verify the non-changing properties.
  std::string ToStringForTest() const;

 private:
  friend struct mojo::StructTraits<display::mojom::DisplayModeDataView,
                                   std::unique_ptr<DisplayMode>>;

  // Display size of the mode (i.e. hdisplay x vdisplay).
  const gfx::Size size_;
  // Precise refresh rate of the mode in Hz (not necessarily equal to vrefresh).
  const float refresh_rate_;
  // True if the mode is interlaced.
  const bool is_interlaced_;
  // Precise minimum vsync rate achievable by this mode in Hz. May be nullopt if
  // display range limits are not specified by the EDID or if this object is
  // being used in a configuration request.
  const std::optional<float> vsync_rate_min_;
};

// Used by gtest to print readable errors.
DISPLAY_TYPES_EXPORT void PrintTo(const DisplayMode& mode, std::ostream* os);

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_MODE_H_
