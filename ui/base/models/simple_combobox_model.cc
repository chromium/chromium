// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/simple_combobox_model.h"

#include <utility>

#include "ui/base/models/combobox_model_observer.h"

namespace ui {

SimpleComboboxModel::Item::Item(std::u16string text) : text(std::move(text)) {}
SimpleComboboxModel::Item::Item(std::u16string text,
                                std::u16string dropdown_secondary_text,
                                ui::ImageModel icon)
    : text(std::move(text)),
      dropdown_secondary_text(std::move(dropdown_secondary_text)),
      icon(std::move(icon)) {}
SimpleComboboxModel::Item::Item(const SimpleComboboxModel::Item& other) =
    default;
SimpleComboboxModel::Item& SimpleComboboxModel::Item::operator=(
    const SimpleComboboxModel::Item& other) = default;
SimpleComboboxModel::Item::Item(SimpleComboboxModel::Item&& other) = default;
SimpleComboboxModel::Item& SimpleComboboxModel::Item::operator=(
    SimpleComboboxModel::Item&& other) = default;
SimpleComboboxModel::Item::~Item() = default;

// static
SimpleComboboxModel::Item SimpleComboboxModel::Item::CreateSeparator() {
  return SimpleComboboxModel::Item(std::u16string());
}

SimpleComboboxModel::SimpleComboboxModel(std::vector<Item> items)
    : items_(std::move(items)) {}

SimpleComboboxModel::~SimpleComboboxModel() = default;

void SimpleComboboxModel::UpdateItemList(std::vector<Item> items) {
  items_ = std::move(items);

  for (auto& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

size_t SimpleComboboxModel::GetItemCount() const {
  return items_.size();
}

std::u16string SimpleComboboxModel::GetItemAt(size_t index) const {
  return items_[index].text;
}

std::u16string SimpleComboboxModel::GetDropDownSecondaryTextAt(
    size_t index) const {
  return items_[index].dropdown_secondary_text;
}

ui::ImageModel SimpleComboboxModel::GetIconAt(size_t index) const {
  return items_[index].icon;
}

bool SimpleComboboxModel::IsItemSeparatorAt(size_t index) const {
  return items_[index].text.empty();
}

std::optional<size_t> SimpleComboboxModel::GetDefaultIndex() const {
  return items_.empty() ? std::nullopt : std::make_optional(0u);
}

}  // namespace ui
