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

void ActionList::Reset() {
  children_.clear();
  if (delegate_) {
    delegate_->ActionListChanged();
  }
}

BaseAction::BaseAction() = default;

BaseAction::~BaseAction() = default;

BaseAction* BaseAction::GetParent() const {
  return parent_;
}

ActionItem* BaseAction::AddChild(std::unique_ptr<ActionItem> action_item) {
  DCHECK(!action_item->GetParent());
  action_item->parent_ = this;
  return children_.AddAction(std::move(action_item));
}

std::unique_ptr<ActionItem> BaseAction::RemoveChild(ActionItem* action_item) {
  DCHECK(action_item);
  DCHECK_EQ(action_item->GetParent(), this);
  action_item->parent_ = nullptr;
  return children_.RemoveAction(action_item);
}

void BaseAction::ActionListChanged() {}

void BaseAction::ResetActionList() {
  children_.Reset();
}

BEGIN_METADATA_BASE(BaseAction)
END_METADATA

ScopedActionUpdate::ScopedActionUpdate(ActionItem* action_item)
    : action_item_(action_item) {}

ScopedActionUpdate::ScopedActionUpdate(
    ScopedActionUpdate&& scoped_action_update)
    : action_item_(std::move(scoped_action_update.action_item_)) {
  scoped_action_update.action_item_ = nullptr;
}

ScopedActionUpdate& ScopedActionUpdate::operator=(
    ScopedActionUpdate&& scoped_action_update) = default;

ScopedActionUpdate::~ScopedActionUpdate() {
  if (action_item_) {
    action_item_->EndUpdate();
  }
}

ActionItem::ActionItemBuilder::ActionItemBuilder() {
  action_item_ = std::make_unique<ActionItem>();
}

ActionItem::ActionItemBuilder::ActionItemBuilder(
    InvokeActionCallback callback) {
  action_item_ = std::make_unique<ActionItem>(std::move(callback));
}

ActionItem::ActionItemBuilder::ActionItemBuilder(
    ActionItem::ActionItemBuilder&&) = default;

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::operator=(
    ActionItem::ActionItemBuilder&&) = default;

