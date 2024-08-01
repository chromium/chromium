// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/x/x11_display_util.h"

#include <dlfcn.h>

#include <bit>
#include <bitset>
#include <numeric>
#include <queue>
#include <unordered_set>

#include "base/bits.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/randr.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

// Need at least xrandr version 1.3
constexpr std::pair<uint32_t, uint32_t> kMinVersionXrandr{1, 3};

constexpr const char kRandrEdidProperty[] = "EDID";

std::map<x11::RandR::Output, size_t> GetMonitors(
    const x11::Response<x11::RandR::GetMonitorsReply>& reply) {
  std::map<x11::RandR::Output, size_t> output_to_monitor;
  if (!reply) {
    return output_to_monitor;
  }
  for (size_t monitor = 0; monitor < reply->monitors.size(); monitor++) {
    for (x11::RandR::Output output : reply->monitors[monitor].outputs) {
      output_to_monitor[output] = monitor;
    }
  }
  return output_to_monitor;
}

x11::Future<x11::GetPropertyReply> GetWorkAreaFuture(
    x11::Connection* connection) {
  return connection->GetProperty({
      .window = connection->default_root(),
      .property = connection->GetAtom("_NET_WORKAREA"),
      .long_length = 4,
  });
}

gfx::Rect GetWorkAreaSync(x11::Future<x11::GetPropertyReply> future) {
  auto response = future.Sync();
  if (!response || response->format != 32 || response->value_len != 4) {
    return gfx::Rect();
  }
  const uint32_t* value = response->value->cast_to<uint32_t>();
  return gfx::Rect(value[0], value[1], value[2], value[3]);
}

x11::Future<x11::GetPropertyReply> GetIccProfileFuture(
    x11::Connection* connection,
    size_t monitor) {
  std::string atom_name = monitor == 0
                              ? "_ICC_PROFILE"
                              : base::StringPrintf("_ICC_PROFILE_%zu", monitor);
  auto future = connection->GetProperty({
      .window = connection->default_root(),
      .property = x11::GetAtom(atom_name.c_str()),
      .long_length = std::numeric_limits<uint32_t>::max(),
  });
  future.IgnoreError();
  return future;
}

gfx::ICCProfile GetIccProfileSync(x11::Future<x11::GetPropertyReply> future) {
  auto response = future.Sync();
  if (!response || !response->value_len) {
    return gfx::ICCProfile();
  }
  return gfx::ICCProfile::FromData(response->value->bytes(),
                                   response->value_len * response->format / 8u);
}

x11::Future<x11::RandR::GetOutputPropertyReply> GetEdidFuture(
    x11::Connection* connection,
    x11::RandR::Output output) {
  auto future = connection->randr().GetOutputProperty({
      .output = output,
      .property = x11::GetAtom(kRandrEdidProperty),
      .long_length = 128,
  });
  future.IgnoreError();
  return future;
}

// Sets the work area on a list of displays.  The work area for each display
// must already be initialized to the display bounds.  At most one display out
// of |displays| will be affected.
void ClipWorkArea(std::vector<display::Display>* displays,
                  size_t primary_display_index,
                  const gfx::Rect& net_workarea) {
  if (net_workarea.IsEmpty()) {
    return;
  }

  auto get_work_area = [&](const display::Display& display) {
    float scale = display::Display::HasForceDeviceScaleFactor()
                      ? display::Display::GetForcedDeviceScaleFactor()
                      : display.device_scale_factor();
    return gfx::ScaleToEnclosingRect(net_workarea, 1.0f / scale);
  };

  // If the work area entirely contains exactly one display, assume it's meant
  // for that display (and so do nothing).
  if (base::ranges::count_if(*displays, [&](const display::Display& display) {
        return get_work_area(display).Contains(display.bounds());
      }) == 1) {
    return;
  }

  // If the work area is entirely contained within exactly one display, assume
  // it's meant for that display and intersect the work area with only that
  // display.
  const auto found =
      base::ranges::find_if(*displays, [&](const display::Display& display) {
        return display.bounds().Contains(get_work_area(display));
      });

  // If the work area spans multiple displays, intersect the work area with the
  // primary display, like GTK does.
  display::Display& primary =
      found == displays->end() ? (*displays)[primary_display_index] : *found;

  gfx::Rect work_area = get_work_area(primary);
  work_area.Intersect(primary.work_area());
  if (!work_area.IsEmpty()) {
    primary.set_work_area(work_area);
  }
}

