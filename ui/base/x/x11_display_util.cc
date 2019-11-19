// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_util.h"

#include <dlfcn.h>

#include <bitset>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/util/display_util.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/matrix3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {

constexpr int kMinVersionXrandr = 103;  // Need at least xrandr version 1.3.

typedef XRRMonitorInfo* (*XRRGetMonitors)(::Display*, Window, bool, int*);
typedef void (*XRRFreeMonitors)(XRRMonitorInfo*);

NO_SANITIZE("cfi-icall")
std::map<RROutput, int> GetMonitors(int version,
                                    XDisplay* xdisplay,
                                    GLXWindow window) {
  std::map<RROutput, int> output_to_monitor;
  if (version >= 105) {
    void* xrandr_lib = dlopen(nullptr, RTLD_NOW);
    if (xrandr_lib) {
      static XRRGetMonitors XRRGetMonitors_ptr =
          reinterpret_cast<XRRGetMonitors>(dlsym(xrandr_lib, "XRRGetMonitors"));
      static XRRFreeMonitors XRRFreeMonitors_ptr =
          reinterpret_cast<XRRFreeMonitors>(
              dlsym(xrandr_lib, "XRRFreeMonitors"));
      if (XRRGetMonitors_ptr && XRRFreeMonitors_ptr) {
        int nmonitors = 0;
        XRRMonitorInfo* monitors =
            XRRGetMonitors_ptr(xdisplay, window, false, &nmonitors);
        for (int monitor = 0; monitor < nmonitors; monitor++) {
          for (int j = 0; j < monitors[monitor].noutput; j++) {
            output_to_monitor[monitors[monitor].outputs[j]] = monitor;
          }
        }
        XRRFreeMonitors_ptr(monitors);
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
  XDisplay* xdisplay = gfx::GetXDisplay();
  GLXWindow x_root_window = DefaultRootWindow(xdisplay);

  std::vector<int> value;
  if (!ui::GetIntArrayProperty(x_root_window, "_NET_WORKAREA", &value) ||
      value.size() < 4) {
    return;
  }
  gfx::Rect work_area = gfx::ScaleToEnclosingRect(
      gfx::Rect(value[0], value[1], value[2], value[3]), 1.0f / scale);

  // If the work area entirely contains exactly one display, assume it's meant
  // for that display (and so do nothing).
  if (std::count_if(displays->begin(), displays->end(),
                    [&](const display::Display& display) {
                      return work_area.Contains(display.bounds());
                    }) == 1) {
    return;
  }

  // If the work area is entirely contained within exactly one display, assume
  // it's meant for that display and intersect the work area with only that
  // display.
  auto found = std::find_if(displays->begin(), displays->end(),
                            [&](const display::Display& display) {
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

float GetRefreshRateFromXRRModeInfo(XRRModeInfo* modes,
                                    int num_of_mode,
                                    RRMode current_mode_id) {
  for (int i = 0; i < num_of_mode; i++) {
    XRRModeInfo mode_info = modes[i];
    if (mode_info.id != current_mode_id)
      continue;
    if (!mode_info.hTotal || !mode_info.vTotal)
      return 0;

    // Refresh Rate = Pixel Clock / (Horizontal Total * Vertical Total)
    return mode_info.dotClock /
           static_cast<float>(mode_info.hTotal * mode_info.vTotal);
  }
  return 0;
}

int DefaultScreenDepth(XDisplay* xdisplay) {
  return DefaultDepth(xdisplay, DefaultScreen(xdisplay));
}

int DefaultBitsPerComponent(XDisplay* xdisplay) {
  Visual* visual = DefaultVisual(xdisplay, DefaultScreen(xdisplay));

  // The mask fields are only valid for DirectColor and TrueColor classes.
  if (visual->c_class == DirectColor || visual->c_class == TrueColor) {
    // RGB components are packed into fixed size integers for each visual.  The
    // layout of bits in the packing is given by
    // |visual->{red,green,blue}_mask|.  Count the number of bits to get the
    // number of bits per component.
    auto bits = [](auto mask) {
      return std::bitset<sizeof(mask) * 8>{mask}.count();
    };
    size_t red_bits = bits(visual->red_mask);
    size_t green_bits = bits(visual->green_mask);
    size_t blue_bits = bits(visual->blue_mask);
    if (red_bits == green_bits && red_bits == blue_bits)
      return red_bits;
  }

  // Next, try getting the number of colormap entries per subfield.  If it's a
  // power of 2, log2 is a possible guess for the number of bits per component.
  if (base::bits::IsPowerOfTwo(visual->map_entries))
    return base::bits::Log2Ceiling(visual->map_entries);

  // |bits_per_rgb| can sometimes be unreliable (may be 11 for 30bpp visuals),
  // so only use it as a last resort.
  return visual->bits_per_rgb;
}

bool IsRandRAvailable() {
  int randr_version_major = 0;
  int randr_version_minor = 0;
  static bool is_randr_available = XRRQueryVersion(
      gfx::GetXDisplay(), &randr_version_major, &randr_version_minor);
  return is_randr_available;
}

// Get the EDID data from the |output| and stores to |edid|.
void GetEDIDProperty(XID output, std::vector<uint8_t>* edid) {
  if (!IsRandRAvailable())
    return;

  Display* display = gfx::GetXDisplay();

  Atom edid_property = gfx::GetAtom(RR_PROPERTY_RANDR_EDID);

  bool has_edid_property = false;
  int num_properties = 0;
  gfx::XScopedPtr<Atom[]> properties(
      XRRListOutputProperties(display, output, &num_properties));
  for (int i = 0; i < num_properties; ++i) {
    if (properties[i] == edid_property) {
      has_edid_property = true;
      break;
    }
  }
  if (!has_edid_property)
    return;

  Atom actual_type;
  int actual_format;
  unsigned long bytes_after;
  unsigned long nitems = 0;
  unsigned char* prop = nullptr;
  XRRGetOutputProperty(display, output, edid_property,
                       0,                // offset
                       128,              // length
                       false,            // _delete
                       false,            // pending
                       AnyPropertyType,  // req_type
                       &actual_type, &actual_format, &nitems, &bytes_after,
                       &prop);
  DCHECK_EQ(XA_INTEGER, actual_type);
  DCHECK_EQ(8, actual_format);
  edid->assign(prop, prop + nitems);
  XFree(prop);
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
    gfx_display.set_work_area(
        gfx::ScaleToEnclosingRect(bounds_in_pixels, 1.0f / scale));
  } else {
    scale = 1;
  }

  gfx_display.set_color_depth(DefaultScreenDepth(display));
  gfx_display.set_depth_per_component(DefaultBitsPerComponent(display));

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

  const int depth = DefaultScreenDepth(xdisplay);
  const int bits_per_component = DefaultBitsPerComponent(xdisplay);

  std::map<RROutput, int> output_to_monitor =
      GetMonitors(version, xdisplay, x_root_window);
  *primary_display_index_out = 0;
  RROutput primary_display_id = XRRGetOutputPrimary(xdisplay, x_root_window);

  int explicit_primary_display_index = -1;
  int monitor_order_primary_display_index = -1;

  // As per-display scale factor is not supported right now,
  // the X11 root window's scale factor is always used.
  for (int i = 0; i < resources->noutput; ++i) {
    RROutput output_id = resources->outputs[i];
    gfx::XScopedPtr<XRROutputInfo,
                    gfx::XObjectDeleter<XRROutputInfo, void, XRRFreeOutputInfo>>
        output_info(XRRGetOutputInfo(xdisplay, resources.get(), output_id));

    // XRRGetOutputInfo returns null in some cases: https://crbug.com/921490
    if (!output_info)
      continue;

    bool is_connected = (output_info->connection == RR_Connected);
    if (!is_connected)
      continue;

    bool is_primary_display = (output_id == primary_display_id);

    if (output_info->crtc) {
      gfx::XScopedPtr<XRRCrtcInfo,
                      gfx::XObjectDeleter<XRRCrtcInfo, void, XRRFreeCrtcInfo>>
          crtc(XRRGetCrtcInfo(xdisplay, resources.get(), output_info->crtc));

      std::vector<uint8_t> edid_bytes;
      GetEDIDProperty(output_id, &edid_bytes);
      display::EdidParser edid_parser(edid_bytes);
      int64_t display_id = edid_parser.GetDisplayId(output_id);
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
        gfx::ColorSpace color_space = icc_profile.GetPrimariesOnlyColorSpace();

        // Most folks do not have an ICC profile set up, but we still want to
        // detect if a display has a wide color gamut so that HDR videos can be
        // enabled.  Only do this if |bits_per_component| > 8 or else SDR
        // screens may have washed out colors.
        if (bits_per_component > 8 && !color_space.IsValid())
          color_space = display::GetColorSpaceFromEdid(edid_parser);

        display.set_color_space(color_space);
      }

      display.set_color_depth(depth);
      display.set_depth_per_component(bits_per_component);

      // Set monitor refresh rate
      int refresh_rate = static_cast<int>(GetRefreshRateFromXRRModeInfo(
          resources->modes, resources->nmode, crtc->mode));
      display.set_display_frequency(refresh_rate);

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

  ClipWorkArea(&displays, *primary_display_index_out, scale);
  return displays;
}

base::TimeDelta GetPrimaryDisplayRefreshIntervalFromXrandr(Display* display) {
  constexpr base::TimeDelta kDefaultInterval =
      base::TimeDelta::FromSecondsD(1. / 60);
  GLXWindow root = DefaultRootWindow(display);
  gfx::XScopedPtr<
      XRRScreenResources,
      gfx::XObjectDeleter<XRRScreenResources, void, XRRFreeScreenResources>>
      resources(XRRGetScreenResourcesCurrent(display, root));
  if (!resources)
    return kDefaultInterval;
  // TODO(crbug.com/726842): It might make sense here to pick the output that
  // the window is on. On the other hand, if compositing is enabled, all drawing
  // might be synced to the primary output anyway. Needs investigation.
  RROutput primary_output = XRRGetOutputPrimary(display, root);
  bool disconnected_primary = false;
  for (int i = 0; i < resources->noutput; i++) {
    if (!disconnected_primary && resources->outputs[i] != primary_output)
      continue;

    gfx::XScopedPtr<XRROutputInfo,
                    gfx::XObjectDeleter<XRROutputInfo, void, XRRFreeOutputInfo>>
        output_info(XRRGetOutputInfo(display, resources.get(), primary_output));
    if (!output_info)
      continue;

    if (output_info->connection != RR_Connected) {
      // If the primary monitor is disconnected, then start over and choose the
      // first connected monitor instead.
      if (!disconnected_primary) {
        disconnected_primary = true;
        i = -1;
      }
      continue;
    }
    gfx::XScopedPtr<XRRCrtcInfo,
                    gfx::XObjectDeleter<XRRCrtcInfo, void, XRRFreeCrtcInfo>>
        crtc(XRRGetCrtcInfo(display, resources.get(), output_info->crtc));
    if (!crtc)
      continue;
    float refresh_rate = GetRefreshRateFromXRRModeInfo(
        resources->modes, resources->nmode, crtc->mode);
    if (refresh_rate == 0)
      continue;

    return base::TimeDelta::FromSecondsD(1. / refresh_rate);
  }
  return kDefaultInterval;
}

}  // namespace ui
