// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/tracked_element_handler.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/interaction/element_highlighter.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/webui/tracked_element/element_highlighter_webui.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#endif

namespace ui {

#if !BUILDFLAG(IS_ANDROID)
namespace {

views::WebView* FindWebViewWithContentsRecursive(
    views::View* from_view,
    const content::WebContents* contents) {
  if (!from_view || !contents) {
    return nullptr;
  }
  auto* const web_view = views::AsViewClass<views::WebView>(from_view);
  if (web_view && web_view->web_contents() == contents) {
    return web_view;
  }

  for (views::View* const child_view : from_view->children()) {
    auto* const result = FindWebViewWithContentsRecursive(child_view, contents);
    if (result) {
      return result;
    }
  }

  return nullptr;
}

}  // namespace
#endif

TrackedElementHandler::TrackedElementHandler(
    content::WebUIController* controller)
    : TrackedElementHandler(
          controller->web_ui()->GetWebContents(),
          ui::ElementContext(controller,
                             base::PassKey<TrackedElementHandler>())) {}

TrackedElementHandler::TrackedElementHandler(content::WebContents* web_contents,
                                             ui::ElementContext context)
    : context_(context), receiver_(this) {
  ui::ElementHighlighter::GetElementHighlighter()
      ->MaybeRegisterBackend<ElementHighlighterWebUI>();

  if (web_contents) {
    Observe(web_contents);
    is_web_contents_visible_ =
        web_contents->GetVisibility() == content::Visibility::VISIBLE;
  }
}

TrackedElementHandler::~TrackedElementHandler() = default;

void TrackedElementHandler::BindInterface(
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void TrackedElementHandler::OnVisibilityChanged(
    content::Visibility new_visibility) {
  const bool visible = new_visibility == content::Visibility::VISIBLE;
  if (visible == is_web_contents_visible_) {
    return;
  }
  is_web_contents_visible_ = visible;
  UpdateAllEffectiveVisibilities();
}

void TrackedElementHandler::UpdateAllEffectiveVisibilities() {
  // This is complicated because it is possible that UpdateEffectiveVisibility
  // could invoke this class's destructor.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  for (auto& [_, elements] : elements_) {
    for (auto& [_, element] : elements) {
      element->UpdateEffectiveVisibility();
      if (!weak_ptr) {
        return;
      }
    }
  }
}

void TrackedElementHandler::SetHighlightState(
    TrackedElementWebUI& element,
    bool highlight,
    base::PassKey<TrackedElementWebUI>) {
  if (manager_remote_) {
    manager_remote_->OnElementHighlightChanged(
        tracked_element::mojom::TrackedElementIdentifier::New(
            element.identifier().GetName(), element.GetSecondaryIdentifier()),
        highlight);
  }
}

void TrackedElementHandler::FlushManagerRemoteForTesting() {
  manager_remote_.FlushForTesting();  // IN-TEST
}

bool TrackedElementHandler::ClickElement(
    TrackedElementWebUI& element,
    tracked_element::mojom::InputType input_type) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->ClickElement(
      tracked_element::mojom::TrackedElementIdentifier::New(
          element.identifier().GetName(), element.GetSecondaryIdentifier()),
      input_type,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::FocusElement(TrackedElementWebUI& element) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->FocusElement(
      tracked_element::mojom::TrackedElementIdentifier::New(
          element.identifier().GetName(), element.GetSecondaryIdentifier()),
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::SelectTab(TrackedElementWebUI& element,
                                      size_t index) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->SelectTab(
      tracked_element::mojom::TrackedElementIdentifier::New(
          element.identifier().GetName(), element.GetSecondaryIdentifier()),
      index,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::SelectDropdownItem(TrackedElementWebUI& element,
                                               size_t index) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->SelectDropdownItem(
      tracked_element::mojom::TrackedElementIdentifier::New(
          element.identifier().GetName(), element.GetSecondaryIdentifier()),
      index,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::EnterText(
    TrackedElementWebUI& element,
    const std::u16string& text,
    tracked_element::mojom::TextEntryMode mode) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->EnterText(
      tracked_element::mojom::TrackedElementIdentifier::New(
          element.identifier().GetName(), element.GetSecondaryIdentifier()),
      text, mode,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::Confirm(TrackedElementWebUI& element) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->Confirm(
      tracked_element::mojom::TrackedElementIdentifier::New(
          element.identifier().GetName(), element.GetSecondaryIdentifier()),
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

void TrackedElementHandler::SetManager(
    mojo::PendingRemote<tracked_element::mojom::TrackedElementManager>
        manager) {
  manager_remote_.reset();
  manager_remote_.Bind(std::move(manager));
}

void TrackedElementHandler::TrackedElementVisibilityChanged(
    tracked_element::mojom::TrackedElementIdentifierPtr id,
    bool visible,
    const gfx::RectF& rect) {
  TrackedElementWebUI* const element =
      GetElement(id, /*create_if_not_present=*/visible);
  if (!element) {
    if (visible) {
      ReportBadMessage(
          "TrackedElementVisibilityChanged(true) for invalid element.", id);
    }
    return;
  }

  std::unique_ptr<TrackedElementWebUI> to_be_discarded;
  if (!visible) {
    // There are a finite and small number of registered `ui::ElementIdentifier`
    // values, so the top-level map of `elements_` stays small regardless of how
    // many elements are created or destroyed.
    //
    // However, since many elements may be created or destroyed in the same
    // WebUI, the size of each secondary ID map is potentially unbounded and
    // could grow with each new element created.
    //
    // To avoid this, and because `ui::TrackedElement` objects aren't guaranteed
    // to stick around after loss of visibility, we delete elements that have
    // lost visibility.

    // Pull the no-longer-visible element out of the map and let it go away at
    // the end of the function, after the visibility is set and callbacks have
    // been called.
    //
    // This ensures that if there are other changes to the map (up to and
    // including another element being created with the same secondary
    // identifier), everything should continue to function as normal.
    auto& secondary_map = elements_[id->native_identifier];
    const auto it = secondary_map.find(id->secondary_identifier);
    CHECK(it != secondary_map.end());
    to_be_discarded = std::move(it->second);
    secondary_map.erase(it);
  }

  element->SetRawVisible(visible, rect);
}

void TrackedElementHandler::TrackedElementActivated(
    tracked_element::mojom::TrackedElementIdentifierPtr id) {
  TrackedElementWebUI* const element = GetElement(id);
  if (!element || !element->visible()) {
    ReportBadMessage(
        "TrackedElementActivated for nonexistent or non-visible element.", id);
    return;
  }
  element->Activate();
}

void TrackedElementHandler::TrackedElementCustomEvent(
    tracked_element::mojom::TrackedElementIdentifierPtr id,
    const std::string& event_name) {
  TrackedElementWebUI* const element = GetElement(id);
  if (!element || !element->visible()) {
    const auto message = base::StringPrintf(
        "TrackedElementCustomEvent of type \"%s\" for nonexistent or "
        "non-visible element.",
        event_name);
    ReportBadMessage(message, id);
    return;
  }
  const ui::CustomElementEventType event_type =
      ui::CustomElementEventType::FromName(event_name.c_str());
  if (!event_type) {
    ReportBadMessage(
        base::StringPrintf(
            "TrackedElementCustomEvent received invalid event name \"%s\".",
            event_name),
        id);
    return;
  }
  element->CustomEvent(event_type);
}

void TrackedElementHandler::TrackedElementCanHighlightChanged(
    tracked_element::mojom::TrackedElementIdentifierPtr id,
    bool can_highlight) {
  CHECK(id);
  TrackedElementWebUI* const element =
      GetElement(id,
                 /*create_if_not_present=*/can_highlight);
  if (!element) {
    if (can_highlight) {
      ReportBadMessage("TrackedElementCanHighlightChanged for invalid element.",
                       id);
    }
    return;
  }
  element->set_can_highlight(can_highlight);
}

void TrackedElementHandler::RegisterIdentifier(ui::ElementIdentifier id) {
  elements_.emplace(id.GetName(), SecondaryIdentifierMapType());
}

std::vector<std::string> TrackedElementHandler::GetIdentifiers() {
  std::vector<std::string> identifiers;
  identifiers.reserve(elements_.size());
  for (const auto& [identifier, _] : elements_) {
    identifiers.emplace_back(identifier);
  }
  return identifiers;
}

TrackedElementWebUI* TrackedElementHandler::GetElement(
    const tracked_element::mojom::TrackedElementIdentifierPtr& id,
    bool create_if_not_present) {
  const std::string& name = id->native_identifier;
  const std::string& secondary_identifier = id->secondary_identifier;
  if (name.empty()) {
    LOG(ERROR) << "Empty ElementIdentifier for TrackedElement via IPC.";
    return nullptr;
  }
  if (secondary_identifier.empty()) {
    LOG(ERROR)
        << "Empty sub-identifier received via IPC for TrackedElement of type \""
        << secondary_identifier << "\"";
    return nullptr;
  }
  auto it = elements_.find(name);
  if (it == elements_.end()) {
    NOTREACHED(base::NotFatalUntil::M153)
        << "ElementIdentifier \"" << name << "\" not registered for "
        << web_contents()->GetURL();
    return nullptr;
  }
  auto it2 = it->second.find(secondary_identifier);
  if (it2 == it->second.end()) {
    if (!create_if_not_present) {
      return nullptr;
    }
    it2 = it->second
              .emplace(secondary_identifier,
                       std::make_unique<TrackedElementWebUI>(
                           this, ElementIdentifier::FromName(name.c_str()),
                           secondary_identifier, context()))
              .first;
  }
  return it2->second.get();
}

void TrackedElementHandler::ReportBadMessage(
    std::string_view description,
    const tracked_element::mojom::TrackedElementIdentifierPtr& id) {
  receiver_.ReportBadMessage(base::StringPrintf(
      "%s Element has native id \"%s\" and secondary identifier "
      "\"%s\"",
      description, id->native_identifier, id->secondary_identifier));
}

#if !BUILDFLAG(IS_ANDROID)
void TrackedElementHandler::SetWebViewForTesting(views::WebView* web_view) {
  web_view_tracker_.SetView(web_view);
}

views::WebView* TrackedElementHandler::GetWebView() const {
  if (!web_contents()) {
    return nullptr;
  }
  if (auto* const view = web_view_tracker_.view()) {
    if (auto* const web_view = views::AsViewClass<views::WebView>(view)) {
      if (web_view->web_contents() == web_contents()) {
        return web_view;
      }
    }
  }
  auto* const widget = views::Widget::GetWidgetForNativeWindow(
      web_contents()->GetTopLevelNativeWindow());
  if (!widget) {
    return nullptr;
  }
  auto* const web_view = FindWebViewWithContentsRecursive(
      widget->GetContentsView(), web_contents());
  if (web_view) {
    web_view_tracker_.SetView(web_view);
  }
  return web_view;
}
#endif

}  // namespace ui
