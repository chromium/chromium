// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/stylus_handwriting_properties_win.h"

#include <ShellHandwriting.h>

#include "base/debug/dump_without_crashing.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace ui {

namespace {

using GetHandwritingStrokeIdForPointerFunc = HRESULT (*)(uint32_t, uint64_t*);

constexpr char kPropertyHandwritingPointerId[] = "handwriting_pointer_id";
constexpr char kPropertyHandwritingStrokeId[] = "handwriting_stroke_id";

GetHandwritingStrokeIdForPointerFunc
GetHandwritingStrokeIdForPointerFuncFromModule() {
  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"msctf.dll",
                         &module)) {
    return reinterpret_cast<GetHandwritingStrokeIdForPointerFunc>(
        GetProcAddress(module, "GetHandwritingStrokeIdForPointer"));
  }

  return nullptr;
}

}  // namespace

const char* GetPropertyHandwritingPointerIdKeyForTesting() {
  return kPropertyHandwritingPointerId;
}

const char* GetPropertyHandwritingStrokeIdKeyForTesting() {
  return kPropertyHandwritingStrokeId;
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
}

std::optional<StylusHandwritingPropertiesWin> GetStylusHandwritingProperties(
    const Event& event) {
  std::optional<StylusHandwritingPropertiesWin> handwriting_properties;
  if (const Event::Properties* event_properties = event.properties()) {
    auto it = event_properties->find(kPropertyHandwritingPointerId);
    if (it != event_properties->end()) {
      CHECK_EQ(it->second.size(), sizeof(uint32_t));
      uint32_t handwriting_pointer_id = 0;
      std::memcpy(&handwriting_pointer_id, it->second.data(),
                  it->second.size());
      handwriting_properties =
          std::make_optional<StylusHandwritingPropertiesWin>();
      handwriting_properties->handwriting_pointer_id = handwriting_pointer_id;
    }

    it = event_properties->find(kPropertyHandwritingStrokeId);
    if (it != event_properties->end()) {
      CHECK_EQ(it->second.size(), sizeof(uint64_t));
      uint64_t handwriting_stroke_id = 0;
      std::memcpy(&handwriting_stroke_id, it->second.data(), it->second.size());
      if (!handwriting_properties.has_value()) [[unlikely]] {
        handwriting_properties =
            std::make_optional<StylusHandwritingPropertiesWin>();
      }
      handwriting_properties->handwriting_stroke_id = handwriting_stroke_id;
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
