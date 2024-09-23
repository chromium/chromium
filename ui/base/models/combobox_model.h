// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_COMBOBOX_MODEL_H_
#define UI_BASE_MODELS_COMBOBOX_MODEL_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/color/color_id.h"

namespace ui {

class ComboboxModelObserver;
class ImageModel;

// A data model for a combo box.
class COMPONENT_EXPORT(UI_BASE) ComboboxModel {
 public:
  ComboboxModel();
  virtual ~ComboboxModel();

  // Returns the number of items in the combo box.
  virtual size_t GetItemCount() const = 0;

  // Returns the string at the specified index.
  virtual std::u16string GetItemAt(size_t index) const = 0;

  // Returns the secondary string at the specified index. Secondary strings are
  // displayed in a second line inside every menu item.
  virtual std::u16string GetDropDownSecondaryTextAt(size_t index) const;

  // Gets the icon for the item at the specified index. ImageModel is empty if
  // there is no icon.
  virtual ImageModel GetIconAt(size_t index) const;

  // Gets the icon for the item at |index|. ImageModel is empty if there is no
  // icon. By default, it returns GetIconAt(index).
  virtual ImageModel GetDropDownIconAt(size_t index) const;

  // Should return true if the item at |index| is a non-selectable separator
  // item.
  virtual bool IsItemSeparatorAt(size_t index) const;

  // The index of the item that is selected by default (before user
  // interaction).
  virtual std::optional<size_t> GetDefaultIndex() const;

  // Returns true if the item at |index| is enabled.
  virtual bool IsItemEnabledAt(size_t index) const;

  // Adds/removes an observer.
  void AddObserver(ComboboxModelObserver* observer);
  void RemoveObserver(ComboboxModelObserver* observer);

  // The foreground color of the dropdown. If not overridden, this returns
  // std::nullopt and the default color will be used.
  virtual std::optional<ui::ColorId> GetDropdownForegroundColorIdAt(
      size_t index) const;

  // The background color of the dropdown. If not overridden, this returns
  // std::nullopt and the default color will be used.
  virtual std::optional<ui::ColorId> GetDropdownBackgroundColorIdAt(
      size_t index) const;

  // The hover / selected color for the dropdown. If not overridden, this
  // returns std::nullopt and the default color will be used.
  virtual std::optional<ui::ColorId> GetDropdownSelectedBackgroundColorIdAt(
      size_t index) const;

 protected:
  base::ObserverList<ui::ComboboxModelObserver>& observers() {
    return observers_;
  }

 private:
  base::ObserverList<ui::ComboboxModelObserver> observers_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_COMBOBOX_MODEL_H_
