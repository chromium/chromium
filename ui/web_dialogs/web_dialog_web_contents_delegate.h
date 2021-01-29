// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_WEB_DIALOG_WEB_CONTENTS_DELEGATE_H_
#define UI_WEB_DIALOGS_WEB_DIALOG_WEB_CONTENTS_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/web_dialogs/web_dialogs_export.h"

namespace content {
class BrowserContext;
}

namespace ui {

// This class implements (and mostly ignores) most of
// content::WebContentsDelegate for use in a Web dialog. Subclasses need only
// override a few methods instead of the everything from
// content::WebContentsDelegate; this way, implementations on all platforms
// behave consistently.
class WEB_DIALOGS_EXPORT WebDialogWebContentsDelegate
    : public content::WebContentsDelegate {
 public:
  // Handles OpenURLFromTab and AddNewContents for WebDialogWebContentsDelegate.
  class WebContentsHandler {
   public:
    virtual ~WebContentsHandler() {}
    virtual content::WebContents* OpenURLFromTab(
        content::BrowserContext* context,
        content::WebContents* source,
        const content::OpenURLParams& params) = 0;
    virtual void AddNewContents(
        content::BrowserContext* context,
        content::WebContents* source,
        std::unique_ptr<content::WebContents> new_contents,
        const GURL& target_url,
        WindowOpenDisposition disposition,
        const gfx::Rect& initial_rect,
        bool user_gesture) = 0;
  };

  // |context| and |handler| must be non-NULL.
  WebDialogWebContentsDelegate(content::BrowserContext* context,
                               std::unique_ptr<WebContentsHandler> handler);
  ~WebDialogWebContentsDelegate() override;

  // The returned browser context is guaranteed to be original if non-NULL.
  content::BrowserContext* browser_context() const {
    return browser_context_;
  }

  // Calling this causes all following events sent from the
  // WebContents object to be ignored.  It also makes all following
  // calls to browser_context() return NULL.
  void Detach();

  // content::WebContentsDelegate declarations.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;

 private:
  // Weak pointer.  Always an original profile.
  content::BrowserContext* browser_context_;

  std::unique_ptr<WebContentsHandler> const handler_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogWebContentsDelegate);
};

}  // namespace ui

#endif  // UI_WEB_DIALOGS_WEB_DIALOG_WEB_CONTENTS_DELEGATE_H_
