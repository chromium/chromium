// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACTIONS_ACTIONS_H_
#define UI_ACTIONS_ACTIONS_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/actions/action_id.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"

namespace actions {

class ActionItem;
using ActionListVector = std::vector<std::unique_ptr<ActionItem>>;
using ActionItemVector = std::vector<ActionItem*>;

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
      public ActionList::Delegate {
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

class COMPONENT_EXPORT(ACTIONS) ActionItem : public BaseAction {
 public:
  using ActionChangedCallback = ui::metadata::PropertyChangedCallback;
  using InvokeActionCallback = base::RepeatingCallback<void(ActionItem*)>;

  class COMPONENT_EXPORT(ACTIONS) ActionItemBuilder {
   public:
    using ChildList = std::vector<std::unique_ptr<ActionItemBuilder>>;
    ActionItemBuilder();
    explicit ActionItemBuilder(InvokeActionCallback callback);
    ActionItemBuilder(ActionItemBuilder&&);
    ActionItemBuilder& operator=(ActionItemBuilder&&);
    ~ActionItemBuilder();

    ActionItemBuilder& AddChild(ActionItemBuilder&& child_item) &;
    ActionItemBuilder&& AddChild(ActionItemBuilder&& child_item) &&;
    template <typename Child, typename... Types>
    ActionItemBuilder& AddChildren(Child&& child, Types&&... args) & {
      return AddChildrenImpl(&child, &args...);
    }
    template <typename Child, typename... Types>
    ActionItemBuilder&& AddChildren(Child&& child, Types&&... args) && {
      return std::move(this->AddChildrenImpl(&child, &args...));
    }
    ActionItemBuilder& SetActionId(absl::optional<ActionId> action_id) &;
    ActionItemBuilder&& SetActionId(absl::optional<ActionId> action_id) &&;
    ActionItemBuilder& SetAccelerator(ui::Accelerator accelerator) &;
    ActionItemBuilder&& SetAccelerator(ui::Accelerator accelerator) &&;
    ActionItemBuilder& SetChecked(bool checked) &;
    ActionItemBuilder&& SetChecked(bool checked) &&;
    ActionItemBuilder& SetEnabled(bool enabled) &;
    ActionItemBuilder&& SetEnabled(bool enabled) &&;
    ActionItemBuilder& SetGroupId(absl::optional<int> group_id) &;
    ActionItemBuilder&& SetGroupId(absl::optional<int> group_id) &&;
    ActionItemBuilder& SetImage(const ui::ImageModel& image) &;
    ActionItemBuilder&& SetImage(const ui::ImageModel& image) &&;
    ActionItemBuilder& SetText(const std::u16string& text) &;
    ActionItemBuilder&& SetText(const std::u16string& text) &&;
    ActionItemBuilder& SetTooltipText(const std::u16string& tooltip) &;
    ActionItemBuilder&& SetTooltipText(const std::u16string& tooltip) &&;
    ActionItemBuilder& SetVisible(bool visible) &;
    ActionItemBuilder&& SetVisible(bool visible) &&;
    ActionItemBuilder& SetInvokeActionCallback(InvokeActionCallback callback) &;
    ActionItemBuilder&& SetInvokeActionCallback(
        InvokeActionCallback callback) &&;
    [[nodiscard]] std::unique_ptr<ActionItem> Build() &&;

   private:
    template <typename... Args>
    ActionItemBuilder& AddChildrenImpl(Args*... args) & {
      std::vector<ActionItemBuilder*> children = {args...};
      for (auto* child : children) {
        children_.emplace_back(child->Release());
      }
      return *this;
    }
    void CreateChildren();
    [[nodiscard]] std::unique_ptr<ActionItemBuilder> Release();

    std::unique_ptr<ActionItem> action_item_;
    ChildList children_;
  };

  METADATA_HEADER(ActionItem);

  ActionItem();
  explicit ActionItem(InvokeActionCallback callback);
  ActionItem(const ActionItem&) = delete;
  ActionItem& operator=(const ActionItem&) = delete;
  ~ActionItem() override;

  absl::optional<ActionId> GetActionId() const;
  void SetActionId(absl::optional<ActionId> action_id);
  ui::Accelerator GetAccelerator() const;
  void SetAccelerator(ui::Accelerator accelerator);
  bool GetChecked() const;
  void SetChecked(bool checked);
  bool GetEnabled() const;
  void SetEnabled(bool enabled);
  absl::optional<int> GetGroupId() const;
  void SetGroupId(absl::optional<int> group_id);
  const ui::ImageModel& GetImage() const;
  void SetImage(const ui::ImageModel& image);
  const std::u16string GetText() const;
  void SetText(const std::u16string& text);
  const std::u16string GetTooltipText() const;
  void SetTooltipText(const std::u16string& tooltip);
  bool GetVisible() const;
  void SetVisible(bool visible);
  void SetInvokeActionCallback(InvokeActionCallback callback);

  [[nodiscard]] base::CallbackListSubscription AddActionChangedCallback(
      ActionChangedCallback callback);

  // Alternative terms used to identify this action. Used for search indexing
  void AddSynonyms(std::initializer_list<std::u16string> synonyms);

  void InvokeAction();

  static ActionItemBuilder Builder(InvokeActionCallback callback);
  static ActionItemBuilder Builder();

  [[nodiscard]] ScopedActionUpdate BeginUpdate();

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
  absl::optional<ActionId> action_id_;
  ui::Accelerator accelerator_;
  bool checked_ = false;
  bool enabled_ = true;
  absl::optional<int> group_id_;
  bool visible_ = true;
  std::u16string text_;
  std::u16string tooltip_;
  ui::ImageModel image_;
  Synonyms synonyms_;
  InvokeActionCallback callback_;
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

  // Clears the actions stored in `root_action_parent_`.
  void ResetActions();

  // Resets the current `initializer_list_`.
  void ResetActionItemInitializerList();

  // Appends `initializer` to the end of the current `initializer_list_`.
  void AppendActionItemInitializer(
      ActionItemInitializerList::CallbackType initializer);

 protected:
  ActionManager();
  ~ActionManager() override;

 private:
  ActionItem* FindActionImpl(ActionId action_id, const ActionList& list);
  void GetActionsImpl(ActionItem* item, ActionItemVector& items);

  // Holds the chain of ActionManager initializer callbacks.
  std::unique_ptr<ActionItemInitializerList> initializer_list_;

  // Holds the subscriptions for initializers in the `initializer_list_`.
  std::vector<base::CallbackListSubscription> initializer_subscriptions_;

  // All "root" actions are parented to this action.
  BaseAction root_action_parent_;
};

}  // namespace actions

#endif  // UI_ACTIONS_ACTIONS_H_
