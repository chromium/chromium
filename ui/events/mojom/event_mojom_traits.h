// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_MOJOM_EVENT_MOJOM_TRAITS_H_
#define UI_EVENTS_MOJOM_EVENT_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "ui/events/event_constants.h"
#include "ui/events/mojom/event.mojom.h"
#include "ui/events/mojom/event_constants.mojom.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"

namespace ui {
class Event;
class LatencyInfo;
}

namespace mojo {

using EventUniquePtr = std::unique_ptr<ui::Event>;

template <>
struct TypeConverter<ui::mojom::EventType, ui::EventType> {
  static ui::mojom::EventType Convert(ui::EventType type);
};

template <>
struct TypeConverter<ui::EventType, ui::mojom::EventType> {
  static ui::EventType Convert(ui::mojom::EventType type);
};

template <>
struct StructTraits<ui::mojom::EventDataView, EventUniquePtr> {
  static ui::mojom::EventType action(const EventUniquePtr& event);
  static int32_t flags(const EventUniquePtr& event);
  static base::TimeTicks time_stamp(const EventUniquePtr& event);
  static const ui::LatencyInfo& latency(const EventUniquePtr& event);
  static ui::mojom::KeyDataPtr key_data(const EventUniquePtr& event);
  static ui::mojom::GestureDataPtr gesture_data(const EventUniquePtr& event);
  static ui::mojom::ScrollDataPtr scroll_data(const EventUniquePtr& event);
  static ui::mojom::TouchDataPtr touch_data(const EventUniquePtr& event);
  static ui::mojom::MouseDataPtr mouse_data(const EventUniquePtr& event);
  static base::flat_map<std::string, std::vector<uint8_t>> properties(
      const EventUniquePtr& event);
  static bool Read(ui::mojom::EventDataView r, EventUniquePtr* out);
};

template <>
struct EnumTraits<ui::mojom::EventMomentumPhase, ui::EventMomentumPhase> {
  static ui::mojom::EventMomentumPhase ToMojom(ui::EventMomentumPhase input) {
    switch (input) {
      case ui::EventMomentumPhase::NONE:
        return ui::mojom::EventMomentumPhase::NONE;
      case ui::EventMomentumPhase::BEGAN:
        return ui::mojom::EventMomentumPhase::BEGAN;
      case ui::EventMomentumPhase::MAY_BEGIN:
        return ui::mojom::EventMomentumPhase::MAY_BEGIN;
      case ui::EventMomentumPhase::INERTIAL_UPDATE:
        return ui::mojom::EventMomentumPhase::INERTIAL_UPDATE;
      case ui::EventMomentumPhase::END:
        return ui::mojom::EventMomentumPhase::END;
      case ui::EventMomentumPhase::BLOCKED:
        return ui::mojom::EventMomentumPhase::BLOCKED;
    }
    NOTREACHED();
    return ui::mojom::EventMomentumPhase::NONE;
  }

