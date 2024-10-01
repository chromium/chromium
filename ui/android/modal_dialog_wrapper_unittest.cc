// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/modal_dialog_wrapper.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/fake_modal_dialog_manager_bridge.h"
#include "ui/android/window_android.h"
#include "ui/base/models/dialog_model.h"

namespace ui {

TEST(ModalDialogWrapperTest, ShowTabModal) {
  bool ok_called = false;
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(u"title")
      .AddParagraph(ui::DialogModelLabel(u"paragraph"))
      .AddOkButton(
          base::BindLambdaForTesting([&ok_called]() { ok_called = true; }),
          ui::DialogModel::Button::Params().SetLabel(u"ok"))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(u"cancel"))
      .SetCloseActionCallback(base::DoNothing());

  auto window = ui::WindowAndroid::CreateForTesting();
  auto fake_dialog_manager = FakeModalDialogManagerBridge::CreateForTab(
      window->get(), /*use_empty_java_presenter=*/false);

  ModalDialogWrapper::ShowTabModal(dialog_builder.Build(), window->get());
  fake_dialog_manager->ClickPositiveButton();
  EXPECT_TRUE(ok_called);
}

TEST(ModalDialogWrapperTest, CloseDialogFromNative) {
  bool closed = false;
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(u"title")
      .AddParagraph(ui::DialogModelLabel(u"paragraph"))
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params().SetLabel(u"ok"))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(u"cancel"))
      .SetCloseActionCallback(
          base::BindLambdaForTesting([&closed]() { closed = true; }));

  auto window = ui::WindowAndroid::CreateForTesting();
  auto fake_dialog_manager = FakeModalDialogManagerBridge::CreateForTab(
      window->get(), /*use_empty_java_presenter=*/false);

  ModalDialogWrapper::ShowTabModal(dialog_builder.Build(), window->get());
  ModalDialogWrapper::GetDialogForTesting()->Close();
  EXPECT_TRUE(closed);
}

}  // namespace ui
