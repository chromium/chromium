// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ENUM_UTIL_H_
#define UI_ACCESSIBILITY_AX_ENUM_UTIL_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

// ax::mojom::Event
AX_BASE_EXPORT const char* ToString(ax::mojom::Event event);

// ax::mojom::Role
AX_BASE_EXPORT const char* ToString(ax::mojom::Role role);
AX_BASE_EXPORT ax::mojom::Role StringToRole(const std::string& role);

// ax::mojom::State
AX_BASE_EXPORT const char* ToString(ax::mojom::State state);
AX_BASE_EXPORT ax::mojom::State StringToState(const std::string& state);

// ax::mojom::Action
AX_BASE_EXPORT const char* ToString(ax::mojom::Action action);

// ax::mojom::ActionFlags
AX_BASE_EXPORT const char* ToString(ax::mojom::ActionFlags action_flags);

// ax::mojom::DefaultActionVerb
AX_BASE_EXPORT const char* ToString(
    ax::mojom::DefaultActionVerb default_action_verb);

// ax::mojom::Mutation
AX_BASE_EXPORT const char* ToString(ax::mojom::Mutation mutation);

// ax::mojom::StringAttribute
AX_BASE_EXPORT const char* ToString(
    ax::mojom::StringAttribute string_attribute);
AX_BASE_EXPORT ax::mojom::StringAttribute StringToStringAttribute(
    const std::string& string_attribute);

// ax::mojom::IntAttribute
AX_BASE_EXPORT const char* ToString(ax::mojom::IntAttribute int_attribute);
AX_BASE_EXPORT ax::mojom::IntAttribute StringToIntAttribute(
    const std::string& int_attribute);

// ax::mojom::FloatAttribute
AX_BASE_EXPORT const char* ToString(ax::mojom::FloatAttribute float_attribute);

// ax::mojom::BoolAttribute
AX_BASE_EXPORT const char* ToString(ax::mojom::BoolAttribute bool_attribute);
AX_BASE_EXPORT ax::mojom::BoolAttribute StringToBoolAttribute(
    const std::string& bool_attribute);

// ax::mojom::IntListAttribute
AX_BASE_EXPORT const char* ToString(
    ax::mojom::IntListAttribute int_list_attribute);

// ax::mojom::StringListAttribute
AX_BASE_EXPORT const char* ToString(
    ax::mojom::StringListAttribute string_list_attribute);

// ax::mojom::ListStyle
AX_BASE_EXPORT const char* ToString(ax::mojom::ListStyle list_style);

// ax::mojom::MarkerType
AX_BASE_EXPORT const char* ToString(ax::mojom::MarkerType marker_type);

// ax::mojom::HighlightType
AX_BASE_EXPORT const char* ToString(ax::mojom::HighlightType highlight_type);

// ax::mojom::MoveDirection
AX_BASE_EXPORT const char* ToString(ax::mojom::MoveDirection move_direction);

// ax::mojom::Command
AX_BASE_EXPORT const char* ToString(ax::mojom::Command command);

// ax::mojom::InputEventType
AX_BASE_EXPORT const char* ToString(ax::mojom::InputEventType input_event_type);

// ax::mojom::TextBoundary
AX_BASE_EXPORT const char* ToString(ax::mojom::TextBoundary text_boundary);

// ax::mojom::TextAlign
AX_BASE_EXPORT const char* ToString(ax::mojom::TextAlign text_align);

// ax::mojom::WritingDirection
AX_BASE_EXPORT const char* ToString(
    ax::mojom::WritingDirection writing_direction);

// ax::mojom::TextPosition
AX_BASE_EXPORT const char* ToString(ax::mojom::TextPosition text_position);

// ax::mojom::TextStyle
AX_BASE_EXPORT const char* ToString(ax::mojom::TextStyle text_style);

// ax:mojom::TextDecorationStyle
AX_BASE_EXPORT const char* ToString(
    ax::mojom::TextDecorationStyle text_decoration_style);