  static bool FromMojom(ui::mojom::EventMomentumPhase input,
                        ui::EventMomentumPhase* out) {
    switch (input) {
      case ui::mojom::EventMomentumPhase::NONE:
        *out = ui::EventMomentumPhase::NONE;
        return true;
      case ui::mojom::EventMomentumPhase::BEGAN:
        *out = ui::EventMomentumPhase::BEGAN;
        return true;
      case ui::mojom::EventMomentumPhase::MAY_BEGIN:
        *out = ui::EventMomentumPhase::MAY_BEGIN;
        return true;
      case ui::mojom::EventMomentumPhase::INERTIAL_UPDATE:
        *out = ui::EventMomentumPhase::INERTIAL_UPDATE;
        return true;
      case ui::mojom::EventMomentumPhase::END:
        *out = ui::EventMomentumPhase::END;
        return true;
      case ui::mojom::EventMomentumPhase::BLOCKED:
        *out = ui::EventMomentumPhase::BLOCKED;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<ui::mojom::PointerDetailsDataView, ui::PointerDetails> {
  static ui::EventPointerType pointer_type(const ui::PointerDetails& i) {
    return i.pointer_type;
  }
  static float radius_x(const ui::PointerDetails& i) { return i.radius_x; }
  static float radius_y(const ui::PointerDetails& i) { return i.radius_y; }
  static float force(const ui::PointerDetails& i) { return i.force; }
  static float tilt_x(const ui::PointerDetails& i) { return i.tilt_x; }
  static float tilt_y(const ui::PointerDetails& i) { return i.tilt_y; }
  static float tangential_pressure(const ui::PointerDetails& i) {
    return i.tangential_pressure;
  }
  static float twist(const ui::PointerDetails& i) { return i.twist; }
  static int32_t id(const ui::PointerDetails& i) { return i.id; }
  static int32_t offset_x(const ui::PointerDetails& i) { return i.offset.x(); }
  static int32_t offset_y(const ui::PointerDetails& i) { return i.offset.y(); }

  static bool Read(ui::mojom::PointerDetailsDataView data,
                   ui::PointerDetails* out);
};

template <>
struct EnumTraits<ui::mojom::ScrollEventPhase, ui::ScrollEventPhase> {
  static ui::mojom::ScrollEventPhase ToMojom(ui::ScrollEventPhase input) {
    switch (input) {
      case ui::ScrollEventPhase::kNone:
        return ui::mojom::ScrollEventPhase::kNone;
      case ui::ScrollEventPhase::kBegan:
        return ui::mojom::ScrollEventPhase::kBegan;
      case ui::ScrollEventPhase::kUpdate:
        return ui::mojom::ScrollEventPhase::kUpdate;
      case ui::ScrollEventPhase::kEnd:
        return ui::mojom::ScrollEventPhase::kEnd;
    }
    NOTREACHED();
    return ui::mojom::ScrollEventPhase::kNone;
  }

  static bool FromMojom(ui::mojom::ScrollEventPhase input,
                        ui::ScrollEventPhase* out) {
    switch (input) {
      case ui::mojom::ScrollEventPhase::kNone:
        *out = ui::ScrollEventPhase::kNone;
        return true;
      case ui::mojom::ScrollEventPhase::kBegan:
        *out = ui::ScrollEventPhase::kBegan;
        return true;
      case ui::mojom::ScrollEventPhase::kUpdate:
        *out = ui::ScrollEventPhase::kUpdate;
        return true;
      case ui::mojom::ScrollEventPhase::kEnd:
        *out = ui::ScrollEventPhase::kEnd;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<ui::mojom::GestureDeviceType, ui::GestureDeviceType> {
  static ui::mojom::GestureDeviceType ToMojom(ui::GestureDeviceType input) {
    switch (input) {
      case ui::GestureDeviceType::DEVICE_UNKNOWN:
        return ui::mojom::GestureDeviceType::DEVICE_UNKNOWN;
      case ui::GestureDeviceType::DEVICE_TOUCHPAD:
        return ui::mojom::GestureDeviceType::DEVICE_TOUCHPAD;
      case ui::GestureDeviceType::DEVICE_TOUCHSCREEN:
        return ui::mojom::GestureDeviceType::DEVICE_TOUCHSCREEN;
    }
    NOTREACHED();
    return ui::mojom::GestureDeviceType::DEVICE_UNKNOWN;
  }

  static bool FromMojom(ui::mojom::GestureDeviceType input,
                        ui::GestureDeviceType* out) {
    switch (input) {
      case ui::mojom::GestureDeviceType::DEVICE_UNKNOWN:
        *out = ui::GestureDeviceType::DEVICE_UNKNOWN;
        return true;
      case ui::mojom::GestureDeviceType::DEVICE_TOUCHPAD:
        *out = ui::GestureDeviceType::DEVICE_TOUCHPAD;
        return true;
      case ui::mojom::GestureDeviceType::DEVICE_TOUCHSCREEN:
        *out = ui::GestureDeviceType::DEVICE_TOUCHSCREEN;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_EVENTS_MOJOM_EVENT_MOJOM_TRAITS_H_
