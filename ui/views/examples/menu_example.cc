// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/menu_example.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

enum CommandID {
  COMMAND_ROOT = -1,

  COMMAND_DO_SOMETHING,
  COMMAND_SELECT_ASCII,
  COMMAND_SELECT_UTF8,
  COMMAND_SELECT_UTF16,
  COMMAND_CHECK_APPLE,
  COMMAND_CHECK_ORANGE,
  COMMAND_CHECK_KIWI,
  COMMAND_GO_HOME,
};

// The base class for creating a menu runner.
class MenuRunnerFactory {
 public:
  MenuRunnerFactory();
  virtual ~MenuRunnerFactory();

  virtual std::unique_ptr<views::MenuRunner> CreateMenuRunner() = 0;
};

class CommandExecutor {
 public:
  CommandExecutor();
  virtual ~CommandExecutor();

  bool IsCommandIdChecked(int command_id) const;
  bool IsCommandIdEnabled(int command_id) const;
  virtual void ExecuteCommand(int command_id, int event_flags);

 private:
  std::set<int> checked_fruits_;
  int current_encoding_command_id_ = COMMAND_SELECT_ASCII;
};

class ExampleMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  ExampleMenuModel();

  ExampleMenuModel(const ExampleMenuModel&) = delete;
  ExampleMenuModel& operator=(const ExampleMenuModel&) = delete;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  enum GroupID {
    GROUP_MAKE_DECISION,
  };

  std::unique_ptr<ui::SimpleMenuModel> submenu_;
  CommandExecutor executor_;
};

class ExampleMenuButton : public MenuButton {
  METADATA_HEADER(ExampleMenuButton, MenuButton)

 public:
  explicit ExampleMenuButton(const std::u16string& test = std::u16string());

  ExampleMenuButton(const ExampleMenuButton&) = delete;
  ExampleMenuButton& operator=(const ExampleMenuButton&) = delete;

  ~ExampleMenuButton() override;

  void set_menu_runner_factory(
      std::unique_ptr<MenuRunnerFactory> menu_runner_factory) {
    menu_runner_factory_ = std::move(menu_runner_factory);
  }

 private:
  void ButtonPressed();

  // The factory for creating a menu runner.
  std::unique_ptr<MenuRunnerFactory> menu_runner_factory_;

  // The menu runner created by `menu_runner_factory`.
  std::unique_ptr<MenuRunner> menu_runner_;
};

BEGIN_METADATA(ExampleMenuButton)
END_METADATA

BEGIN_VIEW_BUILDER(/* no export */, ExampleMenuButton, MenuButton)
END_VIEW_BUILDER

// Shows a simple menu using `ui::SimpleMenuModel`.
class ExampleMenuRunnerFactory : public MenuRunnerFactory {
 public:
  ExampleMenuRunnerFactory();
  ~ExampleMenuRunnerFactory() override;

 protected:
  // MenuRunnerFactory:
  std::unique_ptr<views::MenuRunner> CreateMenuRunner() override;

 private:
  ui::SimpleMenuModel* GetMenuModel();

  std::unique_ptr<ExampleMenuModel> menu_model_;
};