// ax::mojom::AriaCurrentState
AX_BASE_EXPORT const char* ToString(
    ax::mojom::AriaCurrentState aria_current_state);

// ax::mojom::HasPopup
AX_BASE_EXPORT const char* ToString(ax::mojom::HasPopup has_popup);

// ax::mojom::IsPopup
AX_BASE_EXPORT const char* ToString(ax::mojom::IsPopup is_popup);

// ax::mojom::InvalidState
AX_BASE_EXPORT const char* ToString(ax::mojom::InvalidState invalid_state);

// ax::mojom::Restriction
AX_BASE_EXPORT const char* ToString(ax::mojom::Restriction restriction);

// ax::mojom::CheckedState
AX_BASE_EXPORT const char* ToString(ax::mojom::CheckedState checked_state);

// ax::mojom::SortDirection
AX_BASE_EXPORT const char* ToString(ax::mojom::SortDirection sort_direction);

// ax::mojom::NameFrom
AX_BASE_EXPORT const char* ToString(ax::mojom::NameFrom name_from);

// ax::mojom::DescriptionFrom
AX_BASE_EXPORT const char* ToString(
    ax::mojom::DescriptionFrom description_from);

// ax::mojom::DetailsFrom
AX_BASE_EXPORT const char* ToString(ax::mojom::DetailsFrom details_from);

// ax::mojom::EventFrom
AX_BASE_EXPORT const char* ToString(ax::mojom::EventFrom event_from);

// ax::mojom::Gesture
AX_BASE_EXPORT const char* ToString(ax::mojom::Gesture gesture);

// ax::mojom::TextAffinity
AX_BASE_EXPORT const char* ToString(ax::mojom::TextAffinity text_affinity);

// ax::mojom::TreeOrder
AX_BASE_EXPORT const char* ToString(ax::mojom::TreeOrder tree_order);

// ax::mojom::ImageAnnotationStatus
AX_BASE_EXPORT const char* ToString(ax::mojom::ImageAnnotationStatus status);

// ax::mojom::AriaNotificationInterrupt
AX_BASE_EXPORT const char* ToString(
    ax::mojom::AriaNotificationInterrupt interrupt);

// ax::mojom::AriaNotificationPriority
AX_BASE_EXPORT const char* ToString(
    ax::mojom::AriaNotificationPriority priority);

template <typename T>
std::optional<T> MaybeParseAXEnum(const char* attribute) {
  static const base::NoDestructor<base::flat_map<std::string, T>> attr_map([] {
    static_assert(T::kNone == T::kMinValue);
    static_assert(base::to_underlying(T::kNone) == 0);
    std::vector<std::pair<std::string, T>> entries;
    // Normally, this would be max - min + 1, but as a special case, the empty
    // string always maps to kNone as well.
    entries.reserve(base::to_underlying(T::kMaxValue) -
                    base::to_underlying(T::kMinValue) + 2);
    entries.push_back({"", T::kNone});
    for (auto i = base::to_underlying(T::kMinValue);
         i <= base::to_underlying(T::kMaxValue); ++i) {
      T attr = T{i};
      std::string str = ToString(attr);
      entries.push_back({std::move(str), attr});
    }
    return base::flat_map(std::move(entries));
  }());
  auto iter = attr_map->find(attribute);
  if (iter != attr_map->end()) {
    return iter->second;
  }
  return std::nullopt;
}

// Convert from the string representation of an enum defined in ax_enums.mojom
// into the enum value. The first time this is called, builds up a map.
// Relies on the existence of ToString(enum).
template <typename T>
T ParseAXEnum(const char* attribute) {
  auto result = MaybeParseAXEnum<T>(attribute);
  if (result.has_value()) {
    return result.value();
  }

  LOG(ERROR) << "Could not parse: " << attribute;
  NOTREACHED_IN_MIGRATION();
  return T::kNone;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ENUM_UTIL_H_