ActionItem::ActionItemBuilder::~ActionItemBuilder() = default;

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::AddChild(
    ActionItemBuilder&& child_item) & {
  children_.emplace_back(child_item.Release());
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::AddChild(
    ActionItemBuilder&& child_item) && {
  return std::move(this->AddChild(std::move(child_item)));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetActionId(
    absl::optional<ActionId> action_id) & {
  action_item_->SetActionId(action_id);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetActionId(
    absl::optional<ActionId> action_id) && {
  return std::move(this->SetActionId(action_id));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetAccelerator(
    ui::Accelerator accelerator) & {
  action_item_->SetAccelerator(accelerator);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetAccelerator(
    ui::Accelerator accelerator) && {
  return std::move(this->SetAccelerator(accelerator));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetChecked(
    bool checked) & {
  action_item_->SetChecked(checked);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetChecked(
    bool checked) && {
  return std::move(this->SetChecked(checked));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetEnabled(
    bool enabled) & {
  action_item_->SetEnabled(enabled);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetEnabled(
    bool enabled) && {
  return std::move(this->SetEnabled(enabled));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetGroupId(
    absl::optional<int> group_id) & {
  action_item_->SetGroupId(group_id);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetGroupId(
    absl::optional<int> group_id) && {
  return std::move(this->SetGroupId(group_id));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetImage(
    const ui::ImageModel& image) & {
  action_item_->SetImage(image);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetImage(
    const ui::ImageModel& image) && {
  return std::move(this->SetImage(image));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetText(
    const std::u16string& text) & {
  action_item_->SetText(text);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetText(
    const std::u16string& text) && {
  return std::move(this->SetText(text));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetTooltipText(
    const std::u16string& tooltip) & {
  action_item_->SetTooltipText(tooltip);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetTooltipText(
    const std::u16string& tooltip) && {
  return std::move(this->SetTooltipText(tooltip));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetVisible(
    bool visible) & {
  action_item_->SetVisible(visible);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetVisible(
    bool visible) && {
  return std::move(this->SetVisible(visible));
}

ActionItem::ActionItemBuilder&
ActionItem::ActionItemBuilder::SetInvokeActionCallback(
    InvokeActionCallback callback) & {
  action_item_->SetInvokeActionCallback(std::move(callback));
  return *this;
}

ActionItem::ActionItemBuilder&&
ActionItem::ActionItemBuilder::SetInvokeActionCallback(
    InvokeActionCallback callback) && {
  return std::move(this->SetInvokeActionCallback(std::move(callback)));
}

std::unique_ptr<ActionItem> ActionItem::ActionItemBuilder::Build() && {
  CreateChildren();
  return std::move(action_item_);
}

void ActionItem::ActionItemBuilder::CreateChildren() {
  for (auto& child : children_) {
    action_item_->AddChild(std::move(*child).Build());
  }
}

std::unique_ptr<ActionItem::ActionItemBuilder>
ActionItem::ActionItemBuilder::Release() {
  return std::make_unique<ActionItemBuilder>(std::move(*this));
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

bool ActionItem::GetChecked() const {
  return checked_;
}

void ActionItem::SetChecked(bool checked) {
  if (checked_ == checked) {
    return;
  }
  checked_ = checked;
  if (group_id_.has_value() && checked_ && GetParent()) {
    const ActionList& peer_actions = GetParent()->GetChildren();
    for (auto& child : peer_actions.children()) {
      if (child.get() == this) {
        continue;
      }
      auto child_id = child->GetGroupId();
      if (child_id.has_value() && group_id_ == child_id) {
        child->SetChecked(false);
      }
    }
  }
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

absl::optional<int> ActionItem::GetGroupId() const {
  return group_id_;
}

void ActionItem::SetGroupId(absl::optional<int> group_id) {
  if (group_id_ == group_id) {
    return;
  }
  group_id_ = group_id;
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

[[nodiscard]] base::CallbackListSubscription
ActionItem::AddActionChangedCallback(ActionChangedCallback callback) {
  return AddPropertyChangedCallback(this, callback);
}

// Alternative terms used to identify this action. Used for search indexing
void ActionItem::AddSynonyms(std::initializer_list<std::u16string> synonyms) {
  synonyms_.insert(synonyms_.end(), synonyms);
}

void ActionItem::InvokeAction() {
  if (callback_ && enabled_) {
    callback_.Run(this);
  }
}

// static
ActionItem::ActionItemBuilder ActionItem::Builder(
    InvokeActionCallback callback) {
  return ActionItemBuilder(std::move(callback));
}

// static
ActionItem::ActionItemBuilder ActionItem::Builder() {
  return ActionItemBuilder();
}

ScopedActionUpdate ActionItem::BeginUpdate() {
  ++updating_;
  return ScopedActionUpdate(this);
}

void ActionItem::ActionListChanged() {
  BaseAction::ActionListChanged();
  ActionItemChanged();
}

void ActionItem::ActionItemChanged() {
  if (updating_ > 0) {
    updated_ = true;
    return;
  }
  updated_ = false;
  TriggerChangedCallback(this);
}

void ActionItem::EndUpdate() {
  if (updating_ > 0) {
    --updating_;
    if (!updating_ && updated_) {
      ActionItemChanged();
    }
  }
}

BEGIN_METADATA(ActionItem, BaseAction)
ADD_PROPERTY_METADATA(absl::optional<ActionId>, ActionId)
ADD_PROPERTY_METADATA(ui::Accelerator, Accelerator)
ADD_PROPERTY_METADATA(bool, Checked)
ADD_PROPERTY_METADATA(bool, Enabled)
ADD_PROPERTY_METADATA(absl::optional<int>, GroupId)
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
  if (root_action_parent_.GetChildren().children().empty() &&
      !initializer_list_->empty()) {
    initializer_list_->Notify(this);
  }
}

ActionItem* ActionManager::FindAction(std::u16string term, ActionItem* scope) {
  IndexActions();
  return nullptr;
}

ActionItem* ActionManager::FindAction(ActionId action_id, ActionItem* scope) {
  IndexActions();
  if (scope) {
    auto scope_action_id = scope->GetActionId();
    if (scope_action_id.has_value() && action_id == scope_action_id.value()) {
      return scope;
    }
  }
  const ActionList& action_list =
      scope ? scope->GetChildren() : root_action_parent_.GetChildren();
  return FindActionImpl(action_id, action_list);
}

ActionItem* ActionManager::FindAction(const ui::KeyEvent& key_event,
                                      ActionItem* scope) {
  IndexActions();
  return nullptr;
}

void ActionManager::GetActions(ActionItemVector& items, ActionItem* scope) {
  IndexActions();
  const ActionList& action_list =
      scope ? scope->GetChildren() : root_action_parent_.GetChildren();
  for (auto& child : action_list.children()) {
    GetActionsImpl(child.get(), items);
  }
}

ActionItem* ActionManager::AddAction(std::unique_ptr<ActionItem> action_item) {
  return root_action_parent_.AddChild(std::move(action_item));
}

std::unique_ptr<ActionItem> ActionManager::RemoveAction(
    ActionItem* action_item) {
  return root_action_parent_.RemoveChild(action_item);
}

void ActionManager::ResetActions() {
  root_action_parent_.ResetActionList();
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

ActionItem* ActionManager::FindActionImpl(ActionId action_id,
                                          const ActionList& list) {
  for (const auto& item : list.children()) {
    auto id = item->GetActionId();
    if (id && id == action_id) {
      return item.get();
    }
    if (!item->GetChildren().empty()) {
      ActionItem* result = FindActionImpl(action_id, item->GetChildren());
      if (result) {
        return result;
      }
    }
  }
  return nullptr;
}

void ActionManager::GetActionsImpl(ActionItem* item, ActionItemVector& items) {
  items.push_back(item);
  for (auto& child : item->GetChildren().children()) {
    GetActionsImpl(child.get(), items);
  }
}

BEGIN_METADATA_BASE(ActionManager)
END_METADATA

}  // namespace actions
