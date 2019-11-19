// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ENUM_UTIL_H_
#define UI_ACCESSIBILITY_AX_ENUM_UTIL_H_

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// ax::mojom::Event
AX_EXPORT const char* ToString(ax::mojom::Event event);
AX_EXPORT ax::mojom::Event ParseEvent(const char* event);

// ax::mojom::Role
AX_EXPORT const char* ToString(ax::mojom::Role role);
AX_EXPORT ax::mojom::Role ParseRole(const char* role);

// ax::mojom::State
AX_EXPORT const char* ToString(ax::mojom::State state);
AX_EXPORT ax::mojom::State ParseState(const char* state);

// ax::mojom::Action
AX_EXPORT const char* ToString(ax::mojom::Action action);
AX_EXPORT ax::mojom::Action ParseAction(const char* action);

// ax::mojom::ActionFlags
AX_EXPORT const char* ToString(ax::mojom::ActionFlags action_flags);
AX_EXPORT ax::mojom::ActionFlags ParseActionFlags(const char* action_flags);

// ax::mojom::DefaultActionVerb
AX_EXPORT const char* ToString(
    ax::mojom::DefaultActionVerb default_action_verb);
AX_EXPORT ax::mojom::DefaultActionVerb ParseDefaultActionVerb(
    const char* default_action_verb);

// ax::mojom::Mutation
AX_EXPORT const char* ToString(ax::mojom::Mutation mutation);
AX_EXPORT ax::mojom::Mutation ParseMutation(const char* mutation);

// ax::mojom::StringAttribute
AX_EXPORT const char* ToString(ax::mojom::StringAttribute string_attribute);
AX_EXPORT ax::mojom::StringAttribute ParseStringAttribute(
    const char* string_attribute);

// ax::mojom::IntAttribute
AX_EXPORT const char* ToString(ax::mojom::IntAttribute int_attribute);
AX_EXPORT ax::mojom::IntAttribute ParseIntAttribute(const char* int_attribute);

// ax::mojom::FloatAttribute
AX_EXPORT const char* ToString(ax::mojom::FloatAttribute float_attribute);
AX_EXPORT ax::mojom::FloatAttribute ParseFloatAttribute(
    const char* float_attribute);

// ax::mojom::BoolAttribute
AX_EXPORT const char* ToString(ax::mojom::BoolAttribute bool_attribute);
AX_EXPORT ax::mojom::BoolAttribute ParseBoolAttribute(
    const char* bool_attribute);

// ax::mojom::IntListAttribute
AX_EXPORT const char* ToString(ax::mojom::IntListAttribute int_list_attribute);
AX_EXPORT ax::mojom::IntListAttribute ParseIntListAttribute(
    const char* int_list_attribute);

// ax::mojom::StringListAttribute
AX_EXPORT const char* ToString(
    ax::mojom::StringListAttribute string_list_attribute);
AX_EXPORT ax::mojom::StringListAttribute ParseStringListAttribute(
    const char* string_list_attribute);

// ax::mojom::ListStyle
AX_EXPORT const char* ToString(ax::mojom::ListStyle list_style);
AX_EXPORT ax::mojom::ListStyle ParseListStyle(const char* list_style);

// ax::mojom::MarkerType
AX_EXPORT const char* ToString(ax::mojom::MarkerType marker_type);
AX_EXPORT ax::mojom::MarkerType ParseMarkerType(const char* marker_type);

// ax:mojom::TextDecorationStyle
AX_EXPORT const char* ToString(
    ax::mojom::TextDecorationStyle text_decoration_style);
AX_EXPORT ax::mojom::TextDecorationStyle ParseTextDecorationStyle(
    const char* text_decoration_style);

// ax::mojom::TextDirection
AX_EXPORT const char* ToString(ax::mojom::TextDirection text_direction);
AX_EXPORT ax::mojom::TextDirection ParseTextDirection(
    const char* text_direction);

// ax::mojom::TextPosition
AX_EXPORT const char* ToString(ax::mojom::TextPosition text_position);
AX_EXPORT ax::mojom::TextPosition ParseTextPosition(const char* text_position);

// ax::mojom::TextStyle
AX_EXPORT const char* ToString(ax::mojom::TextStyle text_style);
AX_EXPORT ax::mojom::TextStyle ParseTextStyle(const char* text_style);

// ax::mojom::AriaCurrentState
AX_EXPORT const char* ToString(ax::mojom::AriaCurrentState aria_current_state);
AX_EXPORT ax::mojom::AriaCurrentState ParseAriaCurrentState(
    const char* aria_current_state);

// ax::mojom::HasPopup
AX_EXPORT const char* ToString(ax::mojom::HasPopup has_popup);
AX_EXPORT ax::mojom::HasPopup ParseHasPopup(const char* has_popup);

// ax::mojom::InvalidState
AX_EXPORT const char* ToString(ax::mojom::InvalidState invalid_state);
AX_EXPORT ax::mojom::InvalidState ParseInvalidState(const char* invalid_state);

// ax::mojom::Restriction
AX_EXPORT const char* ToString(ax::mojom::Restriction restriction);
AX_EXPORT ax::mojom::Restriction ParseRestriction(const char* restriction);

// ax::mojom::CheckedState
AX_EXPORT const char* ToString(ax::mojom::CheckedState checked_state);
AX_EXPORT ax::mojom::CheckedState ParseCheckedState(const char* checked_state);

// ax::mojom::SortDirection
AX_EXPORT const char* ToString(ax::mojom::SortDirection sort_direction);
AX_EXPORT ax::mojom::SortDirection ParseSortDirection(
    const char* sort_direction);

// ax::mojom::NameFrom
AX_EXPORT const char* ToString(ax::mojom::NameFrom name_from);
AX_EXPORT ax::mojom::NameFrom ParseNameFrom(const char* name_from);

// ax::mojom::DescriptionFrom
AX_EXPORT const char* ToString(ax::mojom::DescriptionFrom description_from);
AX_EXPORT ax::mojom::DescriptionFrom ParseDescriptionFrom(
    const char* description_from);

// ax::mojom::EventFrom
AX_EXPORT const char* ToString(ax::mojom::EventFrom event_from);
AX_EXPORT ax::mojom::EventFrom ParseEventFrom(const char* event_from);

// ax::mojom::Gesture
AX_EXPORT const char* ToString(ax::mojom::Gesture gesture);
AX_EXPORT ax::mojom::Gesture ParseGesture(const char* gesture);

// ax::mojom::TextAffinity
AX_EXPORT const char* ToString(ax::mojom::TextAffinity text_affinity);
AX_EXPORT ax::mojom::TextAffinity ParseTextAffinity(const char* text_affinity);

// ax::mojom::TreeOrder
AX_EXPORT const char* ToString(ax::mojom::TreeOrder tree_order);
AX_EXPORT ax::mojom::TreeOrder ParseTreeOrder(const char* tree_order);

// ax::mojom::ImageAnnotationStatus
AX_EXPORT const char* ToString(ax::mojom::ImageAnnotationStatus status);
AX_EXPORT ax::mojom::ImageAnnotationStatus ParseImageAnnotationStatus(
    const char* status);

// ax::mojom::Dropeffect
AX_EXPORT const char* ToString(ax::mojom::Dropeffect dropeffect);
AX_EXPORT ax::mojom::Dropeffect ParseDropeffect(const char* dropeffect);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ENUM_UTIL_H_
