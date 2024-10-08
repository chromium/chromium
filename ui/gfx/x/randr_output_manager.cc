// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/randr_output_manager.h"

#include <stdint.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11_crtc_resizer.h"

// Xrandr has a number of restrictions that make exact resize more complex:
//
//   1. It's not possible to change the resolution of an existing mode. Instead,
//      the mode must be deleted and recreated.
//   2. It's not possible to delete a mode that's in use.
//   3. Errors are communicated via Xlib's spectacularly unhelpful mechanism
//      of terminating the process unless you install an error handler.
//   4. The root window size must always enclose any enabled Outputs (that is,
//      any output which is attached to a CRTC).
//   5. An Output cannot be given properties (xy-offsets, mode) which would
//      extend its rectangle beyond the root window size.
//
// Since we want the current mode name to be consistent (for each Output), the
// approach is as follows:
//
//   1. Fetch information about all the active (enabled) CRTCs.
//   2. Disable the RANDR Output being resized.
//   3. Delete the CRD mode, if it exists.
//   4. Create the CRD mode at the new resolution, and add it to the Output's
//      list of modes.
//   5. Adjust the properties (in memory) of any CRTCs to be modified:
//      * Width/height (mode) of the CRTC being resized.
//      * xy-offsets to avoid overlapping CRTCs.
//   6. Disable any CRTCs that might prevent changing the root window size.
//   7. Compute the bounding rectangle of all CRTCs (after adjustment), and set
//      it as the new root window size.
//   8. Apply all adjusted CRTC properties to the CRTCs. This will set the
//      Output being resized to the new CRD mode (which re-enables it), and it
//      will re-enable any other CRTCs that were disabled.

