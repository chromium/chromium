// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_COMBOBOX_MODEL_H_
#define UI_BASE_MODELS_COMBOBOX_MODEL_H_

#include <string>

#include "base/component_export.h"
#include "base/observer_list.h"

namespace ui {

class ComboboxModelObserver;
class ImageModel;

// A data model for a combo box.
class COMPONENT_EXPORT(UI_BASE) ComboboxModel {
 public:
  ComboboxModel();
  virtual ~ComboboxModel();

  // Returns the number of items in the combo box.
  virtual int GetItemCount() const = 0;

  // Returns the string at the specified index.
  virtual std::u16string GetItemAt(int index) const = 0;

  // Returns the string to be shown in the dropdown for the item at |index|. By
  // default, it returns GetItemAt(index).
  virtual std::u16string GetDropDownTextAt(int index) const;

  // Returns the secondary string at the specified index. Secondary strings are
  // displayed in a second line inside every menu item.
  virtual std::u16string GetDropDownSecondaryTextAt(int index) const;

  // Gets the icon for the item at the specified index. ImageModel is empty if
  // there is no icon.
  virtual ImageModel GetIconAt(int index) const;

  // Gets the icon for the item at |index|. ImageModel is empty if there is no
  // icon. By default, it returns GetIconAt(index).
  virtual ImageModel GetDropDownIconAt(int index) const;

  // Should return true if the item at |index| is a non-selectable separator
  // item.
  virtual bool IsItemSeparatorAt(int index) const;

  // The index of the item that is selected by default (before user
  // interaction).
  virtual int GetDefaultIndex() const;

  // Returns true if the item at |index| is enabled.
  virtual bool IsItemEnabledAt(int index) const;

  // Adds/removes an observer.
  void AddObserver(ComboboxModelObserver* observer);
  void RemoveObserver(ComboboxModelObserver* observer);

 protected:
  base::ObserverList<ui::ComboboxModelObserver>& observers() {
    return observers_;
  }

 private:
  base::ObserverList<ui::ComboboxModelObserver> observers_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_COMBOBOX_MODEL_H_
