// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/views_text_services_context_menu.h"

#import <Cocoa/Cocoa.h>

#include "ui/base/cocoa/text_services_context_menu.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/decorated_text.h"
#import "ui/gfx/decorated_text_mac.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/views_text_services_context_menu_base.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// This class serves as a bridge to TextServicesContextMenu to add and handle
// text service items in the context menu. The items include Speech, Look Up
// and BiDi.
class ViewsTextServicesContextMenuMac
    : public ViewsTextServicesContextMenuBase,
      public ui::TextServicesContextMenu::Delegate {
 public:
  ViewsTextServicesContextMenuMac(ui::SimpleMenuModel* menu, Textfield* client);
  ViewsTextServicesContextMenuMac(const ViewsTextServicesContextMenuMac&) =
      delete;
  ViewsTextServicesContextMenuMac& operator=(
      const ViewsTextServicesContextMenuMac&) = delete;
  ~ViewsTextServicesContextMenuMac() override = default;

  // ViewsTextServicesContextMenu:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool SupportsCommand(int command_id) const override;

  // TextServicesContextMenu::Delegate:
  std::u16string GetSelectedText() const override;
  bool IsTextDirectionEnabled(
      base::i18n::TextDirection direction) const override;
  bool IsTextDirectionChecked(
      base::i18n::TextDirection direction) const override;
  void UpdateTextDirection(base::i18n::TextDirection direction) override;

 private:
  // Handler for the "Look Up" menu item.
  void LookUpInDictionary();

  ui::TextServicesContextMenu text_services_menu_{this};
};

ViewsTextServicesContextMenuMac::ViewsTextServicesContextMenuMac(
    ui::SimpleMenuModel* menu,
    Textfield* client)
    : ViewsTextServicesContextMenuBase(menu, client) {
  // Insert the "Look up" item in the first position.
  const std::u16string text = GetSelectedText();
  if (!text.empty()) {
    menu->InsertSeparatorAt(0, ui::NORMAL_SEPARATOR);
    menu->InsertItemAt(
        0, IDS_CONTENT_CONTEXT_LOOK_UP,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_LOOK_UP, text));

    text_services_menu_.AppendToContextMenu(menu);
  }

  text_services_menu_.AppendEditableItems(menu);
}

bool ViewsTextServicesContextMenuMac::IsCommandIdChecked(int command_id) const {
  return text_services_menu_.SupportsCommand(command_id)
             ? text_services_menu_.IsCommandIdChecked(command_id)
             : ViewsTextServicesContextMenuBase::IsCommandIdChecked(command_id);
}

bool ViewsTextServicesContextMenuMac::IsCommandIdEnabled(int command_id) const {
  if (text_services_menu_.SupportsCommand(command_id))
    return text_services_menu_.IsCommandIdEnabled(command_id);
  return (command_id == IDS_CONTENT_CONTEXT_LOOK_UP) ||
         ViewsTextServicesContextMenuBase::IsCommandIdEnabled(command_id);
}

void ViewsTextServicesContextMenuMac::ExecuteCommand(int command_id,
                                                     int event_flags) {
  if (text_services_menu_.SupportsCommand(command_id))
    text_services_menu_.ExecuteCommand(command_id, event_flags);
  else if (command_id == IDS_CONTENT_CONTEXT_LOOK_UP)
    LookUpInDictionary();
  else
    ViewsTextServicesContextMenuBase::ExecuteCommand(command_id, event_flags);
}

bool ViewsTextServicesContextMenuMac::SupportsCommand(int command_id) const {
  return text_services_menu_.SupportsCommand(command_id) ||
         command_id == IDS_CONTENT_CONTEXT_LOOK_UP ||
         ViewsTextServicesContextMenuBase::SupportsCommand(command_id);
}

std::u16string ViewsTextServicesContextMenuMac::GetSelectedText() const {
  return (client()->GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD)
             ? std::u16string()
             : client()->GetSelectedText();
}

bool ViewsTextServicesContextMenuMac::IsTextDirectionEnabled(
    base::i18n::TextDirection direction) const {
  if (client()->force_text_directionality())
    return false;
  return direction != base::i18n::UNKNOWN_DIRECTION;
}

bool ViewsTextServicesContextMenuMac::IsTextDirectionChecked(
    base::i18n::TextDirection direction) const {
  if (client()->force_text_directionality())
    return direction == base::i18n::UNKNOWN_DIRECTION;
  return IsTextDirectionEnabled(direction) &&
         client()->GetTextDirection() == direction;
}

void ViewsTextServicesContextMenuMac::UpdateTextDirection(
    base::i18n::TextDirection direction) {
  DCHECK(IsTextDirectionEnabled(direction));
  client()->ChangeTextDirectionAndLayoutAlignment(direction);
}

void ViewsTextServicesContextMenuMac::LookUpInDictionary() {
  gfx::DecoratedText text;
  gfx::Rect rect;
  if (client()->GetWordLookupDataFromSelection(&text, &rect)) {
    Widget* widget = client()->GetWidget();

    // We only care about the baseline of the glyph, not the space it occupies.
    gfx::Point baseline_point = rect.origin();
    views::View::ConvertPointToTarget(client(), widget->GetRootView(),
                                      &baseline_point);
    NSView* view = widget->GetNativeView().GetNativeNSView();
    NSPoint lookup_point = NSMakePoint(
        baseline_point.x(), NSHeight([view frame]) - baseline_point.y());
    [view showDefinitionForAttributedString:
              gfx::GetAttributedStringFromDecoratedText(text)
                                    atPoint:lookup_point];
  }
}

}  // namespace

// static
std::unique_ptr<ViewsTextServicesContextMenu>
ViewsTextServicesContextMenu::Create(ui::SimpleMenuModel* menu,
                                     Textfield* client) {
  return std::make_unique<ViewsTextServicesContextMenuMac>(menu, client);
}

}  // namespace views
