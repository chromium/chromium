// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/actions/actions.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace {

class GlobalActionManager : public actions::ActionManager {
 public:
  GlobalActionManager() = default;
  GlobalActionManager(const GlobalActionManager&) = delete;
  GlobalActionManager& operator=(const GlobalActionManager&) = delete;
  ~GlobalActionManager() override = default;
};

absl::optional<GlobalActionManager>& GetGlobalManager() {
  static base::NoDestructor<absl::optional<GlobalActionManager>> manager;
  return *manager;
}

}  // namespace

namespace actions {

ActionList::ActionList(Delegate* delegate) : delegate_(delegate) {}

ActionList::~ActionList() = default;

ActionItem* ActionList::AddAction(std::unique_ptr<ActionItem> action_item) {
  ActionItem* result = action_item.get();
  children_.push_back(std::move(action_item));
  if (delegate_) {
    delegate_->ActionListChanged();
  }
  return result;
}

std::unique_ptr<ActionItem> ActionList::RemoveAction(ActionItem* action_item) {
  auto result = std::find_if(
      children_.begin(), children_.end(),
      [action_item](auto& item) { return item.get() == action_item; });
  if (result != children_.end()) {
    auto result_item = std::move(*result);
    children_.erase(result);
    if (delegate_) {
      delegate_->ActionListChanged();
    }
    return result_item;
  }
  return nullptr;
}

ActionItem::ActionItem() = default;

ActionItem::ActionItem(InvokeActionCallback callback)
    : callback_(std::move(callback)) {}

ActionItem::~ActionItem() = default;

absl::optional<ActionId> ActionItem::GetActionId() const {
  return action_id_;
}

void ActionItem::SetActionId(absl::optional<ActionId> action_id) {
  if (action_id_ == action_id) {
    return;
  }
  action_id_ = action_id;
  ActionItemChanged();
}

ui::Accelerator ActionItem::GetAccelerator() const {
  return accelerator_;
}

void ActionItem::SetAccelerator(ui::Accelerator accelerator) {
  if (accelerator_ == accelerator) {
    return;
  }
  accelerator_ = accelerator;
  ActionItemChanged();
}

bool ActionItem::GetEnabled() const {
  return enabled_;
}

void ActionItem::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  ActionItemChanged();
}

const ui::ImageModel& ActionItem::GetImage() const {
  return image_;
}

void ActionItem::SetImage(const ui::ImageModel& image) {
  if (image_ == image) {
    return;
  }
  image_ = image;
  ActionItemChanged();
}

const std::u16string ActionItem::GetText() const {
  return text_;
}

void ActionItem::SetText(const std::u16string& text) {
  if (text_ == text) {
    return;
  }
  text_ = text;
  ActionItemChanged();
}

const std::u16string ActionItem::GetTooltipText() const {
  return tooltip_;
}

void ActionItem::SetTooltipText(const std::u16string& tooltip) {
  if (tooltip_ == tooltip) {
    return;
  }
  tooltip_ = tooltip;
  ActionItemChanged();
}

ActionItem* ActionItem::GetParent() const {
  return parent_.get();
}

bool ActionItem::GetVisible() const {
  return visible_;
}

void ActionItem::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }
  visible_ = visible;
  ActionItemChanged();
}

void ActionItem::SetInvokeActionCallback(InvokeActionCallback callback) {
  if (callback_ == callback) {
    return;
  }
  callback_ = std::move(callback);
  ActionItemChanged();
}

ActionItem* ActionItem::AddChild(std::unique_ptr<ActionItem> action_item) {
  return children_.AddAction(std::move(action_item));
}

std::unique_ptr<ActionItem> ActionItem::RemoveChild(ActionItem* action_item) {
  return children_.RemoveAction(action_item);
}

[[nodiscard]] base::CallbackListSubscription
ActionItem::AddActionChangedCallback(ActionChangedCallback callback) {
  return AddPropertyChangedCallback(this, callback);
}

// Alternative terms used to identify this action. Used for search indexing
void ActionItem::AddSynonyms(std::initializer_list<std::u16string> synonyms) {
  synonyms_.insert(synonyms_.end(), synonyms);
}

void ActionItem::InvokeAction() {
  if (callback_) {
    callback_.Run();
  }
}

void ActionItem::ActionListChanged() {
  ActionItemChanged();
}

void ActionItem::ActionItemChanged() {
  TriggerChangedCallback(this);
}

BEGIN_METADATA_BASE(ActionItem)
ADD_PROPERTY_METADATA(ui::Accelerator, Accelerator)
ADD_PROPERTY_METADATA(bool, Enabled)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_PROPERTY_METADATA(std::u16string, TooltipText)
ADD_PROPERTY_METADATA(bool, Visible)
END_METADATA

ActionManager::ActionManager() {
  ResetActionItemInitializerList();
}

ActionManager::~ActionManager() = default;

// static
ActionManager& ActionManager::Get() {
  absl::optional<GlobalActionManager>& manager = GetGlobalManager();
  if (!manager.has_value()) {
    manager.emplace();
  }
  return manager.value();
}

// static
ActionManager& ActionManager::GetForTesting() {
  return Get();
}

// static
void ActionManager::ResetForTesting() {
  GetGlobalManager().reset();
}

void ActionManager::IndexActions() {
  if (root_action_list_->empty() && !initializer_list_->empty()) {
    initializer_list_->Notify(this);
  }
}

ActionItem* ActionManager::FindAction(std::u16string term) {
  IndexActions();
  return nullptr;
}

ActionItem* ActionManager::FindAction(ActionId action_id) {
  IndexActions();
  auto iter = std::find_if(root_action_list_->children().begin(),
                           root_action_list_->children().end(),
                           [action_id](auto& item) {
                             auto id = item->GetActionId();
                             return id && *id == action_id;
                           });
  if (iter != root_action_list_->children().end()) {
    return (*iter).get();
  }
  return nullptr;
}

ActionItem* ActionManager::FindAction(const ui::KeyEvent& key_event) {
  IndexActions();
  return nullptr;
}

ActionItem* ActionManager::AddAction(std::unique_ptr<ActionItem> action_item) {
  return root_action_list_->AddAction(std::move(action_item));
}

std::unique_ptr<ActionItem> ActionManager::RemoveAction(
    ActionItem* action_item) {
  return root_action_list_->RemoveAction(action_item);
}

void ActionManager::ResetActions() {
  root_action_list_ = std::make_unique<ActionList>(this);
}

void ActionManager::ResetActionItemInitializerList() {
  ResetActions();
  initializer_list_ = std::make_unique<ActionItemInitializerList>();
  initializer_subscriptions_.clear();
}

void ActionManager::AppendActionItemInitializer(
    ActionItemInitializerList::CallbackType initializer) {
  DCHECK(initializer_list_);
  ResetActions();

  initializer_subscriptions_.push_back(
      initializer_list_->Add(std::move(initializer)));
}

void ActionManager::ActionListChanged() {}

BEGIN_METADATA_BASE(ActionManager)
END_METADATA

}  // namespace actions
