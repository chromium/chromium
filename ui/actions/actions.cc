// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/actions/actions.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "base/no_destructor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
namespace actions {

namespace {

class GlobalActionManager : public ActionManager {
 public:
  GlobalActionManager() = default;
  GlobalActionManager(const GlobalActionManager&) = delete;
  GlobalActionManager& operator=(const GlobalActionManager&) = delete;
  ~GlobalActionManager() override = default;
};

std::optional<GlobalActionManager>& GetGlobalManager() {
  static base::NoDestructor<std::optional<GlobalActionManager>> manager;
  return *manager;
}

}  // namespace

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kActionItemPinnableKey, false)

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

ActionInvocationContext::ContextBuilder::ContextBuilder() = default;

ActionInvocationContext::ContextBuilder::ContextBuilder(ContextBuilder&&) =
    default;

ActionInvocationContext::ContextBuilder&
ActionInvocationContext::ContextBuilder::operator=(ContextBuilder&&) = default;

ActionInvocationContext::ContextBuilder::ContextBuilder::~ContextBuilder() =
    default;

ActionInvocationContext ActionInvocationContext::ContextBuilder::Build() && {
  return std::move(*context_);
}

ActionInvocationContext::ActionInvocationContext() = default;

ActionInvocationContext::ActionInvocationContext(ActionInvocationContext&&) =
    default;

ActionInvocationContext& ActionInvocationContext::operator=(
    ActionInvocationContext&&) = default;

ActionInvocationContext::~ActionInvocationContext() = default;