float GetRefreshRateFromXRRModeInfo(
    const std::vector<x11::RandR::ModeInfo>& modes,
    x11::RandR::Mode current_mode_id) {
  for (const auto& mode_info : modes) {
    if (static_cast<x11::RandR::Mode>(mode_info.id) != current_mode_id) {
      continue;
    }
    if (!mode_info.htotal || !mode_info.vtotal) {
      return 0;
    }

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
    if (red_bits == green_bits && red_bits == blue_bits) {
      return red_bits;
    }
  }

  // Next, try getting the number of colormap entries per subfield.  If it's a
  // power of 2, log2 is a possible guess for the number of bits per component.
  if (std::has_single_bit(visual.colormap_entries)) {
    return base::bits::Log2Ceiling(visual.colormap_entries);
  }

  // |bits_per_rgb| can sometimes be unreliable (may be 11 for 30bpp visuals),
  // so only use it as a last resort.
  return visual.bits_per_rgb_value;
}

// Get the EDID data from the `output` and stores to `edid`.
std::vector<uint8_t> GetEdidProperty(
    x11::Response<x11::RandR::GetOutputPropertyReply> response) {
  std::vector<uint8_t> edid;
  if (response && response->format == 8 && response->type != x11::Atom::None) {
    edid = std::move(response->data);
  }
  return edid;
}

float GetDisplayScale(const gfx::Rect& bounds,
                      const display::DisplayConfig& display_config) {
  constexpr auto kMaxDist = std::make_pair(INT_MAX, INT_MAX);
  auto min_dist_scale = std::make_pair(kMaxDist, display_config.primary_scale);
  for (const auto& geometry : display_config.display_geometries) {
    const auto dist_scale = std::make_pair(
        RectDistance(geometry.bounds_px, bounds), geometry.scale);
    min_dist_scale = std::min(min_dist_scale, dist_scale);
  }
  return min_dist_scale.second;
}

gfx::PointF DisplayOriginPxToDip(const display::Display& parent,
                                 const display::Display& child,
                                 const gfx::PointF& parent_origin_dip) {
  const gfx::Rect parent_px = parent.bounds();
  const gfx::Rect child_px = child.bounds();
  const float parent_scale = parent.device_scale_factor();
  const float child_scale = child.device_scale_factor();
  // Given a range [parent_l_px, parent_r_px) with scale factor `parent_scale`
  // and with `parent_l_px` mapping to `parent_l_dip`, and another range
  // [child_l_px, child_r_px) with scale factor `child_scale`, converts
  // `child_l_px` to DIPs in the child's coordinate system.
  auto map_coordinate = [&](int parent_l_px, int parent_r_px, int child_l_px,
                            int child_r_px, float parent_l_dip) {
    const base::ClampedNumeric<int> l = std::max(parent_l_px, child_l_px);
    const base::ClampedNumeric<int> r = std::min(parent_r_px, child_r_px);
    const float mid_px = std::midpoint<float>(float(l), float(r));
    const float mid_dip = (mid_px - parent_l_px) / parent_scale + parent_l_dip;
    return (child_l_px - mid_px) / child_scale + mid_dip;
  };
  const float x = map_coordinate(parent_px.x(), parent_px.right(), child_px.x(),
                                 child_px.right(), parent_origin_dip.x());
  const float y =
      map_coordinate(parent_px.y(), parent_px.bottom(), child_px.y(),
                     child_px.bottom(), parent_origin_dip.y());
  return {x, y};
}

}  // namespace

