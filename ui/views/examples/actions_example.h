// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_ACTIONS_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_ACTIONS_EXAMPLE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/actions/actions.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace views {

class BoxLayoutView;
class Checkbox;
class Combobox;
class Textarea;
class Textfield;

namespace examples {

class VIEWS_EXAMPLES_EXPORT ActionsExample : public ExampleBase {
 public:
  ActionsExample();
  ActionsExample(const ActionsExample&) = delete;
  ActionsExample& operator=(const ActionsExample&) = delete;
  ~ActionsExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void ActionSelected();
  void AssignAction(actions::ActionItem* action,
                    actions::ActionInvocationContext context);
  void ActionInvoked(actions::ActionItem* action,
                     actions::ActionInvocationContext context);
  void CreateControl(actions::ActionItem* action,
                     actions::ActionInvocationContext context);
  void CreateActions(actions::ActionManager* manager);
  void CheckedChanged();
  void EnabledChanged();
  actions::ActionItem* GetSelectedAction() const;
  void TextChanged();
  void TooltipTextChanged();
  void VisibleChanged();

  ActionViewController action_view_controller_ = ActionViewController();
  std::vector<base::CallbackListSubscription> subscriptions_;
  raw_ptr<actions::ActionItem> example_actions_ = nullptr;
  raw_ptr<View> action_panel_ = nullptr;
  raw_ptr<BoxLayoutView> control_panel_ = nullptr;
  raw_ptr<Combobox> available_controls_ = nullptr;
  raw_ptr<Combobox> available_actions_ = nullptr;
  raw_ptr<Combobox> controls_ = nullptr;
  raw_ptr<Checkbox> action_checked_ = nullptr;
  raw_ptr<Checkbox> action_enabled_ = nullptr;
  raw_ptr<Checkbox> action_visible_ = nullptr;
  raw_ptr<Textfield> action_text_ = nullptr;
  raw_ptr<Textfield> action_tooltip_text_ = nullptr;
  raw_ptr<Textarea> actions_trigger_info_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_ACTIONS_EXAMPLE_H_
