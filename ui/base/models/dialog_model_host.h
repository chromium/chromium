// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_DIALOG_MODEL_HOST_H_
#define UI_BASE_MODELS_DIALOG_MODEL_HOST_H_

#include "base/util/type_safety/pass_key.h"

namespace ui {

class DialogModel;

// Platform-agnostic interface for toolkit integrations.
class COMPONENT_EXPORT(UI_BASE) DialogModelHost {
 public:
  // Immediately closes the DialogModelHost. Calling Close() destroys the
  // DialogModel and no subsequent calls should be made into either DialogModel
  // or DialogModelHost.
  virtual void Close() = 0;

  // Selects all text of a textfield.
  // TODO(pbos): Reconsider whether this should be implied by if the textfield
  // is initially focused.
  virtual void SelectAllText(int unique_id) = 0;

 protected:
  friend class DialogModel;
  friend class DialogModelField;

  // This PassKey is used to make sure that some methods on DialogModel
  // are only called as part of the host integration.
  static util::PassKey<DialogModelHost> GetPassKey() {
    return util::PassKey<DialogModelHost>();
  }

  // Called when various parts of the model changes.
  // TODO(pbos): Break this down to API that says what was added/removed/changed
  // to not have to reset everything.
  virtual void OnFieldAdded(DialogModelField* field) = 0;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_DIALOG_MODEL_HOST_H_