std::vector<display::Display> GetFallbackDisplayList(
    float scale,
    size_t* primary_display_index_out) {
  auto* connection = x11::Connection::Get();
  const auto& screen = connection->default_screen();
  gfx::Size physical_size(screen.width_in_millimeters,
                          screen.height_in_millimeters);

  int width = screen.width_in_pixels;
  int height = screen.height_in_pixels;
  gfx::Rect bounds_in_pixels(0, 0, width, height);
  display::Display gfx_display(0, bounds_in_pixels);

  if (!display::Display::HasForceDeviceScaleFactor() &&
      display::IsDisplaySizeValid(physical_size)) {
    DCHECK_LE(1.0f, scale);
    gfx_display.set_size_in_pixels(bounds_in_pixels.size());
    gfx_display.SetScale(scale);
    auto bounds_dip = gfx::ScaleToEnclosingRect(bounds_in_pixels, 1.0f / scale);
    gfx_display.set_bounds(bounds_dip);
    gfx_display.set_work_area(bounds_dip);
  } else {
    scale = 1;
  }

  gfx_display.set_color_depth(screen.root_depth);
  gfx_display.set_depth_per_component(DefaultBitsPerComponent());

  std::vector<display::Display> displays{gfx_display};
  *primary_display_index_out = 0;

  ClipWorkArea(&displays, *primary_display_index_out,
               GetWorkAreaSync(GetWorkAreaFuture(connection)));

  return displays;
}