namespace {

constexpr auto kInvalidMode = static_cast<x11::RandR::Mode>(0);
constexpr auto kDisabledCrtc = static_cast<x11::RandR::Crtc>(0);
constexpr int kDefaultScreenDpi = 96;
constexpr double kMillimetersPerInch = 25.4;

int CalculateDpi(uint16_t length_in_pixels, uint32_t length_in_mm) {
  if (length_in_mm == 0) {
    return kDefaultScreenDpi;
  }
  double pixels_per_mm = static_cast<double>(length_in_pixels) / length_in_mm;
  double pixels_per_inch = pixels_per_mm * kMillimetersPerInch;
  return base::ClampRound(pixels_per_inch);
}

gfx::Vector2d GetMonitorDpi(const x11::RandR::MonitorInfo& monitor) {
  return gfx::Vector2d(
      CalculateDpi(monitor.width, monitor.width_in_millimeters),
      CalculateDpi(monitor.height, monitor.height_in_millimeters));
}

x11::RandRMonitorConfig ToRandRMonitorConfig(
    const x11::RandR::MonitorInfo& monitor) {
  return x11::RandRMonitorConfig(
      static_cast<intptr_t>(monitor.name),
      gfx::Rect(monitor.x, monitor.y, monitor.width, monitor.height),
      GetMonitorDpi(monitor));
}

// Gets current layout with context information from a list of monitors.
std::vector<x11::RandRMonitorConfigWithContext> GetLayoutWithContext(
    std::vector<x11::RandR::MonitorInfo>& monitors) {
  std::vector<x11::RandRMonitorConfigWithContext> current_displays;
  for (auto& monitor : monitors) {
    // This implementation only supports resizing synthesized Monitors which
    // automatically track their Outputs.
    // TODO(crbug.com/40225767): Maybe support resizing manually-created
    // monitors?
    if (monitor.automatic) {
      current_displays.emplace_back(ToRandRMonitorConfig(monitor), &monitor);
    }
  }
  return current_displays;
}

x11::RandR::Output GetOutputFromContext(void* context) {
  return reinterpret_cast<x11::RandR::MonitorInfo*>(context)->outputs[0];
}

int PixelsToMillimeters(int pixels, int dpi) {
  DCHECK(dpi != 0);

  // (pixels / dpi) is the length in inches. Multiplying by
  // kMillimetersPerInch converts to mm. Multiplication is done first to
  // avoid integer division.
  return static_cast<int>(kMillimetersPerInch * pixels / dpi);
}

// Returns a physical size in mm that will work well with GNOME's
// automatic scale-selection algorithm.
gfx::Size CalculateSizeInMmForGnome(const gfx::Size& dimensions,
                                    const gfx::Vector2d& dpi) {
  int width_mm = PixelsToMillimeters(dimensions.width(), dpi.x());
  int height_mm = PixelsToMillimeters(dimensions.height(), dpi.y());

  // GNOME will, by default, choose an automatic scaling-factor based on the
  // monitor's physical size (mm) and resolution (pixels). Some versions of
  // GNOME have a problem when the computed DPI is close to 192. GNOME
  // calculates the DPI using:
  // dpi = size_pixels / (size_mm / 25.4)
  // This is the reverse of PixelsToMillimeters() which should result in
  // the same values as resolution.dpi() except for any floating-point
  // truncation errors. GNOME will choose 2x scaling only if both the width and
  // height DPIs are strictly greater than 192. The problem is that a user might
  // connect from a 192dpi device and then GNOME's choice of scaling is randomly
  // subject to rounding errors. If the calculation worked out at exactly
  // 192dpi, the inequality test would fail and GNOME would choose 1x scaling.
  // To address this, width_mm/height_mm are decreased slightly (increasing the
  // calculated DPI) to favor 2x over 1x scaling for 192dpi devices.
  width_mm--;
  height_mm--;

  // GNOME treats some pairs of width/height values as untrustworthy and will
  // always choose 1x scaling for them. These values come from
  // meta_monitor_has_aspect_as_size() in
  // https://gitlab.gnome.org/GNOME/mutter/-/blob/main/src/backends/meta-monitor-manager.c
  constexpr std::pair<int, int> kBadSizes[] = {
      {16, 9}, {16, 10}, {160, 90}, {160, 100}, {1600, 900}, {1600, 1000}};
  if (base::Contains(kBadSizes, std::pair(width_mm, height_mm))) {
    width_mm--;
  }
  return {width_mm, height_mm};
}

x11::Atom GetX11Atom(x11::Connection* connection, const std::string& name) {
  auto reply = connection->InternAtom({false, name}).Sync();
  if (!reply) {
    LOG(ERROR) << "Failed to intern atom " << name;
    return x11::Atom::None;
  }
  return reply->atom;
}

void SetOutputPhysicalSizeInMM(x11::Connection* connection,
                               x11::RandR::Output output,
                               int width,
                               int height) {
  static const x11::Atom width_mm_atom = GetX11Atom(connection, "WIDTH_MM");
  static const x11::Atom height_mm_atom = GetX11Atom(connection, "HEIGHT_MM");
  if (width_mm_atom == x11::Atom::None || height_mm_atom == x11::Atom::None) {
    return;
  }

  auto width_32 = static_cast<uint32_t>(width);
  auto height_32 = static_cast<uint32_t>(height);

  x11::RandR::ChangeOutputPropertyRequest request = {
      .output = output,
      .property = width_mm_atom,
      .type = x11::Atom::INTEGER,
      .format = 32,
      .mode = x11::PropMode::Replace,
      .num_units = 1,
      .data = base::MakeRefCounted<base::RefCountedStaticMemory>(
          base::U32ToNativeEndian(width_32)),
  };
  connection->randr().ChangeOutputProperty(request).Sync();

  request.property = height_mm_atom;
  request.data = base::MakeRefCounted<base::RefCountedStaticMemory>(
      base::U32ToNativeEndian(height_32));
  connection->randr().ChangeOutputProperty(request).Sync();
}

}  // namespace