// Shows a menu with draggable items using `views::MenuItemView`.
class DraggableMenuRunnerFactory : public MenuRunnerFactory,
                                   public views::MenuDelegate {
 public:
  DraggableMenuRunnerFactory();
  ~DraggableMenuRunnerFactory() override;

 protected:
  // MenuRunnerFactory:
  std::unique_ptr<views::MenuRunner> CreateMenuRunner() override;

  // views::MenuDelegate:
  bool CanDrag(MenuItemView* menu) override;
  bool CanDrop(MenuItemView* menu, const OSExchangeData& data) override;
  ui::mojom::DragOperation GetDropOperation(MenuItemView* item,
                                            const ui::DropTargetEvent& event,
                                            DropPosition* position) override;
  int GetDragOperations(MenuItemView* sender) override;
  void WriteDragData(MenuItemView* sender, OSExchangeData* data) override;
  views::View::DropCallback GetDropCallback(
      MenuItemView* menu,
      DropPosition position,
      const ui::DropTargetEvent& event) override;
  bool AreDropTypesRequired(MenuItemView* menu) override;
  bool GetDropFormats(views::MenuItemView* menu,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool ShouldCloseOnDragComplete() override;
  bool IsCommandEnabled(int id) const override;
  bool IsItemChecked(int id) const override;
  void ExecuteCommand(int id) override;

 private:
  struct Model {
    int command_id;
    int message_id;
    views::MenuItemView::Type type;
  };

  // Gets the clipboard format type for draggable menu items. It is only a
  // format type used in this example, and should not be used in production.
  static ui::ClipboardFormatType MenuItemClipboardFormatType();

  void BuildMenuItems(views::MenuItemView* menu);

  // Moves the model element at `from` to `to`. If `before` is true, the
  // element is moved before `to`, otherwise after.
  void MoveModelElement(size_t from, size_t to, bool before);
  void PerformDrop(MenuItemView* menu,
                   DropPosition position,
                   const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  std::vector<Model> models_;
  CommandExecutor executor_;
};

}  // namespace views::examples

DEFINE_VIEW_BUILDER(/* no export */, views::examples::ExampleMenuButton)

namespace views::examples {

void CommandExecutor::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case COMMAND_DO_SOMETHING: {
      PrintStatus("Done something");
      break;
    }

    // Radio items.
    case COMMAND_SELECT_ASCII: {
      current_encoding_command_id_ = COMMAND_SELECT_ASCII;
      PrintStatus("Selected ASCII");
      break;
    }
    case COMMAND_SELECT_UTF8: {
      current_encoding_command_id_ = COMMAND_SELECT_UTF8;
      PrintStatus("Selected UTF-8");
      break;
    }
    case COMMAND_SELECT_UTF16: {
      current_encoding_command_id_ = COMMAND_SELECT_UTF16;
      PrintStatus("Selected UTF-16");
      break;
    }

    // Check items.
    case COMMAND_CHECK_APPLE:
    case COMMAND_CHECK_ORANGE:
    case COMMAND_CHECK_KIWI: {
      // Print what fruit is checked.
      const char* checked_fruit = "";
      if (command_id == COMMAND_CHECK_APPLE) {
        checked_fruit = "Apple";
      } else if (command_id == COMMAND_CHECK_ORANGE) {
        checked_fruit = "Orange";
      } else if (command_id == COMMAND_CHECK_KIWI) {
        checked_fruit = "Kiwi";
      }

      // Update the check status.
      auto iter = checked_fruits_.find(command_id);
      if (iter == checked_fruits_.end()) {
        PrintStatus(std::string("Checked ") + checked_fruit);
        checked_fruits_.insert(command_id);
      } else {
        PrintStatus(std::string("Unchecked ") + checked_fruit);
        checked_fruits_.erase(iter);
      }
      break;
    }
  }
}

CommandExecutor::CommandExecutor() = default;

CommandExecutor::~CommandExecutor() = default;

bool CommandExecutor::IsCommandIdChecked(int command_id) const {
  // Radio items.
  if (command_id == current_encoding_command_id_) {
    return true;
  }

  // Check items.
  if (checked_fruits_.find(command_id) != checked_fruits_.end()) {
    return true;
  }

  return false;
}

bool CommandExecutor::IsCommandIdEnabled(int command_id) const {
  // All commands are enabled except for COMMAND_GO_HOME.
  return command_id != COMMAND_GO_HOME;
}

// ExampleMenuModel ---------------------------------------------------------

ExampleMenuModel::ExampleMenuModel() : ui::SimpleMenuModel(this) {
  AddItem(COMMAND_DO_SOMETHING, GetStringUTF16(IDS_MENU_DO_SOMETHING_LABEL));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddRadioItem(COMMAND_SELECT_ASCII, GetStringUTF16(IDS_MENU_ASCII_LABEL),
               GROUP_MAKE_DECISION);
  AddRadioItem(COMMAND_SELECT_UTF8, GetStringUTF16(IDS_MENU_UTF8_LABEL),
               GROUP_MAKE_DECISION);
  AddRadioItem(COMMAND_SELECT_UTF16, GetStringUTF16(IDS_MENU_UTF16_LABEL),
               GROUP_MAKE_DECISION);
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddCheckItem(COMMAND_CHECK_APPLE, GetStringUTF16(IDS_MENU_APPLE_LABEL));
  AddCheckItem(COMMAND_CHECK_ORANGE, GetStringUTF16(IDS_MENU_ORANGE_LABEL));
  AddCheckItem(COMMAND_CHECK_KIWI, GetStringUTF16(IDS_MENU_KIWI_LABEL));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItem(COMMAND_GO_HOME, GetStringUTF16(IDS_MENU_GO_HOME_LABEL));

  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  submenu_->AddItem(COMMAND_DO_SOMETHING,
                    GetStringUTF16(IDS_MENU_DO_SOMETHING_2_LABEL));
  AddSubMenu(0, GetStringUTF16(IDS_MENU_SUBMENU_LABEL), submenu_.get());
}

