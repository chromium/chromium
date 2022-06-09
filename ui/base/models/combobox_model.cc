// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/combobox_model.h"

#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/image_model.h"

namespace ui {

ComboboxModel::ComboboxModel() = default;

ComboboxModel::~ComboboxModel() {
  for (auto& observer : observers_)
    observer.OnComboboxModelDestroying(this);
}

std::u16string ComboboxModel::GetDropDownTextAt(int index) const {
  return GetItemAt(index);
}

std::u16string ComboboxModel::GetDropDownSecondaryTextAt(int index) const {
  return std::u16string();
}

ImageModel ComboboxModel::GetIconAt(int index) const {
  return ui::ImageModel();
}

ImageModel ComboboxModel::GetDropDownIconAt(int index) const {
  return GetIconAt(index);
}

bool ComboboxModel::IsItemSeparatorAt(int index) const {
  return false;
}

int ComboboxModel::GetDefaultIndex() const {
  return 0;
}

bool ComboboxModel::IsItemEnabledAt(int index) const {
  return true;
}

void ComboboxModel::AddObserver(ComboboxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ComboboxModel::RemoveObserver(ComboboxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
