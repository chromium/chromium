// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_TOUCHPAD_HAPTICS_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_TOUCHPAD_HAPTICS_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;

// Wraps the zcr_touchpad_haptics object.
class WaylandZcrTouchpadHaptics
    : public wl::GlobalObjectRegistrar<WaylandZcrTouchpadHaptics> {
 public:
  static constexpr char kInterfaceName[] = "zcr_touchpad_haptics_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandZcrTouchpadHaptics(zcr_touchpad_haptics_v1* zcr_touchpad_haptics,
                            WaylandConnection* connection);
  WaylandZcrTouchpadHaptics(const WaylandZcrTouchpadHaptics&) = delete;
  WaylandZcrTouchpadHaptics& operator=(const WaylandZcrTouchpadHaptics&) =
      delete;
  virtual ~WaylandZcrTouchpadHaptics();

  // Calls zcr_touchpad_haptics_v1_play(). See interface descriptions for values
  // for |effect| and |strength|
  void Play(int32_t effect, int32_t strength);

 private:
  // zcr_touchpad_haptics_v1_listener callbacks:
  static void OnActivated(void* data,
                          zcr_touchpad_haptics_v1* touchpad_haptics);
  static void OnDeactivated(void* data,
                            zcr_touchpad_haptics_v1* touchpad_haptics);

  wl::Object<zcr_touchpad_haptics_v1> obj_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_TOUCHPAD_HAPTICS_H_
