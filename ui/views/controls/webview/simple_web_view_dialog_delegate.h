// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_SIMPLE_WEB_VIEW_DIALOG_DELEGATE_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_SIMPLE_WEB_VIEW_DIALOG_DELEGATE_H_

#include <memory>

#include "content/public/browser/invalidate_type.h"
#include "ui/views/controls/webview/webview_export.h"

namespace content {
class WebContents;
}

namespace views {

class WidgetDelegate;

// Interface for receiving callbacks from SimpleWebViewDialog.
class WEBVIEW_EXPORT SimpleWebViewDialogDelegate {
 public:
  virtual ~SimpleWebViewDialogDelegate() = default;

  // Called when the navigation state changes.
  virtual void OnNavigationStateChanged(
      content::WebContents* source,
      content::InvalidateTypes changed_flags) = 0;

  // Called when the loading state changes.
  virtual void OnLoadingStateChanged(content::WebContents* source,
                                     bool to_different_document) = 0;

  // Allows the delegate to customize the widget delegate.
  virtual std::unique_ptr<WidgetDelegate> MakeWidgetDelegate(
      std::unique_ptr<WidgetDelegate> base_delegate) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_SIMPLE_WEB_VIEW_DIALOG_DELEGATE_H_
