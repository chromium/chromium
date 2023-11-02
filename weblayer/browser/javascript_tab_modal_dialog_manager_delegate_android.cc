// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/javascript_tab_modal_dialog_manager_delegate_android.h"

#include "components/javascript_dialogs/android/tab_modal_dialog_view_android.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

JavaScriptTabModalDialogManagerDelegateAndroid::
    JavaScriptTabModalDialogManagerDelegateAndroid(
        content::WebContents* web_contents)
    : web_contents_(web_contents) {}

JavaScriptTabModalDialogManagerDelegateAndroid::
    ~JavaScriptTabModalDialogManagerDelegateAndroid() = default;

base::WeakPtr<javascript_dialogs::TabModalDialogView>
JavaScriptTabModalDialogManagerDelegateAndroid::CreateNewDialog(
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback
        callback_on_button_clicked,
    base::OnceClosure callback_on_cancelled) {
  return javascript_dialogs::TabModalDialogViewAndroid::Create(
      web_contents_, alerting_web_contents, title, dialog_type, message_text,
      default_prompt_text, std::move(callback_on_button_clicked),
      std::move(callback_on_cancelled));
}

void JavaScriptTabModalDialogManagerDelegateAndroid::WillRunDialog() {}

void JavaScriptTabModalDialogManagerDelegateAndroid::DidCloseDialog() {}

void JavaScriptTabModalDialogManagerDelegateAndroid::SetTabNeedsAttention(
    bool attention) {}

bool JavaScriptTabModalDialogManagerDelegateAndroid::IsWebContentsForemost() {
  // TODO(estade): this should also check if the browser is active/showing.
  DCHECK(TabImpl::FromWebContents(web_contents_));
  return TabImpl::FromWebContents(web_contents_)->IsActive();
}

bool JavaScriptTabModalDialogManagerDelegateAndroid::IsApp() {
  return false;
}

}  // namespace weblayer