std::vector<display::Display> BuildDisplaysFromXRandRInfo(
    const display::DisplayConfig& display_config,
    size_t* primary_display_index_out) {
  DCHECK(primary_display_index_out);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const float primary_scale = display_config.primary_scale;

  auto* connection = x11::Connection::Get();
  DCHECK(connection->randr_version() >= kMinVersionXrandr);
  auto& randr = connection->randr();
  auto x_root_window = ui::GetX11RootWindow();
  std::vector<display::Display> displays;

  auto resources_future = randr.GetScreenResourcesCurrent({x_root_window});
  auto output_primary_future = randr.GetOutputPrimary({x_root_window});
  x11::Future<x11::RandR::GetMonitorsReply> monitors_future;
  if (connection->randr_version() >= std::pair<uint32_t, uint32_t>{1, 5}) {
    monitors_future = randr.GetMonitors(x_root_window);
  }
  auto work_area_future = GetWorkAreaFuture(connection);
  connection->Flush();

  auto resources = resources_future.Sync();
  if (!resources) {
    LOG(ERROR) << "XRandR returned no displays; falling back to root window";
    return GetFallbackDisplayList(primary_scale, primary_display_index_out);
  }

  const int depth = connection->default_screen().root_depth;
  const int bits_per_component = DefaultBitsPerComponent();

  auto output_primary = output_primary_future.Sync();
  if (!output_primary) {
    return GetFallbackDisplayList(primary_scale, primary_display_index_out);
  }
  x11::RandR::Output primary_display_id = output_primary->output;

  const auto monitors_reply = monitors_future.Sync();
  const auto output_to_monitor = GetMonitors(monitors_reply);
  const size_t n_iccs =
      monitors_reply ? std::max<size_t>(1, monitors_reply->monitors.size()) : 1;

  int explicit_primary_display_index = -1;
  int monitor_order_primary_display_index = -1;

  std::vector<x11::Future<x11::RandR::GetCrtcInfoReply>> crtc_futures{};
  crtc_futures.reserve(resources->crtcs.size());
  for (auto crtc : resources->crtcs) {
    crtc_futures.push_back(
        randr.GetCrtcInfo({crtc, resources->config_timestamp}));
  }
  connection->Flush();

  std::vector<x11::Future<x11::GetPropertyReply>> icc_futures{n_iccs};
  if (!command_line->HasSwitch(switches::kHeadless)) {
    for (size_t monitor = 0; monitor < n_iccs; ++monitor) {
      icc_futures[monitor] = GetIccProfileFuture(connection, monitor);
    }
    connection->Flush();
  }

  std::vector<x11::Future<x11::RandR::GetOutputInfoReply>> output_futures{};
  output_futures.reserve(resources->outputs.size());
  for (auto output : resources->outputs) {
    output_futures.push_back(
        randr.GetOutputInfo({output, resources->config_timestamp}));
  }
  connection->Flush();

  std::vector<x11::Future<x11::RandR::GetOutputPropertyReply>> edid_futures{};
  edid_futures.reserve(resources->outputs.size());
  for (auto output : resources->outputs) {
    edid_futures.push_back(GetEdidFuture(connection, output));
  }
  connection->Flush();

  base::flat_map<x11::RandR::Crtc, x11::RandR::GetCrtcInfoResponse> crtcs;
  for (size_t i = 0; i < resources->crtcs.size(); ++i) {
    crtcs.emplace(resources->crtcs[i], crtc_futures[i].Sync());
  }

  std::vector<gfx::ICCProfile> iccs;
  iccs.reserve(n_iccs);
  for (auto& future : icc_futures) {
    iccs.push_back(GetIccProfileSync(std::move(future)));
  }

  for (size_t i = 0; i < resources->outputs.size(); i++) {
    x11::RandR::Output output_id = resources->outputs[i];
    auto output_info = output_futures[i].Sync();
    if (!output_info) {
      continue;
    }

    if (output_info->connection != x11::RandR::RandRConnection::Connected) {
      continue;
    }

    bool is_primary_display = (output_id == primary_display_id);

    if (output_info->crtc == static_cast<x11::RandR::Crtc>(0)) {
      continue;
    }

    auto crtc_it = crtcs.find(output_info->crtc);
    if (crtc_it == crtcs.end()) {
      continue;
    }
    const auto& crtc = crtc_it->second;
    if (!crtc) {
      continue;
    }

    display::EdidParser edid_parser(GetEdidProperty(edid_futures[i].Sync()));
    auto output_32 = static_cast<uint32_t>(output_id);
    int64_t display_id =
        output_32 > 0xff ? 0 : edid_parser.GetIndexBasedDisplayId(output_32);
    // It isn't ideal, but if we can't parse the EDID data, fall back on the
    // display number.
    if (!display_id) {
      display_id = i;
    }

    gfx::Rect crtc_bounds(crtc->x, crtc->y, crtc->width, crtc->height);
    const size_t display_index = displays.size();
    display::Display& display = displays.emplace_back(display_id, crtc_bounds);
    display.set_native_origin(crtc_bounds.origin());

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

    if (is_primary_display) {
      explicit_primary_display_index = display_index;
    }

    const std::string name(output_info->name.begin(), output_info->name.end());
    auto process_type =
        command_line->GetSwitchValueASCII("type");
    if (base::StartsWith(name, "eDP") || base::StartsWith(name, "LVDS")) {
      display::SetInternalDisplayIds({display_id});
      // For browser process which has access to resource bundle,
      // use localized variant of "Built-in display" for internal displays.
      // This follows the ozone DRM behavior (i.e. ChromeOS).
      if (process_type.empty()) {
        display.set_label(l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_INTERNAL));
      } else {
        display.set_label("Built-in display");
      }
    } else {
      display.set_label(edid_parser.display_name());
    }

    auto monitor_iter =
        output_to_monitor.find(static_cast<x11::RandR::Output>(output_id));
    if (monitor_iter != output_to_monitor.end() && monitor_iter->second == 0) {
      monitor_order_primary_display_index = display_index;
    }

    if (!display::HasForceDisplayColorProfile()) {
      const size_t monitor =
          monitor_iter == output_to_monitor.end() ? 0 : monitor_iter->second;
      const auto& icc_profile = iccs[monitor < iccs.size() ? monitor : 0];
      gfx::ColorSpace color_space = icc_profile.GetPrimariesOnlyColorSpace();

      // Most folks do not have an ICC profile set up, but we still want to
      // detect if a display has a wide color gamut so that HDR videos can be
      // enabled.  Only do this if |bits_per_component| > 8 or else SDR
      // screens may have washed out colors.
      if (bits_per_component > 8 && !color_space.IsValid()) {
        color_space = display::GetColorSpaceFromEdid(edid_parser);
      }

      display.SetColorSpaces(
          gfx::DisplayColorSpaces(color_space, gfx::BufferFormat::BGRA_8888));
    }

    display.set_color_depth(depth);
    display.set_depth_per_component(bits_per_component);

    // Set monitor refresh rate
    float refresh_rate =
        GetRefreshRateFromXRRModeInfo(resources->modes, crtc->mode);
    display.set_display_frequency(refresh_rate);
  }

  if (displays.empty()) {
    return GetFallbackDisplayList(primary_scale, primary_display_index_out);
  }

  if (explicit_primary_display_index != -1) {
    *primary_display_index_out = explicit_primary_display_index;
  } else if (monitor_order_primary_display_index != -1) {
    *primary_display_index_out = monitor_order_primary_display_index;
  } else {
    *primary_display_index_out = 0;
  }

  if (!display::Display::HasForceDeviceScaleFactor()) {
    for (auto& display : displays) {
      display.set_device_scale_factor(
          GetDisplayScale(display.bounds(), display_config));
    }

    ConvertDisplayBoundsToDips(&displays, *primary_display_index_out);
  }

  ClipWorkArea(&displays, *primary_display_index_out,
               GetWorkAreaSync(std::move(work_area_future)));
  return displays;
}

