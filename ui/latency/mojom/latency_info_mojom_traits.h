// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_MOJOM_LATENCY_INFO_MOJOM_TRAITS_H_
#define UI_LATENCY_MOJOM_LATENCY_INFO_MOJOM_TRAITS_H_

#include "ui/latency/latency_info.h"
#include "ui/latency/mojom/latency_info.mojom-shared.h"

namespace mojo {

static_assert(static_cast<int>(ui::mojom::LatencyComponentType::kMaxValue) ==
                  static_cast<int>(ui::LATENCY_COMPONENT_TYPE_LAST),
              "Enum size mismatch");

static_assert(static_cast<int>(ui::mojom::SourceEventType::kMaxValue) ==
                  static_cast<int>(ui::SourceEventType::LAST),
              "Enum size mismatch");

template <>
struct ArrayTraits<ui::LatencyInfo::LatencyMap> {
  using Element = ui::LatencyInfo::LatencyMap::value_type;
  using Iterator = ui::LatencyInfo::LatencyMap::iterator;
  using ConstIterator = ui::LatencyInfo::LatencyMap::const_iterator;

  static ConstIterator GetBegin(const ui::LatencyInfo::LatencyMap& input) {
    return input.begin();
  }
  static Iterator GetBegin(ui::LatencyInfo::LatencyMap& input) {
    return input.begin();
  }

  static void AdvanceIterator(ConstIterator& iterator) { iterator++; }
  static void AdvanceIterator(Iterator& iterator) { iterator++; }

  static const Element& GetValue(ConstIterator& iterator) { return *iterator; }
  static Element& GetValue(Iterator& iterator) { return *iterator; }

  static size_t GetSize(const ui::LatencyInfo::LatencyMap& input) {
    return input.size();
  }
};

template <>
struct StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo> {
  static const std::string& trace_name(const ui::LatencyInfo& info);
  static const ui::LatencyInfo::LatencyMap& latency_components(
      const ui::LatencyInfo& info);
  static int64_t trace_id(const ui::LatencyInfo& info);
  static ukm::SourceId ukm_source_id(const ui::LatencyInfo& info);
  static bool coalesced(const ui::LatencyInfo& info);
  static bool began(const ui::LatencyInfo& info);
  static bool terminated(const ui::LatencyInfo& info);
  static ui::mojom::SourceEventType source_event_type(
      const ui::LatencyInfo& info);
  static float scroll_update_delta(const ui::LatencyInfo& info);
  static float predicted_scroll_update_delta(const ui::LatencyInfo& info);
  static bool Read(ui::mojom::LatencyInfoDataView data, ui::LatencyInfo* out);
};

template <>
struct EnumTraits<ui::mojom::LatencyComponentType, ui::LatencyComponentType> {
  static ui::mojom::LatencyComponentType ToMojom(ui::LatencyComponentType type);
  static bool FromMojom(ui::mojom::LatencyComponentType input,
                        ui::LatencyComponentType* output);
};

}  // namespace mojo

#endif  // UI_LATENCY_MOJOM_LATENCY_INFO_MOJOM_TRAITS_H_
