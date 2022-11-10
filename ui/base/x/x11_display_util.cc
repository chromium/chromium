// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_util.h"

#include <dlfcn.h>

#include <bitset>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/matrix3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

constexpr int kMinVersionXrandr = 103;  // Need at least xrandr version 1.3.

constexpr const char kRandrEdidProperty[] = "EDID";

std::map<x11::RandR::Output, int> GetMonitors(int version,
                                              x11::RandR* randr,
                                              x11::Window window) {
  std::map<x11::RandR::Output, int> output_to_monitor;
  if (version >= 105) {
    if (auto reply = randr->GetMonitors({window}).Sync()) {
      for (size_t monitor = 0; monitor < reply->monitors.size(); monitor++) {
        for (x11::RandR::Output output : reply->monitors[monitor].outputs)
          output_to_monitor[output] = monitor;
      }
    }
  }
  return output_to_monitor;
}

// Sets the work area on a list of displays.  The work area for each display
// must already be initialized to the display bounds.  At most one display out
// of |displays| will be affected.
void ClipWorkArea(std::vector<display::Display>* displays,
                  int64_t primary_display_index,
                  float scale) {
  x11::Window x_root_window = ui::GetX11RootWindow();

  std::vector<int32_t> value;
  if (!GetArrayProperty(x_root_window, x11::GetAtom("_NET_WORKAREA"), &value) ||
      value.size() < 4) {
    return;
  }
  gfx::Rect work_area = gfx::ScaleToEnclosingRect(
      gfx::Rect(value[0], value[1], value[2], value[3]), 1.0f / scale);

  // If the work area entirely contains exactly one display, assume it's meant
  // for that display (and so do nothing).
  if (base::ranges::count_if(*displays, [&](const display::Display& display) {
        return work_area.Contains(display.bounds());
      }) == 1) {
    return;
  }

  // If the work area is entirely contained within exactly one display, assume
  // it's meant for that display and intersect the work area with only that
  // display.
  const auto found =
      base::ranges::find_if(*displays, [&](const display::Display& display) {
        return display.bounds().Contains(work_area);
      });

  // If the work area spans multiple displays, intersect the work area with the
  // primary display, like GTK does.
  display::Display& primary =
      found == displays->end() ? (*displays)[primary_display_index] : *found;

  work_area.Intersect(primary.work_area());
  if (!work_area.IsEmpty())
    primary.set_work_area(work_area);
}

float GetRefreshRateFromXRRModeInfo(
    const std::vector<x11::RandR::ModeInfo>& modes,
    x11::RandR::Mode current_mode_id) {
  for (const auto& mode_info : modes) {
    if (static_cast<x11::RandR::Mode>(mode_info.id) != current_mode_id)
      continue;
    if (!mode_info.htotal || !mode_info.vtotal)
      return 0;

    // Refresh Rate = Pixel Clock / (Horizontal Total * Vertical Total)
    return mode_info.dot_clock /
           static_cast<float>(mode_info.htotal * mode_info.vtotal);
  }
  return 0;
}

int DefaultBitsPerComponent() {
  auto* connection = x11::Connection::Get();
  const x11::VisualType& visual = connection->default_root_visual();

  // The mask fields are only valid for DirectColor and TrueColor classes.
  if (visual.c_class == x11::VisualClass::DirectColor ||
      visual.c_class == x11::VisualClass::TrueColor) {
    // RGB components are packed into fixed size integers for each visual.  The
    // layout of bits in the packing is given by
    // |visual.{red,green,blue}_mask|.  Count the number of bits to get the
    // number of bits per component.
    auto bits = [](auto mask) {
      return std::bitset<sizeof(mask) * 8>{mask}.count();
    };
    size_t red_bits = bits(visual.red_mask);
    size_t green_bits = bits(visual.green_mask);
    size_t blue_bits = bits(visual.blue_mask);
    if (red_bits == green_bits && red_bits == blue_bits)
      return red_bits;
  }

  // Next, try getting the number of colormap entries per subfield.  If it's a
  // power of 2, log2 is a possible guess for the number of bits per component.
  if (base::bits::IsPowerOfTwo(visual.colormap_entries))
    return base::bits::Log2Ceiling(visual.colormap_entries);

  // |bits_per_rgb| can sometimes be unreliable (may be 11 for 30bpp visuals),
  // so only use it as a last resort.
  return visual.bits_per_rgb_value;
}

// Get the EDID data from the |output| and stores to |edid|.
std::vector<uint8_t> GetEDIDProperty(x11::RandR* randr,
                                     x11::RandR::Output output) {
  auto future = randr->GetOutputProperty(x11::RandR::GetOutputPropertyRequest{
      .output = output,
      .property = x11::GetAtom(kRandrEdidProperty),
      .long_length = 128});
  auto response = future.Sync();
  std::vector<uint8_t> edid;
  if (response && response->format == 8 && response->type != x11::Atom::None)
    edid = std::move(response->data);
  return edid;
}

}  // namespace

