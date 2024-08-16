// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_delegate.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/events/event.h"
#include "ui/views/controls/menu/menu_config.h"

namespace views {

MenuDelegate::~MenuDelegate() = default;

bool MenuDelegate::IsItemChecked(int id) const {
  return false;
}

std::u16string MenuDelegate::GetLabel(int id) const {
  return std::u16string();
}

const gfx::FontList* MenuDelegate::GetLabelFontList(int id) const {
  return nullptr;
}

std::optional<SkColor> MenuDelegate::GetLabelColor(int id) const {
  return std::nullopt;
}

std::u16string MenuDelegate::GetTooltipText(
    int id,
    const gfx::Point& screen_loc) const {
  return std::u16string();
}

bool MenuDelegate::GetAccelerator(int id, ui::Accelerator* accelerator) const {
  return false;
}

bool MenuDelegate::ShowContextMenu(MenuItemView* source,
                                   int id,
                                   const gfx::Point& p,
                                   ui::MenuSourceType source_type) {
  return false;
}

bool MenuDelegate::SupportsCommand(int id) const {
  return true;
}

bool MenuDelegate::IsCommandEnabled(int id) const {
  return true;
}

bool MenuDelegate::IsCommandVisible(int id) const {
  return true;
}

bool MenuDelegate::GetContextualLabel(int id, std::u16string* out) const {
  return false;
}

bool MenuDelegate::ShouldCloseAllMenusOnExecute(int id) {
  return true;
}

void MenuDelegate::ExecuteCommand(int id, int mouse_event_flags) {
  ExecuteCommand(id);
}

bool MenuDelegate::ShouldExecuteCommandWithoutClosingMenu(int id,
                                                          const ui::Event& e) {
  return false;
}

bool MenuDelegate::IsTriggerableEvent(MenuItemView* source,
                                      const ui::Event& e) {
  return e.type() == ui::EventType::kGestureTap ||
         e.type() == ui::EventType::kGestureTapDown ||
         (e.IsMouseEvent() &&
          (e.flags() & (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)));
}

bool MenuDelegate::CanDrop(MenuItemView* menu, const OSExchangeData& data) {
  return false;
}

bool MenuDelegate::GetDropFormats(
    MenuItemView* menu,
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return false;
}

bool MenuDelegate::AreDropTypesRequired(MenuItemView* menu) {
  return false;
}

ui::mojom::DragOperation MenuDelegate::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  NOTREACHED() << "If you override CanDrop, you must override this too";
}

views::View::DropCallback MenuDelegate::GetDropCallback(
    MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  NOTREACHED() << "If you override CanDrop, you must override this too";
}

bool MenuDelegate::CanDrag(MenuItemView* menu) {
  return false;
}

void MenuDelegate::WriteDragData(MenuItemView* sender, OSExchangeData* data) {
  NOTREACHED() << "If you override CanDrag, you must override this too.";
}

int MenuDelegate::GetDragOperations(MenuItemView* sender) {
  NOTREACHED() << "If you override CanDrag, you must override this too.";
}

bool MenuDelegate::ShouldCloseOnDragComplete() {
  return true;
}

MenuItemView* MenuDelegate::GetSiblingMenu(MenuItemView* menu,
                                           const gfx::Point& screen_point,
                                           MenuAnchorPosition* anchor,
                                           bool* has_mnemonics,
                                           MenuButton** button) {
  return nullptr;
}

int MenuDelegate::GetMaxWidthForMenu(MenuItemView* menu) {
  // NOTE: this needs to be large enough to accommodate the wrench menu with
  // big fonts.
  return 800;
}

void MenuDelegate::WillShowMenu(MenuItemView* menu) {}

void MenuDelegate::WillHideMenu(MenuItemView* menu) {}

bool MenuDelegate::ShouldTryPositioningBesideAnchor() const {
  return true;
}

bool MenuDelegate::IsTearingDown() const {
  return false;
}

}  // namespace views
