// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_

#include <cstdint>
#include <ostream>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class XDGOutput;
class WaylandZcrColorManager;
class WaylandZcrColorManagementOutput;
class WaylandConnection;

// WaylandOutput objects keep track of wl_output information received through
// the Wayland protocol, along with other related protocol extensions, such as,
// xdg-output and ChromeOS's aura-shell.
class WaylandOutput : public wl::GlobalObjectRegistrar<WaylandOutput> {
 public:
  // Instances of this class are identified by an 32-bit unsigned int value,
  // corresponding to its global wl_output object 'name' value.  On
  // wayland-linux, it is mostly used interchangeably with WaylandScreen's
  // `display::Display::id1` property, which is an int64_t instead, though it is
  // worth bearing in mind they are slightly different, under the hood.
  // On lacros, the display id sent from ash-chrome is used for
  // `display::Display::id`.
  using Id = uint32_t;

  static constexpr char kInterfaceName[] = "wl_output";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  // All parameters are in DIP screen coordinates/units except |physical_size|,
  // which is in physical pixels.
  struct Metrics {
    // TODO(aluh): Remove explicit constructors/destructor to enable aggregate
    // initialization if chromium-style check for complex struct is removed.
    // See:
    // https://groups.google.com/a/google.com/g/chromeos-chatty-eng/c/nM1_QC6qcuA
    Metrics();
    Metrics(Id output_id,
            int64_t display_id,
            gfx::Point origin,
            gfx::Size logical_size,
            gfx::Size physical_size,
            gfx::Insets insets,
            gfx::Insets physical_overscan_insets,
            float scale_factor,
            int32_t panel_transform,
            int32_t logical_transform,
            const std::string& description);
    Metrics(const Metrics&);
    Metrics& operator=(const Metrics&);
    Metrics(Metrics&&);
    Metrics& operator=(Metrics&&);
    ~Metrics();

    bool operator==(const Metrics&) const = default;

    Id output_id = 0;
    int64_t display_id = -1;
    gfx::Point origin;
    gfx::Size logical_size;
    gfx::Size physical_size;
    // Work area insets in DIP.
    gfx::Insets insets;
    // Overscan insets in physical pixels.
    gfx::Insets physical_overscan_insets;
    float scale_factor = 0.0;
    int32_t panel_transform = 0;
    int32_t logical_transform = 0;
    std::string name;
    std::string description;

    void DumpState(std::ostream& out) const;
  };

  class Delegate {
   public:
    virtual void OnOutputHandleMetrics(const Metrics& metrics) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  WaylandOutput(Id output_id, wl_output* output, WaylandConnection* connection);

  WaylandOutput(const WaylandOutput&) = delete;
  WaylandOutput& operator=(const WaylandOutput&) = delete;

  ~WaylandOutput();

  void Initialize(Delegate* delegate);
  void InitializeXdgOutput(zxdg_output_manager_v1* manager);
  void InitializeColorManagementOutput(WaylandZcrColorManager* manager);

  const Metrics& GetMetrics() const;
  void SetMetrics(const Metrics& metrics);

  // TODO(tuluk): Metrics getters below are rendundant and should be replaced
  // with calls to GetMetrics().
  Id output_id() const { return output_id_; }
  float scale_factor() const;
  WaylandZcrColorManagementOutput* color_management_output() const {
    return color_management_output_.get();
  }

  // Returns true if the output has all the state information available
  // necessary to represent its associated display. This information arrives
  // asynchronously via events across potentially multiple wayland objects.
  bool IsReady() const;

  wl_output* get_output() { return output_.get(); }

  void SetScaleFactorForTesting(float scale_factor);

  void TriggerDelegateNotifications();

  void DumpState(std::ostream& out) const;

  void set_delegate_for_testing(Delegate* delegate) { delegate_ = delegate; }
  XDGOutput* xdg_output_for_testing() { return xdg_output_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, NameAndDescriptionFallback);
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, ScaleFactorCalculation);
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, ScaleFactorFallback);
  FRIEND_TEST_ALL_PREFIXES(WaylandOutputTest, ScaleFactorCalculationNoop);

  static constexpr int32_t kDefaultScaleFactor = 1;

  // Called when the wl_output.done event is received and atomically updates
  // `metrics_` based on the previously received output state events.
  void UpdateMetrics();

  // wl_output_listener callbacks:
  static void OnGeometry(void* data,
                         wl_output* output,
                         int32_t x,
                         int32_t y,
                         int32_t physical_width,
                         int32_t physical_height,
                         int32_t subpixel,
                         const char* make,
                         const char* model,
                         int32_t output_transform);
  static void OnMode(void* data,
                     wl_output* output,
                     uint32_t flags,
                     int32_t width,
                     int32_t height,
                     int32_t refresh);
  static void OnDone(void* data, wl_output* output);
  static void OnScale(void* data, wl_output* output, int32_t factor);
  static void OnName(void* data, wl_output* output, const char* name);
  static void OnDescription(void* data,
                            wl_output* output,
                            const char* description);

  // Tracks whether this wl_output is considered "ready". I.e. it has received
  // all of its relevant state from the server followed by a wl_output.done
  // event.
  bool is_ready_ = false;

  // Metrics represents the current state of the display represented by this
  // output object. This state is updated atomically after the client has
  // received the wl_output.done event.
  Metrics metrics_;

  const Id output_id_ = 0;
  wl::Object<wl_output> output_;
  std::unique_ptr<XDGOutput> xdg_output_;
  std::unique_ptr<WaylandZcrColorManagementOutput> color_management_output_;

  float scale_factor_ = kDefaultScaleFactor;
  int32_t panel_transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
  // Origin of the output in DIP screen coordinate.
  gfx::Point origin_;
  // Size of the output in physical pixels.
  gfx::Size physical_size_;
  // Fallback name and description.
  // The XDG output specification suggests using it as the primary source of
  // the information about the output.  Two attributes below are used if
  // xdg_output_ is not present.
  // See https://wayland.app/protocols/xdg-output-unstable-v1
  std::string name_;
  std::string description_;

  raw_ptr<Delegate> delegate_ = nullptr;
  raw_ptr<WaylandConnection> connection_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_OUTPUT_H_