namespace x11 {

DisplayLayoutDiff::DisplayLayoutDiff() = default;
DisplayLayoutDiff::DisplayLayoutDiff(
    x11::RandRMonitorLayout new_displays,
    std::vector<RandRMonitorConfigWithContext> updated_displays,
    std::vector<RandRMonitorConfigWithContext> removed_displays)
    : new_displays(new_displays),
      updated_displays(updated_displays),
      removed_displays(removed_displays) {}
DisplayLayoutDiff::~DisplayLayoutDiff() = default;
DisplayLayoutDiff::DisplayLayoutDiff(const DisplayLayoutDiff&) = default;

DisplayLayoutDiff CalculateDisplayLayoutDiff(
    const std::vector<RandRMonitorConfigWithContext>& current_displays,
    const x11::RandRMonitorLayout& new_layout) {
  DisplayLayoutDiff diff;

  // A list where the index is the index of |current_displays| and the value
  // denotes whether the display is found in the new layout. Used to detect
  // deletion of displays.
  std::vector<bool> current_display_found(current_displays.size(), false);

  for (const x11::RandRMonitorConfig& new_config : new_layout.configs) {
    if (!new_config.id().has_value()) {
      diff.new_displays.configs.push_back(new_config);
      continue;
    }
    auto current_display_it = base::ranges::find(
        current_displays, new_config.id(),
        [](const auto& display) { return display.config.id(); });
    if (current_display_it == current_displays.end()) {
      LOG(ERROR) << "Ignoring unknown screen_id " << *new_config.id();
      continue;
    }
    current_display_found[current_display_it - current_displays.begin()] = true;
    if (new_config.rect() != current_display_it->config.rect() ||
        new_config.dpi() != current_display_it->config.dpi()) {
      VLOG(1) << "Video layout for screen id " << *new_config.id()
              << " has been changed.";
      diff.updated_displays.emplace_back(new_config,
                                         current_display_it->context);
    } else {
      VLOG(1) << "Video layout for screen id " << *new_config.id()
              << " has not been changed.";
    }
  }

  for (size_t i = 0u; i < current_display_found.size(); i++) {
    if (!current_display_found[i]) {
      diff.removed_displays.push_back(current_displays[i]);
    }
  }
  return diff;
}

ScreenResources::ScreenResources() = default;

ScreenResources::~ScreenResources() = default;

bool ScreenResources::Refresh(x11::RandR* randr, x11::Window window) {
  resources_ = nullptr;
  if (auto response = randr->GetScreenResourcesCurrent({window}).Sync()) {
    resources_ = std::move(response.reply);
  }
  return resources_ != nullptr;
}

x11::RandR::Mode ScreenResources::GetIdForMode(const std::string& name) {
  CHECK(resources_);
  base::span<const char> names =
      base::as_chars(base::span<uint8_t>(resources_->names));
  uint16_t idx = 0;
  for (const auto& mode_info : resources_->modes) {
    std::string mode_name(names.subspan(idx, mode_info.name_len).data(),
                          mode_info.name_len);
    idx += mode_info.name_len;
    if (name == mode_name) {
      return static_cast<x11::RandR::Mode>(mode_info.id);
    }
  }
  return kInvalidMode;
}

x11::RandR::GetScreenResourcesCurrentReply* ScreenResources::get() {
  return resources_.get();
}

RandRMonitorConfig::RandRMonitorConfig(std::optional<ScreenId> id,
                                       gfx::Rect rect,
                                       gfx::Vector2d dpi)
    : id_(id), rect_(rect), dpi_(dpi) {}
bool RandRMonitorConfig::operator==(const RandRMonitorConfig& rhs) const =
    default;
RandRMonitorConfig::RandRMonitorConfig(const RandRMonitorConfig& other) =
    default;
RandRMonitorConfig& RandRMonitorConfig::operator=(
    const RandRMonitorConfig& other) = default;

RandRMonitorLayout::RandRMonitorLayout() = default;
RandRMonitorLayout::RandRMonitorLayout(const RandRMonitorLayout&) = default;
RandRMonitorLayout::RandRMonitorLayout(std::vector<RandRMonitorConfig> configs)
    : configs(std::move(configs)) {}
RandRMonitorLayout& RandRMonitorLayout::operator=(const RandRMonitorLayout&) =
    default;
RandRMonitorLayout::~RandRMonitorLayout() = default;
bool RandRMonitorLayout::operator==(const RandRMonitorLayout& rhs) const =
    default;

RandROutputManager::RandROutputManager(std::string output_name_prefix,
                                       uint32_t default_mode_dot_clock)
    : connection_(x11::Connection::Get()),
      randr_(&connection_->randr()),
      root_(connection_->default_screen().root),
      output_name_prefix_(output_name_prefix),
      default_mode_dot_clock_(default_mode_dot_clock) {
  has_randr_ = randr_->present();
}
RandROutputManager::~RandROutputManager() = default;

bool RandROutputManager::TryGetCurrentMonitors(
    std::vector<x11::RandR::MonitorInfo>& list) {
  if (!has_randr_) {
    return false;
  }

  if (!resources_.Refresh(randr_, root_)) {
    return false;
  }

  auto reply = randr_->GetMonitors({root_}).Sync();
  if (!reply) {
    return false;
  }
  std::copy(reply->monitors.begin(), reply->monitors.end(),
            std::back_inserter(list));
  return true;
}

RandRMonitorLayout RandROutputManager::GetLayout() {
  RandRMonitorLayout result;
  std::vector<x11::RandR::MonitorInfo> monitors;
  if (!TryGetCurrentMonitors(monitors)) {
    return RandRMonitorLayout();
  }
  for (const auto& config_context : GetLayoutWithContext(monitors)) {
    result.configs.emplace_back(config_context.config);
  }
  return result;
}

void RandROutputManager::SetLayout(const RandRMonitorLayout& layout) {
  if (!has_randr_) {
    return;
  }
  // Grab the X server while we're changing the display resolution. This ensures
  // that the display configuration doesn't change under our feet.
  ScopedXGrabServer grabber(connection_);

  std::vector<x11::RandR::MonitorInfo> monitors;
  if (!TryGetCurrentMonitors(monitors)) {
    return;
  }
  std::vector<RandRMonitorConfigWithContext> current_displays =
      GetLayoutWithContext(monitors);

  // TODO(yuweih): Verify that the layout is valid, e.g. no overlaps or gaps
  // between displays.
  DisplayLayoutDiff diff = CalculateDisplayLayoutDiff(current_displays, layout);

  X11CrtcResizer resizer(resources_.get(), connection_);
  resizer.FetchActiveCrtcs();

  const std::vector<RandRMonitorConfig>& new_layouts =
      diff.new_displays.configs;
  // Add displays
  if (!new_layouts.empty()) {
    auto outputs = GetDisabledOutputs();
    size_t i = 0u;
    for (; i < outputs.size() && i < new_layouts.size(); i++) {
      auto& output_pair = outputs[i];
      auto output = output_pair.first;
      auto& output_info = output_pair.second;
      // For the video-dummy driver, the size of |crtcs| is exactly 1 and is
      // different for each Output. In general, this is not true for other
      // video-drivers, and the lists can overlap.
      // TODO(yuweih): Consider making CRTC allocation smarter so it works with
      // non-video-dummy drivers.
      if (output_info.crtcs.empty()) {
        LOG(ERROR) << "No available CRTC found associated with "
                   << reinterpret_cast<char*>(output_info.name.data());
        continue;
      }
      auto crtc = output_info.crtcs.front();
      auto new_layout = new_layouts[i];
      // Note that this has a weird behavior in GNOME, such that, if |output| is
      // "disconnected", creating the mode somehow resizes all existing displays
      // to 1024x768. Once the output is successfully enabled, it will remain
      // "connected" and will no longer have the problem. The problem doesn't
      // occur on XFCE or Cinnamon.
      // TODO(yuweih): See if this is fixable, or at least implement some
      // workaround, such as re-applying the layout.
      auto mode = UpdateMode(output, new_layout.rect().width(),
                             new_layout.rect().height());
      if (mode == kInvalidMode) {
        LOG(ERROR) << "Failed to create new mode.";
        continue;
      }
      resizer.AddActiveCrtc(crtc, mode, {output}, new_layout.rect());
      VLOG(0) << "Added display with crtc: " << base::to_underlying(crtc)
              << ", output: " << base::to_underlying(output);
    }
    if (i < diff.new_displays.configs.size()) {
      LOG(WARNING) << "Failed to create "
                   << (diff.new_displays.configs.size() - i)
                   << " display(s) due to insufficient resources.";
    }
  }

  // Update displays
  for (const auto& updated_display : diff.updated_displays) {
    auto updated_layout = updated_display.config;
    auto output = GetOutputFromContext(updated_display.context);
    auto crtc = resizer.GetCrtcForOutput(output);
    if (crtc == kDisabledCrtc) {
      // This is not expected to happen. Disabled Outputs are not expected to
      // have any Monitor, but |output| was found in the RRGetMonitors response,
      // so it should have a CRTC attached.
      LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
      continue;
    }
    resizer.DisableCrtc(crtc);
    auto mode = UpdateMode(output, updated_layout.rect().width(),
                           updated_layout.rect().height());
    if (mode == kInvalidMode) {
      LOG(ERROR) << "Failed to create new mode.";
      continue;
    }
    resizer.UpdateActiveCrtc(crtc, mode, updated_layout.rect());
    VLOG(0) << "Updated display with screen ID: "
            << updated_display.config.id().value_or(-1);
  }

  // Remove displays
  for (const auto& removed_display : diff.removed_displays) {
    auto output = GetOutputFromContext(removed_display.context);
    auto crtc = resizer.GetCrtcForOutput(output);
    if (crtc == kDisabledCrtc) {
      LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
      continue;
    }
    resizer.DisableCrtc(crtc);
    resizer.RemoveActiveCrtc(crtc);
    DeleteMode(output, GetModeNameForOutput(output));
    VLOG(0) << "Removed display with screen ID: "
            << removed_display.config.id().value_or(-1);
  }

  resizer.NormalizeCrtcs();
  resizer.UpdateRootWindow(root_);
}

x11::RandR::Mode RandROutputManager::UpdateMode(x11::RandR::Output output,
                                                int width,
                                                int height) {
  std::string mode_name = GetModeNameForOutput(output);
  DeleteMode(output, mode_name);

  // Set some clock values so that the computed refresh-rate is a realistic
  // number:
  // 60Hz = dot_clock / (htotal * vtotal).
  // This allows GNOME's Display Settings tool to apply new settings for
  // resolution/scaling - see crbug.com/1374488.
  x11::RandR::ModeInfo mode;
  mode.width = width;
  mode.height = height;
  mode.dot_clock = default_mode_dot_clock_;
  mode.htotal = 1000;
  mode.vtotal = 1000;
  mode.name_len = mode_name.size();
  if (auto reply =
          randr_->CreateMode({root_, mode, mode_name.c_str()}).Sync()) {
    randr_->AddOutputMode({
        output,
        reply->mode,
    });
    return reply->mode;
  }
  return kInvalidMode;
}

void RandROutputManager::DeleteMode(x11::RandR::Output output,
                                    const std::string& name) {
  x11::RandR::Mode mode_id = resources_.GetIdForMode(name);
  if (mode_id != kInvalidMode) {
    randr_->DeleteOutputMode({output, mode_id});
    randr_->DestroyMode({mode_id});
    resources_.Refresh(randr_, root_);
  }
}

void RandROutputManager::SetResolutionForOutput(x11::RandR::Output output,
                                                const gfx::Size& dimensions,
                                                const gfx::Vector2d& dpi) {
  if (!resources_.Refresh(randr_, root_)) {
    LOG(ERROR) << "Failed to Refresh RandR resources.";
  }
  x11::X11CrtcResizer resizer(resources_.get(), connection_);

  resizer.FetchActiveCrtcs();
  auto crtc = resizer.GetCrtcForOutput(output);

  if (crtc == kDisabledCrtc) {
    // This is not expected to happen. Disabled Outputs are not expected to
    // have any Monitor, but |output| was found in the RRGetMonitors response,
    // so it should have a CRTC attached.
    LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
    return;
  }

  // Disable the output now, so that the old mode can be deleted and the new
  // mode created and added to the output's available modes. The previous size
  // and offsets will be stored in |resizer|.
  resizer.DisableCrtc(crtc);

  auto mode = UpdateMode(output, dimensions.width(), dimensions.height());
  if (mode == kInvalidMode) {
    // The CRTC is disabled, but there's no easy way to recover it here
    // (the mode it was attached to has gone).
    LOG(ERROR) << "Failed to create new mode.";
    return;
  }

  // Update |active_crtcs_| with new sizes and offsets.
  resizer.UpdateActiveCrtcs(crtc, mode, dimensions);
  resizer.UpdateRootWindow(root_);

  gfx::Size size_mm = CalculateSizeInMmForGnome(dimensions, dpi);
  int width_mm = size_mm.width();
  int height_mm = size_mm.height();
  VLOG(0) << "Setting physical size in mm: " << width_mm << "x" << height_mm;
  SetOutputPhysicalSizeInMM(connection_, output, width_mm, height_mm);
}

RandROutputManager::OutputInfoList RandROutputManager::GetDisabledOutputs() {
  OutputInfoList disabled_outputs;
  for (x11::RandR::Output output : resources_.get()->outputs) {
    auto reply = randr_
                     ->GetOutputInfo({.output = output,
                                      .config_timestamp =
                                          resources_.get()->config_timestamp})
                     .Sync();
    if (!reply) {
      continue;
    }
    if (reply->crtc == kDisabledCrtc) {
      disabled_outputs.emplace_back(output, std::move(*reply.reply));
    }
  }
  return disabled_outputs;
}

std::string RandROutputManager::GetModeNameForOutput(
    x11::RandR::Output output) {
  // The name of the mode representing the current client view resolution. This
  // must be unique per Output, so that Outputs can be resized independently.
  return base::StringPrintf("%s%i", output_name_prefix_.c_str(),
                            base::to_underlying(output));
}

}  // namespace x11
