// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_
#define UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_

#include "base/component_export.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"

#include <vector>

namespace ui {

// A simple data model for a combobox that takes a vector of
// SimpleComboboxModel::Item that has support for icons and secondary dropdown
// text. Items with empty text represent separators.
class COMPONENT_EXPORT(UI_BASE) SimpleComboboxModel : public ComboboxModel {
 public:
  struct COMPONENT_EXPORT(UI_BASE) Item {
    explicit Item(std::u16string text);
    Item(std::u16string text,
         std::u16string dropdown_secondary_text,
         ui::ImageModel icon);
    Item(const Item& other);
    Item& operator=(const Item& other);
    Item(Item&& other);
    Item& operator=(Item&& other);
    ~Item();

    static Item CreateSeparator();

    std::u16string text;
    std::u16string dropdown_secondary_text;
    ui::ImageModel icon;
  };

  explicit SimpleComboboxModel(std::vector<Item> items);

  SimpleComboboxModel(const SimpleComboboxModel&) = delete;
  SimpleComboboxModel& operator=(const SimpleComboboxModel&) = delete;

  ~SimpleComboboxModel() override;

  // Updates the list of items stored at `items_`.
  void UpdateItemList(std::vector<Item> items);

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::u16string GetDropDownSecondaryTextAt(size_t index) const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  bool IsItemSeparatorAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

 private:
  std::vector<Item> items_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_
