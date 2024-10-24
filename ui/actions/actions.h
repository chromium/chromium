// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACTIONS_ACTIONS_H_
#define UI_ACTIONS_ACTIONS_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/actions/action_id.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"

namespace actions {

class ActionItem;
using ActionListVector = std::vector<std::unique_ptr<ActionItem>>;
using ActionItemVector = std::vector<raw_ptr<ActionItem, VectorExperimental>>;

class COMPONENT_EXPORT(ACTIONS) ActionList {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void ActionListChanged() = 0;
  };

  explicit ActionList(Delegate* delegate);
  ~ActionList();
  const ActionListVector& children() const { return children_; }
  bool empty() const { return children_.empty(); }

  ActionItem* AddAction(std::unique_ptr<ActionItem> action_item);
  std::unique_ptr<ActionItem> RemoveAction(ActionItem* action_item);
  // Clear the action list vector
  void Reset();

 private:
  ActionListVector children_;
  raw_ptr<Delegate> delegate_;
};

class COMPONENT_EXPORT(ACTIONS) BaseAction
    : public ui::metadata::MetaDataProvider,
      public ActionList::Delegate,
      public ui::PropertyHandler {
 public:
  METADATA_HEADER_BASE(BaseAction);
  BaseAction();
  BaseAction(const BaseAction&) = delete;
  BaseAction& operator=(const BaseAction&) = delete;
  ~BaseAction() override;

  BaseAction* GetParent() const;

  ActionItem* AddChild(std::unique_ptr<ActionItem> action_item);
  std::unique_ptr<ActionItem> RemoveChild(ActionItem* action_item);

  const ActionList& GetChildren() const { return children_; }
  void ResetActionList();

 protected:
  void ActionListChanged() override;

 private:
  raw_ptr<BaseAction> parent_ = nullptr;
  ActionList children_{this};
};

// Class returned from ActionItem::BeginUpdate() in order to allow a "batch"
// update of the ActionItem state without triggering ActionChanged callbacks
// for each state change. Will trigger one update once the instance goes out of
// scope, assuming any changes were actually made.
class COMPONENT_EXPORT(ACTIONS) ScopedActionUpdate {
 public:
  explicit ScopedActionUpdate(ActionItem* action_item);
  ScopedActionUpdate(ScopedActionUpdate&& scoped_action_update);
  ScopedActionUpdate& operator=(ScopedActionUpdate&& scoped_action_update);
  ~ScopedActionUpdate();

 private:
  raw_ptr<ActionItem> action_item_;
};

// Context object designed to allow any class property to be attached to it.
// This allows invoking the action with any additional contextual information
// without requiring the action item itself have any knowledge of that
// information.
class COMPONENT_EXPORT(ACTIONS) ActionInvocationContext
    : public ui::PropertyHandler {
 public:
  ActionInvocationContext();
  ActionInvocationContext(ActionInvocationContext&&);
  ActionInvocationContext& operator=(ActionInvocationContext&&);
  ~ActionInvocationContext() override;

  class COMPONENT_EXPORT(ACTIONS) ContextBuilder {
   public:
    ContextBuilder(ContextBuilder&&);
    ContextBuilder& operator=(ContextBuilder&&);
    ~ContextBuilder();

    template <typename T>
    ContextBuilder&& SetProperty(const ui::ClassProperty<T>* property,
                                 ui::metadata::ArgType<T> value) && {
      context_->SetProperty(property, value);
      return std::move(*this);
    }

    [[nodiscard]] ActionInvocationContext Build() &&;

   private:
    friend class ActionInvocationContext;
    ContextBuilder();
    std::unique_ptr<ActionInvocationContext> context_ =
        std::make_unique<ActionInvocationContext>();
  };

  static ContextBuilder Builder();
};

