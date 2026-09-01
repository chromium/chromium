// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"

#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/controls/webview/webview.h"
#endif

namespace ui {

namespace {

tracked_element::mojom::InputType ToMojomInputType(
    ui::test::InteractionTestUtil::InputType input_type) {
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      return tracked_element::mojom::InputType::kDontCare;
    case ui::test::InteractionTestUtil::InputType::kMouse:
      return tracked_element::mojom::InputType::kMouse;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
      return tracked_element::mojom::InputType::kKeyboard;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      return tracked_element::mojom::InputType::kTouch;
  }
}

}  // namespace

InteractionTestUtilSimulatorWebUI::InteractionTestUtilSimulatorWebUI() =
    default;
InteractionTestUtilSimulatorWebUI::~InteractionTestUtilSimulatorWebUI() =
    default;

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::PressButton(
    ui::TrackedElement* element,
    InputType input_type) {
  if (auto* webui_el = element->AsA<TrackedElementWebUI>()) {
    if (webui_el->handler()->ClickElement(
            *webui_el, ToMojomInputType(input_type),
            base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::SelectMenuItem(
    ui::TrackedElement* element,
    InputType input_type) {
  if (auto* webui_el = element->AsA<TrackedElementWebUI>()) {
    if (webui_el->handler()->ClickElement(
            *webui_el, ToMojomInputType(input_type),
            base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::DoDefaultAction(
    ui::TrackedElement* element,
    InputType input_type) {
  if (auto* webui_el = element->AsA<TrackedElementWebUI>()) {
    if (webui_el->handler()->ClickElement(
            *webui_el, ToMojomInputType(input_type),
            base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::SelectTab(
    ui::TrackedElement* tab_collection,
    size_t index,
    InputType input_type,
    std::optional<size_t> expected_index_after_selection) {
  if (auto* webui_el = tab_collection->AsA<TrackedElementWebUI>()) {
    if (webui_el->handler()->SelectTab(
            *webui_el, index,
            base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::SelectDropdownItem(
    ui::TrackedElement* dropdown,
    size_t index,
    InputType input_type) {
  if (auto* webui_el = dropdown->AsA<TrackedElementWebUI>()) {
    if (webui_el->handler()->SelectDropdownItem(
            *webui_el, index,
            base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::EnterText(
    ui::TrackedElement* element,
    std::u16string text,
    TextEntryMode mode) {
  if (auto* webui_el = element->AsA<TrackedElementWebUI>()) {
    tracked_element::mojom::TextEntryMode mojom_mode;
    switch (mode) {
      case TextEntryMode::kReplaceAll:
        mojom_mode = tracked_element::mojom::TextEntryMode::kReplaceAll;
        break;
      case TextEntryMode::kInsertOrReplace:
        mojom_mode = tracked_element::mojom::TextEntryMode::kInsertOrReplace;
        break;
      case TextEntryMode::kAppend:
        mojom_mode = tracked_element::mojom::TextEntryMode::kAppend;
        break;
    }
    if (webui_el->handler()->EnterText(
            *webui_el, text, mojom_mode,
            base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::FocusElement(
    ui::TrackedElement* element) {
  if (auto* webui_el = element->AsA<TrackedElementWebUI>()) {
#if !BUILDFLAG(IS_ANDROID)
    if (auto* web_view = webui_el->GetWebView()) {
      web_view->RequestFocus();
    }
#endif
    if (webui_el->handler()->FocusElement(
            *webui_el, base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

ui::test::ActionResult InteractionTestUtilSimulatorWebUI::Confirm(
    ui::TrackedElement* element) {
  if (auto* webui_el = element->AsA<TrackedElementWebUI>()) {
    if (webui_el->handler()->Confirm(
            *webui_el, base::PassKey<InteractionTestUtilSimulatorWebUI>())) {
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kNotAttempted;
}

}  // namespace ui
