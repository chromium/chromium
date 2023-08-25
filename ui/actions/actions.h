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

class COMPONENT_EXPORT(ACTIONS) ActionList {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void ActionListChanged() = 0;
  };

  explicit ActionList(Delegate* delegate);
  ~ActionList();
  ActionListVector& children() { return children_; }
  bool empty() { return children_.empty(); }

  ActionItem* AddAction(std::unique_ptr<ActionItem> action_item);
  std::unique_ptr<ActionItem> RemoveAction(ActionItem* action_item);

 private:
  ActionListVector children_;
  raw_ptr<Delegate> delegate_;
};

class COMPONENT_EXPORT(ACTIONS) ActionItem
    : public ui::metadata::MetaDataProvider,
      public ActionList::Delegate {
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
    ActionItemBuilder& SetEnabled(bool enabled) &;
    ActionItemBuilder&& SetEnabled(bool enabled) &&;
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

  METADATA_HEADER_BASE(ActionItem);

  ActionItem();
  explicit ActionItem(InvokeActionCallback callback);
  ActionItem(const ActionItem&) = delete;
  ActionItem& operator=(const ActionItem&) = delete;
  ~ActionItem() override;

  absl::optional<ActionId> GetActionId() const;
  void SetActionId(absl::optional<ActionId> action_id);
  ui::Accelerator GetAccelerator() const;
  void SetAccelerator(ui::Accelerator accelerator);
  bool GetEnabled() const;
  void SetEnabled(bool enabled);
  const ui::ImageModel& GetImage() const;
  void SetImage(const ui::ImageModel& image);
  const std::u16string GetText() const;
  void SetText(const std::u16string& text);
  const std::u16string GetTooltipText() const;
  void SetTooltipText(const std::u16string& tooltip);
  ActionItem* GetParent() const;
  bool GetVisible() const;
  void SetVisible(bool visible);
  void SetInvokeActionCallback(InvokeActionCallback callback);

  ActionItem* AddChild(std::unique_ptr<ActionItem> action_item);
  std::unique_ptr<ActionItem> RemoveChild(ActionItem* action_item);
  [[nodiscard]] base::CallbackListSubscription AddActionChangedCallback(
      ActionChangedCallback callback);

  // Alternative terms used to identify this action. Used for search indexing
  void AddSynonyms(std::initializer_list<std::u16string> synonyms);

  void InvokeAction();

  static ActionItemBuilder Builder(InvokeActionCallback callback);
  static ActionItemBuilder Builder();

  ActionList& GetChildrenForTesting() { return children_; }

 protected:
  // ActionList::Delegate override.
  void ActionListChanged() override;
  void ActionItemChanged();

 private:
  using Synonyms = std::vector<std::u16string>;
  raw_ptr<ActionItem> parent_ = nullptr;
  ActionList children_{this};
  absl::optional<ActionId> action_id_;
  ui::Accelerator accelerator_;
  bool enabled_ = true;
  bool visible_ = true;
  std::u16string text_;
  std::u16string tooltip_;
  ui::ImageModel image_;
  Synonyms synonyms_;
  InvokeActionCallback callback_;
};

class COMPONENT_EXPORT(ACTIONS) ActionManager
    : public ui::metadata::MetaDataProvider,
      public ActionList::Delegate {
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
  ActionItem* FindAction(std::u16string term);
  ActionItem* FindAction(ActionId action_id);
  ActionItem* FindAction(const ui::KeyEvent& key_event);

  ActionItem* AddAction(std::unique_ptr<ActionItem> action_item);
  std::unique_ptr<ActionItem> RemoveAction(ActionItem* action_item);

  // Clears the actions stored in `root_action_list_`.
  void ResetActions();

  // Resets the current `initializer_list_`.
  void ResetActionItemInitializerList();

  // Appends `initializer` to the end of the current `initializer_list_`.
  void AppendActionItemInitializer(
      ActionItemInitializerList::CallbackType initializer);

 protected:
  ActionManager();
  ~ActionManager() override;

  // ActionList::Delegate override.
  void ActionListChanged() override;

 private:
  // Holds the chain of ActionManager initializer callbacks.
  std::unique_ptr<ActionItemInitializerList> initializer_list_;

  // Holds the subscriptions for initializers in the `initializer_list_`.
  std::vector<base::CallbackListSubscription> initializer_subscriptions_;

  // Holds all the "root" actions. Most actions will live here.
  std::unique_ptr<ActionList> root_action_list_;
};

}  // namespace actions

#endif  // UI_ACTIONS_ACTIONS_H_
