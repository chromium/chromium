// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_DIALOG_MODEL_MENU_MODEL_ADAPTER_H_
#define UI_BASE_MODELS_DIALOG_MODEL_MENU_MODEL_ADAPTER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_host.h"
#include "ui/base/models/menu_model.h"

namespace ui {

class DialogModel;

class COMPONENT_EXPORT(UI_BASE) DialogModelMenuModelAdapter final
    : public DialogModelHost,
      public DialogModelFieldHost,
      public MenuModel {
 public:
  explicit DialogModelMenuModelAdapter(std::unique_ptr<DialogModel> model);
  ~DialogModelMenuModelAdapter() override;

  // DialogModelHost:
  void Close() override;
  void OnDialogButtonChanged() override;

  // MenuModel:
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override;
  size_t GetItemCount() const override;
  ItemType GetTypeAt(size_t index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override;
  int GetCommandIdAt(size_t index) const override;
  std::u16string GetLabelAt(size_t index) const override;
  bool IsItemDynamicAt(size_t index) const override;
  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(size_t index) const override;
  int GetGroupIdAt(size_t index) const override;
  ImageModel GetIconAt(size_t index) const override;
  ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override;
  bool IsEnabledAt(size_t index) const override;
  ElementIdentifier GetElementIdentifierAt(size_t index) const override;
  MenuModel* GetSubmenuModelAt(size_t index) const override;
  void ActivatedAt(size_t index) override;
  void ActivatedAt(size_t index, int event_flags) override;

 private:
  const DialogModelField* GetField(size_t index) const;
  DialogModelField* GetField(size_t index);

  std::unique_ptr<DialogModel> model_;

  base::WeakPtrFactory<DialogModelMenuModelAdapter> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_MODELS_DIALOG_MODEL_MENU_MODEL_ADAPTER_H_