template <typename BuilderT, typename ActionItemClass>
class BaseActionItemBuilderT {
 public:
  using ChildList = std::vector<std::unique_ptr<BuilderT>>;
  using ActionChangedCallback = ui::metadata::PropertyChangedCallback;
  using InvokeActionCallback =
      base::RepeatingCallback<void(ActionItem*, ActionInvocationContext)>;
  BaseActionItemBuilderT() {
    action_item_ = std::make_unique<ActionItemClass>();
  }
  explicit BaseActionItemBuilderT(InvokeActionCallback callback) {
    action_item_ = std::make_unique<ActionItemClass>(std::move(callback));
  }
  BaseActionItemBuilderT(BaseActionItemBuilderT&&) = default;
  BaseActionItemBuilderT& operator=(BaseActionItemBuilderT&&) = default;
  ~BaseActionItemBuilderT() = default;

  // Build an action.
  static BuilderT Builder(InvokeActionCallback callback) {
    return BuilderT(std::move(callback));
  }
  static BuilderT Builder() { return BuilderT(); }

  BuilderT& AddChild(BuilderT&& child_item) & {
    children_.emplace_back(child_item.Release());
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& AddChild(BuilderT&& child_item) && {
    return std::move(this->AddChild(std::move(child_item)));
  }

  template <typename Child, typename... Types>
  BuilderT& AddChildren(Child&& child, Types&&... args) & {
    return AddChildrenImpl(&child, &args...);
  }

  template <typename Child, typename... Types>
  BuilderT&& AddChildren(Child&& child, Types&&... args) && {
    return std::move(this->AddChildrenImpl(&child, &args...));
  }

  template <typename ActionPtr>
  BuilderT& CopyAddressTo(ActionPtr* action_address) & {
    *action_address = action_item_.get();
    return static_cast<BuilderT&>(*this);
  }

  template <typename ActionPtr>
  BuilderT&& CopyAddressTo(ActionPtr* action_address) && {
    return std::move(this->CopyAddressTo(action_address));
  }

  template <typename Action>
  BuilderT& CopyWeakPtrTo(base::WeakPtr<Action>* weak_ptr) & {
    *weak_ptr = action_item_->GetAsWeakPtr();
    return static_cast<BuilderT&>(*this);
  }

  template <typename Action>
  BuilderT&& CopyWeakPtrTo(base::WeakPtr<Action>* weak_ptr) && {
    return std::move(this->CopyWeakPtrTo(weak_ptr));
  }

  template <typename T>
  BuilderT& SetProperty(const ui::ClassProperty<T>* property,
                        ui::metadata::ArgType<T> value) & {
    action_item_->SetProperty(property, value);
    return static_cast<BuilderT&>(*this);
  }

  template <typename T>
  BuilderT&& SetProperty(const ui::ClassProperty<T>* property,
                         ui::metadata::ArgType<T> value) && {
    return std::move(this->SetProperty(property, value));
  }

  BuilderT& SetAccessibleName(const std::u16string accessible_name) & {
    action_item_->SetAccessibleName(accessible_name);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetAccessibleName(const std::u16string accessible_name) && {
    return std::move(this->SetAccessibleName(accessible_name));
  }

  BuilderT& SetActionId(std::optional<ActionId> action_id) & {
    action_item_->SetActionId(action_id);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetActionId(std::optional<ActionId> action_id) && {
    return std::move(this->SetActionId(action_id));
  }

  BuilderT& SetAccelerator(ui::Accelerator accelerator) & {
    action_item_->SetAccelerator(accelerator);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetAccelerator(ui::Accelerator accelerator) && {
    return std::move(this->SetAccelerator(accelerator));
  }

  BuilderT& SetChecked(bool checked) & {
    action_item_->SetChecked(checked);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetChecked(bool checked) && {
    return std::move(this->SetChecked(checked));
  }

  BuilderT& SetEnabled(bool enabled) & {
    action_item_->SetEnabled(enabled);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetEnabled(bool enabled) && {
    return std::move(this->SetEnabled(enabled));
  }

  BuilderT& SetGroupId(std::optional<int> group_id) & {
    action_item_->SetGroupId(group_id);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetGroupId(std::optional<int> group_id) && {
    return std::move(this->SetGroupId(group_id));
  }

  BuilderT& SetImage(const ui::ImageModel& image) & {
    action_item_->SetImage(image);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetImage(const ui::ImageModel& image) && {
    return std::move(this->SetImage(image));
  }

  BuilderT& SetText(const std::u16string& text) & {
    action_item_->SetText(text);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetText(const std::u16string& text) && {
    return std::move(this->SetText(text));
  }

  BuilderT& SetTooltipText(const std::u16string& tooltip) & {
    action_item_->SetTooltipText(tooltip);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetTooltipText(const std::u16string& tooltip) && {
    return std::move(this->SetTooltipText(tooltip));
  }

  BuilderT& SetVisible(bool visible) & {
    action_item_->SetVisible(visible);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetVisible(bool visible) && {
    return std::move(this->SetVisible(visible));
  }

  BuilderT& SetInvokeActionCallback(InvokeActionCallback callback) & {
    action_item_->SetInvokeActionCallback(std::move(callback));
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetInvokeActionCallback(InvokeActionCallback callback) && {
    return std::move(this->SetInvokeActionCallback(std::move(callback)));
  }

  BuilderT& SetIsShowingBubble(bool showing_bubble) & {
    action_item_->SetIsShowingBubble(showing_bubble);
    return static_cast<BuilderT&>(*this);
  }

  BuilderT&& SetIsShowingBubble(bool showing_bubble) && {
    return std::move(this->SetIsShowingBubble(showing_bubble));
  }

  [[nodiscard]] std::unique_ptr<ActionItemClass> Build() && {
    CreateChildren();
    return std::move(action_item_);
  }

 protected:
  template <typename... Args>
  BuilderT& AddChildrenImpl(Args*... args) & {
    std::vector<BuilderT*> children = {args...};
    for (auto* child : children) {
      children_.emplace_back(child->Release());
    }
    return static_cast<BuilderT&>(*this);
  }
  void CreateChildren() {
    for (auto& child : children_) {
      action_item_->AddChild(std::move(*child).Build());
    }
  }
  [[nodiscard]] std::unique_ptr<BuilderT> Release() {
    return std::make_unique<BuilderT>(std::move(static_cast<BuilderT&>(*this)));
  }

  // Owned and meaningful during the Builder building process. Its
  // ownership will be transferred out upon Build() call.
  std::unique_ptr<ActionItemClass> action_item_;
  ChildList children_;
};

class COMPONENT_EXPORT(ACTIONS) ActionItem : public BaseAction {
  METADATA_HEADER(ActionItem, BaseAction)

 public:
  using ActionChangedCallback = ui::metadata::PropertyChangedCallback;
  using InvokeActionCallback =
      base::RepeatingCallback<void(ActionItem*, ActionInvocationContext)>;

  class COMPONENT_EXPORT(ACTIONS) ActionItemBuilder
      : public BaseActionItemBuilderT<ActionItemBuilder, ActionItem> {
    // TODO: possibly construct a Core class of
    // ctors to avoid writing this in every derived
    // class.
    using BaseActionItemBuilderT::BaseActionItemBuilderT;
  };

  ActionItem();
  explicit ActionItem(InvokeActionCallback callback);
  ActionItem(const ActionItem&) = delete;
  ActionItem& operator=(const ActionItem&) = delete;
  ~ActionItem() override;

  // Build an action.
  static ActionItemBuilder Builder() { return ActionItemBuilder(); }
  static ActionItemBuilder Builder(InvokeActionCallback callback) {
    return ActionItemBuilder(std::move(callback));
  }

  // Configure action states and attributes.
  std::u16string GetAccessibleName() const;
  void SetAccessibleName(const std::u16string accessible_name);
  std::optional<ActionId> GetActionId() const;
  void SetActionId(std::optional<ActionId> action_id);
  ui::Accelerator GetAccelerator() const;
  void SetAccelerator(ui::Accelerator accelerator);
  bool GetChecked() const;
  void SetChecked(bool checked);
  bool GetEnabled() const;
  void SetEnabled(bool enabled);
  std::optional<int> GetGroupId() const;
  void SetGroupId(std::optional<int> group_id);
  const ui::ImageModel& GetImage() const;
  void SetImage(const ui::ImageModel& image);
  const std::u16string GetText() const;
  void SetText(const std::u16string& text);
  const std::u16string GetTooltipText() const;
  void SetTooltipText(const std::u16string& tooltip);
  bool GetVisible() const;
  void SetVisible(bool visible);
  void SetInvokeActionCallback(InvokeActionCallback callback);
  bool GetIsShowingBubble() const;
  void SetIsShowingBubble(bool showing_bubble);

  [[nodiscard]] base::CallbackListSubscription AddActionChangedCallback(
      ActionChangedCallback callback);

  // Alternative terms used to identify this action. Used for search indexing.
  void AddSynonyms(std::initializer_list<std::u16string> synonyms);

  // Do a "batch" update of the ActionItem state without triggering
  // ActionChanged callbacks for each state change.
  [[nodiscard]] ScopedActionUpdate BeginUpdate();

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  // Invoke an action.
  void InvokeAction(
      ActionInvocationContext context = ActionInvocationContext());

  // Get action metrics.
  int GetInvokeCount() const;
  std::optional<base::TimeTicks> GetLastInvokeTime() const;

  base::WeakPtr<ActionItem> GetAsWeakPtr();

 protected:
  // ActionList::Delegate override.
  void ActionListChanged() override;
  void ActionItemChanged();

 private:
  friend class ScopedActionUpdate;
  void EndUpdate();

  using Synonyms = std::vector<std::u16string>;
  // When `updating_` > 0, calling ActionItemChanged() will only record whether
  // is item was updated in `updated_`. Once `updating_` returns to 0 and
  // `updated_` = true, the ActionChanged callbacks will trigger.
  int updating_ = 0;
  bool updated_ = false;
  std::u16string accessible_name_;
  std::optional<ActionId> action_id_;
  ui::Accelerator accelerator_;
  bool checked_ = false;
  bool enabled_ = true;
  std::optional<int> group_id_;
  bool visible_ = true;
  std::u16string text_;
  std::u16string tooltip_;
  ui::ImageModel image_;
  Synonyms synonyms_;
  InvokeActionCallback callback_;
  int invoke_count_ = 0;
  std::optional<base::TimeTicks> last_invoke_time_;
  // Represents whether this action is currently showing associated ephemeral
  // UI. Pinned action buttons which execute on mouse release won't execute if
  // `is_showing_bubble_` was true on mouse press. Used to avoid immediately
  // re-triggering actions when mouse press was intended to dismiss their
  // ephemeral UI.
  // TODO(b/361251892): Rename this to appropriately reflect bubbles that do not
  // close on deactivate.
  bool is_showing_bubble_ = false;
  base::WeakPtrFactory<ActionItem> weak_ptr_factory_{this};
};

// TODO(crbug.com/375261318): Make it so that this ActionItem descendant can
// also be subclassed along with the builder.
// A subclass of ActionItem that has an additional image that reflects the
// current state of the action.
class COMPONENT_EXPORT(ACTIONS) StatefulImageActionItem : public ActionItem {
  METADATA_HEADER(StatefulImageActionItem, ActionItem)

 public:
  using ActionItem::ActionItem;
  ~StatefulImageActionItem() override;

  class StatefulImageActionItemBuilder
      : public BaseActionItemBuilderT<StatefulImageActionItemBuilder,
                                      StatefulImageActionItem> {
   public:
    using BaseActionItemBuilderT::BaseActionItemBuilderT;

    StatefulImageActionItemBuilder& SetStatefulImage(
        const ui::ImageModel& image) & {
      action_item_->SetStatefulImage(image);
      return *this;
    }

    StatefulImageActionItemBuilder&& SetStatefulImage(
        const ui::ImageModel& image) && {
      return std::move(this->SetStatefulImage(image));
    }
  };

  static StatefulImageActionItemBuilder Builder() {
    return StatefulImageActionItemBuilder();
  }
  static StatefulImageActionItemBuilder Builder(InvokeActionCallback callback) {
    return StatefulImageActionItemBuilder(std::move(callback));
  }

  const ui::ImageModel& GetStatefulImage() const;
  void SetStatefulImage(const ui::ImageModel& image);

 private:
  ui::ImageModel stateful_image_;
};

class COMPONENT_EXPORT(ACTIONS) ActionManager
    : public ui::metadata::MetaDataProvider {
 public:
  METADATA_HEADER_BASE(ActionManager);

  using ActionItemInitializerList =
      base::RepeatingCallbackList<void(ActionManager*)>;

  ActionManager(const ActionManager&) = delete;
  ActionManager& operator=(const ActionManager&) = delete;

  static ActionManager& Get();
  static ActionManager& GetForTesting();
  static void ResetForTesting();

  void IndexActions();
  ActionItem* FindAction(std::u16string term, ActionItem* scope = nullptr);
  ActionItem* FindAction(ActionId action_id, ActionItem* scope = nullptr);
  ActionItem* FindAction(const ui::KeyEvent& key_event,
                         ActionItem* scope = nullptr);
  void GetActions(ActionItemVector& items, ActionItem* scope = nullptr);

  ActionItem* AddAction(std::unique_ptr<ActionItem> action_item);
  std::unique_ptr<ActionItem> RemoveAction(ActionItem* action_item);
  template <typename Action, typename... Types>
  void AddActions(Action&& action, Types&&... args) & {
    AddActionsImpl(&action, &args...);
  }

  // Clears the actions stored in `root_action_parent_`.
  void ResetActions();

  // Resets the current `initializer_list_`.
  void ResetActionItemInitializerList();

  // Appends `initializer` to the end of the current `initializer_list_`. If the
  // initializers have already been run or actions have already been added to
  // the manager, the initializer will be run immediately.
  [[nodiscard]] base::CallbackListSubscription AppendActionItemInitializer(
      ActionItemInitializerList::CallbackType initializer);

 protected:
  ActionManager();
  ~ActionManager() override;

 private:
  template <typename... Args>
  void AddActionsImpl(Args*... args) {
    std::vector<std::unique_ptr<ActionItem>*> actions = {args...};
    for (auto* action : actions) {
      AddAction(std::move(*action));
    }
  }
  ActionItem* FindActionImpl(ActionId action_id, const ActionList& list);
  void GetActionsImpl(ActionItem* item, ActionItemVector& items);

  // Holds the chain of ActionManager initializer callbacks.
  std::unique_ptr<ActionItemInitializerList> initializer_list_;

  // All "root" actions are parented to this action.
  BaseAction root_action_parent_;
};

class COMPONENT_EXPORT(ACTIONS) ActionIdMap {
 public:
  using ActionIdToStringMap = base::flat_map<ActionId, std::string>;
  using StringToActionIdMap = base::flat_map<std::string, ActionId>;

  ActionIdMap(const ActionIdMap&) = delete;
  ActionIdMap& operator=(const ActionIdMap&) = delete;

  // Searches existing maps for the given ActionId and returns the corresponding
  // string if found, otherwise returns an empty string.
  static std::optional<std::string> ActionIdToString(const ActionId action_id);
  // Searches existing maps for the given string and returns the corresponding
  // ActionId if found, otherwise returns kActionsEnd.
  static std::optional<ActionId> StringToActionId(
      const std::string action_id_string);
  static std::vector<std::optional<std::string>> ActionIdsToStrings(
      std::vector<ActionId> action_ids);
  static std::vector<std::optional<ActionId>> StringsToActionIds(
      std::vector<std::string> action_id_strings);

  static void AddActionIdToStringMappings(ActionIdToStringMap map);
  static void AddStringToActionIdMappings(StringToActionIdMap map);

  // The second element in the pair is set to true if a new ActionId is
  // created, or false if an ActionId with the given name already exists.
  static std::pair<ActionId, bool> CreateActionId(
      const std::string& action_name);

  static void ResetMapsForTesting();

 private:
  // Merges `map2` into `map1`.
  template <typename T, typename U>
  static void MergeMaps(base::flat_map<T, U>& map1, base::flat_map<T, U>& map2);

  static std::optional<ActionIdToStringMap>& GetGlobalActionIdToStringMap();
  static std::optional<StringToActionIdMap>& GetGlobalStringToActionIdMap();
  static ActionIdToStringMap& GetActionIdToStringMap();
  static StringToActionIdMap& GetStringToActionIdMap();
};

COMPONENT_EXPORT(ACTIONS)
extern const ui::ClassProperty<bool>* const kActionItemPinnableKey;

}  // namespace actions

#endif  // UI_ACTIONS_ACTIONS_H_
