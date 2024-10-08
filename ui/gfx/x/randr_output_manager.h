// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_RANDR_OUTPUT_MANAGER_H_
#define UI_GFX_X_RANDR_OUTPUT_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/randr.h"

namespace x11 {

// Wrapper class for the XRRScreenResources struct.
class COMPONENT_EXPORT(X11) ScreenResources {
 public:
  ScreenResources();
  ~ScreenResources();

  bool Refresh(x11::RandR* randr, x11::Window window);

  x11::RandR::Mode GetIdForMode(const std::string& name);

  x11::RandR::GetScreenResourcesCurrentReply* get();

 private:
  std::unique_ptr<x11::RandR::GetScreenResourcesCurrentReply> resources_;
};

// Encapsulates basic configuration properties for a single RandR monitor.
class COMPONENT_EXPORT(X11) RandRMonitorConfig {
 public:
  using ScreenId = int64_t;
  RandRMonitorConfig(std::optional<ScreenId> id,
                     gfx::Rect rect,
                     gfx::Vector2d dpi);
  RandRMonitorConfig(const RandRMonitorConfig& other);
  RandRMonitorConfig& operator=(const RandRMonitorConfig& other);
  bool operator==(const RandRMonitorConfig& rhs) const;

  std::optional<ScreenId> id() const { return id_; }

  const gfx::Rect& rect() const { return rect_; }
  const gfx::Vector2d& dpi() const { return dpi_; }

 private:
  // An opaque ID used to identify this monitor. Unset when the ID is unknown,
  // for example, when a caller is setting a new layout and the screen ID
  // does not exist yet.
  std::optional<ScreenId> id_;
  gfx::Rect rect_;
  gfx::Vector2d dpi_;
};

// Encapsulates a set of monitors to represent an entire screen layout.
struct COMPONENT_EXPORT(X11) RandRMonitorLayout {
  RandRMonitorLayout();
  RandRMonitorLayout(const RandRMonitorLayout&);
  explicit RandRMonitorLayout(std::vector<RandRMonitorConfig> configs);
  RandRMonitorLayout& operator=(const RandRMonitorLayout&);
  ~RandRMonitorLayout();
  bool operator==(const RandRMonitorLayout& rhs) const;

  std::vector<RandRMonitorConfig> configs;
  std::optional<int64_t> primary_screen_id;
};

// Attaches an arbitrary pointer to a monitor config for internal tracking
// purposes.
struct COMPONENT_EXPORT(X11) RandRMonitorConfigWithContext {
  x11::RandRMonitorConfig config;
  raw_ptr<void> context;
};

// Structure to hold data which describes changes between two layouts.
struct COMPONENT_EXPORT(X11) DisplayLayoutDiff {
  DisplayLayoutDiff();
  DisplayLayoutDiff(
      x11::RandRMonitorLayout new_displays,
      std::vector<RandRMonitorConfigWithContext> updated_displays,
      std::vector<RandRMonitorConfigWithContext> removed_displays);
  DisplayLayoutDiff(const DisplayLayoutDiff&);
  ~DisplayLayoutDiff();
  x11::RandRMonitorLayout new_displays;
  std::vector<RandRMonitorConfigWithContext> updated_displays;
  std::vector<RandRMonitorConfigWithContext> removed_displays;
};

// Calculates the difference between the current display layout and the new
// display layout. Displays are matched using the screen ID.
COMPONENT_EXPORT(X11)
DisplayLayoutDiff CalculateDisplayLayoutDiff(
    const std::vector<RandRMonitorConfigWithContext>& current_displays,
    const x11::RandRMonitorLayout& new_layout);

// Manages modes and layout of RandR outputs.
class COMPONENT_EXPORT(X11) RandROutputManager final {
 public:
  // `output_name_prefix` is used when creating new RandR outputs. Optionally
  // override the default mode dot clock with `default_mode_dot_clock`.
  explicit RandROutputManager(
      std::string output_name_prefix,
      uint32_t default_mode_dot_clock = 60 * 1e6 /* Realistic default */);
  ~RandROutputManager();

  // Attempts to get the current list of XRandR monitors from the current
  // connection. Returns true on success in which case `list` is populated with
  // the monitors. Returns false otherwise.
  bool TryGetCurrentMonitors(std::vector<x11::RandR::MonitorInfo>& list);

  // Obtains the current RandR layout.
  RandRMonitorLayout GetLayout();
  // Adjusts outputs to match the specified layout.
  void SetLayout(const RandRMonitorLayout& layout);

  // Removes the existing mode from the output and replaces it with the new
  // size. Returns the new mode ID, or None (0) on failure.
  x11::RandR::Mode UpdateMode(x11::RandR::Output output, int width, int height);
  // Remove the specified mode from the output, and delete it. If the mode is in
  // use, it is not deleted.
  // |name| should be set to GetModeNameForOutput(output). The parameter is to
  // avoid creating the mode name twice.
  void DeleteMode(x11::RandR::Output output, const std::string& name);

  // Add a mode matching the specified resolution and switch to it.
  void SetResolutionForOutput(x11::RandR::Output output,
                              const gfx::Size& dimensions,
                              const gfx::Vector2d& dpi);

 private:
  using OutputInfoList = std::vector<
      std::pair<x11::RandR::Output, x11::RandR::GetOutputInfoReply>>;

  OutputInfoList GetDisabledOutputs();

  std::string GetModeNameForOutput(x11::RandR::Output output);

  raw_ptr<x11::Connection> connection_ = nullptr;
  const raw_ptr<x11::RandR> randr_ = nullptr;
  x11::Window root_;
  ScreenResources resources_;
  bool has_randr_;
  std::string output_name_prefix_;
  const uint32_t default_mode_dot_clock_;
};

}  // namespace x11

#endif  // UI_GFX_X_RANDR_OUTPUT_MANAGER_H_
