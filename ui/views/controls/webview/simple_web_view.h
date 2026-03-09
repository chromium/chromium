// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_SIMPLE_WEB_VIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_SIMPLE_WEB_VIEW_H_

#include <memory>

#include "ui/views/controls/webview/webview_export.h"

class GURL;

namespace views {

class View;
class WidgetDelegate;

// Interface for SimpleWebViewDialog to expose its functionality.
// SimpleWebViewDialog is a Window/Widget that contains a location-bar,
// lock-icon, and web-contents. This interface is here since it is available
// from both the browser and ash. The actual implementation of the dialog
// is in the browser. See SimpleWebViewDialog for more information about its
// use.
class WEBVIEW_EXPORT SimpleWebView {
 public:
  virtual ~SimpleWebView() = default;

  // Returns the underlying view.
  virtual View* GetView() = 0;

  // Initializes the view. Should be attached to a Widget before call.
  virtual void Init() = 0;

  // Creates a widget delegate for the view.
  virtual std::unique_ptr<WidgetDelegate> MakeWidgetDelegate() = 0;

  // Starts loading the given URL.
  virtual void StartLoad(const GURL& url) = 0;

  // Takes the underlying view. This will transfer ownership of the view to the
  // caller and the SimpleWebView instance will be destroyed.
  virtual std::unique_ptr<View> TakeView(
      std::unique_ptr<SimpleWebView> self) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_SIMPLE_WEB_VIEW_H_