bool ExampleMenuModel::IsCommandIdChecked(int command_id) const {
  return executor_.IsCommandIdChecked(command_id);
}

bool ExampleMenuModel::IsCommandIdEnabled(int command_id) const {
  return executor_.IsCommandIdEnabled(command_id);
}

void ExampleMenuModel::ExecuteCommand(int command_id, int event_flags) {
  executor_.ExecuteCommand(command_id, event_flags);
}

// ExampleMenuButton ---------------------------------------------------------

ExampleMenuButton::ExampleMenuButton(const std::u16string& test)
    : MenuButton(base::BindRepeating(&ExampleMenuButton::ButtonPressed,
                                     base::Unretained(this)),
                 test) {}

ExampleMenuButton::~ExampleMenuButton() = default;

void ExampleMenuButton::ButtonPressed() {
  CHECK(menu_runner_factory_);
  menu_runner_ = menu_runner_factory_->CreateMenuRunner();

  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(this, &screen_loc);
  gfx::Rect bounds(screen_loc, this->size());

  menu_runner_->RunMenuAt(GetWidget()->GetTopLevelWidget(), button_controller(),
                          bounds, MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kNone);
}

// MenuRunnerFactory ---------------------------------------------------

MenuRunnerFactory::MenuRunnerFactory() = default;
MenuRunnerFactory::~MenuRunnerFactory() = default;

// ExampleMenuRunnerFactory --------------------------------------------

ExampleMenuRunnerFactory::ExampleMenuRunnerFactory() = default;
ExampleMenuRunnerFactory::~ExampleMenuRunnerFactory() = default;

ui::SimpleMenuModel* ExampleMenuRunnerFactory::GetMenuModel() {
  if (!menu_model_) {
    menu_model_ = std::make_unique<ExampleMenuModel>();
  }
  return menu_model_.get();
}

std::unique_ptr<views::MenuRunner>
ExampleMenuRunnerFactory::CreateMenuRunner() {
  return std::make_unique<MenuRunner>(GetMenuModel(),
                                      MenuRunner::HAS_MNEMONICS);
}

// DraggableMenuRunnerFactory -------------------------------------------------

DraggableMenuRunnerFactory::DraggableMenuRunnerFactory() = default;
DraggableMenuRunnerFactory::~DraggableMenuRunnerFactory() = default;

std::unique_ptr<views::MenuRunner>
DraggableMenuRunnerFactory::CreateMenuRunner() {
  auto menu_item_view = std::make_unique<views::MenuItemView>(this);
  menu_item_view->SetCommand(COMMAND_ROOT);

  if (models_.empty()) {
    models_.emplace_back(COMMAND_DO_SOMETHING, IDS_MENU_DO_SOMETHING_LABEL,
                         views::MenuItemView::Type::kNormal);
    models_.emplace_back(Model{.type = views::MenuItemView::Type::kSeparator});
    models_.emplace_back(COMMAND_SELECT_ASCII, IDS_MENU_ASCII_LABEL,
                         views::MenuItemView::Type::kRadio);
    models_.emplace_back(COMMAND_SELECT_UTF8, IDS_MENU_UTF8_LABEL,
                         views::MenuItemView::Type::kRadio);
    models_.emplace_back(COMMAND_SELECT_UTF16, IDS_MENU_UTF16_LABEL,
                         views::MenuItemView::Type::kRadio);
    models_.emplace_back(Model{.type = views::MenuItemView::Type::kSeparator});
    models_.emplace_back(COMMAND_CHECK_APPLE, IDS_MENU_APPLE_LABEL,
                         views::MenuItemView::Type::kCheckbox);
    models_.emplace_back(COMMAND_CHECK_ORANGE, IDS_MENU_ORANGE_LABEL,
                         views::MenuItemView::Type::kCheckbox);
    models_.emplace_back(COMMAND_CHECK_KIWI, IDS_MENU_KIWI_LABEL,
                         views::MenuItemView::Type::kCheckbox);
    models_.emplace_back(Model{.type = views::MenuItemView::Type::kSeparator});
    models_.emplace_back(COMMAND_GO_HOME, IDS_MENU_GO_HOME_LABEL,
                         views::MenuItemView::Type::kNormal);
  }
  BuildMenuItems(menu_item_view.get());
  return std::make_unique<MenuRunner>(std::move(menu_item_view),
                                      MenuRunner::HAS_MNEMONICS);
}

