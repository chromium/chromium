// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_MENU_MODEL_DELEGATE_H_
#define UI_BASE_MODELS_MENU_MODEL_DELEGATE_H_

namespace ui {

class MenuModelDelegate {
 public:
  // Invoked when an icon has been loaded from history. The |command_id|
  // may be part of a submenu, which is why we use command id here rather
  // than index.
  virtual void OnIconChanged(int command_id) = 0;

  // Invoked after items in |MenuModel| have been removed and/or added,
  // delegate should assume the entire contents of the model has changed.
  virtual void OnMenuStructureChanged() {}

  // Invoked when |MenuModel| is clearing its current delegate field. This
  // indicates to |this| that it is not that MenuModel's delegate anymore.
  virtual void OnMenuClearingDelegate() {}

 protected:
  virtual ~MenuModelDelegate() = default;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_MENU_MODEL_DELEGATE_H_