base::TimeDelta GetPrimaryDisplayRefreshIntervalFromXrandr() {
  constexpr base::TimeDelta kDefaultInterval = base::Seconds(1. / 60);

  size_t primary_display_index = 0;
  auto displays = BuildDisplaysFromXRandRInfo(display::DisplayConfig(),
                                              &primary_display_index);
  CHECK_LT(primary_display_index, displays.size());

  // TODO(crbug.com/41321728): It might make sense here to pick the output that
  // the window is on. On the other hand, if compositing is enabled, all drawing
  // might be synced to the primary output anyway. Needs investigation.
  auto frequency = displays[primary_display_index].display_frequency();
  return frequency > 0 ? base::Seconds(1. / frequency) : kDefaultInterval;
}

int RangeDistance(int min1, int max1, int min2, int max2) {
  base::ClampedNumeric<int> l1 = min1;
  base::ClampedNumeric<int> r1 = max1;
  base::ClampedNumeric<int> l2 = min2;
  base::ClampedNumeric<int> r2 = max2;
  return std::max(std::min(l2 - r1, r2 - l1), std::min(l1 - r2, r1 - l2));
}

std::pair<int, int> RectDistance(const gfx::Rect& p, const gfx::Rect& q) {
  const int dx = RangeDistance(p.x(), p.right(), q.x(), q.right());
  const int dy = RangeDistance(p.y(), p.bottom(), q.y(), q.bottom());
  return {std::max(dx, dy), std::min(dx, dy)};
}

void ConvertDisplayBoundsToDips(std::vector<display::Display>* displays,
                                size_t primary_display_index) {
  // Position displays starting with the primary display, which will have it's
  // origin directly converted from pixels to DIPs.
  std::vector<gfx::PointF> origins_dip(displays->size());
  const auto& primary_display = displays->at(primary_display_index);
  origins_dip[primary_display_index] =
      gfx::ScalePoint(gfx::PointF(primary_display.bounds().origin()),
                      1.0f / primary_display.device_scale_factor());

  // Construct a minimum spanning tree of displays using Prim's algorithm.  The
  // root of the tree is the primary display, and every other display will be
  // positioned relative to it's parent display.
  using EdgeDistance = std::tuple<std::pair<int, int>, size_t, size_t>;
  std::priority_queue<EdgeDistance, std::vector<EdgeDistance>, std::greater<>>
      queue;
  std::unordered_set<size_t> fringe;
  for (size_t i = 0; i < displays->size(); i++) {
    fringe.insert(i);
  }
  auto remove_from_fringe = [&](size_t parent) {
    fringe.erase(parent);
    for (size_t child : fringe) {
      const auto dist = RectDistance(displays->at(parent).bounds(),
                                     displays->at(child).bounds());
      queue.emplace(dist, parent, child);
    }
  };
  remove_from_fringe(primary_display_index);
  while (!queue.empty()) {
    auto [_, parent, child] = queue.top();
    queue.pop();
    if (fringe.contains(child)) {
      origins_dip[child] = DisplayOriginPxToDip(
          displays->at(parent), displays->at(child), origins_dip[parent]);
      remove_from_fringe(child);
    }
  }

  // Update the displays with the converted origins.
  for (size_t i = 0; i < displays->size(); i++) {
    auto& display = displays->at(i);
    gfx::SizeF size_dip = gfx::ScaleSize(gfx::SizeF(display.size()),
                                         1.0f / display.device_scale_factor());
    gfx::Rect bounds_dip =
        gfx::ToEnclosingRect(gfx::RectF(origins_dip[i], size_dip));
    display.set_bounds(bounds_dip);
    display.set_work_area(bounds_dip);
  }
}

}  // namespace ui