bool DraggableMenuRunnerFactory::CanDrag(views::MenuItemView* menu) {
  if (menu->GetType() == views::MenuItemView::Type::kSeparator) {
    return false;
  }

  return true;
}

bool DraggableMenuRunnerFactory::CanDrop(views::MenuItemView* menu,
                                         const OSExchangeData& data) {
  return data.HasCustomFormat(MenuItemClipboardFormatType());
}

ui::mojom::DragOperation DraggableMenuRunnerFactory::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  return ui::mojom::DragOperation::kMove;
}

int DraggableMenuRunnerFactory::GetDragOperations(MenuItemView* sender) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void DraggableMenuRunnerFactory::WriteDragData(MenuItemView* sender,
                                               OSExchangeData* data) {
  // Simply write the command ID of the menu item to be moved.
  base::Pickle pickle;
  pickle.WriteInt(sender->GetCommand());
  data->SetPickledData(MenuItemClipboardFormatType(), pickle);
}

views::View::DropCallback DraggableMenuRunnerFactory::GetDropCallback(
    MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&DraggableMenuRunnerFactory::PerformDrop,
                        base::Unretained(this), menu, position);
}

bool DraggableMenuRunnerFactory::AreDropTypesRequired(MenuItemView* menu) {
  return true;
}

bool DraggableMenuRunnerFactory::GetDropFormats(
    views::MenuItemView* menu,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(MenuItemClipboardFormatType());
  return true;
}

bool DraggableMenuRunnerFactory::ShouldCloseOnDragComplete() {
  return false;
}

bool DraggableMenuRunnerFactory::IsCommandEnabled(int id) const {
  return executor_.IsCommandIdEnabled(id);
}
bool DraggableMenuRunnerFactory::IsItemChecked(int id) const {
  return executor_.IsCommandIdChecked(id);
}

void DraggableMenuRunnerFactory::ExecuteCommand(int id) {
  executor_.ExecuteCommand(id, 0);
}

void DraggableMenuRunnerFactory::BuildMenuItems(views::MenuItemView* menu) {
  CHECK(menu);
  if (menu->HasSubmenu()) {
    menu->RemoveAllMenuItems();
  }

  size_t index = 0u;
  for (const auto& model : models_) {
    menu->AddMenuItemAt(
        index++, model.command_id,
        model.message_id ? GetStringUTF16(model.message_id) : std::u16string(),
        std::u16string(), std::u16string(), ui::ImageModel(), ui::ImageModel(),
        model.type, ui::NORMAL_SEPARATOR, std::nullopt, std::nullopt,
        std::nullopt);
  }
  menu->InvalidateLayout();
}

void DraggableMenuRunnerFactory::MoveModelElement(size_t from,
                                                  size_t to,
                                                  bool before) {
  if (from == to) {
    return;
  }

  CHECK(from < models_.size());
  Model model = models_[from];

  if (before) {
    if (from < to) {
      models_.insert(models_.begin() + to, model);
      models_.erase(models_.begin() + from);
    } else {
      models_.erase(models_.begin() + from);
      models_.insert(models_.begin() + to, model);
    }
  } else {
    if (to >= models_.size()) {
      models_.push_back(model);
      models_.erase(models_.begin() + from);
    } else if (from < to) {
      models_.insert(models_.begin() + to + 1, model);
      models_.erase(models_.begin() + from);
    } else {
      models_.erase(models_.begin() + from);
      models_.insert(models_.begin() + to + 1, model);
    }
  }
}

