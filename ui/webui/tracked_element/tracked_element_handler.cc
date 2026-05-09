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
#include "ui/base/interaction/element_highlighter.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/webui/tracked_element/element_highlighter_webui.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace ui {

TrackedElementHandler::TrackedElementHandler(
    content::WebContents* web_contents,
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        receiver,
    ui::ElementContext context,
    const std::vector<ui::ElementIdentifier>& identifiers)
    : context_(context),
      receiver_(this, std::move(receiver)) {
  ui::ElementHighlighter::GetElementHighlighter()
      ->MaybeRegisterBackend<ElementHighlighterWebUI>();

  if (web_contents) {
    Observe(web_contents);
    is_web_contents_visible_ =
        web_contents->GetVisibility() == content::Visibility::VISIBLE;
  }

  for (const ui::ElementIdentifier& id : identifiers) {
    elements_[id.GetName()] =
        std::make_unique<TrackedElementWebUI>(this, id, context);
  }
}

TrackedElementHandler::~TrackedElementHandler() = default;

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
  for (auto& [_, element] : elements_) {
    element->UpdateEffectiveVisibility();
    if (!weak_ptr) {
      return;
    }
  }
}

void TrackedElementHandler::SetHighlightState(
    const std::string& identifier_name,
    bool highlight) {
  if (manager_remote_) {
    manager_remote_->OnElementHighlightChanged(identifier_name, highlight);
  }
}

void TrackedElementHandler::FlushManagerRemoteForTesting() {
  manager_remote_.FlushForTesting();  // IN-TEST
}

bool TrackedElementHandler::ClickElement(const std::string& identifier_name) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->ClickElement(
      identifier_name,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::FocusElement(const std::string& identifier_name) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->FocusElement(
      identifier_name,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::SelectTab(const std::string& identifier_name,
                                      size_t index) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->SelectTab(
      identifier_name, index,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::SelectDropdownItem(
    const std::string& identifier_name,
    size_t index) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->SelectDropdownItem(
      identifier_name, index,
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
    const std::string& identifier_name,
    const std::u16string& text,
    tracked_element::mojom::TextEntryMode mode) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->EnterText(
      identifier_name, text, mode,
      base::BindOnce(
          [](bool* success_ptr, base::OnceClosure quit_closure, bool result) {
            *success_ptr = result;
            std::move(quit_closure).Run();
          },
          &success, run_loop.QuitClosure()));
  run_loop.Run();
  return success;
}

bool TrackedElementHandler::Confirm(const std::string& identifier_name) {
  CHECK_IS_TEST();
  if (!manager_remote_) {
    return false;
  }
  bool success = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  manager_remote_->Confirm(
      identifier_name,
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
    const std::string& identifier_name,
    bool visible,
    const gfx::RectF& rect) {
  TrackedElementWebUI* element = GetElement(identifier_name);
  if (!element) {
    return;
  }
  element->SetRawVisible(visible, rect);
}

void TrackedElementHandler::TrackedElementActivated(
    const std::string& identifier_name) {
  TrackedElementWebUI* const element = GetElement(identifier_name);
  if (!element) {
    return;
  }
  if (!element->visible()) {
    receiver_.ReportBadMessage(
        base::StringPrintf("TrackedElementActivated message received for "
                           "anchor element \"%s\" but element was not visible.",
                           identifier_name.c_str()));
    return;
  }
  element->Activate();
}

void TrackedElementHandler::TrackedElementCustomEvent(
    const std::string& identifier_name,
    const std::string& event_name) {
  TrackedElementWebUI* const element = GetElement(identifier_name);
  if (!element) {
    return;
  }
  if (!element->visible()) {
    receiver_.ReportBadMessage(
        base::StringPrintf("TrackedElementCustomEvent message received for "
                           "anchor element \"%s\" but element was not visible.",
                           identifier_name.c_str()));
    return;
  }
  const ui::CustomElementEventType event_type =
      ui::CustomElementEventType::FromName(event_name.c_str());
  if (!event_type) {
    return;
  }
  element->CustomEvent(event_type);
}

void TrackedElementHandler::TrackedElementCanHighlightChanged(
    const std::string& identifier_name,
    bool can_highlight) {
  TrackedElementWebUI* const element = GetElement(identifier_name);
  if (!element) {
    return;
  }
  element->set_can_highlight(can_highlight);
}

std::unique_ptr<TrackedElementVisibilityLock>
TrackedElementHandler::LockVisible(const std::string& identifier_name) {
  TrackedElementWebUI* const element = GetElement(identifier_name);
  if (!element) {
    return nullptr;
  }
  return element->LockVisible();
}

TrackedElementWebUI* TrackedElementHandler::GetElement(
    const std::string& identifier_name) {
  auto it = elements_.find(identifier_name);
  if (it == elements_.end()) {
    LOG(ERROR) << "TrackedElement message received for element \""
               << identifier_name
               << "\" but element was not known to the handler.";
    return nullptr;
  }
  return it->second.get();
}

}  // namespace ui
