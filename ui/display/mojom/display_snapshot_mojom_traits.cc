// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_snapshot_mojom_traits.h"

#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace mojo {

namespace {

// Returns the index of |mode| in |modes| or not found.
static uint64_t GetModeIndex(
    const display::DisplaySnapshot::DisplayModeList& modes,
    const display::DisplayMode* mode) {
  if (!mode)
    return std::numeric_limits<uint64_t>::max();

  for (size_t i = 0; i < modes.size(); ++i) {
    if (modes[i].get()->size() == mode->size() &&
        modes[i].get()->is_interlaced() == mode->is_interlaced() &&
        modes[i].get()->refresh_rate() == mode->refresh_rate())
      return i;
  }
  NOTREACHED();
  return std::numeric_limits<uint64_t>::max();
}

}  // namespace

// static
std::vector<std::unique_ptr<display::DisplayMode>>
StructTraits<display::mojom::DisplaySnapshotDataView,
             std::unique_ptr<display::DisplaySnapshot>>::
    modes(const std::unique_ptr<display::DisplaySnapshot>& display_snapshot) {
  std::vector<std::unique_ptr<display::DisplayMode>> display_mode_list;

  for (const auto& display_mode : display_snapshot->modes())
    display_mode_list.push_back(display_mode->Clone());

  return display_mode_list;
}

// static
uint64_t StructTraits<display::mojom::DisplaySnapshotDataView,
                      std::unique_ptr<display::DisplaySnapshot>>::
    current_mode_index(
        const std::unique_ptr<display::DisplaySnapshot>& display_snapshot) {
  return GetModeIndex(display_snapshot->modes(),
                      display_snapshot->current_mode());
}

// static
uint64_t StructTraits<display::mojom::DisplaySnapshotDataView,
                      std::unique_ptr<display::DisplaySnapshot>>::
    native_mode_index(
        const std::unique_ptr<display::DisplaySnapshot>& display_snapshot) {
  return GetModeIndex(display_snapshot->modes(),
                      display_snapshot->native_mode());
}

// static
bool StructTraits<display::mojom::DisplaySnapshotDataView,
                  std::unique_ptr<display::DisplaySnapshot>>::
    Read(display::mojom::DisplaySnapshotDataView data,
         std::unique_ptr<display::DisplaySnapshot>* out) {
  gfx::Point origin;
  if (!data.ReadOrigin(&origin))
    return false;

  gfx::Size physical_size;
  if (!data.ReadPhysicalSize(&physical_size))
    return false;

  display::DisplayConnectionType type;
  if (!data.ReadType(&type))
    return false;

  display::PanelOrientation panel_orientation;
  if (!data.ReadPanelOrientation(&panel_orientation))
    return false;

  gfx::ColorSpace color_space;
  if (!data.ReadColorSpace(&color_space))
    return false;

  std::string display_name;
  if (!data.ReadDisplayName(&display_name))
    return false;

  base::FilePath file_path;
  if (!data.ReadSysPath(&file_path))
    return false;

  // There is a type mismatch between vectors containing unique_ptr<T> vs
  // unique_ptr<const T>. We deserialize into a vector of unique_ptr<T>
  // then create a vector of unique_ptr<const T> after.
  std::vector<std::unique_ptr<display::DisplayMode>> non_const_modes;
  if (!data.ReadModes(&non_const_modes))
    return false;
  display::DisplaySnapshot::DisplayModeList modes;
  for (auto& mode : non_const_modes)
    modes.push_back(std::move(mode));

  // Get current_mode pointer from modes array.
  const display::DisplayMode* current_mode = nullptr;
  if (data.has_current_mode()) {
    size_t current_mode_index = data.current_mode_index();
    if (current_mode_index >= modes.size())
      return false;
    current_mode = modes[current_mode_index].get();
  }

  // Get native_mode pointer from modes array.
  const display::DisplayMode* native_mode = nullptr;
  if (data.has_native_mode()) {
    size_t native_mode_index = data.native_mode_index();
    if (native_mode_index >= modes.size())
      return false;
    native_mode = modes[native_mode_index].get();
  }

  std::vector<uint8_t> edid;
  if (!data.ReadEdid(&edid))
    return false;

  gfx::Size maximum_cursor_size;
  if (!data.ReadMaximumCursorSize(&maximum_cursor_size))
    return false;

  *out = std::make_unique<display::DisplaySnapshot>(
      data.display_id(), origin, physical_size, type,
      data.is_aspect_preserving_scaling(), data.has_overscan(),
      data.has_color_correction_matrix(),
      data.color_correction_in_linear_space(), color_space,
      data.bits_per_channel(), display_name, file_path, std::move(modes),
      panel_orientation, std::move(edid), current_mode, native_mode,
      data.product_code(), data.year_of_manufacture(), maximum_cursor_size);
  return true;
}

}  // namespace mojo
