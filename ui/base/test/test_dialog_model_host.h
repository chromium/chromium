// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_
#define UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_

#include <algorithm>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_host.h"

namespace ui {

class TestDialogModelHost final : public DialogModelHost,
                                  public DialogModelFieldHost {
 public:
  enum class ButtonId {
    kCancel,
    kExtra,
    kOk,
  };

  explicit TestDialogModelHost(std::unique_ptr<DialogModel> dialog_model);

  TestDialogModelHost(const TestDialogModelHost&) = delete;
  TestDialogModelHost& operator=(const TestDialogModelHost&) = delete;

  ~TestDialogModelHost();

  // These are static method rather than a method on the host because this needs
  // to result with the destruction of the host.
  static void Accept(std::unique_ptr<TestDialogModelHost> host);
  static void Cancel(std::unique_ptr<TestDialogModelHost> host);
  static void Close(std::unique_ptr<TestDialogModelHost> host);
  static void DestroyWithoutAction(std::unique_ptr<TestDialogModelHost> host);

  void TriggerExtraButton(const ui::Event& event);

  // TODO(pbos): Consider requiring unique IDs here so that we don't need this
  // shorthand for querying for a specific field.
  DialogModelTextfield* FindSingleTextfield();
  void SetSingleTextfieldText(std::u16string text);

  const base::flat_set<Accelerator>& GetAccelerators(ButtonId button_id);
  const std::u16string& GetLabel(ButtonId button_id);
  ElementIdentifier GetId(ButtonId button_id);
  ElementIdentifier GetInitiallyFocusedField();

 private:
  // DialogModelHost:
  void Close() override;
  void OnDialogButtonChanged() override;

  std::unique_ptr<DialogModel> dialog_model_;
};

}  // namespace ui

#endif  // UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_