ActionInvocationContext::ContextBuilder ActionInvocationContext::Builder() {
  return ContextBuilder();
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

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetAccessibleName(
    const std::u16string accessible_name) & {
  action_item_->SetAccessibleName(accessible_name);
  return *this;
}

ActionItem::ActionItemBuilder&&
ActionItem::ActionItemBuilder::SetAccessibleName(
    const std::u16string accessible_name) && {
  return std::move(this->SetAccessibleName(accessible_name));
}

ActionItem::ActionItemBuilder& ActionItem::ActionItemBuilder::SetActionId(
    std::optional<ActionId> action_id) & {
  action_item_->SetActionId(action_id);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetActionId(
    std::optional<ActionId> action_id) && {
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
    std::optional<int> group_id) & {
  action_item_->SetGroupId(group_id);
  return *this;
}

ActionItem::ActionItemBuilder&& ActionItem::ActionItemBuilder::SetGroupId(
    std::optional<int> group_id) && {
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

ActionItem::ActionItemBuilder&
ActionItem::ActionItemBuilder::SetIsShowingBubble(bool showing_bubble) & {
  action_item_->SetIsShowingBubble(showing_bubble);
  return *this;
}

ActionItem::ActionItemBuilder&&
ActionItem::ActionItemBuilder::SetIsShowingBubble(bool showing_bubble) && {
  return std::move(this->SetIsShowingBubble(showing_bubble));
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

std::u16string ActionItem::GetAccessibleName() const {
  return accessible_name_;
}

void ActionItem::SetAccessibleName(const std::u16string accessible_name) {
  if (accessible_name_ == accessible_name) {
    return;
  }
  accessible_name_ = accessible_name;
  ActionItemChanged();
}

std::optional<ActionId> ActionItem::GetActionId() const {
  return action_id_;
}

void ActionItem::SetActionId(std::optional<ActionId> action_id) {
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

void ActionItem::AfterPropertyChange(const void* key, int64_t old_value) {
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

std::optional<int> ActionItem::GetGroupId() const {
  return group_id_;
}

void ActionItem::SetGroupId(std::optional<int> group_id) {
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

bool ActionItem::GetIsShowingBubble() const {
  return is_showing_bubble_;
}

void ActionItem::SetIsShowingBubble(bool showing_bubble) {
  is_showing_bubble_ = showing_bubble;
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

void ActionItem::InvokeAction(ActionInvocationContext context) {
  if (enabled_) {
    invoke_count_++;
    last_invoke_time_ = base::TimeTicks::Now();
    if (callback_) {
      callback_.Run(this, std::move(context));
    }
  }
}

int ActionItem::GetInvokeCount() const {
  return invoke_count_;
}

std::optional<base::TimeTicks> ActionItem::GetLastInvokeTime() const {
  return last_invoke_time_;
}

base::WeakPtr<ActionItem> ActionItem::GetAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

BEGIN_METADATA(ActionItem)
ADD_PROPERTY_METADATA(std::u16string, AccessibleName)
ADD_PROPERTY_METADATA(std::optional<ActionId>, ActionId)
ADD_PROPERTY_METADATA(ui::Accelerator, Accelerator)
ADD_PROPERTY_METADATA(bool, Checked)
ADD_PROPERTY_METADATA(bool, Enabled)
ADD_PROPERTY_METADATA(std::optional<int>, GroupId)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_PROPERTY_METADATA(std::u16string, TooltipText)
ADD_PROPERTY_METADATA(bool, Visible)
ADD_READONLY_PROPERTY_METADATA(int, InvokeCount)
ADD_READONLY_PROPERTY_METADATA(std::optional<base::TimeTicks>, LastInvokeTime)
END_METADATA

ActionManager::ActionManager() {
  ResetActionItemInitializerList();
}

ActionManager::~ActionManager() = default;

// static
ActionManager& ActionManager::Get() {
  std::optional<GlobalActionManager>& manager = GetGlobalManager();
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

// static
void ActionIdMap::ResetMapsForTesting() {
  GetGlobalActionIdToStringMap().reset();
  GetGlobalStringToActionIdMap().reset();
}

// static
std::optional<ActionIdMap::ActionIdToStringMap>&
ActionIdMap::GetGlobalActionIdToStringMap() {
  static base::NoDestructor<std::optional<ActionIdMap::ActionIdToStringMap>>
      map;
  return *map;
}

// static
std::optional<ActionIdMap::StringToActionIdMap>&
ActionIdMap::GetGlobalStringToActionIdMap() {
  static base::NoDestructor<std::optional<ActionIdMap::StringToActionIdMap>>
      map;
  return *map;
}

#define MAP_ACTION_IDS_TO_STRINGS
#include "ui/actions/action_id_macros.inc"

// static
ActionIdMap::ActionIdToStringMap& ActionIdMap::GetActionIdToStringMap() {
  std::optional<ActionIdMap::ActionIdToStringMap>& map =
      GetGlobalActionIdToStringMap();
  if (!map.has_value()) {
    map.emplace(std::vector<std::pair<ActionId, std::string>>{ACTION_IDS});
  }
  return map.value();
}

#include "ui/actions/action_id_macros.inc"
#undef MAP_ACTION_IDS_TO_STRINGS

#define MAP_STRING_TO_ACTION_IDS
#include "ui/actions/action_id_macros.inc"

// static
ActionIdMap::StringToActionIdMap& ActionIdMap::GetStringToActionIdMap() {
  std::optional<ActionIdMap::StringToActionIdMap>& map =
      GetGlobalStringToActionIdMap();
  if (!map.has_value()) {
    map.emplace(std::vector<std::pair<std::string, ActionId>>{ACTION_IDS});
  }
  return map.value();
}

#include "ui/actions/action_id_macros.inc"
#undef MAP_STRING_TO_ACTION_IDS

// static
std::optional<std::string> ActionIdMap::ActionIdToString(
    const ActionId action_id) {
  auto iter = GetActionIdToStringMap().find(action_id);
  if (iter != GetActionIdToStringMap().end()) {
    return iter->second;
  }
  return std::nullopt;
}

// static
std::optional<ActionId> ActionIdMap::StringToActionId(
    const std::string action_id_string) {
  auto iter = GetStringToActionIdMap().find(action_id_string);
  if (iter != GetStringToActionIdMap().end()) {
    return iter->second;
  }
  return std::nullopt;
}

// static
std::vector<std::optional<std::string>> ActionIdMap::ActionIdsToStrings(
    std::vector<ActionId> action_ids) {
  std::vector<std::optional<std::string>> action_id_strings;
  action_id_strings.reserve(action_ids.size());

  for (ActionId action_id : action_ids) {
    action_id_strings.push_back(ActionIdToString(action_id));
  }
  return action_id_strings;
}

// static
std::vector<std::optional<ActionId>> ActionIdMap::StringsToActionIds(
    std::vector<std::string> action_id_strings) {
  std::vector<std::optional<ActionId>> action_ids;
  action_ids.reserve(action_id_strings.size());

  for (std::string action_id_string : action_id_strings) {
    action_ids.push_back(StringToActionId(action_id_string));
  }
  return action_ids;
}

template <typename T, typename U>
void ActionIdMap::MergeMaps(base::flat_map<T, U>& map1,
                            base::flat_map<T, U>& map2) {
  auto vec1 = std::move(map1).extract();
  auto vec2 = std::move(map2).extract();
  std::vector<std::pair<T, U>> vec3(vec1.size() + vec2.size());
  std::merge(vec1.begin(), vec1.end(), vec2.begin(), vec2.end(), vec3.begin());
  map1.replace(std::move(vec3));
}

// static
void ActionIdMap::AddActionIdToStringMappings(ActionIdToStringMap map) {
  MergeMaps(GetActionIdToStringMap(), map);
}

// static
void ActionIdMap::AddStringToActionIdMappings(StringToActionIdMap map) {
  MergeMaps(GetStringToActionIdMap(), map);
}

// static
std::pair<ActionId, bool> ActionIdMap::CreateActionId(
    const std::string& action_name) {
  static ActionId new_action_id = std::numeric_limits<ActionId>::max();

  auto action_id = StringToActionId(action_name);

  if (action_id.has_value()) {
    return {action_id.value(), false};
  }

  GetActionIdToStringMap()[new_action_id] = action_name;
  GetStringToActionIdMap()[action_name] = new_action_id;

  return {new_action_id--, true};
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
}

base::CallbackListSubscription ActionManager::AppendActionItemInitializer(
    ActionItemInitializerList::CallbackType initializer) {
  DCHECK(initializer_list_);
  // If an initializer is added after items have already been added, just run
  // the initializer immediately.
  if (!root_action_parent_.GetChildren().children().empty()) {
    initializer.Run(this);
  }

  return initializer_list_->Add(std::move(initializer));
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