void DraggableMenuRunnerFactory::PerformDrop(
    MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  output_drag_op = ui::mojom::DragOperation::kNone;

  // Get the command ID of the menu item to be moved.
  std::optional<base::Pickle> pickle =
      event.data().GetPickledData(MenuItemClipboardFormatType());
  CHECK(pickle);
  int command_id;
  CHECK(base::PickleIterator(*pickle).ReadInt(&command_id));
  if (command_id == menu->GetCommand()) {
    PrintStatus("The menu item needn't be moved");
    return;
  }

  // Get the index of the drop target menu item.
  auto* parent_menu_item = menu->GetParentMenuItem();
  CHECK(parent_menu_item);
  auto* parent_submenu = parent_menu_item->GetSubmenu();
  CHECK(parent_submenu);

  // Get the index of the dragged menu item.
  auto* dragged_item = parent_menu_item->GetMenuItemByID(command_id);
  CHECK(dragged_item);
  auto drag_index = parent_submenu->GetIndexOf(dragged_item);
  CHECK(drag_index);

  // Get the index of the drop target menu item.
  auto drop_index = parent_submenu->GetIndexOf(menu);
  CHECK(drop_index);

  // Calculate the index to move the dragged menu item.
  if (position == DropPosition::kBefore || position == DropPosition::kAfter) {
    const size_t index_from = *drag_index;
    const size_t index_to = *drop_index;
    MoveModelElement(index_from, index_to, position == DropPosition::kBefore);
    BuildMenuItems(parent_menu_item);
    std::string status = base::StringPrintf(
        "Moved item %d to %d %s", index_from, index_to,
        position == DropPosition::kBefore ? "(before)" : "(after)");
    PrintStatus(status);
    output_drag_op = ui::mojom::DragOperation::kMove;
  } else {
    PrintStatus("Drop cancelled.");
  }
}

ui::ClipboardFormatType
DraggableMenuRunnerFactory::MenuItemClipboardFormatType() {
  constexpr char kDraggableItemType[] = "draggable-item-type";
  static base::NoDestructor<ui::ClipboardFormatType> type(
      ui::ClipboardFormatType::CustomPlatformType(kDraggableItemType));
  return *type;
}

MenuExample::MenuExample()
    : ExampleBase(GetStringUTF8(IDS_MENU_SELECT_LABEL).c_str()) {}

MenuExample::~MenuExample() = default;

void MenuExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetInteriorMargin(gfx::Insets(10))
      .SetOrientation(LayoutOrientation::kVertical)
      .SetDefault(views::kMarginsKey, gfx::Insets(10))
      .SetCrossAxisAlignment(LayoutAlignment::kStart);

  // We add a button to open a simple menu.
  auto example_menu_button = Builder<ExampleMenuButton>()
                                 .SetText(GetStringUTF16(IDS_MENU_BUTTON_LABEL))
                                 .Build();
  example_menu_button->set_menu_runner_factory(
      std::make_unique<ExampleMenuRunnerFactory>());

  example_menu_button->SetBorder(CreatePaddedBorder(
      CreateRoundedRectBorder(1, 5, kColorMenuButtonExampleBorder),
      LayoutProvider::Get()->GetInsetsMetric(
          InsetsMetric::INSETS_LABEL_BUTTON)));

  container->AddChildView(std::move(example_menu_button));

  // We add a button to open a menu with draggable items.
  auto draggable_menu_button =
      Builder<ExampleMenuButton>()
          .SetText(GetStringUTF16(IDS_DRAGGABLE_MENU_BUTTON_LABEL))
          .Build();
  draggable_menu_button->set_menu_runner_factory(
      std::make_unique<DraggableMenuRunnerFactory>());

  draggable_menu_button->SetBorder(CreatePaddedBorder(
      CreateRoundedRectBorder(1, 5, kColorMenuButtonExampleBorder),
      LayoutProvider::Get()->GetInsetsMetric(
          InsetsMetric::INSETS_LABEL_BUTTON)));

  container->AddChildView(std::move(draggable_menu_button));
}

}  // namespace views::examples
