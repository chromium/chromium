// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_
#define UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/views/controls/views_text_services_context_menu.h"
#include "ui/views/views_export.h"

namespace views {

// This base class is used to add and handle text service items in the textfield
// context menu. Specific platforms may subclass and add additional items.
class VIEWS_EXPORT ViewsTextServicesContextMenuBase
    : public ViewsTextServicesContextMenu {
 public:
  ViewsTextServicesContextMenuBase(ui::SimpleMenuModel* menu,
                                   Textfield* client);
  ViewsTextServicesContextMenuBase(const ViewsTextServicesContextMenuBase&) =
      delete;
  ViewsTextServicesContextMenuBase& operator=(
      const ViewsTextServicesContextMenuBase&) = delete;
  ~ViewsTextServicesContextMenuBase() override;

  // ViewsTextServicesContextMenu:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool SupportsCommand(int command_id) const override;

 protected:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  Textfield* client() { return client_; }
  const Textfield* client() const { return client_; }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Returns the string ID of the clipboard history menu option.
  int GetClipboardHistoryStringId() const;
#endif

 private:
  // The view associated with the menu. Weak. Owns |this|.
  const raw_ptr<Textfield> client_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_