int GetXrandrVersion() {
  auto impl = []() -> int {
    auto future = x11::Connection::Get()->randr().QueryVersion(
        {x11::RandR::major_version, x11::RandR::minor_version});
    if (auto response = future.Sync())
      return response->major_version * 100 + response->minor_version;
    return 0;
  };
  static int version = impl();
  return version;
}

std::vector<display::Display> GetFallbackDisplayList(float scale) {
  const auto& screen = x11::Connection::Get()->default_screen();
  gfx::Size physical_size(screen.width_in_millimeters,
                          screen.height_in_millimeters);

  int width = screen.width_in_pixels;
  int height = screen.height_in_pixels;
  gfx::Rect bounds_in_pixels(0, 0, width, height);
  display::Display gfx_display(0, bounds_in_pixels);

  if (!display::Display::HasForceDeviceScaleFactor() &&
      display::IsDisplaySizeValid(physical_size)) {
    DCHECK_LE(1.0f, scale);
    gfx_display.SetScaleAndBounds(scale, bounds_in_pixels);
    gfx_display.set_work_area(
        gfx::ScaleToEnclosingRect(bounds_in_pixels, 1.0f / scale));
  } else {
    scale = 1;
  }

  gfx_display.set_color_depth(screen.root_depth);
  gfx_display.set_depth_per_component(DefaultBitsPerComponent());

  std::vector<display::Display> displays{gfx_display};
  ClipWorkArea(&displays, 0, scale);
  return displays;
}

std::vector<display::Display> BuildDisplaysFromXRandRInfo(
    int version,
    float scale,
    int64_t* primary_display_index_out) {
  DCHECK(primary_display_index_out);
  DCHECK_GE(version, kMinVersionXrandr);
  auto* connection = x11::Connection::Get();
  auto& randr = connection->randr();
  auto x_root_window = ui::GetX11RootWindow();
  std::vector<display::Display> displays;
  auto resources = randr.GetScreenResourcesCurrent({x_root_window}).Sync();
  if (!resources) {
    LOG(ERROR) << "XRandR returned no displays; falling back to root window";
    return GetFallbackDisplayList(scale);
  }

  const int depth = connection->default_screen().root_depth;
  const int bits_per_component = DefaultBitsPerComponent();

  std::map<x11::RandR::Output, int> output_to_monitor =
      GetMonitors(version, &randr, x_root_window);
  *primary_display_index_out = 0;
  auto output_primary = randr.GetOutputPrimary({x_root_window}).Sync();
  if (!output_primary)
    return GetFallbackDisplayList(scale);
  x11::RandR::Output primary_display_id = output_primary->output;

  int explicit_primary_display_index = -1;
  int monitor_order_primary_display_index = -1;

  // As per-display scale factor is not supported right now,
  // the X11 root window's scale factor is always used.
  for (size_t i = 0; i < resources->outputs.size(); i++) {
    x11::RandR::Output output_id = resources->outputs[i];
    auto output_info =
        randr.GetOutputInfo({output_id, resources->config_timestamp}).Sync();
    if (!output_info)
      continue;

    if (output_info->connection != x11::RandR::RandRConnection::Connected)
      continue;

    bool is_primary_display = (output_id == primary_display_id);

    if (output_info->crtc == static_cast<x11::RandR::Crtc>(0))
      continue;

    auto crtc =
        randr.GetCrtcInfo({output_info->crtc, resources->config_timestamp})
            .Sync();
    if (!crtc)
      continue;

    display::EdidParser edid_parser(
        GetEDIDProperty(&randr, static_cast<x11::RandR::Output>(output_id)));
    auto output_32 = static_cast<uint32_t>(output_id);
    int64_t display_id =
        output_32 > 0xff ? 0 : edid_parser.GetIndexBasedDisplayId(output_32);
    // It isn't ideal, but if we can't parse the EDID data, fall back on the
    // display number.
    if (!display_id)
      display_id = i;

    gfx::Rect crtc_bounds(crtc->x, crtc->y, crtc->width, crtc->height);
    display::Display display(display_id, crtc_bounds);

    if (!display::Display::HasForceDeviceScaleFactor()) {
      display.SetScaleAndBounds(scale, crtc_bounds);
      display.set_work_area(
          gfx::ScaleToEnclosingRect(crtc_bounds, 1.0f / scale));
    }

    display.set_audio_formats(edid_parser.audio_formats());
    switch (crtc->rotation) {
      case x11::RandR::Rotation::Rotate_0:
        display.set_rotation(display::Display::ROTATE_0);
        break;
      case x11::RandR::Rotation::Rotate_90:
        display.set_rotation(display::Display::ROTATE_90);
        break;
      case x11::RandR::Rotation::Rotate_180:
        display.set_rotation(display::Display::ROTATE_180);
        break;
      case x11::RandR::Rotation::Rotate_270:
        display.set_rotation(display::Display::ROTATE_270);
        break;
      case x11::RandR::Rotation::Reflect_X:
      case x11::RandR::Rotation::Reflect_Y:
        NOTIMPLEMENTED();
    }

    if (is_primary_display)
      explicit_primary_display_index = displays.size();

    const std::string name(output_info->name.begin(), output_info->name.end());
    if (base::StartsWith(name, "eDP") || base::StartsWith(name, "LVDS")) {
      display::SetInternalDisplayIds({display_id});
      // Use localized variant of "Built-in display" for internal displays.
      // This follows the ozone DRM behavior (i.e. ChromeOS).
      display.set_label(l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_INTERNAL));
    } else {
      display.set_label(edid_parser.display_name());
    }

    auto monitor_iter =
        output_to_monitor.find(static_cast<x11::RandR::Output>(output_id));
    if (monitor_iter != output_to_monitor.end() && monitor_iter->second == 0)
      monitor_order_primary_display_index = displays.size();

    if (!display::HasForceDisplayColorProfile()) {
      gfx::ICCProfile icc_profile = ui::GetICCProfileForMonitor(
          monitor_iter == output_to_monitor.end() ? 0 : monitor_iter->second);
      gfx::ColorSpace color_space = icc_profile.GetPrimariesOnlyColorSpace();

      // Most folks do not have an ICC profile set up, but we still want to
      // detect if a display has a wide color gamut so that HDR videos can be
      // enabled.  Only do this if |bits_per_component| > 8 or else SDR
      // screens may have washed out colors.
      if (bits_per_component > 8 && !color_space.IsValid())
        color_space = display::GetColorSpaceFromEdid(edid_parser);

      display.set_color_spaces(
          gfx::DisplayColorSpaces(color_space, gfx::BufferFormat::BGRA_8888));
    }

    display.set_color_depth(depth);
    display.set_depth_per_component(bits_per_component);

    // Set monitor refresh rate
    int refresh_rate = static_cast<int>(
        GetRefreshRateFromXRRModeInfo(resources->modes, crtc->mode));
    display.set_display_frequency(refresh_rate);

    displays.push_back(display);
  }

  if (explicit_primary_display_index != -1)
    *primary_display_index_out = explicit_primary_display_index;
  else if (monitor_order_primary_display_index != -1)
    *primary_display_index_out = monitor_order_primary_display_index;

  if (displays.empty())
    return GetFallbackDisplayList(scale);

  ClipWorkArea(&displays, *primary_display_index_out, scale);
  return displays;
}

