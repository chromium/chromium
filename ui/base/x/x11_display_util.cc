// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_util.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/memory/protected_memory_cfi.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

constexpr int kMinVersionXrandr = 103;  // Need at least xrandr version 1.3.

typedef XRRMonitorInfo* (*XRRGetMonitors)(::Display*, Window, bool, int*);
typedef void (*XRRFreeMonitors)(XRRMonitorInfo*);

PROTECTED_MEMORY_SECTION base::ProtectedMemory<XRRGetMonitors>
    g_XRRGetMonitors_ptr;
PROTECTED_MEMORY_SECTION base::ProtectedMemory<XRRFreeMonitors>
    g_XRRFreeMonitors_ptr;

std::map<RROutput, int> GetMonitors(int version,
                                    XDisplay* xdisplay,
                                    GLXWindow window) {
  std::map<RROutput, int> output_to_monitor;
  if (version >= 105) {
    void* xrandr_lib = dlopen(nullptr, RTLD_NOW);
    if (xrandr_lib) {
      static base::ProtectedMemory<XRRGetMonitors>::Initializer get_init(
          &g_XRRGetMonitors_ptr, reinterpret_cast<XRRGetMonitors>(
                                     dlsym(xrandr_lib, "XRRGetMonitors")));
      static base::ProtectedMemory<XRRFreeMonitors>::Initializer free_init(
          &g_XRRFreeMonitors_ptr, reinterpret_cast<XRRFreeMonitors>(
                                      dlsym(xrandr_lib, "XRRFreeMonitors")));
      if (*g_XRRGetMonitors_ptr && *g_XRRFreeMonitors_ptr) {
        int nmonitors = 0;
        XRRMonitorInfo* monitors = base::UnsanitizedCfiCall(
            g_XRRGetMonitors_ptr)(xdisplay, window, false, &nmonitors);
        for (int monitor = 0; monitor < nmonitors; monitor++) {
          for (int j = 0; j < monitors[monitor].noutput; j++) {
            output_to_monitor[monitors[monitor].outputs[j]] = monitor;
          }
        }
        base::UnsanitizedCfiCall(g_XRRFreeMonitors_ptr)(monitors);
      }
    }
  }
  return output_to_monitor;
}

}  // namespace

int GetXrandrVersion(XDisplay* xdisplay) {
  int xrandr_version = 0;
  // We only support 1.3+. There were library changes before this and we should
  // use the new interface instead of the 1.2 one.
  int randr_version_major = 0;
  int randr_version_minor = 0;
  if (XRRQueryVersion(xdisplay, &randr_version_major, &randr_version_minor)) {
    xrandr_version = randr_version_major * 100 + randr_version_minor;
  }
  return xrandr_version;
}

std::vector<display::Display> GetFallbackDisplayList(float scale) {
  XDisplay* display = gfx::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  gfx::Size physical_size(WidthMMOfScreen(screen), HeightMMOfScreen(screen));

  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);
  gfx::Rect bounds_in_pixels(0, 0, width, height);
  display::Display gfx_display(0, bounds_in_pixels);

  if (!display::Display::HasForceDeviceScaleFactor() &&
      !display::IsDisplaySizeBlackListed(physical_size)) {
    DCHECK_LE(1.0f, scale);
    gfx_display.SetScaleAndBounds(scale, bounds_in_pixels);
  }

  return {gfx_display};
}

