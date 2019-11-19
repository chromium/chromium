// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/menu_example.h"

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

namespace {

class ExampleMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  ExampleMenuModel();

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  enum GroupID {
    GROUP_MAKE_DECISION,
  };

  enum CommandID {
    COMMAND_DO_SOMETHING,
    COMMAND_SELECT_ASCII,
    COMMAND_SELECT_UTF8,
    COMMAND_SELECT_UTF16,
    COMMAND_CHECK_APPLE,
    COMMAND_CHECK_ORANGE,
    COMMAND_CHECK_KIWI,
    COMMAND_GO_HOME,
  };

  std::unique_ptr<ui::SimpleMenuModel> submenu_;
  std::set<int> checked_fruits_;
  int current_encoding_command_id_ = COMMAND_SELECT_ASCII;

  DISALLOW_COPY_AND_ASSIGN(ExampleMenuModel);
};

class ExampleMenuButton : public MenuButton, public ButtonListener {
 public:
  explicit ExampleMenuButton(const base::string16& test);
  ~ExampleMenuButton() override;

 private:
  // ButtonListener:
  void ButtonPressed(Button* source, const ui::Event& event) override;

  ui::SimpleMenuModel* GetMenuModel();

  std::unique_ptr<ExampleMenuModel> menu_model_;
  std::unique_ptr<MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(ExampleMenuButton);
};

// ExampleMenuModel ---------------------------------------------------------

ExampleMenuModel::ExampleMenuModel() : ui::SimpleMenuModel(this) {
  AddItem(COMMAND_DO_SOMETHING, ASCIIToUTF16("Do Something"));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddRadioItem(COMMAND_SELECT_ASCII, ASCIIToUTF16("ASCII"),
               GROUP_MAKE_DECISION);
  AddRadioItem(COMMAND_SELECT_UTF8, ASCIIToUTF16("UTF-8"),
               GROUP_MAKE_DECISION);
  AddRadioItem(COMMAND_SELECT_UTF16, ASCIIToUTF16("UTF-16"),
               GROUP_MAKE_DECISION);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddCheckItem(COMMAND_CHECK_APPLE, ASCIIToUTF16("Apple"));
  AddCheckItem(COMMAND_CHECK_ORANGE, ASCIIToUTF16("Orange"));
  AddCheckItem(COMMAND_CHECK_KIWI, ASCIIToUTF16("Kiwi"));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItem(COMMAND_GO_HOME, ASCIIToUTF16("Go Home"));

  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  submenu_->AddItem(COMMAND_DO_SOMETHING, ASCIIToUTF16("Do Something 2"));
  AddSubMenu(0, ASCIIToUTF16("Submenu"), submenu_.get());
}

bool ExampleMenuModel::IsCommandIdChecked(int command_id) const {
  // Radio items.
  if (command_id == current_encoding_command_id_)
    return true;

  // Check items.
  if (checked_fruits_.find(command_id) != checked_fruits_.end())
    return true;

  return false;
}

bool ExampleMenuModel::IsCommandIdEnabled(int command_id) const {
  // All commands are enabled except for COMMAND_GO_HOME.
  return command_id != COMMAND_GO_HOME;
}

void ExampleMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case COMMAND_DO_SOMETHING: {
      VLOG(0) << "Done something";
      break;
    }

    // Radio items.
    case COMMAND_SELECT_ASCII: {
      current_encoding_command_id_ = COMMAND_SELECT_ASCII;
      VLOG(0) << "Selected ASCII";
      break;
    }
    case COMMAND_SELECT_UTF8: {
      current_encoding_command_id_ = COMMAND_SELECT_UTF8;
      VLOG(0) << "Selected UTF-8";
      break;
    }
    case COMMAND_SELECT_UTF16: {
      current_encoding_command_id_ = COMMAND_SELECT_UTF16;
      VLOG(0) << "Selected UTF-16";
      break;
    }

    // Check items.
    case COMMAND_CHECK_APPLE:
    case COMMAND_CHECK_ORANGE:
    case COMMAND_CHECK_KIWI: {
      // Print what fruit is checked.
      const char* checked_fruit = "";
      if (command_id == COMMAND_CHECK_APPLE)
        checked_fruit = "Apple";
      else if (command_id == COMMAND_CHECK_ORANGE)
        checked_fruit = "Orange";
      else if (command_id == COMMAND_CHECK_KIWI)
        checked_fruit = "Kiwi";

      // Update the check status.
      auto iter = checked_fruits_.find(command_id);
      if (iter == checked_fruits_.end()) {
        DVLOG(1) << "Checked " << checked_fruit;
        checked_fruits_.insert(command_id);
      } else {
        DVLOG(1) << "Unchecked " << checked_fruit;
        checked_fruits_.erase(iter);
      }
      break;
    }
  }
}

// ExampleMenuButton -----------------------------------------------------------

ExampleMenuButton::ExampleMenuButton(const base::string16& test)
    : MenuButton(test, this) {}

ExampleMenuButton::~ExampleMenuButton() = default;

void ExampleMenuButton::ButtonPressed(Button* source, const ui::Event& event) {
  menu_runner_ =
      std::make_unique<MenuRunner>(GetMenuModel(), MenuRunner::HAS_MNEMONICS);

  menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                          button_controller(),
                          gfx::Rect(source->GetMenuPosition(), gfx::Size()),
                          MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_NONE);
}

ui::SimpleMenuModel* ExampleMenuButton::GetMenuModel() {
  if (!menu_model_.get())
    menu_model_ = std::make_unique<ExampleMenuModel>();
  return menu_model_.get();
}

}  // namespace

MenuExample::MenuExample() : ExampleBase("Menu") {
}

MenuExample::~MenuExample() = default;

void MenuExample::CreateExampleView(View* container) {
  // We add a button to open a menu.
  ExampleMenuButton* menu_button = new ExampleMenuButton(
      ASCIIToUTF16("Open a menu"));
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(menu_button);
}

}  // namespace examples
}  // namespace views
