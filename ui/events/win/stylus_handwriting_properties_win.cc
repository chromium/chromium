// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/stylus_handwriting_properties_win.h"

#include <ShellHandwriting.h>

#include <type_traits>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/win/dpi.h"
#include "ui/events/event_utils.h"

namespace ui {

namespace {

using GetHandwritingStrokeIdForPointerFunc = HRESULT(WINAPI*)(uint32_t,
                                                              uint64_t*);

constexpr char kPropertyHandwritingPointerId[] = "handwriting_pointer_id";
constexpr char kPropertyHandwritingStrokeId[] = "handwriting_stroke_id";
constexpr char kPropertyHandwritingPixelsPerInchX[] =
    "handwriting_pixels_per_inch_x";
constexpr char kPropertyHandwritingPixelsPerInchY[] =
    "handwriting_pixels_per_inch_y";

gfx::Vector2dF GetDefaultPixelsPerInch() {
  const float default_dpi =
      static_cast<float>(display::win::GetDPIFromScalingFactor(1.0f));
  return gfx::Vector2dF(default_dpi, default_dpi);
}

template <typename T>
std::optional<T> GetEventPropertyValue(const Event::Properties& properties,
                                       const Event::PropertyKey& key) {
  const auto it = properties.find(key);
  if (it == properties.end()) {
    return std::nullopt;
  }
  CHECK_EQ(it->second.size(), sizeof(T));
  T value{};
  if constexpr (std::has_unique_object_representations_v<T>) {
    base::byte_span_from_ref(value).copy_from(base::span(it->second));
  } else {
    base::byte_span_from_ref(base::allow_nonunique_obj, value)
        .copy_from(base::span(it->second));
  }
  return value;
}

Event::PropertyValue ConvertFloatToEventPropertyValue(float value) {
  Event::PropertyValue property_value(sizeof(value));
  base::span(property_value)
      .copy_from(base::byte_span_from_ref(base::allow_nonunique_obj, value));
  return property_value;
}

GetHandwritingStrokeIdForPointerFunc
GetHandwritingStrokeIdForPointerFuncFromModule() {
  HMODULE module = nullptr;
  if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"msctf.dll",
                           &module)) {
    return reinterpret_cast<GetHandwritingStrokeIdForPointerFunc>(
        ::GetProcAddress(module, "GetHandwritingStrokeIdForPointer"));
  }

  return nullptr;
}

}  // namespace

StylusHandwritingPropertiesWin::StylusHandwritingPropertiesWin()
    : StylusHandwritingPropertiesWin(
          /*handwriting_pointer_id=*/0,
          /*handwriting_stroke_id=*/0) {}

StylusHandwritingPropertiesWin::StylusHandwritingPropertiesWin(
    uint32_t handwriting_pointer_id,
    uint64_t handwriting_stroke_id)
    : StylusHandwritingPropertiesWin(handwriting_pointer_id,
                                     handwriting_stroke_id,
                                     std::nullopt) {}

StylusHandwritingPropertiesWin::StylusHandwritingPropertiesWin(
    uint32_t handwriting_pointer_id,
    uint64_t handwriting_stroke_id,
    std::optional<gfx::Vector2dF> pixels_per_inch)
    : handwriting_pointer_id(handwriting_pointer_id),
      handwriting_stroke_id(handwriting_stroke_id),
      pixels_per_inch(pixels_per_inch ? *pixels_per_inch
                                      : GetDefaultPixelsPerInch()) {}

const char* GetPropertyHandwritingPointerIdKeyForTesting() {
  return kPropertyHandwritingPointerId;
}

const char* GetPropertyHandwritingStrokeIdKeyForTesting() {
  return kPropertyHandwritingStrokeId;
}

Event::Properties CreateEventPropertiesForTesting(  // IN-TEST
    const StylusHandwritingPropertiesWin& properties) {
  Event::Properties event_properties;
  event_properties[kPropertyHandwritingPointerId] =
      ConvertToEventPropertyValue(properties.handwriting_pointer_id);
  event_properties[kPropertyHandwritingStrokeId] =
      ConvertToEventPropertyValue(properties.handwriting_stroke_id);
  event_properties[kPropertyHandwritingPixelsPerInchX] =
      ConvertFloatToEventPropertyValue(properties.pixels_per_inch.x());
  event_properties[kPropertyHandwritingPixelsPerInchY] =
      ConvertFloatToEventPropertyValue(properties.pixels_per_inch.y());
  return event_properties;
}

void SetStylusHandwritingProperties(
    Event& event,
    const StylusHandwritingPropertiesWin& properties) {
  event.SetProperty(
      kPropertyHandwritingPointerId,
      ConvertToEventPropertyValue(properties.handwriting_pointer_id));
  event.SetProperty(
      kPropertyHandwritingStrokeId,
      ConvertToEventPropertyValue(properties.handwriting_stroke_id));
  event.SetProperty(
      kPropertyHandwritingPixelsPerInchX,
      ConvertFloatToEventPropertyValue(properties.pixels_per_inch.x()));
  event.SetProperty(
      kPropertyHandwritingPixelsPerInchY,
      ConvertFloatToEventPropertyValue(properties.pixels_per_inch.y()));
}

std::optional<StylusHandwritingPropertiesWin> GetStylusHandwritingProperties(
    const Event& event) {
  std::optional<StylusHandwritingPropertiesWin> handwriting_properties;
  if (const Event::Properties* event_properties = event.properties()) {
    const std::optional<uint32_t> pointer_id = GetEventPropertyValue<uint32_t>(
        *event_properties, kPropertyHandwritingPointerId);
    const std::optional<uint64_t> stroke_id = GetEventPropertyValue<uint64_t>(
        *event_properties, kPropertyHandwritingStrokeId);

    const std::optional<float> pixels_per_inch_x = GetEventPropertyValue<float>(
        *event_properties, kPropertyHandwritingPixelsPerInchX);
    const std::optional<float> pixels_per_inch_y = GetEventPropertyValue<float>(
        *event_properties, kPropertyHandwritingPixelsPerInchY);

    if (pointer_id) {
      CHECK(stroke_id);
      CHECK(pixels_per_inch_x);
      CHECK(pixels_per_inch_y);
      handwriting_properties.emplace(
          *pointer_id, *stroke_id,
          gfx::Vector2dF(*pixels_per_inch_x, *pixels_per_inch_y));
    }
  }
  return handwriting_properties;
}

uint64_t GetHandwritingStrokeId(uint32_t pointer_id) {
  static const GetHandwritingStrokeIdForPointerFunc
      kGetHandwritingStrokeIdForPointerFunc =
          GetHandwritingStrokeIdForPointerFuncFromModule();

  if (!kGetHandwritingStrokeIdForPointerFunc) [[unlikely]] {
    TRACE_EVENT1("ime", "GetHandwritingStrokeIdForPointer", "func_pointer",
                 "nullptr");
    // TODO(crbug.com/355578906): Add telemetry.
    return 0;
  }

  uint64_t stroke_id;
  const HRESULT hr =
      kGetHandwritingStrokeIdForPointerFunc(pointer_id, &stroke_id);
  if (FAILED(hr)) [[unlikely]] {
    TRACE_EVENT1("ime", "GetHandwritingStrokeIdForPointer", "hr", hr);
    // TODO(crbug.com/355578906): Add telemetry.
    stroke_id = 0;
  }

  return stroke_id;
}

}  // namespace ui