base::TimeDelta GetPrimaryDisplayRefreshIntervalFromXrandr() {
  constexpr base::TimeDelta kDefaultInterval = base::Seconds(1. / 60);
  x11::RandR randr = x11::Connection::Get()->randr();
  auto root = ui::GetX11RootWindow();
  auto resources = randr.GetScreenResourcesCurrent({root}).Sync();
  if (!resources)
    return kDefaultInterval;
  // TODO(crbug.com/726842): It might make sense here to pick the output that
  // the window is on. On the other hand, if compositing is enabled, all drawing
  // might be synced to the primary output anyway. Needs investigation.
  auto output_primary = randr.GetOutputPrimary({root}).Sync();
  if (!output_primary)
    return kDefaultInterval;
  x11::RandR::Output primary_output = output_primary->output;
  bool disconnected_primary = false;
  for (size_t i = 0; i < resources->outputs.size(); i++) {
    if (!disconnected_primary && resources->outputs[i] != primary_output)
      continue;

    auto output_info =
        randr.GetOutputInfo({primary_output, resources->config_timestamp})
            .Sync();
    if (!output_info)
      continue;

    if (output_info->connection != x11::RandR::RandRConnection::Connected) {
      // If the primary monitor is disconnected, then start over and choose the
      // first connected monitor instead.
      if (!disconnected_primary) {
        disconnected_primary = true;
        i = -1;
      }
      continue;
    }
    auto crtc =
        randr.GetCrtcInfo({output_info->crtc, resources->config_timestamp})
            .Sync();
    if (!crtc)
      continue;
    float refresh_rate =
        GetRefreshRateFromXRRModeInfo(resources->modes, crtc->mode);
    if (refresh_rate == 0)
      continue;

    return base::Seconds(1. / refresh_rate);
  }
  return kDefaultInterval;
}

}  // namespace ui
