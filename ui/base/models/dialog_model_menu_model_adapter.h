// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_DIALOG_MODEL_MENU_MODEL_ADAPTER_H_
#define UI_BASE_MODELS_DIALOG_MODEL_MENU_MODEL_ADAPTER_H_

#include <memory>

#include "ui/base/models/dialog_model_host.h"
#include "ui/base/models/menu_model.h"

namespace ui {

class DialogModel;

class COMPONENT_EXPORT(UI_BASE) DialogModelMenuModelAdapter final
    : public DialogModelHost,
      public MenuModel {
 public:
  explicit DialogModelMenuModelAdapter(std::unique_ptr<DialogModel> model);
  ~DialogModelMenuModelAdapter() override;

  // DialogModelHost:
  void Close() override;
  void OnFieldAdded(DialogModelField* field) override;

  // MenuModel:
  bool HasIcons() const override;
  int GetItemCount() const override;
  ItemType GetTypeAt(int index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override;
  int GetCommandIdAt(int index) const override;
  std::u16string GetLabelAt(int index) const override;
  bool IsItemDynamicAt(int index) const override;
  bool GetAcceleratorAt(int index, ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(int index) const override;
  int GetGroupIdAt(int index) const override;
  ImageModel GetIconAt(int index) const override;
  ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override;
  bool IsEnabledAt(int index) const override;
  MenuModel* GetSubmenuModelAt(int index) const override;
  void ActivatedAt(int index) override;
  void ActivatedAt(int index, int event_flags) override;

 private:
  const DialogModelField* GetField(int index) const;
  DialogModelField* GetField(int index);

  std::unique_ptr<DialogModel> model_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_DIALOG_MODEL_MENU_MODEL_ADAPTER_H_