std::vector<display::Display> BuildDisplaysFromXRandRInfo(
    int version,
    float scale,
    int64_t* primary_display_index_out) {
  DCHECK(primary_display_index_out);
  DCHECK_GE(version, kMinVersionXrandr);
  XDisplay* xdisplay = gfx::GetXDisplay();
  GLXWindow x_root_window = DefaultRootWindow(xdisplay);
  std::vector<display::Display> displays;
  gfx::XScopedPtr<
      XRRScreenResources,
      gfx::XObjectDeleter<XRRScreenResources, void, XRRFreeScreenResources>>
      resources(XRRGetScreenResourcesCurrent(xdisplay, x_root_window));
  if (!resources) {
    LOG(ERROR) << "XRandR returned no displays; falling back to root window";
    return GetFallbackDisplayList(scale);
  }

  std::map<RROutput, int> output_to_monitor =
      GetMonitors(version, xdisplay, x_root_window);
  *primary_display_index_out = 0;
  RROutput primary_display_id = XRRGetOutputPrimary(xdisplay, x_root_window);

  int explicit_primary_display_index = -1;
  int monitor_order_primary_display_index = -1;

  bool has_work_area = false;
  gfx::Rect work_area_in_pixels;
  std::vector<int> value;
  if (ui::GetIntArrayProperty(x_root_window, "_NET_WORKAREA", &value) &&
      value.size() >= 4) {
    work_area_in_pixels = gfx::Rect(value[0], value[1], value[2], value[3]);
    has_work_area = true;
  }

  // As per-display scale factor is not supported right now,
  // the X11 root window's scale factor is always used.
  for (int i = 0; i < resources->noutput; ++i) {
    RROutput output_id = resources->outputs[i];
    gfx::XScopedPtr<XRROutputInfo,
                    gfx::XObjectDeleter<XRROutputInfo, void, XRRFreeOutputInfo>>
        output_info(XRRGetOutputInfo(xdisplay, resources.get(), output_id));

    bool is_connected = (output_info->connection == RR_Connected);
    if (!is_connected)
      continue;

    bool is_primary_display = (output_id == primary_display_id);

    if (output_info->crtc) {
      gfx::XScopedPtr<XRRCrtcInfo,
                      gfx::XObjectDeleter<XRRCrtcInfo, void, XRRFreeCrtcInfo>>
          crtc(XRRGetCrtcInfo(xdisplay, resources.get(), output_info->crtc));

      int64_t display_id = -1;
      if (!display::EDIDParserX11(output_id).GetDisplayId(
              static_cast<uint8_t>(i), &display_id)) {
        // It isn't ideal, but if we can't parse the EDID data, fall back on the
        // display number.
        display_id = i;
      }

      gfx::Rect crtc_bounds(crtc->x, crtc->y, crtc->width, crtc->height);
      display::Display display(display_id, crtc_bounds);

      if (!display::Display::HasForceDeviceScaleFactor()) {
        display.SetScaleAndBounds(scale, crtc_bounds);
      }

      if (has_work_area) {
        gfx::Rect intersection_in_pixels = crtc_bounds;
        if (is_primary_display) {
          intersection_in_pixels.Intersect(work_area_in_pixels);
        }
        // SetScaleAndBounds() above does the conversion from pixels to DIP for
        // us, but set_work_area does not, so we need to do it here.
        display.set_work_area(gfx::Rect(
            gfx::ScaleToFlooredPoint(intersection_in_pixels.origin(),
                                     1.0f / display.device_scale_factor()),
            gfx::ScaleToFlooredSize(intersection_in_pixels.size(),
                                    1.0f / display.device_scale_factor())));
      }

      switch (crtc->rotation) {
        case RR_Rotate_0:
          display.set_rotation(display::Display::ROTATE_0);
          break;
        case RR_Rotate_90:
          display.set_rotation(display::Display::ROTATE_90);
          break;
        case RR_Rotate_180:
          display.set_rotation(display::Display::ROTATE_180);
          break;
        case RR_Rotate_270:
          display.set_rotation(display::Display::ROTATE_270);
          break;
      }

      if (is_primary_display)
        explicit_primary_display_index = displays.size();

      auto monitor_iter = output_to_monitor.find(output_id);
      if (monitor_iter != output_to_monitor.end() && monitor_iter->second == 0)
        monitor_order_primary_display_index = displays.size();

      if (!display::Display::HasForceDisplayColorProfile()) {
        gfx::ICCProfile icc_profile = ui::GetICCProfileForMonitor(
            monitor_iter == output_to_monitor.end() ? 0 : monitor_iter->second);
        icc_profile.HistogramDisplay(display.id());
        display.set_color_space(icc_profile.GetColorSpace());
      }

      displays.push_back(display);
    }
  }

  if (explicit_primary_display_index != -1) {
    *primary_display_index_out = explicit_primary_display_index;
  } else if (monitor_order_primary_display_index != -1) {
    *primary_display_index_out = monitor_order_primary_display_index;
  }

  if (displays.empty())
    return GetFallbackDisplayList(scale);

  return displays;
}

}  // namespace ui
